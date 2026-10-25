
#include "codesentinel/integrity/FileHasher.hpp"

#include <array>
#include <chrono>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void writeFile(
        const fs::path& path,
        const std::string& content
    )
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);

        if (!file)
        {
            throw std::runtime_error(
                "Nao foi possivel criar ficheiro: " + path.string()
            );
        }

        file.write(
            content.data(),
            static_cast<std::streamsize>(content.size())
        );

        file.close();

        if (!file)
        {
            throw std::runtime_error(
                "Erro ao escrever ficheiro: " + path.string()
            );
        }
    }

    void testEmptyFile(const fs::path& root)
    {
        const fs::path file = root / "empty.txt";
        writeFile(file, "");

        const std::string expected =
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855";

        const std::string actual = FileHasher::sha256(file);

        require(actual == expected,
            "SHA-256 incorreto para ficheiro vazio.");

        std::cout << "[PASS] Empty file\n";
    }

    void testKnownContent(const fs::path& root)
    {
        const fs::path file = root / "abc.txt";
        writeFile(file, "abc");

        const std::string expected =
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad";

        const std::string actual = FileHasher::sha256(file);

        require(actual == expected,
            "SHA-256 incorreto para o conteudo abc.");

        std::cout << "[PASS] Known content\n";
    }

    void testMissingFile(const fs::path& root)
    {
        const fs::path file = root / "missing.txt";

        bool exceptionThrown = false;

        try
        {
            FileHasher::sha256(file);
        }
        catch (const std::runtime_error&)
        {
            exceptionThrown = true;
        }

        require(exceptionThrown,
            "Ficheiro inexistente deveria provocar erro.");

        std::cout << "[PASS] Missing file\n";
    }

    void testModifiedContent(const fs::path& root)
    {
        const fs::path file = root / "modified.txt";

        writeFile(file, "original");
        const std::string firstHash = FileHasher::sha256(file);

        writeFile(file, "alterado");
        const std::string secondHash = FileHasher::sha256(file);

        require(firstHash != secondHash,
            "O hash nao mudou apos alterar o ficheiro.");

        std::cout << "[PASS] Modified content\n";
    }

    void testLargeFile(const fs::path& root)
    {
        const fs::path file = root / "large.txt";

        // Um milhao de caracteres 'a', para testar
        // a leitura em varios blocos de 64 KiB.
        writeFile(file, std::string(1000000, 'a'));

        const std::string expected =
            "cdc76e5c9914fb9281a1c7e284d73e67"
            "f1809a48a497200e046d39ccc7112cd0";

        const std::string actual = FileHasher::sha256(file);

        require(actual == expected,
            "SHA-256 incorreto para ficheiro grande.");

        std::cout << "[PASS] Large file\n";
    }

    void testBlockBoundaries(const fs::path& root)
    {
        const std::array<std::size_t, 4> sizes = {
            65535, 65536, 65537, 131072
        };
        const std::array<std::string, 4> expected = {
            "6e1bebca6a8229364a162a72ef064826c4cd7457bf54f190ef782bd9deff3e42", "bf718b6f653bebc184e1479f1935b8da974d701b893afcf49e701f3e2f9f9c5a", "008ffc88d3c96a9f307524eb361e47c5222a887fc45fa0c1fb8d429c5c23b430", "b44ffb72fcc259676bd80495fef1b44b808ca8f1ffe1b1706a4d7911b0e31f11"
        };

        const fs::path file = root / "block_boundaries.bin";
        for (std::size_t i = 0; i < sizes.size(); ++i)
        {
            writeFile(file, std::string(sizes[i], 'a'));
            require(
                FileHasher::sha256(file) == expected[i],
                "SHA-256 incorreto no limite de bloco: " +
                    std::to_string(sizes[i])
            );
        }
        std::cout << "[PASS] Block boundaries\n";
    }

    void testDirectoryRejected(const fs::path& root)
    {
        const fs::path directory = root / "not_a_file";
        fs::create_directories(directory);

        bool rejected = false;
        try
        {
            (void)FileHasher::sha256(directory);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        require(rejected, "Um diretorio deveria ser rejeitado.");
        std::cout << "[PASS] Directory rejected\n";
    }

    void testSymbolicLinkRejected(const fs::path& root)
    {
        const fs::path target = root / "link_target.txt";
        const fs::path link = root / "link_to_target.txt";
        writeFile(target, "conteudo de teste");

        std::error_code ec;
        fs::create_symlink(target, link, ec);
        if (ec)
        {
            // A criacao de symlinks pode exigir permissoes especiais.
            std::cout << "[SKIP] Symbolic link (sem permissao para criar)\n";
            return;
        }

        bool rejected = false;
        try
        {
            (void)FileHasher::sha256(link);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        require(rejected, "Um link simbolico deveria ser rejeitado.");
        std::cout << "[PASS] Symbolic link rejected\n";
    }
}

int main()
{
    fs::path testRoot;

    try
    {
        const auto timestamp =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        testRoot = fs::temp_directory_path() /
            ("codesentinel_integrity_tests_" + std::to_string(timestamp));

        fs::create_directories(testRoot);

        testEmptyFile(testRoot);
        testKnownContent(testRoot);
        testMissingFile(testRoot);
        testModifiedContent(testRoot);
        testLargeFile(testRoot);
        testBlockBoundaries(testRoot);
        testDirectoryRejected(testRoot);
        testSymbolicLinkRejected(testRoot);

        fs::remove_all(testRoot);

        std::cout << "All integrity tests passed!\n";

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] " << e.what() << "\n";

        if (!testRoot.empty())
        {
            std::error_code ec;
            fs::remove_all(testRoot, ec);
        }

        return 1;
    }
}
