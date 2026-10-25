
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace codesentinel::integrity
{

struct VerificationResult
{
    std::vector<std::string> unchanged;
    std::vector<std::string> modified;
    std::vector<std::string> added;
    std::vector<std::string> removed;

    bool completed = false;

    std::size_t totalChecked() const
    {
        return unchanged.size() +
               modified.size() +
               added.size() +
               removed.size();
    }

    bool isClean() const
    {
        return completed &&
               modified.empty() &&
               added.empty() &&
               removed.empty();
    }
};

class ManifestVerifier
{
public:
    static VerificationResult verify(
        const std::filesystem::path& projectDirectory,
        const std::filesystem::path& manifestFile
    );
};

} // namespace codesentinel::integrity
