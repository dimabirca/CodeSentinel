
#include "codesentinel/integrity/IntegrityManifest.hpp"
#include "codesentinel/integrity/FileHasher.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

#include <nlohmann/json.hpp>
#include <cctype>

#ifdef _WIN32
#include <Windows.h>
#endif


namespace fs = std::filesystem;

namespace
{
    bool isWindowsReparsePoint(const fs::path& path)
    {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(path.c_str());

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            throw std::runtime_error(
                "Erro ao consultar atributos Windows: " +
                path.string()
            );
        }

        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
        (void)path;
        return false;
#endif
    }

    std::string escapeJson(const std::string& value)
    {
        std::ostringstream output;

        for (unsigned char c : value)
        {
            switch (c)
            {
                case '"':  output << "\\\""; break;
                case '\\': output << "\\\\"; break;
                case '\b': output << "\\b"; break;
                case '\f': output << "\\f"; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;

                default:
                    if (c < 0x20)
                    {
                        output << "\\u"
                               << std::hex
                               << std::setw(4)
                               << std::setfill('0')
                               << static_cast<unsigned int>(c)
                               << std::dec;
                    }
                    else
                    {
                        output << static_cast<char>(c);
                    }
            }
        }

        return output.str();
    }

    bool isManifestFile(const fs::path& path)
    {
        return path.filename() == "codexkit-manifest.json";
    }

    void addExclusion(
        ManifestResult& result,
        const fs::path& path,
        const fs::path& root,
        const std::string& reason
    )
    {
        result.exclusions.push_back({
            path.lexically_relative(root).generic_string(),
            reason
        });

        ++result.excludedEntries;
    }

    
std::string toUpperAscii(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }

    return value;
}

bool isReservedWindowsName(const std::string& component)
{
    // Os nomes reservados continuam reservados
    // mesmo quando possuem extensao.
    const std::size_t dot = component.find('.');

    const std::string base = toUpperAscii(
        component.substr(0, dot)
    );

    static const std::unordered_set<std::string> reserved = {
        "CON", "PRN", "AUX", "NUL",
        "CONIN$", "CONOUT$",
        "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
        "LPT6", "LPT7", "LPT8", "LPT9"
    };

    return reserved.count(base) > 0;
}

void validateManifestPath(const std::string& path)
{
    if (path.empty())
    {
        throw std::runtime_error(
            "Caminho vazio no manifesto."
        );
    }

    // O manifesto utiliza sempre '/' como separador.
    if (path.front() == '/' ||
        path.back() == '/' ||
        path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos)
    {
        throw std::runtime_error(
            "Caminho absoluto ou invalido: " + path
        );
    }

    // Evitar componentes vazios e barras repetidas.
    if (path.find("//") != std::string::npos)
    {
        throw std::runtime_error(
            "Caminho nao canonico: " + path
        );
    }

    std::size_t start = 0;

    while (start < path.size())
    {
        const std::size_t end = path.find('/', start);

        const std::string component = path.substr(
            start,
            end == std::string::npos
                ? std::string::npos
                : end - start
        );

        if (component.empty() ||
            component == "." ||
            component == "..")
        {
            throw std::runtime_error(
                "Componente inseguro: " + path
            );
        }

        if (component.back() == '.' ||
            component.back() == ' ')
        {
            throw std::runtime_error(
                "Nome terminado em ponto ou espaco: " + path
            );
        }

        if (isReservedWindowsName(component))
        {
            throw std::runtime_error(
                "Nome reservado do Windows: " + path
            );
        }

        for (unsigned char c : component)
        {
            if (c < 0x20 ||
                c == '<' ||
                c == '>' ||
                c == '"' ||
                c == '|' ||
                c == '?' ||
                c == '*')
            {
                throw std::runtime_error(
                    "Caractere invalido no caminho: " + path
                );
            }
        }

        if (end == std::string::npos)
        {
            break;
        }

        start = end + 1;
    }
}

}

ManifestResult IntegrityManifest::create(
    const fs::path& projectDirectory,
    std::size_t maxFiles,
    int maxDepth
)
{
    if (maxFiles == 0 || maxDepth < 0)
    {
        throw std::invalid_argument(
            "Limites do manifesto invalidos."
        );
    }

    std::error_code ec;

    if (!fs::is_directory(projectDirectory, ec))
    {
        throw std::runtime_error(
            "Pasta invalida ou inacessivel: " +
            projectDirectory.string()
        );
    }

    if (isWindowsReparsePoint(projectDirectory))
    {
        throw std::runtime_error(
            "A pasta raiz nao pode ser um reparse point."
        );
    }

    const std::unordered_set<std::string> ignoredDirectories =
    {
        "build",
        ".git",
        "node_modules",
        ".vs",
        ".vscode"
    };

    ManifestResult result;

    // Registar caminhos ja adicionados, sem distinguir maiusculas/minusculas.
    std::unordered_set<std::string> seenPaths;

    fs::recursive_directory_iterator it(
        projectDirectory,
        fs::directory_options::none,
        ec
    );

    if (ec)
    {
        throw std::runtime_error(
            "Erro ao abrir projeto: " + ec.message()
        );
    }

    const fs::recursive_directory_iterator end;

    while (it != end)
    {
        const fs::directory_entry& entry = *it;

        std::error_code statusError;

        const fs::file_status status =
            entry.symlink_status(statusError);

        if (statusError)
        {
            throw std::runtime_error(
                "Erro ao consultar entrada: " +
                entry.path().string() + " - " +
                statusError.message()
            );
        }

        if (fs::is_symlink(status))
        {
            addExclusion(
                result, entry.path(), projectDirectory,
                "Symbolic link"
            );

            it.disable_recursion_pending();
        }
        else if (isWindowsReparsePoint(entry.path()))
        {
            addExclusion(
                result, entry.path(), projectDirectory,
                "Windows reparse point"
            );

            it.disable_recursion_pending();
        }
        else if (fs::is_directory(status))
        {
            const std::string name =
                entry.path().filename().string();

            if (ignoredDirectories.count(name) > 0)
            {
                addExclusion(
                    result, entry.path(), projectDirectory,
                    "Ignored directory"
                );

                it.disable_recursion_pending();
            }
            else if (it.depth() >= maxDepth)
            {
                result.completed = false;
                result.depthLimitReached = true;

                addExclusion(
                    result, entry.path(), projectDirectory,
                    "Depth limit reached"
                );

                it.disable_recursion_pending();
            }
        }
        else if (fs::is_regular_file(status))
        {
            if (isManifestFile(entry.path()))
            {
                addExclusion(
                    result, entry.path(), projectDirectory,
                    "Manifest file"
                );
            }
            else if (result.files.size() >= maxFiles)
            {
                result.completed = false;
                result.fileLimitReached = true;

                addExclusion(
                    result, entry.path(), projectDirectory,
                    "File limit reached"
                );

                break;
            }
            else
            {
                const fs::path relativePath =
                    entry.path().lexically_relative(
                        projectDirectory
                    );

                
                
            ManifestEntry item;

            item.path = relativePath.generic_string();

            // Validar o caminho antes de o adicionar.
            validateManifestPath(item.path);

            // Impedir caminhos duplicados que diferem apenas
            // em maiusculas/minusculas.
            const std::string normalizedPath =
                toUpperAscii(item.path);

            if (!seenPaths.insert(normalizedPath).second)
            {
                throw std::runtime_error(
                    "Caminho duplicado durante a criacao "
                    "do manifesto: " + item.path
                );
            }

            // Calcular SHA-256 apenas depois de validar
            // o caminho e confirmar a sua unicidade.
            item.sha256 =
                FileHasher::sha256(entry.path());

            result.files.push_back(std::move(item));


            }
        }

        it.increment(ec);

        if (ec)
        {
            throw std::runtime_error(
                "Erro ao percorrer projeto: " +
                ec.message()
            );
        }
    }

    std::sort(
        result.files.begin(),
        result.files.end(),
        [](const ManifestEntry& a, const ManifestEntry& b)
        {
            return a.path < b.path;
        }
    );

    std::sort(
        result.exclusions.begin(),
        result.exclusions.end(),
        [](const ManifestExclusion& a,
           const ManifestExclusion& b)
        {
            return a.path < b.path;
        }
    );

    return result;
}


void IntegrityManifest::save(
    const ManifestResult& manifest,
    const fs::path& outputFile
)
{
    if (!manifest.completed)
    {
        throw std::runtime_error(
            "Manifesto incompleto: gravacao recusada."
        );
    }

    if (outputFile.empty() || outputFile.filename().empty())
    {
        throw std::invalid_argument(
            "Caminho de destino invalido."
        );
    }

#ifndef _WIN32
    throw std::runtime_error(
        "Gravacao segura requer Windows nesta versao."
    );
#else
    // Criar um nome temporario exclusivo na mesma pasta.
    // O ficheiro e criado com CREATE_NEW para evitar
    // sobrescrever um temporario existente.

    static std::atomic<std::uint64_t> counter{0};

    const fs::path parent =
        outputFile.has_parent_path()
            ? outputFile.parent_path()
            : fs::path(".");

    const fs::path absoluteParent = fs::absolute(parent);

    fs::path temporaryFile;
    HANDLE temporaryHandle = INVALID_HANDLE_VALUE;

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const auto timestamp =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        const auto sequence = ++counter;

        const std::wstring name =
            L".codesentinel-" +
            std::to_wstring(GetCurrentProcessId()) +
            L"-" +
            std::to_wstring(timestamp) +
            L"-" +
            std::to_wstring(sequence) +
            L".tmp";

        temporaryFile = absoluteParent / name;

        temporaryHandle = CreateFileW(
            temporaryFile.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (temporaryHandle != INVALID_HANDLE_VALUE)
        {
            break;
        }

        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_ALREADY_EXISTS)
        {
            throw std::runtime_error(
                "Nao foi possivel criar ficheiro temporario."
            );
        }
    }

    if (temporaryHandle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error(
            "Nao foi possivel obter nome temporario exclusivo."
        );
    }

    // Garantir limpeza mesmo quando ocorre uma excecao.
    struct TemporaryFileGuard
    {
        fs::path path;
        HANDLE handle = INVALID_HANDLE_VALUE;

        ~TemporaryFileGuard()
        {
            if (handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(handle);
            }

            if (!path.empty())
            {
                DeleteFileW(path.c_str());
            }
        }
    };

    TemporaryFileGuard guard{
        temporaryFile,
        temporaryHandle
    };

    // Preparar o JSON em memoria antes de escrever.
    std::ostringstream json;

    json << "{\n";
    json << "  \"version\": 1,\n";
    json << "  \"algorithm\": \"SHA-256\",\n";
    json << "  \"files\": [\n";

    for (std::size_t i = 0;
         i < manifest.files.size(); ++i)
    {
        const ManifestEntry& entry = manifest.files[i];

        json << "    {\n";

        json << "      \"path\": \""
             << escapeJson(entry.path)
             << "\",\n";

        json << "      \"sha256\": \""
             << escapeJson(entry.sha256)
             << "\"\n";

        json << "    }";

        if (i + 1 < manifest.files.size())
        {
            json << ",";
        }

        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    if (!json)
    {
        throw std::runtime_error(
            "Erro ao construir JSON."
        );
    }

    const std::string content = json.str();

    // Escrever todos os bytes, tratando escritas parciais.
    std::size_t offset = 0;

    while (offset < content.size())
    {
        const std::size_t remaining =
            content.size() - offset;

        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(remaining, 65536)
        );

        DWORD written = 0;

        const BOOL success = WriteFile(
            guard.handle,
            content.data() + offset,
            chunk,
            &written,
            nullptr
        );

        if (!success || written == 0)
        {
            throw std::runtime_error(
                "Erro ao escrever ficheiro temporario."
            );
        }

        offset += written;
    }

    // Solicitar a descarga dos dados antes da substituicao.
    if (!FlushFileBuffers(guard.handle))
    {
        throw std::runtime_error(
            "Erro ao sincronizar ficheiro temporario."
        );
    }

    if (!CloseHandle(guard.handle))
    {
        guard.handle = INVALID_HANDLE_VALUE;

        throw std::runtime_error(
            "Erro ao fechar ficheiro temporario."
        );
    }

    guard.handle = INVALID_HANDLE_VALUE;

    // Impedir substituir um diretorio ou um reparse point.
    const fs::path absoluteOutput = fs::absolute(outputFile);

    const DWORD attributes =
        GetFileAttributesW(absoluteOutput.c_str());

    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            throw std::runtime_error(
                "Destino inseguro: diretorio ou reparse point."
            );
        }
    }
    else if (GetLastError() != ERROR_FILE_NOT_FOUND)
    {
        throw std::runtime_error(
            "Erro ao consultar destino do manifesto."
        );
    }

    // Substituir o destino sem o truncar previamente.
    if (!MoveFileExW(
            temporaryFile.c_str(),
            absoluteOutput.c_str(),
            MOVEFILE_REPLACE_EXISTING |
            MOVEFILE_WRITE_THROUGH
        ))
    {
        throw std::runtime_error(
            "Nao foi possivel substituir o manifesto."
        );
    }

    // O temporario deixou de existir com este nome.
    guard.path.clear();
#endif
}


ManifestResult IntegrityManifest::load(
    const fs::path& manifestFile
)
{
    std::error_code ec;

    if (!fs::is_regular_file(manifestFile, ec) || ec)
    {
        throw std::runtime_error(
            "Manifesto inexistente ou inacessivel: " +
            manifestFile.string()
        );
    }

    if (fs::is_symlink(fs::symlink_status(manifestFile)) ||
        isWindowsReparsePoint(manifestFile))
    {
        throw std::runtime_error(
            "Manifesto inseguro: link ou reparse point."
        );
    }

    // Limite inicial de 32 MiB para o JSON.
    constexpr std::uintmax_t maxManifestSize =
        32ULL * 1024ULL * 1024ULL;

    const auto size = fs::file_size(manifestFile, ec);

    if (ec || size > maxManifestSize)
    {
        throw std::runtime_error(
            "Manifesto demasiado grande ou inacessivel."
        );
    }

    std::ifstream file(manifestFile, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "Nao foi possivel abrir o manifesto."
        );
    }

    nlohmann::json document;

    try
    {
        file >> document;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw std::runtime_error(
            std::string("JSON invalido: ") + e.what()
        );
    }

    if (file.bad())
    {
        throw std::runtime_error(
            "Erro durante a leitura do manifesto."
        );
    }

    if (!document.is_object() ||
        !document.contains("version") ||
        !document["version"].is_number_integer() ||
        document["version"] != 1)
    {
        throw std::runtime_error(
            "Versao do manifesto invalida."
        );
    }

    if (!document.contains("algorithm") ||
        !document["algorithm"].is_string() ||
        document["algorithm"] != "SHA-256")
    {
        throw std::runtime_error(
            "Algoritmo do manifesto invalido."
        );
    }

    if (!document.contains("files") ||
        !document["files"].is_array())
    {
        throw std::runtime_error(
            "Lista de ficheiros invalida."
        );
    }

    if (document["files"].size() > 100000)
    {
        throw std::runtime_error(
            "Manifesto excede o limite de ficheiros."
        );
    }

    ManifestResult result;

    // Guardar os caminhos ja encontrados.
    std::unordered_set<std::string> seenPaths;

    for (const auto& item : document["files"])
    {
        if (!item.is_object() ||
            !item.contains("path") ||
            !item["path"].is_string() ||
            !item.contains("sha256") ||
            !item["sha256"].is_string())
        {
            throw std::runtime_error(
                "Entrada do manifesto invalida."
            );
        }

        const std::string path =
            item["path"].get<std::string>();

        const std::string hash =
            item["sha256"].get<std::string>();

            
        // Validar o caminho antes de o aceitar.
        validateManifestPath(path);

        // Comparacao sem distinguir maiusculas de minusculas,
        // para evitar duplicados ambiguos no Windows.
        const std::string normalizedPath = toUpperAscii(path);

        if (!seenPaths.insert(normalizedPath).second)
        {
            throw std::runtime_error(
                "Caminho duplicado no manifesto: " + path
            );
        }

        if (path.empty() ||
            path.find('\\') != std::string::npos ||
            path.find(':') != std::string::npos ||
            path.front() == '/' ||
            path.back() == '/')
        {
            throw std::runtime_error(
                "Caminho inseguro no manifesto: " + path
            );
        }

        const fs::path relativePath(path);

        if (relativePath.is_absolute())
        {
            throw std::runtime_error(
                "Caminho absoluto no manifesto."
            );
        }

        for (const auto& component : relativePath)
        {
            if (component == "." || component == "..")
            {
                throw std::runtime_error(
                    "Caminho com navegacao insegura: " + path
                );
            }
        }

        if (hash.size() != 64)
        {
            throw std::runtime_error(
                "Comprimento SHA-256 invalido: " + path
            );
        }

        for (unsigned char c : hash)
        {
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F')))
            {
                throw std::runtime_error(
                    "SHA-256 invalido: " + path
                );
            }
        }

        result.files.push_back({path, hash});
    }

    return result;
}
