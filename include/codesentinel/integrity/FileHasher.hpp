
#pragma once

#include <filesystem>
#include <string>

class FileHasher
{
public:
    static std::string sha256(
        const std::filesystem::path& filePath
    );
};
