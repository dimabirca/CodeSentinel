
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include "codesentinel/inspector/ProjectScanner.hpp"

namespace fs = std::filesystem;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void createFile(const fs::path& path)
{
    std::ofstream file(path);

    require(
        file.is_open(),
        "Nao foi possivel criar: " + path.string()
    );

    file << "// Teste CodeSentinel\n";
}

void testBasicScan(const fs::path& root)
{
    fs::create_directories(root / "src");
    fs::create_directories(root / "build");

    createFile(root / "src" / "main.cpp");
    createFile(root / "src" / "utils.hpp");
    createFile(root / "README.md");
    createFile(root / "build" / "ignored.cpp");

    ProjectScanner scanner;
    const ScanResult result = scanner.scan(root);

    require(result.completed, "Scan basico incompleto.");
    require(result.totalFiles == 3, "Contagem de ficheiros incorreta.");
    require(result.totalDirectories == 1, "Contagem de diretorios incorreta.");

    require(result.languages.at("C++") == 1, "C++ nao detetado.");
    require(result.languages.at("Headers") == 1, "Headers nao detetados.");
    require(result.languages.at("Markdown") == 1, "Markdown nao detetado.");

    std::cout << "[PASS] Basic scan\n";
}

void testFileLimit(const fs::path& root)
{
    fs::create_directories(root);

    for (int i = 0; i < 8; ++i)
    {
        createFile(root / ("file" + std::to_string(i) + ".cpp"));
    }

    ProjectScanner scanner(5, 20);
    const ScanResult result = scanner.scan(root);

    require(!result.completed, "O limite de ficheiros nao foi detetado.");
    require(result.fileLimitReached, "Falta o indicador fileLimitReached.");
    require(result.totalFiles == 5, "O scanner ultrapassou o limite.");

    std::cout << "[PASS] File limit\n";
}


void testDepthLimit(const fs::path& root)
{
    fs::path current = root;
    fs::create_directories(current);

    for (int i = 1; i <= 25; ++i)
    {
        current /= "d";
        fs::create_directory(current);
    }

    createFile(current / "deep.cpp");

    ProjectScanner scanner(100000, 20);
    const ScanResult result = scanner.scan(root);

    require(
        !result.completed,
        "O limite de profundidade nao foi detetado."
    );

    require(
        result.depthLimitReached,
        "Falta o indicador depthLimitReached."
    );

    require(
        result.totalFiles == 0,
        "O ficheiro profundo nao devia ser analisado."
    );

    std::cout << "[PASS] Depth limit\n";
}


void testInvalidDirectory(const fs::path& root)
{
    ProjectScanner scanner;
    bool exceptionCaught = false;

    try
    {
        scanner.scan(root / "nao_existe");
    }
    catch (const std::runtime_error&)
    {
        exceptionCaught = true;
    }

    require(exceptionCaught, "Uma pasta inexistente devia provocar um erro.");

    std::cout << "[PASS] Invalid directory\n";
}


void testInvalidLimits()
{
    bool fileLimitRejected = false;
    bool depthLimitRejected = false;

    try
    {
        ProjectScanner scanner(0, 20);
    }
    catch (const std::invalid_argument&)
    {
        fileLimitRejected = true;
    }

    try
    {
        ProjectScanner scanner(100000, -1);
    }
    catch (const std::invalid_argument&)
    {
        depthLimitRejected = true;
    }

    require(
        fileLimitRejected,
        "O limite de zero ficheiros devia ser rejeitado."
    );

    require(
        depthLimitRejected,
        "Uma profundidade negativa devia ser rejeitada."
    );

    std::cout << "[PASS] Invalid limits\n";
}

void testSymbolicLink(const fs::path& root)
{
    const fs::path source = root / "source";
    const fs::path link = root / "link";

    fs::create_directories(source);
    createFile(source / "example.cpp");

    std::error_code ec;
    fs::create_directory_symlink(source, link, ec);

    if (ec)
    {
        std::cout
            << "[SKIP] Symbolic link: "
            << ec.message() << "\n";
        return;
    }

    ProjectScanner scanner;
    const ScanResult result = scanner.scan(root);

    require(
        result.completed,
        "O scan com link simbolico deveria terminar."
    );

    require(
        result.totalFiles == 1,
        "O scanner seguiu um link simbolico."
    );

    std::cout << "[PASS] Symbolic link\n";
}


void testWindowsJunction(const fs::path& root)
{
#ifdef _WIN32
    fs::create_directories(root);

    const fs::path target = root / "target";
    const fs::path junction = root / "junction";

    fs::create_directories(target);
    createFile(target / "example.cpp");

    const std::wstring command =
        L"cmd /C mklink /J \"" +
        junction.wstring() +
        L"\" \"" +
        target.wstring() +
        L"\" >nul";

    const int resultCode = _wsystem(command.c_str());

    if (resultCode != 0)
    {
        std::cout
            << "[SKIP] Windows junction creation failed\n";
        return;
    }

    ProjectScanner scanner;
    const ScanResult result = scanner.scan(root);

    require(
        result.completed,
        "O scan com junction deveria terminar."
    );

    require(
        result.totalFiles == 1,
        "O scanner seguiu a junction."
    );

    fs::remove(junction);
    std::cout << "[PASS] Windows junction\n";
#else
    (void)root;
    std::cout << "[SKIP] Windows junction (non-Windows)\n";
#endif
}




void testExclusionLog(const fs::path& root)
{
    fs::create_directories(root / "build");
    fs::create_directories(root / "src");

    createFile(root / "build" / "ignored.cpp");
    createFile(root / "src" / "main.cpp");

    ProjectScanner scanner;
    const ScanResult result = scanner.scan(root);

    require(
        result.completed,
        "O scan com exclusoes intencionais deveria estar completo."
    );

    require(
        result.totalFiles == 1,
        "A pasta build nao foi corretamente ignorada."
    );

    bool foundIgnoredDirectory = false;

    for (const auto& exclusion : result.exclusions)
    {
        if (exclusion.reason == "Ignored directory" &&
            exclusion.path.filename() == "build")
        {
            foundIgnoredDirectory = true;
        }
    }

    require(
        foundIgnoredDirectory,
        "A exclusao da pasta build nao foi registada."
    );

    std::cout << "[PASS] Exclusion log\n";
}


int main()
{
    fs::path testRoot;

    try
    {
        const auto timestamp =
            std::chrono::steady_clock::now()
                .time_since_epoch().count();

        testRoot = fs::temp_directory_path() /
            ("CodeSentinelTests_" + std::to_string(timestamp));

        fs::create_directories(testRoot);

        testBasicScan(testRoot / "basic");
        testFileLimit(testRoot / "files");
        testDepthLimit(testRoot / "depth");
        testInvalidDirectory(testRoot);
        testInvalidLimits();
        testSymbolicLink(testRoot / "symlinks");
        testWindowsJunction(testRoot / "junctions");
        testExclusionLog(testRoot / "exclusions");

        fs::remove_all(testRoot);

        std::cout << "\nAll scanner tests passed!\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] " << e.what() << "\n";

        if (!testRoot.empty())
        {
            std::cerr << "Test files retained at: "
                      << testRoot.string() << "\n";
        }

        return 1;
    }


    
}
