
cmake_minimum_required(VERSION 3.16)

project(CodeSentinel VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# DEPENDENCIAS
# ==========================================

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# ==========================================
# INSPECTOR
# ==========================================

add_library(codesentinel_inspector
    src/inspector/ProjectScanner.cpp
    src/inspector/ScanReportJson.cpp
)

target_include_directories(codesentinel_inspector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(codesentinel_inspector
    PUBLIC
        nlohmann_json::nlohmann_json
)

# ==========================================
# INTEGRITY GUARD
# ==========================================

add_library(codesentinel_integrity
    src/integrity/FileHasher.cpp
    src/integrity/IntegrityManifest.cpp
    src/integrity/ManifestVerifier.cpp
)

target_include_directories(codesentinel_integrity
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(codesentinel_integrity
    PUBLIC
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(codesentinel_integrity
        PUBLIC
            bcrypt
    )
else()
    message(FATAL_ERROR
        "Integrity Guard requer Windows nesta versao."
    )
endif()

# ==========================================
# EXECUTAVEL PRINCIPAL
# ==========================================

add_executable(codesentinel
    src/main.cpp
)

target_link_libraries(codesentinel
    PRIVATE
        codesentinel_inspector
        codesentinel_integrity
)

# ==========================================
# TESTES AUTOMATIZADOS
# ==========================================

enable_testing()

# ScannerTests
add_executable(codesentinel_tests
    tests/ScannerTests.cpp
)

target_link_libraries(codesentinel_tests
    PRIVATE
        codesentinel_inspector
)

add_test(
    NAME ScannerTests
    COMMAND codesentinel_tests
)

# IntegrityTests
add_executable(codesentinel_integrity_tests
    tests/IntegrityTests.cpp
)

target_link_libraries(codesentinel_integrity_tests
    PRIVATE
        codesentinel_integrity
)

add_test(
    NAME IntegrityTests
    COMMAND codesentinel_integrity_tests
)

# ManifestTests
add_executable(codesentinel_manifest_tests
    tests/ManifestTests.cpp
)

target_link_libraries(codesentinel_manifest_tests
    PRIVATE
        codesentinel_integrity
)

add_test(
    NAME ManifestTests
    COMMAND codesentinel_manifest_tests
)

# VerifierTests
add_executable(codesentinel_verifier_tests
    tests/VerifierTests.cpp
)

target_link_libraries(codesentinel_verifier_tests
    PRIVATE
        codesentinel_integrity
)

add_test(
    NAME VerifierTests
    COMMAND codesentinel_verifier_tests
)

# ==========================================
# CLI INTEGRATION TESTS
# ==========================================

add_executable(codesentinel_cli_tests
    tests/CliTests.cpp
)

add_test(
    NAME CliTests
    COMMAND codesentinel_cli_tests $<TARGET_FILE:codesentinel>
)

set_tests_properties(
    CliTests
    PROPERTIES
        TIMEOUT 30
)
