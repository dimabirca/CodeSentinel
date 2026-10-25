
#pragma once

#include <cstddef>
#include <filesystem>

#include "codesentinel/inspector/ScanResult.hpp"

class ProjectScanner
{
public:
    explicit ProjectScanner(
        std::size_t maxFiles = 100000,
        int maxDepth = 20
    );

    ScanResult scan(const std::filesystem::path& directory);

private:
    std::size_t maxFiles_;
    int maxDepth_;
};
