
#include "codesentinel/integrity/IntegrityManifest.hpp"
#include "codesentinel/integrity/FileHasher.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
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
        const std::string& content
    )
    {
        fs::create_directories(path.parent_path());

        std::ofstream file(
            path,
            std::ios::binary | std::ios::trunc
        );

        if (!file)
        {
            throw std::runtime_error(
                "Nao foi possivel criar: " + path.string()
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
                "Erro ao escrever: " + path.string()
            );
        }
    }

    void testBasicManifest(const fs::path& root)
    {
        const fs::path project = root / "basic";

        writeFile(project / "main.cpp", "int main() {}");
        writeFile(project / "README.md", "Projeto de teste");

        const ManifestResult result =
            IntegrityManifest::create(project);

        require(
            result.files.size() == 2,
            "O manifesto deveria conter dois ficheiros."
        );

        for (const auto& entry : result.files)
        {
            require(
                !fs::path(entry.path).is_absolute(),
                "O manifesto contem um caminho absoluto."
            );

            require(
                entry.sha256.size() == 64,
                "O hash SHA-256 deveria ter 64 caracteres."
            );

            require(
                entry.sha256 ==
                    FileHasher::sha256(project / fs::path(entry.path)),
                "O hash do manifesto nao corresponde ao ficheiro."
            );
        }

        std::cout << "[PASS] Basic manifest\n";
    }

    void testDeterministicOrder(const fs::path& root)
    {
        const fs::path project = root / "order";

        writeFile(project / "z.txt", "Z");
        writeFile(project / "a.txt", "A");
        writeFile(project / "folder" / "b.txt", "B");

        const ManifestResult first =
            IntegrityManifest::create(project);

        const ManifestResult second =
            IntegrityManifest::create(project);

        require(
            first.files.size() == 3,
            "O manifesto deveria conter tres ficheiros."
        );

        require(
            first.files[0].path == "a.txt" &&
            first.files[1].path == "folder/b.txt" &&
            first.files[2].path == "z.txt",
            "Os caminhos nao estao ordenados."
        );

        for (std::size_t i = 0; i < first.files.size(); ++i)
        {
            require(
                first.files[i].path == second.files[i].path &&
                first.files[i].sha256 == second.files[i].sha256,
                "O manifesto mudou sem alterar os ficheiros."
            );
        }

        std::cout << "[PASS] Deterministic order\n";
    }

    void testIgnoredDirectories(const fs::path& root)
    {
        const fs::path project = root / "ignored";

        writeFile(project / "src" / "main.cpp", "codigo");
        writeFile(project / "build" / "generated.obj", "build");
        writeFile(project / ".git" / "config", "git");
        writeFile(project / "node_modules" / "lib.js", "module");

        const ManifestResult result =
            IntegrityManifest::create(project);

        require(
            result.files.size() == 1,
            "As pastas ignoradas nao foram excluidas."
        );

        require(
            result.files[0].path == "src/main.cpp",
            "O ficheiro esperado nao foi encontrado."
        );

        require(
            result.excludedEntries == 3,
            "Deveriam existir tres exclusoes."
        );

        std::cout << "[PASS] Ignored directories\n";
    }

    void testSelfExclusion(const fs::path& root)
    {
        const fs::path project = root / "self";

        writeFile(project / "hello.txt", "Ola CodeSentinel");

        const ManifestResult first =
            IntegrityManifest::create(project);

        IntegrityManifest::save(
            first,
            project / "codexkit-manifest.json"
        );

        const ManifestResult second =
            IntegrityManifest::create(project);

        require(
            first.files.size() == 1 &&
            second.files.size() == 1,
            "O manifesto foi incluido na analise."
        );

        require(
            second.excludedEntries == 1,
            "A exclusao do manifesto nao foi registada."
        );

        require(
            first.files[0].sha256 == second.files[0].sha256,
            "O hash mudou sem alteracao do ficheiro."
        );

        std::cout << "[PASS] Self exclusion\n";
    }

    void testJsonOutput(const fs::path& root)
    {
        const fs::path project = root / "json";

        writeFile(project / "hello.txt", "abc");

        const ManifestResult result =
            IntegrityManifest::create(project);

        const fs::path output =
            project / "codexkit-manifest.json";

        IntegrityManifest::save(result, output);

        std::ifstream file(output, std::ios::binary);

        require(
            file.is_open(),
            "O ficheiro JSON nao foi criado."
        );

        const std::string json(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        require(
            json.find("\"version\": 1") != std::string::npos,
            "Versao do manifesto ausente."
        );

        require(
            json.find("\"algorithm\": \"SHA-256\"") !=
                std::string::npos,
            "Algoritmo SHA-256 ausente."
        );

        require(
            json.find("\"path\": \"hello.txt\"") !=
                std::string::npos,
            "Caminho do ficheiro ausente."
        );

        require(
            json.find(result.files[0].sha256) !=
                std::string::npos,
            "Hash do ficheiro ausente."
        );

        std::cout << "[PASS] JSON output\n";
    }

    void testInvalidDirectory(const fs::path& root)
    {
        bool exceptionThrown = false;

        try
        {
            IntegrityManifest::create(
                root / "directory-does-not-exist"
            );
        }
        catch (const std::runtime_error&)
        {
            exceptionThrown = true;
        }

        require(
            exceptionThrown,
            "Uma pasta inexistente deveria provocar erro."
        );

        std::cout << "[PASS] Invalid directory\n";
    }

    
void testFileLimit(const fs::path& root)
{
    const fs::path project = root / "file_limit";

    writeFile(project / "a.txt", "A");
    writeFile(project / "b.txt", "B");
    writeFile(project / "c.txt", "C");

    // Permitir apenas dois ficheiros.
    const ManifestResult result =
        IntegrityManifest::create(project, 2, 20);

    require(
        !result.completed,
        "O manifesto deveria estar incompleto."
    );

    require(
        result.fileLimitReached,
        "O limite de ficheiros nao foi detetado."
    );

    require(
        result.files.size() == 2,
        "O manifesto deveria conter apenas dois ficheiros."
    );

    bool foundExclusion = false;

    for (const auto& exclusion : result.exclusions)
    {
        if (exclusion.reason == "File limit reached")
        {
            foundExclusion = true;
        }
    }

    require(
        foundExclusion,
        "A exclusao por limite de ficheiros nao foi registada."
    );

    std::cout << "[PASS] File limit\n";
}

void testDepthLimit(const fs::path& root)
{
    const fs::path project = root / "depth_limit";

    writeFile(project / "root.txt", "Raiz");

    writeFile(
        project / "level1" / "level2" / "deep.txt",
        "Profundo"
    );

    // Permitir apenas profundidade zero.
    const ManifestResult result =
        IntegrityManifest::create(project, 100, 0);

    require(
        !result.completed,
        "O manifesto deveria estar incompleto."
    );

    require(
        result.depthLimitReached,
        "O limite de profundidade nao foi detetado."
    );

    bool foundExclusion = false;

    for (const auto& exclusion : result.exclusions)
    {
        if (exclusion.reason == "Depth limit reached")
        {
            foundExclusion = true;
        }
    }

    require(
        foundExclusion,
        "A exclusao por profundidade nao foi registada."
    );

    std::cout << "[PASS] Depth limit\n";
}

    void testIncompleteManifest(const fs::path& root)
    {
        const fs::path project = root / "incomplete";

        writeFile(project / "a.txt", "A");
        writeFile(project / "b.txt", "B");

        const ManifestResult result =
            IntegrityManifest::create(project, 1, 20);

        require(
            !result.completed,
            "O manifesto deveria estar incompleto."
        );

        const fs::path output =
            project / "codexkit-manifest.json";

        bool exceptionThrown = false;

        try
        {
            IntegrityManifest::save(result, output);
        }
        catch (const std::runtime_error&)
        {
            exceptionThrown = true;
        }

        require(
            exceptionThrown,
            "A gravacao de manifesto incompleto deveria falhar."
        );

        require(
            !fs::exists(output),
            "Foi criado um manifesto apesar de estar incompleto."
        );

        std::cout << "[PASS] Incomplete manifest\n";
    }

    
void testSafeReplacement(const fs::path& root)
{
    const fs::path project = root / "safe_replacement";

    writeFile(project / "hello.txt", "abc");

    const fs::path output =
        project / "codexkit-manifest.json";

    // Criar um manifesto anterior.
    writeFile(output, "MANIFESTO ANTIGO");

    const ManifestResult result =
        IntegrityManifest::create(project);

    require(
        result.completed,
        "A analise deveria estar completa."
    );

    IntegrityManifest::save(result, output);

    std::ifstream file(output, std::ios::binary);

    require(
        file.is_open(),
        "O novo manifesto nao foi criado."
    );

    const std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    require(
        content.find("\"algorithm\": \"SHA-256\"") !=
            std::string::npos,
        "O manifesto anterior nao foi substituido."
    );

    require(
        content.find("MANIFESTO ANTIGO") ==
            std::string::npos,
        "O conteudo antigo permaneceu no manifesto."
    );

    std::cout << "[PASS] Safe replacement\n";
}

void testPreserveOnFailure(const fs::path& root)
{
    const fs::path project = root / "preserve_failure";

    writeFile(project / "hello.txt", "abc");

    const fs::path output =
        project / "codexkit-manifest.json";

    const std::string original = "MANIFESTO ORIGINAL";

    writeFile(output, original);

    ManifestResult result =
        IntegrityManifest::create(project);

    // Simular uma analise incompleta.
    result.completed = false;

    bool exceptionThrown = false;

    try
    {
        IntegrityManifest::save(result, output);
    }
    catch (const std::runtime_error&)
    {
        exceptionThrown = true;
    }

    require(
        exceptionThrown,
        "A gravacao incompleta deveria falhar."
    );

    std::ifstream file(output, std::ios::binary);

    const std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    require(
        content == original,
        "O manifesto anterior foi alterado apos a falha."
    );

    std::cout << "[PASS] Preserve on failure\n";
}


void testLoadValidManifest(const fs::path& root)
{
    const fs::path project = root / "load_valid";

    writeFile(project / "hello.txt", "abc");
    writeFile(project / "readme.md", "CodeSentinel");

    const ManifestResult original =
        IntegrityManifest::create(project);

    const fs::path output =
        project / "codexkit-manifest.json";

    IntegrityManifest::save(original, output);

    const ManifestResult loaded =
        IntegrityManifest::load(output);

    require(
        loaded.completed,
        "O manifesto carregado deveria estar completo."
    );

    require(
        loaded.files.size() == original.files.size(),
        "Numero de ficheiros diferente apos leitura."
    );

    for (std::size_t i = 0; i < original.files.size(); ++i)
    {
        require(
            loaded.files[i].path == original.files[i].path,
            "Caminho diferente apos leitura."
        );

        require(
            loaded.files[i].sha256 == original.files[i].sha256,
            "Hash diferente apos leitura."
        );
    }

    std::cout << "[PASS] Load valid manifest\n";
}

void testLoadMalformedJson(const fs::path& root)
{
    const fs::path project = root / "load_malformed";
    const fs::path output = project / "codexkit-manifest.json";

    writeFile(output, "{ invalid json !!!");

    bool rejected = false;

    try
    {
        IntegrityManifest::load(output);
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(
        rejected,
        "JSON malformado deveria ser rejeitado."
    );

    std::cout << "[PASS] Load malformed JSON\n";
}

void testLoadInvalidMetadata(const fs::path& root)
{
    const fs::path project = root / "load_metadata";
    const fs::path output = project / "codexkit-manifest.json";

    const std::string invalidVersion = R"({
        "version": 99,
        "algorithm": "SHA-256",
        "files": []
    })";

    writeFile(output, invalidVersion);

    bool versionRejected = false;

    try
    {
        IntegrityManifest::load(output);
    }
    catch (const std::exception&)
    {
        versionRejected = true;
    }

    require(
        versionRejected,
        "Versao invalida deveria ser rejeitada."
    );

    const std::string invalidAlgorithm = R"({
        "version": 1,
        "algorithm": "MD5",
        "files": []
    })";

    writeFile(output, invalidAlgorithm);

    bool algorithmRejected = false;

    try
    {
        IntegrityManifest::load(output);
    }
    catch (const std::exception&)
    {
        algorithmRejected = true;
    }

    require(
        algorithmRejected,
        "Algoritmo invalido deveria ser rejeitado."
    );

    std::cout << "[PASS] Load invalid metadata\n";
}

void testLoadUnsafePaths(const fs::path& root)
{
    const fs::path project = root / "load_unsafe_paths";
    const fs::path output = project / "codexkit-manifest.json";

    const std::string hash(64, 'a');

    const std::vector<std::string> unsafePaths = {
        "../secret.txt",
        "/absolute.txt",
        "C:/Windows/system.ini",
        "folder/../../secret.txt",
        "folder\\secret.txt",
        "./hello.txt"
    };

    for (const std::string& path : unsafePaths)
    {
        const std::string json = std::string(R"({
            "version": 1,
            "algorithm": "SHA-256",
            "files": [
                {
                    "path": ")") + path + R"(",
                    "sha256": ")" + hash + R"("
                }
            ]
        })";

        writeFile(output, json);

        bool rejected = false;

        try
        {
            IntegrityManifest::load(output);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }

        require(
            rejected,
            "Caminho inseguro deveria ser rejeitado: " + path
        );
    }

    std::cout << "[PASS] Load unsafe paths\n";
}

void testLoadInvalidHashes(const fs::path& root)
{
    const fs::path project = root / "load_invalid_hashes";
    const fs::path output = project / "codexkit-manifest.json";

    const std::vector<std::string> invalidHashes = {
        "",
        "abc123",
        std::string(63, 'a'),
        std::string(65, 'a'),
        std::string(64, 'z')
    };

    for (const std::string& hash : invalidHashes)
    {
        const std::string json = std::string(R"({
            "version": 1,
            "algorithm": "SHA-256",
            "files": [
                {
                    "path": "hello.txt",
                    "sha256": ")") + hash + R"("
                }
            ]
        })";

        writeFile(output, json);

        bool rejected = false;

        try
        {
            IntegrityManifest::load(output);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }

        require(
            rejected,
            "Hash SHA-256 invalido deveria ser rejeitado."
        );
    }

    std::cout << "[PASS] Load invalid hashes\n";
}


void testLoadDuplicatePaths(const fs::path& root)
{
    const fs::path project = root / "load_duplicates";
    const fs::path output =
        project / "codexkit-manifest.json";

    const std::string hash(64, 'a');

    const std::string json =
        std::string(R"({
            "version": 1,
            "algorithm": "SHA-256",
            "files": [
                {
                    "path": "Hello.txt",
                    "sha256": ")") + hash + R"("
                },
                {
                    "path": "hello.txt",
                    "sha256": ")" + hash + R"("
                }
            ]
        })";

    writeFile(output, json);

    bool rejected = false;

    try
    {
        IntegrityManifest::load(output);
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(
        rejected,
        "Caminhos duplicados deveriam ser rejeitados."
    );

    std::cout << "[PASS] Load duplicate paths\n";
}

void testLoadWindowsUnsafeNames(const fs::path& root)
{
    const fs::path project = root / "load_windows_names";
    const fs::path output =
        project / "codexkit-manifest.json";

    const std::string hash(64, 'a');

    const std::vector<std::string> invalidPaths = {
        "CON",
        "CON.txt",
        "NUL.log",
        "pasta/COM1.txt",
        "pasta/LPT9",
        "pasta/CONOUT$",
        "pasta//ficheiro.txt",
        "pasta/ficheiro.",
        "pasta/ficheiro ",
        "pasta/ficheiro?.txt",
        "pasta/ficheiro*.txt"
    };

    for (const std::string& path : invalidPaths)
    {
        const std::string json =
            std::string(R"({
                "version": 1,
                "algorithm": "SHA-256",
                "files": [
                    {
                        "path": ")") + path + R"(",
                        "sha256": ")" + hash + R"("
                    }
                ]
            })";

        writeFile(output, json);

        bool rejected = false;

        try
        {
            IntegrityManifest::load(output);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }

        require(
            rejected,
            "Nome inseguro deveria ser rejeitado: " + path
        );
    }

    std::cout << "[PASS] Load Windows unsafe names\n";
}



// Confirmar que cada entrada e rejeitada pelo motivo esperado,
// e nao apenas por um erro JSON generico.
void testSecurityErrorReasons(const fs::path& root)
{
    const fs::path output =
        root / "security_error_reasons" / "codexkit-manifest.json";

    const std::string hash(64, 'a');

    struct Case
    {
        const char* name;
        std::string json;
        const char* expectedMessage;
    };

    const std::vector<Case> cases = {
        {
            "Duplicate paths",
            std::string(R"({"version":1,"algorithm":"SHA-256","files":[{"path":"readme.txt","sha256":")") +
                hash + R"("},{"path":"README.txt","sha256":")" + hash + R"("}]})",
            "Caminho duplicado no manifesto:"
        },
        {
            "Parent traversal",
            std::string(R"({"version":1,"algorithm":"SHA-256","files":[{"path":"../segredo.txt","sha256":")") +
                hash + R"("}]})",
            "Componente inseguro:"
        },
        {
            "Reserved Windows name",
            std::string(R"({"version":1,"algorithm":"SHA-256","files":[{"path":"CON.txt","sha256":")") +
                hash + R"("}]})",
            "Nome reservado do Windows:"
        }
    };

    for (const Case& testCase : cases)
    {
        writeFile(output, testCase.json);
        bool rejectedForExpectedReason = false;

        try
        {
            (void)IntegrityManifest::load(output);
        }
        catch (const std::runtime_error& e)
        {
            rejectedForExpectedReason =
                std::string(e.what()).find(testCase.expectedMessage) !=
                std::string::npos;
        }

        require(
            rejectedForExpectedReason,
            std::string("Motivo de rejeicao incorreto: ") + testCase.name
        );
    }

    std::cout << "[PASS] Security error reasons\n";
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
            ("codesentinel_manifest_tests_" +
             std::to_string(timestamp));

        fs::create_directories(testRoot);

        testBasicManifest(testRoot);
        testDeterministicOrder(testRoot);
        testIgnoredDirectories(testRoot);
        testSelfExclusion(testRoot);
        testJsonOutput(testRoot);
        testInvalidDirectory(testRoot);
        testFileLimit(testRoot);
        testDepthLimit(testRoot);
        testIncompleteManifest(testRoot);
        testSafeReplacement(testRoot);
        testPreserveOnFailure(testRoot);

        
        testLoadValidManifest(testRoot);
        testLoadMalformedJson(testRoot);
        testLoadInvalidMetadata(testRoot);
        testLoadUnsafePaths(testRoot);
        testLoadInvalidHashes(testRoot);

        testLoadDuplicatePaths(testRoot);
        testLoadWindowsUnsafeNames(testRoot);
        testSecurityErrorReasons(testRoot);


        fs::remove_all(testRoot);

        std::cout << "All manifest tests passed!\n";

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
