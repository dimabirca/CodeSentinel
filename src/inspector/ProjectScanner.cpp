
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "codesentinel/inspector/ProjectScanner.hpp"

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

    std::string detectLanguage(const fs::path& path)
    {
        std::string extension = path.extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );

        if (extension == ".cpp" ||
            extension == ".cc" ||
            extension == ".cxx")
            return "C++";

        if (extension == ".h" ||
            extension == ".hpp" ||
            extension == ".hxx")
            return "Headers";

        if (extension == ".c")
            return "C";

        if (extension == ".py")
            return "Python";

        if (extension == ".js" ||
            extension == ".mjs")
            return "JavaScript";

        if (extension == ".ts")
            return "TypeScript";

        if (extension == ".md")
            return "Markdown";

        if (extension == ".cmake" ||
            path.filename() == "CMakeLists.txt")
            return "CMake";

        return "Other";
    }
}

ProjectScanner::ProjectScanner(
    std::size_t maxFiles,
    int maxDepth
)
    : maxFiles_(maxFiles),
      maxDepth_(maxDepth)
{
    if (maxFiles_ == 0 || maxDepth_ < 0)
    {
        throw std::invalid_argument(
            "Limites do scanner invalidos."
        );
    }
}

ScanResult ProjectScanner::scan(const fs::path& directory)
{
    std::error_code ec;

    if (!fs::is_directory(directory, ec))
    {
        throw std::runtime_error(
            "Pasta invalida ou inacessivel: " +
            directory.string()
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

    ScanResult result;

    fs::recursive_directory_iterator it(
        directory,
        fs::directory_options::none,
        ec
    );

    if (ec)
    {
        throw std::runtime_error(
            "Erro ao abrir pasta: " + ec.message()
        );
    }

    const fs::recursive_directory_iterator end;

    while (it != end)
    {
        const auto& entry = *it;
        std::error_code entryError;

        const fs::file_status status =
            entry.symlink_status(entryError);

        if (entryError)
        {
            throw std::runtime_error(
                "Erro ao consultar entrada: " +
                entry.path().string() +
                " - " + entryError.message()
            );
        }

        if (fs::is_symlink(status))
        {
            result.exclusions.push_back({
                entry.path(),
                "Symbolic link"
            });

            it.disable_recursion_pending();
        }
        else if (isWindowsReparsePoint(entry.path()))
        {
            result.exclusions.push_back({
                entry.path(),
                "Windows reparse point"
            });

            it.disable_recursion_pending();
        }
        else if (fs::is_directory(status))
        {
            const std::string directoryName =
                entry.path().filename().string();

            if (ignoredDirectories.count(directoryName) > 0)
            {
                result.exclusions.push_back({
                    entry.path(),
                    "Ignored directory"
                });

                it.disable_recursion_pending();
            }
            else
            {
                ++result.totalDirectories;

                if (it.depth() >= maxDepth_)
                {
                    result.completed = false;
                    result.depthLimitReached = true;

                    result.exclusions.push_back({
                        entry.path(),
                        "Depth limit reached"
                    });

                    it.disable_recursion_pending();
                }
            }
        }
        else if (fs::is_regular_file(status))
        {
            if (result.totalFiles >= maxFiles_)
            {
                result.completed = false;
                result.fileLimitReached = true;

                result.exclusions.push_back({
                    entry.path(),
                    "File limit reached"
                });

                break;
            }

            ++result.totalFiles;

            const std::string language =
                detectLanguage(entry.path());

            ++result.languages[language];
        }

        it.increment(ec);

        if (ec)
        {
            throw std::runtime_error(
                "Erro ao percorrer pasta: " +
                ec.message()
            );
        }
    }

    return result;
}
