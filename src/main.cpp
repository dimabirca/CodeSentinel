
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "codesentinel/integrity/FileHasher.hpp"
#include "codesentinel/integrity/IntegrityManifest.hpp"
#include "codesentinel/integrity/ManifestVerifier.hpp"
#include "codesentinel/inspector/ProjectScanner.hpp"
#include "codesentinel/inspector/ScanReportJson.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "CodeSentinel v0.2\n";
        std::cout << "Use --help para ver os comandos.\n";
        return 0;
    }

    const std::string comando = argv[1];

    // ==========================================
    // VERSION
    // ==========================================
    if (comando == "--version")
    {
        std::cout << "CodeSentinel v0.2\n";
        return 0;
    }

    // ==========================================
    // HELP
    // ==========================================
    else if (comando == "--help")
    {
        std::cout << "CodeSentinel - Universal Project Analysis & Integrity Toolkit\n\n";

        std::cout << "Comandos disponiveis:\n";
        std::cout << "  --help          Mostra ajuda\n";
        std::cout << "  --version       Mostra versao\n";
        std::cout << "  scan <path>     Analisa um projeto\n";
        std::cout << "  scan <path> --json <file>  Exporta relatorio JSON\n";
        std::cout << "  hash <file>     Calcula SHA-256 de um ficheiro\n";
        std::cout << "  manifest <dir>  Cria manifesto SHA-256 do projeto\n";
        std::cout << "  verify <dir>    Verifica a integridade do projeto\n";

        std::cout << "\nCodigos de saida do comando verify:\n";
        std::cout << "  0 - Integridade confirmada\n";
        std::cout << "  1 - Alteracoes detetadas\n";
        std::cout << "  2 - Erro ou verificacao incompleta\n";

        return 0;
    }

    // ==========================================
    // SCAN
    // ==========================================
    else if (comando == "scan")
    {
        if (argc < 3)
        {
            std::cerr << "Erro: indica uma pasta.\n";
            return 1;
        }

        // Aceita apenas: scan <path> [--json <file>].
        const bool exportJson = (argc == 5 && std::string(argv[3]) == "--json");
        if (argc != 3 && !exportJson)
        {
            std::cerr << "Erro: utilizacao: scan <path> [--json <file>].\n";
            return 1;
        }

        if (exportJson && std::string(argv[4]).empty())
        {
            std::cerr << "Erro: indica o ficheiro de destino JSON.\n";
            return 1;
        }

        try
        {
            ProjectScanner scanner;

            const ScanResult result =
                scanner.scan(argv[2]);

            if (exportJson)
            {
                ScanReportJson::save(result, fs::path(argv[4]));
                std::cout << "JSON report: " << argv[4] << "\n";
            }

            std::cout << "CodeSentinel - Project Analysis\n\n";

            std::cout << "Files: "
                      << result.totalFiles << "\n";

            std::cout << "Directories: "
                      << result.totalDirectories << "\n";

            std::cout << "\nLanguages:\n";

            for (const auto& [language, count] :
                 result.languages)
            {
                std::cout << "  "
                          << language
                          << ": "
                          << count
                          << "\n";
            }

            std::cout << "\nExclusions: "
                      << result.exclusions.size()
                      << "\n";

            for (const auto& exclusion :
                 result.exclusions)
            {
                std::cout << "  ["
                          << exclusion.reason
                          << "] "
                          << exclusion.path.string()
                          << "\n";
            }

            if (result.completed)
            {
                std::cout << "\nScan completed!\n";
                return 0;
            }

            std::cout << "\nWARNING: Scan incomplete!\n";

            if (result.fileLimitReached)
            {
                std::cout
                    << "Reason: File limit reached.\n";
            }

            if (result.depthLimitReached)
            {
                std::cout
                    << "Reason: Depth limit reached.\n";
            }

            return 2;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Erro: "
                      << e.what() << "\n";
            return 1;
        }
    }

    // ==========================================
    // HASH
    // ==========================================
    else if (comando == "hash")
    {
        if (argc < 3)
        {
            std::cerr
                << "Erro: indica um ficheiro.\n";
            return 1;
        }

        try
        {
            const std::string hash =
                FileHasher::sha256(argv[2]);

            std::cout
                << "CodeSentinel - Integrity Guard\n\n";

            std::cout << "File: "
                      << argv[2] << "\n";

            std::cout << "SHA-256: "
                      << hash << "\n";

            std::cout << "\nHash completed!\n";

            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Erro: "
                      << e.what() << "\n";
            return 1;
        }
    }

    // ==========================================
    // MANIFEST
    // ==========================================
    else if (comando == "manifest")
    {
        if (argc < 3)
        {
            std::cerr
                << "Erro: indica uma pasta.\n";
            return 1;
        }

        try
        {
            const fs::path projectPath = argv[2];

            const ManifestResult manifest =
                IntegrityManifest::create(projectPath);

            if (!manifest.completed)
            {
                std::cerr
                    << "Erro: manifesto incompleto.\n";

                if (manifest.fileLimitReached)
                {
                    std::cerr
                        << "Limite de ficheiros atingido.\n";
                }

                if (manifest.depthLimitReached)
                {
                    std::cerr
                        << "Limite de profundidade atingido.\n";
                }

                return 1;
            }

            const fs::path outputPath =
                projectPath / "codexkit-manifest.json";

            IntegrityManifest::save(
                manifest,
                outputPath
            );

            std::cout
                << "CodeSentinel - Integrity Manifest\n\n";

            std::cout << "Project: "
                      << projectPath.string()
                      << "\n";

            std::cout << "Files hashed: "
                      << manifest.files.size()
                      << "\n";

            std::cout << "Excluded entries: "
                      << manifest.excludedEntries
                      << "\n";

            std::cout << "Manifest: "
                      << outputPath.string()
                      << "\n";

            std::cout
                << "\nManifest completed!\n";

            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Erro: "
                      << e.what() << "\n";
            return 1;
        }
    }

    // ==========================================
    // VERIFY
    // ==========================================
    else if (comando == "verify")
    {
        if (argc < 3)
        {
            std::cerr
                << "Erro: indica uma pasta.\n";

            return 2;
        }

        try
        {
            const fs::path projectPath = argv[2];

            const fs::path manifestPath =
                projectPath / "codexkit-manifest.json";

            const auto result =
                codesentinel::integrity::ManifestVerifier::verify(
                    projectPath,
                    manifestPath
                );

            std::cout
                << "CodeSentinel - Integrity Verification\n\n";

            std::cout << "Project: "
                      << projectPath.string()
                      << "\n";

            std::cout << "Manifest: "
                      << manifestPath.string()
                      << "\n\n";

            std::cout << "Unchanged: "
                      << result.unchanged.size()
                      << "\n";

            std::cout << "Modified:  "
                      << result.modified.size()
                      << "\n";

            std::cout << "Added:     "
                      << result.added.size()
                      << "\n";

            std::cout << "Removed:   "
                      << result.removed.size()
                      << "\n";

            // Apresentar ficheiros modificados.
            if (!result.modified.empty())
            {
                std::cout << "\nModified files:\n";

                for (const auto& path : result.modified)
                {
                    std::cout << "  [MODIFIED] "
                              << path << "\n";
                }
            }

            // Apresentar ficheiros adicionados.
            if (!result.added.empty())
            {
                std::cout << "\nAdded files:\n";

                for (const auto& path : result.added)
                {
                    std::cout << "  [ADDED] "
                              << path << "\n";
                }
            }

            // Apresentar ficheiros removidos.
            if (!result.removed.empty())
            {
                std::cout << "\nRemoved files:\n";

                for (const auto& path : result.removed)
                {
                    std::cout << "  [REMOVED] "
                              << path << "\n";
                }
            }

            // Nunca declarar integridade se a
            // verificacao estiver incompleta.
            if (!result.completed)
            {
                std::cerr
                    << "\nERROR: Verification incomplete!\n";

                return 2;
            }

            // Projeto sem alteracoes.
            if (result.isClean())
            {
                std::cout
                    << "\nIntegrity verified!\n";

                return 0;
            }

            // Foram encontradas alteracoes.
            std::cout
                << "\nWARNING: Integrity changes detected!\n";

            return 1;
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Erro de verificacao: "
                << e.what()
                << "\n";

            return 2;
        }
    }

    // ==========================================
    // UNKNOWN COMMAND
    // ==========================================
    else
    {
        std::cerr
            << "Comando desconhecido: "
            << comando
            << "\n";

        return 1;
    }
}
