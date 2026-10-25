\# CodeSentinel — Architecture



\## Overview



CodeSentinel is a modular command-line application developed in C++17 for Windows.



Its architecture separates project inspection, integrity verification and command-line operations.



The current version is \*\*v0.2.0\*\*.



\## Core Components



\### 1. Project Inspector



\*\*Location:\*\* `src/inspector/`



Responsible for:



\- Recursive project directory scanning.

\- File and directory counting.

\- Programming language classification using file extensions.

\- Exclusion of selected directories.

\- Scan depth and file count limits.

\- JSON report generation.



\### 2. Integrity Guard



\*\*Location:\*\* `src/integrity/`



Responsible for:



\- SHA-256 file hashing using Windows BCrypt.

\- Integrity manifest generation.

\- File integrity verification.

\- Detection of modified, added and removed files.

\- Reporting verification errors.



The current manifest filename is `codexkit-manifest.json`.



\### 3. Command-Line Interface



\*\*Location:\*\* `src/main.cpp`



Provides the following commands:



\- `scan`

\- `scan --json`

\- `hash`

\- `manifest`

\- `verify`

\- `--help`

\- `--version`



\### 4. Automated Tests



\*\*Location:\*\* `tests/`



The project includes five CTest suites:



\- ScannerTests

\- IntegrityTests

\- ManifestTests

\- VerifierTests

\- CliTests



\## Build System



CodeSentinel uses:



\- C++17

\- CMake

\- Visual Studio 2022 / MSVC

\- nlohmann/json v3.11.3

\- Windows BCrypt API



\## Security Considerations



The implementation includes directory traversal limits, exclusion rules, selected reparse-point protections and safeguards for JSON report writing.



SHA-256 hashes support change detection but do not provide digital signatures or authenticate the manifest.



\## Planned Development



Future development may include:



\- Extended programming language recognition.

\- Project technology and framework detection.

\- Linux and macOS compatibility.

\- MCP integration.

\- REST API.

\- Additional analysis and security auditing capabilities.



These features are not implemented in v0.2.0.



\## Architecture Principles



CodeSentinel aims to maintain:



\- Modular components.

\- Clear separation of responsibilities.

\- Automated regression testing.

\- Explicit error handling.

\- Backward-compatible data formats where required.

\- Extensibility for future integrations.
