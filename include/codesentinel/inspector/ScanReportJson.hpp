
#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

#include "codesentinel/inspector/ScanResult.hpp"

class ScanReportJson
{
public:
    static nlohmann::json toJson(
        const ScanResult& result
    );

    static void save(
        const ScanResult& result,
        const std::filesystem::path& outputPath
    );
};
