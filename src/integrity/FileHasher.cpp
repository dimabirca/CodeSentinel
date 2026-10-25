#include "codesentinel/integrity/FileHasher.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
    void checkStatus(NTSTATUS status)
    {
        if (!BCRYPT_SUCCESS(status))
        {
            throw std::runtime_error("Erro na API BCrypt.");
        }
    }

    class HashContext
    {
    public:
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;

        HashContext() = default;
        HashContext(const HashContext&) = delete;
        HashContext& operator=(const HashContext&) = delete;

        ~HashContext()
        {
            if (hash != nullptr)
                BCryptDestroyHash(hash);
            if (algorithm != nullptr)
                BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    };

    class FileHandle
    {
    public:
        explicit FileHandle(const std::filesystem::path& path)
        {
            // OPEN_REPARSE_POINT impede seguir um link no componente final.
            // FILE_SHARE_READ impede novas aberturas para escrita enquanto
            // este handle estiver aberto (sujeito aos handles ja existentes).
            handle_ = CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr
            );

            if (handle_ == INVALID_HANDLE_VALUE)
            {
                throw std::runtime_error(
                    "Nao foi possivel abrir o ficheiro: " + path.string()
                );
            }
        }

        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;

        ~FileHandle()
        {
            if (handle_ != INVALID_HANDLE_VALUE)
                CloseHandle(handle_);
        }

        HANDLE get() const { return handle_; }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };

    struct FileSnapshot
    {
        BY_HANDLE_FILE_INFORMATION info{};
        FILE_BASIC_INFO basic{};
    };

    FileSnapshot snapshot(HANDLE handle)
    {
        FileSnapshot result{};
        if (!GetFileInformationByHandle(handle, &result.info) ||
            !GetFileInformationByHandleEx(
                handle, FileBasicInfo, &result.basic, sizeof(result.basic)))
        {
            throw std::runtime_error("Erro ao consultar metadados do ficheiro.");
        }
        return result;
    }

    bool sameFileState(const FileSnapshot& a, const FileSnapshot& b)
    {
        return a.info.dwVolumeSerialNumber == b.info.dwVolumeSerialNumber &&
               a.info.nFileIndexHigh == b.info.nFileIndexHigh &&
               a.info.nFileIndexLow == b.info.nFileIndexLow &&
               a.info.nFileSizeHigh == b.info.nFileSizeHigh &&
               a.info.nFileSizeLow == b.info.nFileSizeLow &&
               a.basic.LastWriteTime.QuadPart == b.basic.LastWriteTime.QuadPart &&
               a.basic.ChangeTime.QuadPart == b.basic.ChangeTime.QuadPart;
    }
}

std::string FileHasher::sha256(const std::filesystem::path& filePath)
{
    FileHandle file(filePath);
    const FileSnapshot before = snapshot(file.get());

    if ((before.info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (before.info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
        throw std::runtime_error(
            "Ficheiro inseguro: diretorio ou reparse point: " + filePath.string()
        );
    }

    HashContext context;
    checkStatus(BCryptOpenAlgorithmProvider(
        &context.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));

    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD bytesReturned = 0;

    checkStatus(BCryptGetProperty(
        context.algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
        &bytesReturned, 0));

    checkStatus(BCryptGetProperty(
        context.algorithm, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
        &bytesReturned, 0));

    std::vector<unsigned char> hashObject(objectLength);
    std::vector<unsigned char> hashValue(hashLength);

    checkStatus(BCryptCreateHash(
        context.algorithm, &context.hash, hashObject.data(),
        static_cast<ULONG>(hashObject.size()), nullptr, 0, 0));

    std::array<unsigned char, 65536> buffer{};
    std::uint64_t totalRead = 0;

    for (;;)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(file.get(), buffer.data(),
                      static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
        {
            throw std::runtime_error("Erro durante a leitura do ficheiro.");
        }
        if (bytesRead == 0)
            break;

        totalRead += bytesRead;
        checkStatus(BCryptHashData(context.hash, buffer.data(), bytesRead, 0));
    }

    const FileSnapshot after = snapshot(file.get());
    const std::uint64_t initialSize =
        (static_cast<std::uint64_t>(before.info.nFileSizeHigh) << 32) |
        before.info.nFileSizeLow;

    if (totalRead != initialSize || !sameFileState(before, after))
    {
        throw std::runtime_error(
            "O ficheiro foi alterado durante o hashing: " + filePath.string()
        );
    }

    checkStatus(BCryptFinishHash(
        context.hash, hashValue.data(),
        static_cast<ULONG>(hashValue.size()), 0));

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char byte : hashValue)
        output << std::setw(2) << static_cast<unsigned int>(byte);

    return output.str();
}
