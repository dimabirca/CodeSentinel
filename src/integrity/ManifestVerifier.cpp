
#include "codesentinel/integrity/ManifestVerifier.hpp"
#include "codesentinel/integrity/IntegrityManifest.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace codesentinel::integrity
{

VerificationResult ManifestVerifier::verify(
    const std::filesystem::path& projectDirectory,
    const std::filesystem::path& manifestFile)
{
    namespace fs = std::filesystem;

    // 1. Validar o diretorio do projeto.
    if (!fs::exists(projectDirectory) ||
        !fs::is_directory(projectDirectory))
    {
        throw std::runtime_error(
            "Diretorio do projeto invalido."
        );
    }

    // 2. Nesta versao, o manifesto deve estar na
    // raiz do projeto com o nome esperado.
    const fs::path expectedManifestPath =
        projectDirectory / "codexkit-manifest.json";

    if (fs::absolute(manifestFile).lexically_normal() !=
        fs::absolute(expectedManifestPath).lexically_normal())
    {
        throw std::runtime_error(
            "O manifesto deve estar na raiz do projeto "
            "com o nome codexkit-manifest.json."
        );
    }

    // 3. Ler o manifesto original.
    const ::ManifestResult expected =
        ::IntegrityManifest::load(manifestFile);

    // 4. Criar um novo levantamento do projeto.
    const ::ManifestResult current =
        ::IntegrityManifest::create(projectDirectory);

    // 5. Rejeitar levantamentos incompletos.
    if (!current.completed ||
        current.fileLimitReached ||
        current.depthLimitReached)
    {
        throw std::runtime_error(
            "Verificacao incompleta: limites atingidos "
            "ou levantamento interrompido."
        );
    }

    // 6. Aceitar apenas a exclusao intencional
    // do proprio manifesto.
    std::size_t allowedExclusions = 0;

    for (const ::ManifestExclusion& exclusion :
         current.exclusions)
    {
        if (exclusion.path == "codexkit-manifest.json" &&
            exclusion.reason == "Manifest file")
        {
            ++allowedExclusions;
        }
        else
        {
            throw std::runtime_error(
                "Verificacao incompleta. Entrada excluida: " +
                exclusion.path +
                " | Motivo: " +
                exclusion.reason
            );
        }
    }

    // Deve existir exatamente uma exclusao permitida.
    if (allowedExclusions != 1 ||
        current.excludedEntries != allowedExclusions)
    {
        throw std::runtime_error(
            "Contagem inesperada de exclusoes "
            "durante a verificacao."
        );
    }

    VerificationResult result;

    // 7. Construir os mapas de hashes.
    std::unordered_map<std::string, std::string>
        expectedHashes;

    std::unordered_map<std::string, std::string>
        currentHashes;

    for (const ::ManifestEntry& entry : expected.files)
    {
        expectedHashes.emplace(
            entry.path,
            entry.sha256
        );
    }

    for (const ::ManifestEntry& entry : current.files)
    {
        currentHashes.emplace(
            entry.path,
            entry.sha256
        );
    }

    // 8. Comparar os ficheiros originais.
    for (const auto& [path, expectedHash] : expectedHashes)
    {
        const auto it = currentHashes.find(path);

        if (it == currentHashes.end())
        {
            result.removed.push_back(path);
        }
        else if (it->second == expectedHash)
        {
            result.unchanged.push_back(path);
        }
        else
        {
            result.modified.push_back(path);
        }
    }

    // 9. Identificar ficheiros adicionados.
    for (const auto& [path, currentHash] : currentHashes)
    {
        if (expectedHashes.find(path) ==
            expectedHashes.end())
        {
            result.added.push_back(path);
        }
    }

    // 10. Ordenar os resultados.
    std::sort(
        result.unchanged.begin(),
        result.unchanged.end()
    );

    std::sort(
        result.modified.begin(),
        result.modified.end()
    );

    std::sort(
        result.added.begin(),
        result.added.end()
    );

    std::sort(
        result.removed.begin(),
        result.removed.end()
    );

    // 11. Verificacao concluida.
    result.completed = true;

    return result;
}

} // namespace codesentinel::integrity
