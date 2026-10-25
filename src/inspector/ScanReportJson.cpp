#include "codesentinel/inspector/ScanReportJson.hpp"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <utility>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

std::runtime_error windowsError(const std::string& message, DWORD code)
{
    return std::runtime_error(message + " (Windows: " + std::to_string(code) + ")");
}

class FileHandle {
public:
    explicit FileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
    ~FileHandle() { close(); }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    HANDLE get() const { return handle_; }
    void close()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }
private:
    HANDLE handle_;
};

class TemporaryFile {
public:
    explicit TemporaryFile(fs::path path) : path_(std::move(path)) {}
    ~TemporaryFile()
    {
        if (!committed_) {
            DeleteFileW(path_.c_str());
        }
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    void commit() { committed_ = true; }
private:
    fs::path path_;
    bool committed_ = false;
};

void checkDestination(const fs::path& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return;
        }
        throw windowsError("Nao foi possivel verificar o destino JSON", error);
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        throw std::runtime_error("Destino JSON nao pode ser um link ou reparse point.");
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        throw std::runtime_error("Destino JSON e um diretorio.");
    }
}

} // namespace

nlohmann::json ScanReportJson::toJson(const ScanResult& result)
{
    nlohmann::json report;
    report["schema_version"] = 1;
    report["summary"] = {
        {"total_files", result.totalFiles},
        {"total_directories", result.totalDirectories}
    };
    report["languages"] = nlohmann::json::object();
    for (const auto& language : result.languages) {
        report["languages"][language.first] = language.second;
    }
    report["status"] = {
        {"completed", result.completed},
        {"file_limit_reached", result.fileLimitReached},
        {"depth_limit_reached", result.depthLimitReached}
    };
    report["exclusions"] = nlohmann::json::array();
    for (const auto& exclusion : result.exclusions) {
        report["exclusions"].push_back({
            {"path", exclusion.path.generic_string()},
            {"reason", exclusion.reason}
        });
    }
    return report;
}

void ScanReportJson::save(const ScanResult& result, const fs::path& outputPath)
{
    if (outputPath.empty() || outputPath.filename().empty()) {
        throw std::runtime_error("Caminho do relatorio JSON invalido.");
    }

    const std::string content = toJson(result).dump(2) + '\n';
    const fs::path absolutePath = fs::absolute(outputPath);
    checkDestination(absolutePath);

    // O temporario fica no mesmo diretorio para permitir uma substituicao
    // por renomeacao dentro do mesmo volume.
    fs::path temporaryPath;
    HANDLE rawHandle = INVALID_HANDLE_VALUE;
    const std::wstring base = absolutePath.filename().wstring();
    const std::wstring prefix = base + L".tmp-" +
        std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64()) + L"-";

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        temporaryPath = absolutePath.parent_path() /
            (prefix + std::to_wstring(attempt));
        rawHandle = CreateFileW(
            temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (rawHandle != INVALID_HANDLE_VALUE) {
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            throw windowsError("Nao foi possivel criar ficheiro temporario JSON", GetLastError());
        }
    }
    if (rawHandle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Nao foi possivel obter nome temporario exclusivo para JSON.");
    }

    TemporaryFile cleanup(temporaryPath);
    FileHandle file(rawHandle);
    std::size_t offset = 0;
    while (offset < content.size()) {
        const DWORD count = static_cast<DWORD>(
            (std::min)(content.size() - offset, static_cast<std::size_t>(1024 * 1024)));
        DWORD written = 0;
        if (!WriteFile(file.get(), content.data() + offset, count, &written, nullptr)) {
            throw windowsError("Erro ao escrever relatorio JSON", GetLastError());
        }
        if (written == 0) {
            throw std::runtime_error("Escrita incompleta do relatorio JSON.");
        }
        offset += written;
    }
    if (!FlushFileBuffers(file.get())) {
        throw windowsError("Erro ao sincronizar relatorio JSON", GetLastError());
    }
    file.close();

    // Revalidar antes de substituir; nao elimina corridas entre processos.
    checkDestination(absolutePath);
    if (!MoveFileExW(temporaryPath.c_str(), absolutePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw windowsError("Nao foi possivel substituir relatorio JSON", GetLastError());
    }
    cleanup.commit();
}
