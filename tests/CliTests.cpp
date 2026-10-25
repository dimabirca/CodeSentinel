
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

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
    const std::string& content)
{
    fs::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "Nao foi possivel criar: " + path.string()
        );
    }

    file << content;

    if (!file)
    {
        throw std::runtime_error(
            "Erro ao escrever: " + path.string()
        );
    }
}

std::wstring quoteWindowsArgument(const std::wstring& value)
{
    std::wstring result = L"\"";
    std::size_t backslashes = 0;

    for (wchar_t ch : value)
    {
        if (ch == L'\\')
        {
            ++backslashes;
            continue;
        }

        if (ch == L'"')
        {
            result.append(backslashes * 2 + 1, L'\\');
            result += L'"';
        }
        else
        {
            result.append(backslashes, L'\\');
            result += ch;
        }

        backslashes = 0;
    }

    result.append(backslashes * 2, L'\\');
    result += L'"';

    return result;
}

struct CommandResult
{
    DWORD exitCode = 0;
    std::string output;
};

struct HandleGuard
{
    HANDLE handle = INVALID_HANDLE_VALUE;

    explicit HandleGuard(HANDLE value)
        : handle(value)
    {
    }

    ~HandleGuard()
    {
        if (handle != INVALID_HANDLE_VALUE &&
            handle != nullptr)
        {
            CloseHandle(handle);
        }
    }

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
};

CommandResult runCommand(
    const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& outputFile)
{
    // Criar ficheiro para capturar stdout e stderr.
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength =
        sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE outputHandle = CreateFileW(
        outputFile.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &securityAttributes,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    require(
        outputHandle != INVALID_HANDLE_VALUE,
        "Nao foi possivel criar o ficheiro de saida."
    );

    HandleGuard outputGuard(outputHandle);

    // Construir linha de comandos sem usar cmd.exe.
    std::wstring commandLine =
        quoteWindowsArgument(executable.wstring());

    for (const std::wstring& argument : arguments)
    {
        commandLine += L" ";
        commandLine += quoteWindowsArgument(argument);
    }

    std::vector<wchar_t> commandBuffer(
        commandLine.begin(),
        commandLine.end()
    );

    commandBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = outputHandle;
    startupInfo.hStdError = outputHandle;
    startupInfo.hStdInput =
        GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION processInfo{};

    const BOOL started = CreateProcessW(
        executable.c_str(),
        commandBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );

    if (!started)
    {
        throw std::runtime_error(
            "CreateProcessW falhou. Codigo Windows: " +
            std::to_string(GetLastError())
        );
    }

    HandleGuard processGuard(processInfo.hProcess);
    HandleGuard threadGuard(processInfo.hThread);

    const DWORD waitResult = WaitForSingleObject(
        processInfo.hProcess,
        30000
    );

    if (waitResult == WAIT_TIMEOUT)
    {
        TerminateProcess(processInfo.hProcess, 2);
        WaitForSingleObject(processInfo.hProcess, INFINITE);

        throw std::runtime_error(
            "Tempo limite excedido ao executar CodeSentinel."
        );
    }

    require(
        waitResult == WAIT_OBJECT_0,
        "Erro ao aguardar pelo processo CodeSentinel."
    );

    DWORD exitCode = 0;

    require(
        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode
        ) != 0,
        "Nao foi possivel obter o codigo de saida."
    );

    // Fechar o handle de escrita antes de ler.
    CloseHandle(outputGuard.handle);
    outputGuard.handle = INVALID_HANDLE_VALUE;

    std::ifstream input(outputFile, std::ios::binary);

    require(
        static_cast<bool>(input),
        "Nao foi possivel ler a saida do comando."
    );

    const std::string output(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    return {exitCode, output};
}

CommandResult runCodeSentinel(
    const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& root)
{
    return runCommand(
        executable,
        arguments,
        root / "command-output.txt"
    );
}

void createManifest(
    const fs::path& executable,
    const fs::path& project,
    const fs::path& root)
{
    const CommandResult result = runCodeSentinel(
        executable,
        {L"manifest", project.wstring()},
        root
    );

    require(
        result.exitCode == 0,
        "Falha ao criar manifesto:\n" + result.output
    );

    require(
        fs::exists(project / "codexkit-manifest.json"),
        "Manifesto nao foi criado."
    );
}

std::string readFile(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Nao foi possivel ler: " + path.string());
    return std::string(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
}

void testScanLegacy(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan legacy";
    writeFile(project / "hello.txt", "Hello");
    writeFile(project / "readme.md", "Readme");

    const CommandResult result = runCodeSentinel(
        executable, {L"scan", project.wstring()}, root);

    require(result.exitCode == 0,
        "Scan sem JSON deveria devolver 0:\n" + result.output);
    require(result.output.find("Files: 2") != std::string::npos,
        "Scan sem JSON nao apresentou dois ficheiros.");
    require(result.output.find("Scan completed!") != std::string::npos,
        "Scan sem JSON nao terminou normalmente.");
    std::cout << "[PASS] CLI scan legacy\n";
}

void testScanJson(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan json com espacos";
    const fs::path report = root / "relatorio com espacos.json";
    writeFile(project / "hello.txt", "Hello");
    writeFile(project / "readme.md", "Readme");

    const CommandResult result = runCodeSentinel(executable,
        {L"scan", project.wstring(), L"--json", report.wstring()}, root);

    require(result.exitCode == 0,
        "Scan JSON deveria devolver 0:\n" + result.output);
    require(result.output.find("JSON report:") != std::string::npos,
        "Scan JSON nao confirmou a exportacao.");
    require(result.output.find("Scan completed!") != std::string::npos,
        "Scan JSON nao apresentou a saida normal.");
    require(fs::is_regular_file(report), "Relatorio JSON nao foi criado.");

    const std::string json = readFile(report);
    const std::vector<std::string> expected = {
        "\"schema_version\": 1",
        "\"total_files\": 2",
        "\"total_directories\": 0",
        "\"Markdown\": 1",
        "\"Other\": 1",
        "\"completed\": true",
        "\"file_limit_reached\": false",
        "\"depth_limit_reached\": false",
        "\"exclusions\": []"
    };
    for (const auto& field : expected)
    {
        require(json.find(field) != std::string::npos,
            "Campo JSON ausente ou inesperado: " + field);
    }
    std::cout << "[PASS] CLI scan JSON export\n";
}

void testScanInvalidArguments(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan invalid";
    writeFile(project / "hello.txt", "Hello");

    const std::vector<std::vector<std::wstring>> invalid = {
        {L"scan"},
        {L"scan", project.wstring(), L"--json"},
        {L"scan", project.wstring(), L"--unknown", L"out.json"},
        {L"scan", project.wstring(), L"--json", L""}
    };
    for (const auto& arguments : invalid)
    {
        const CommandResult result = runCodeSentinel(executable, arguments, root);
        require(result.exitCode == 1,
            "Argumentos invalidos de scan deveriam devolver 1:\n" +
            result.output);
    }
    std::cout << "[PASS] CLI scan invalid arguments\n";
}

void testScanJsonWriteError(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan write error";
    const fs::path destinationDirectory = root / "json destination directory";
    writeFile(project / "hello.txt", "Hello");
    fs::create_directories(destinationDirectory);

    const CommandResult result = runCodeSentinel(executable,
        {L"scan", project.wstring(), L"--json",
         destinationDirectory.wstring()}, root);

    require(result.exitCode == 1,
        "Gravacao JSON num diretorio deveria devolver 1:\n" + result.output);
    require(result.output.find("Erro:") != std::string::npos,
        "Falha de gravacao JSON nao apresentou mensagem de erro.");
    std::cout << "[PASS] CLI scan JSON write error\n";
}

void testScanJsonReplace(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan replace project";
    const fs::path report = root / "replace report.json";
    writeFile(project / "hello.txt", "Hello");
    writeFile(report, "RELATORIO ANTIGO");

    const CommandResult result = runCodeSentinel(executable,
        {L"scan", project.wstring(), L"--json", report.wstring()}, root);

    require(result.exitCode == 0,
        "Substituicao JSON deveria devolver 0:\n" + result.output);
    const std::string content = readFile(report);
    require(content.find("RELATORIO ANTIGO") == std::string::npos,
        "Conteudo antigo permaneceu no relatorio.");
    require(content.find("\"schema_version\": 1") != std::string::npos,
        "Relatorio substituido nao contem a versao do schema.");
    require(content.find("\"total_files\": 1") != std::string::npos,
        "Relatorio substituido nao contem o total esperado.");

    const std::wstring prefix = report.filename().wstring() + L".tmp";
    for (const auto& entry : fs::directory_iterator(root))
    {
        const std::wstring filename = entry.path().filename().wstring();
        require(filename.compare(0, prefix.size(), prefix) != 0,
            "Ficheiro temporario JSON permaneceu apos a substituicao.");
    }
    std::cout << "[PASS] CLI scan JSON replace and cleanup\n";
}

void testScanJsonSymbolicLink(const fs::path& executable, const fs::path& root)
{
    const fs::path project = root / "scan symlink project";
    const fs::path protectedFile = root / "scan protected target.txt";
    const fs::path link = root / "scan symlink report.json";
    writeFile(project / "hello.txt", "Hello");
    writeFile(protectedFile, "CONTEUDO PROTEGIDO");

    std::error_code error;
    fs::create_symlink(protectedFile, link, error);
    if (error)
    {
        // Windows pode exigir privilegios ou Developer Mode para criar symlinks.
        if (error.value() == ERROR_PRIVILEGE_NOT_HELD ||
            error.value() == ERROR_ACCESS_DENIED)
        {
            std::cout << "[SKIP] CLI scan JSON symlink (sem privilegios)\n";
            return;
        }
        throw std::runtime_error("Erro ao criar symlink de teste: " +
            error.message());
    }

    const CommandResult result = runCodeSentinel(executable,
        {L"scan", project.wstring(), L"--json", link.wstring()}, root);

    require(result.exitCode == 1,
        "Destino symlink deveria devolver 1:\n" + result.output);
    require(result.output.find("reparse point") != std::string::npos ||
            result.output.find("link") != std::string::npos,
        "Rejeicao do symlink nao foi comunicada.");
    require(readFile(protectedFile) == "CONTEUDO PROTEGIDO",
        "O ficheiro protegido foi alterado.");
    require(fs::is_symlink(fs::symlink_status(link)),
        "O destino deixou de ser um link simbolico.");
    std::cout << "[PASS] CLI scan JSON symlink protection\n";
}

void testHelp(
    const fs::path& executable,
    const fs::path& root)
{
    const CommandResult result = runCodeSentinel(
        executable,
        {L"--help"},
        root
    );

    require(
        result.exitCode == 0,
        "Help deveria devolver 0."
    );

    require(
        result.output.find("verify <dir>") !=
            std::string::npos,
        "Comando verify nao aparece na ajuda."
    );

    std::cout << "[PASS] CLI help\n";
}

void testVerifyClean(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project = root / "clean";

    writeFile(project / "hello.txt", "Hello");
    writeFile(project / "readme.md", "Readme");

    createManifest(executable, project, root);

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 0,
        "Projeto intacto deveria devolver 0:\n" +
        result.output
    );

    require(
        result.output.find("Integrity verified!") !=
            std::string::npos,
        "Mensagem de integridade nao encontrada."
    );

    std::cout
        << "[PASS] CLI verify clean (exit 0)\n";
}

void testVerifyModified(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project = root / "modified";

    writeFile(project / "hello.txt", "Original");
    createManifest(executable, project, root);

    writeFile(project / "hello.txt", "Alterado");

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 1,
        "Ficheiro modificado deveria devolver 1:\n" +
        result.output
    );

    require(
        result.output.find("[MODIFIED] hello.txt") !=
            std::string::npos,
        "Ficheiro modificado nao identificado."
    );

    std::cout
        << "[PASS] CLI verify modified (exit 1)\n";
}

void testVerifyAdded(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project = root / "added";

    writeFile(project / "hello.txt", "Original");
    createManifest(executable, project, root);

    writeFile(project / "novo.txt", "Novo");

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 1,
        "Ficheiro adicionado deveria devolver 1:\n" +
        result.output
    );

    require(
        result.output.find("[ADDED] novo.txt") !=
            std::string::npos,
        "Ficheiro adicionado nao identificado."
    );

    std::cout
        << "[PASS] CLI verify added (exit 1)\n";
}

void testVerifyRemoved(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project = root / "removed";

    writeFile(project / "hello.txt", "Original");
    createManifest(executable, project, root);

    fs::remove(project / "hello.txt");

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 1,
        "Ficheiro removido deveria devolver 1:\n" +
        result.output
    );

    require(
        result.output.find("[REMOVED] hello.txt") !=
            std::string::npos,
        "Ficheiro removido nao identificado."
    );

    std::cout
        << "[PASS] CLI verify removed (exit 1)\n";
}

void testVerifyMissingManifest(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project =
        root / "missing_manifest";

    writeFile(project / "hello.txt", "Hello");

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 2,
        "Manifesto inexistente deveria devolver 2:\n" +
        result.output
    );

    require(
        result.output.find("Erro de verificacao:") !=
            std::string::npos,
        "Mensagem de erro nao encontrada."
    );

    std::cout
        << "[PASS] CLI missing manifest (exit 2)\n";
}

void testVerifyInvalidManifest(
    const fs::path& executable,
    const fs::path& root)
{
    const fs::path project =
        root / "invalid_manifest";

    writeFile(project / "hello.txt", "Hello");

    writeFile(
        project / "codexkit-manifest.json",
        "{ invalid json"
    );

    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify", project.wstring()},
        root
    );

    require(
        result.exitCode == 2,
        "Manifesto invalido deveria devolver 2:\n" +
        result.output
    );

    std::cout
        << "[PASS] CLI invalid manifest (exit 2)\n";
}

void testVerifyMissingArgument(
    const fs::path& executable,
    const fs::path& root)
{
    const CommandResult result = runCodeSentinel(
        executable,
        {L"verify"},
        root
    );

    require(
        result.exitCode == 2,
        "Verify sem argumento deveria devolver 2:\n" +
        result.output
    );

    std::cout
        << "[PASS] CLI missing argument (exit 2)\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Uso: codesentinel_cli_tests <codesentinel.exe>\n";

        return 1;
    }

    const fs::path executable =
        fs::absolute(argv[1]);

    const fs::path root =
        fs::temp_directory_path() /
        "codesentinel_cli_tests";

    try
    {
        require(
            fs::is_regular_file(executable),
            "Executavel CodeSentinel nao encontrado."
        );

        fs::remove_all(root);
        fs::create_directories(root);

        testHelp(executable, root);
        testScanLegacy(executable, root);
        testScanJson(executable, root);
        testScanInvalidArguments(executable, root);
        testScanJsonWriteError(executable, root);
        testScanJsonReplace(executable, root);
        testScanJsonSymbolicLink(executable, root);
        testVerifyClean(executable, root);
        testVerifyModified(executable, root);
        testVerifyAdded(executable, root);
        testVerifyRemoved(executable, root);
        testVerifyMissingManifest(executable, root);
        testVerifyInvalidManifest(executable, root);
        testVerifyMissingArgument(executable, root);

        fs::remove_all(root);

        std::cout
            << "All CLI tests passed!\n";

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return 1;
    }
}
