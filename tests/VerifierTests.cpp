
#include "codesentinel/integrity/IntegrityManifest.hpp"
#include "codesentinel/integrity/ManifestVerifier.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

using codesentinel::integrity::ManifestVerifier;

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

void createManifest(const fs::path& project)
{
    const auto manifest =
        IntegrityManifest::create(project);

    require(
        manifest.completed &&
        manifest.excludedEntries == 0 &&
        manifest.exclusions.empty(),
        "Nao foi possivel criar manifesto completo."
    );

    IntegrityManifest::save(
        manifest,
        project / "codexkit-manifest.json"
    );
}

void testUnchanged(const fs::path& root)
{
    const fs::path project = root / "unchanged";

    writeFile(project / "hello.txt", "Hello");
    writeFile(project / "readme.md", "Readme");

    createManifest(project);

    const auto result = ManifestVerifier::verify(
        project,
        project / "codexkit-manifest.json"
    );

    require(result.completed, "Verificacao incompleta.");
    require(result.isClean(), "Projeto deveria estar intacto.");
    require(result.unchanged.size() == 2, "Esperados 2 ficheiros intactos.");

    std::cout << "[PASS] Verify unchanged\n";
}

void testModified(const fs::path& root)
{
    const fs::path project = root / "modified";

    writeFile(project / "hello.txt", "Original");
    createManifest(project);

    writeFile(project / "hello.txt", "Alterado");

    const auto result = ManifestVerifier::verify(
        project,
        project / "codexkit-manifest.json"
    );

    require(!result.isClean(), "Alteracao nao detetada.");
    require(result.modified.size() == 1, "Esperado 1 ficheiro modificado.");
    require(result.modified[0] == "hello.txt", "Ficheiro modificado incorreto.");

    std::cout << "[PASS] Verify modified\n";
}

void testAdded(const fs::path& root)
{
    const fs::path project = root / "added";

    writeFile(project / "hello.txt", "Original");
    createManifest(project);

    writeFile(project / "novo.txt", "Novo ficheiro");

    const auto result = ManifestVerifier::verify(
        project,
        project / "codexkit-manifest.json"
    );

    require(!result.isClean(), "Novo ficheiro nao detetado.");
    require(result.added.size() == 1, "Esperado 1 ficheiro adicionado.");
    require(result.added[0] == "novo.txt", "Ficheiro adicionado incorreto.");

    std::cout << "[PASS] Verify added\n";
}

void testRemoved(const fs::path& root)
{
    const fs::path project = root / "removed";

    writeFile(project / "hello.txt", "Original");
    createManifest(project);

    fs::remove(project / "hello.txt");

    const auto result = ManifestVerifier::verify(
        project,
        project / "codexkit-manifest.json"
    );

    require(!result.isClean(), "Remocao nao detetada.");
    require(result.removed.size() == 1, "Esperado 1 ficheiro removido.");
    require(result.removed[0] == "hello.txt", "Ficheiro removido incorreto.");

    std::cout << "[PASS] Verify removed\n";
}

void testMixedChanges(const fs::path& root)
{
    const fs::path project = root / "mixed";

    writeFile(project / "unchanged.txt", "Intacto");
    writeFile(project / "modified.txt", "Original");
    writeFile(project / "removed.txt", "Eliminar");

    createManifest(project);

    writeFile(project / "modified.txt", "Alterado");
    writeFile(project / "added.txt", "Novo");
    fs::remove(project / "removed.txt");

    const auto result = ManifestVerifier::verify(
        project,
        project / "codexkit-manifest.json"
    );

    require(result.completed, "Verificacao incompleta.");
    require(!result.isClean(), "Alteracoes nao detetadas.");

    require(result.unchanged.size() == 1, "Contagem unchanged incorreta.");
    require(result.modified.size() == 1, "Contagem modified incorreta.");
    require(result.added.size() == 1, "Contagem added incorreta.");
    require(result.removed.size() == 1, "Contagem removed incorreta.");

    require(result.unchanged[0] == "unchanged.txt", "Unchanged incorreto.");
    require(result.modified[0] == "modified.txt", "Modified incorreto.");
    require(result.added[0] == "added.txt", "Added incorreto.");
    require(result.removed[0] == "removed.txt", "Removed incorreto.");

    std::cout << "[PASS] Verify mixed changes\n";
}

void testMissingManifest(const fs::path& root)
{
    const fs::path project = root / "missing_manifest";

    writeFile(project / "hello.txt", "Hello");

    bool rejected = false;

    try
    {
        ManifestVerifier::verify(
            project,
            project / "codexkit-manifest.json"
        );
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(rejected, "Manifesto inexistente deveria falhar.");

    std::cout << "[PASS] Verify missing manifest\n";
}


void testManifestOutsideRoot(const fs::path& root)
{
    const fs::path project = root / "outside_root";
    const fs::path other = root / "other_location";

    writeFile(project / "hello.txt", "Hello");
    writeFile(other / "hello.txt", "Other");

    createManifest(project);
    createManifest(other);

    bool rejected = false;

    try
    {
        ManifestVerifier::verify(
            project,
            other / "codexkit-manifest.json"
        );
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(
        rejected,
        "Manifesto fora da raiz deveria ser rejeitado."
    );

    std::cout << "[PASS] Verify manifest outside root\n";
}


void testInvalidManifest(const fs::path& root)
{
    const fs::path project = root / "invalid_manifest";

    writeFile(project / "hello.txt", "Hello");

    writeFile(
        project / "codexkit-manifest.json",
        "{ invalid json"
    );

    bool rejected = false;

    try
    {
        ManifestVerifier::verify(
            project,
            project / "codexkit-manifest.json"
        );
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(
        rejected,
        "Manifesto invalido deveria ser rejeitado."
    );

    std::cout << "[PASS] Verify invalid manifest\n";
}


void testSymlinkExclusion(const fs::path& root)
{
    const fs::path project = root / "symlink_project";
    const fs::path external = root / "external_target.txt";

    writeFile(project / "hello.txt", "Hello");
    writeFile(external, "External");

    createManifest(project);

    const fs::path link = project / "external_link.txt";

    std::error_code ec;

    fs::create_symlink(external, link, ec);

    if (ec)
    {
        // No Windows, a criacao de symlinks pode exigir
        // Developer Mode ou privilegios especificos.
        std::cout
            << "[SKIP] Verify symlink exclusion: "
            << ec.message() << '\n';
        return;
    }

    bool rejected = false;

    try
    {
        ManifestVerifier::verify(
            project,
            project / "codexkit-manifest.json"
        );
    }
    catch (const std::exception&)
    {
        rejected = true;
    }

    require(
        rejected,
        "Link simbolico nao analisado deveria impedir "
        "a confirmacao de integridade."
    );

    std::cout << "[PASS] Verify symlink exclusion\n";
}


} // namespace

int main()
{
    const fs::path root =
        fs::temp_directory_path() /
        "codesentinel_verifier_tests";

    try
    {
        // Remover dados de execucoes anteriores.
        fs::remove_all(root);
        fs::create_directories(root);

        testUnchanged(root);
        testModified(root);
        testAdded(root);
        testRemoved(root);
        testMixedChanges(root);
        testMissingManifest(root);
        testManifestOutsideRoot(root);
        testInvalidManifest(root);
        testSymlinkExclusion(root);

        fs::remove_all(root);

        std::cout << "All verifier tests passed!\n";

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] " << ex.what() << '\n';

        return 1;
    }
}
