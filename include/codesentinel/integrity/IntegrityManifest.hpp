
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct ManifestEntry
{
    std::string path;
    std::string sha256;
};

struct ManifestExclusion
{
    std::string path;
    std::string reason;
};

struct ManifestResult
{
    std::vector<ManifestEntry> files;
    std::vector<ManifestExclusion> exclusions;

    std::size_t excludedEntries = 0;

    bool completed = true;
    bool fileLimitReached = false;
    bool depthLimitReached = false;
};


class IntegrityManifest
{
public:
    static ManifestResult create(
        const std::filesystem::path& projectDirectory,
        std::size_t maxFiles = 100000,
        int maxDepth = 20
    );

    static ManifestResult load(
        const std::filesystem::path& manifestFile
    );

    static void save(
        const ManifestResult& manifest,
        const std::filesystem::path& outputFile
    );
};
