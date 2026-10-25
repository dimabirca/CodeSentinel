\# CodeSentinel



\*\*Universal Project Analysis \& Integrity Toolkit\*\*



CodeSentinel is a modular command-line tool written in C++17 for analyzing software project structures and verifying file integrity.



It is designed to inspect projects written in different programming languages, regardless of the language used to implement the tool itself.



\## Project Status



\*\*Current version: v0.2.0\*\*



\*\*Platform:\*\* Windows (x64)



CodeSentinel currently provides project scanning, programming language detection by file extension, JSON report generation, SHA-256 hashing, integrity manifests, and file integrity verification.



Linux/macOS support, MCP integration, REST APIs, semantic analysis, and vulnerability auditing are potential future capabilities and are not available in this release.



\## Features



\- \*\*Project scanning:\*\* Recursively inspect files and directories.

\- \*\*Language detection:\*\* Identify recognized programming languages using file extensions.

\- \*\*JSON reports:\*\* Export structured project analysis results.

\- \*\*SHA-256 hashing:\*\* Calculate cryptographic hashes for individual files.

\- \*\*Integrity manifests:\*\* Generate a baseline of file hashes for a project.

\- \*\*Integrity verification:\*\* Detect modified, added, and removed files.

\- \*\*Safety controls:\*\* Apply directory exclusions, traversal limits, and selected filesystem protections.

\- \*\*Automated tests:\*\* Five test suites covering the scanner, hashing, manifests, verification, and CLI.



\## Requirements



\- Windows

\- Visual Studio 2022 with C++ development tools (MSVC)

\- CMake 3.16 or later

\- An internet connection for the initial dependency download, when required



The project uses C++17 and nlohmann/json v3.11.3.



\## Build Instructions



Clone the repository:



```powershell

git clone https://github.com/dimabirca/CodeSentinel.git

cd CodeSentinel

```



Configure the project:



```powershell

cmake -S . -B build -G "Visual Studio 17 2022" -A x64

```



Build the Release configuration:



```powershell

cmake --build build --config Release

```



Run the automated tests:



```powershell

ctest --test-dir build -C Release --output-on-failure

```



The executable will be available at:



`build\\\\Release\\\\codesentinel.exe`



\## Usage



Display the version:



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe --version

```



Display available commands:



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe --help

```



\### Scan a Project



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe scan .

```



\### Export a JSON Report



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe scan . --json .\\\\report.json

```



\### Calculate a SHA-256 Hash



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe hash .\\\\README.md

```



\### Generate an Integrity Manifest



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe manifest .\\\\examples\\\\manifest-demo

```



\### Verify File Integrity



```powershell

.\\\\build\\\\Release\\\\codesentinel.exe verify .\\\\examples\\\\manifest-demo

```



The verification command compares the current project files against the stored integrity manifest.



\## Verification Exit Codes



| Exit code | Meaning |

|---|---|

| `0` | Integrity verified |

| `1` | File changes detected |

| `2` | Error or incomplete verification |



\## Integrity Manifest Format



The current default integrity manifest filename is:



`codexkit-manifest.json`



This filename is part of the current technical format and is retained for compatibility with the existing implementation.



Integrity manifests store file information and SHA-256 hashes used to detect changes.



\*\*Important:\*\* SHA-256 integrity verification detects file modifications but does not authenticate the origin of a manifest. A trusted baseline must be protected independently.



\## Project Structure



```text

CodeSentinel/

├── include/

│   └── codesentinel/

│       ├── inspector/

│       └── integrity/

├── src/

│   ├── inspector/

│   ├── integrity/

│   └── main.cpp

├── tests/

├── examples/

├── docs/

├── CMakeLists.txt

└── README.md

```



\## Architecture



CodeSentinel separates its functionality into three main areas:



\*\*Project Inspector\*\* — Project structure scanning, language detection, and JSON reporting.



\*\*Integrity Guard\*\* — SHA-256 hashing, manifest generation, and integrity verification.



\*\*Command-Line Interface\*\* — User-facing commands and execution status reporting.



For more information, see \[Architecture Documentation](docs/ARCHITECTURE.md).



\## Testing



The project contains five automated test suites:



1\. ScannerTests

2\. IntegrityTests

3\. ManifestTests

4\. VerifierTests

5\. CliTests



All five suites passed during local Windows Release validation of v0.2.0.



\## Security and Limitations



\- Programming languages are identified using known file extensions, not semantic analysis.

\- Directory scanning applies exclusion rules and traversal limits.

\- Some filesystem operations include protections against reparse-point-related risks.

\- JSON report writing uses temporary files and additional destination checks.

\- Concurrent file modifications may affect integrity verification results.

\- Integrity manifests are not digitally signed.

\- The tool does not replace a professional security audit.

\- This release requires Windows.



\## Roadmap



Potential future development areas include:



\- Additional programming language recognition.

\- Framework and technology detection.

\- More detailed project analysis reports.

\- Linux and macOS compatibility.

\- MCP integration.

\- REST API support.

\- Extended security analysis.



These items are development goals, not features included in v0.2.0.



\## License



CodeSentinel is released under the MIT License.



Copyright (c) 2026 Dimitru Birca.



See the \[LICENSE](LICENSE) file for the full license text.



\---



\*\*CodeSentinel — Universal Project Analysis \& Integrity Toolkit\*\*

