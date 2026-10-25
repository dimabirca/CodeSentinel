
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ScanExclusion
{
    std::filesystem::path path;
    std::string reason;
};

struct ScanResult
{
    std::size_t totalFiles = 0;
    std::size_t totalDirectories = 0;

    std::map<std::string, std::size_t> languages;

    bool completed = true;
    bool fileLimitReached = false;
    bool depthLimitReached = false;

    std::vector<ScanExclusion> exclusions;
};
