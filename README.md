\# CodeSentinel



\*\*Universal Project Analysis \& Integrity Toolkit\*\*



CodeSentinel é uma ferramenta de linha de comandos (CLI), desenvolvida em C++17, para analisar a estrutura de projetos de software e verificar a integridade de ficheiros.



Foi concebida para analisar projetos de diferentes linguagens de programação, independentemente da linguagem utilizada na sua implementação.



\## Estado do projeto



\*\*Versão atual: v0.2.0\*\*



O CodeSentinel está disponível para Windows e inclui análise estrutural de projetos, exportação JSON, cálculo de hashes SHA-256 e verificação de integridade.



Funcionalidades como suporte para Linux/macOS, integração MCP, API REST, análise semântica e auditoria de vulnerabilidades estão previstas para desenvolvimento futuro e ainda não estão implementadas.



\## Funcionalidades



\- `scan <pasta>` — analisa ficheiros e diretórios e identifica linguagens através de extensões conhecidas.

\- `scan <pasta> --json <ficheiro>` — exporta um relatório JSON com estatísticas, estado e exclusões.

\- `hash <ficheiro>` — calcula o hash SHA-256 de um ficheiro.

\- `manifest <pasta>` — gera um manifesto de integridade com hashes SHA-256.

\- `verify <pasta>` — verifica a integridade dos ficheiros através de um manifesto.

\- `--help` — apresenta os comandos disponíveis.

\- `--version` — apresenta a versão da ferramenta.



\## Requisitos



\- Windows

\- Visual Studio 2022 com ferramentas de desenvolvimento C++ (MSVC)

\- CMake

\- PowerShell

\- Ligação à Internet na configuração inicial, caso seja necessário obter `nlohmann/json` v3.11.3



\## Compilação e testes



Na raiz do projeto, executa:



```powershell

cmake -S . -B build -G "Visual Studio 17 2022" -A x64

cmake --build build --config Debug

ctest --test-dir build -C Debug --output-on-failure

cmake --build build --config Release

ctest --test-dir build -C Release --output-on-failure

```



Os executáveis são gerados em:



\- `build\\Debug\\codesentinel.exe`

\- `build\\Release\\codesentinel.exe`



\## Exemplos de utilização



```powershell

.\\build\\Debug\\codesentinel.exe --version

.\\build\\Debug\\codesentinel.exe --help

.\\build\\Debug\\codesentinel.exe scan .

.\\build\\Debug\\codesentinel.exe scan .\\examples\\manifest-demo --json .\\relatorio.json

.\\build\\Debug\\codesentinel.exe hash .\\README.md

.\\build\\Debug\\codesentinel.exe manifest .\\examples\\manifest-demo

.\\build\\Debug\\codesentinel.exe verify .\\examples\\manifest-demo

```



O comando `verify` utiliza os seguintes códigos de saída:



| Código | Significado |

|---|---|

| `0` | Integridade confirmada |

| `1` | Foram detetadas alterações |

| `2` | Erro ou verificação incompleta |



\## Formato do manifesto



O CodeSentinel utiliza atualmente o ficheiro `codexkit-manifest.json` como nome predefinido do manifesto de integridade.



Este nome faz parte da implementação atual e deve ser preservado para garantir o funcionamento dos comandos `manifest` e `verify`.



\## Segurança e limitações



\- O scanner aplica limites de profundidade e número de ficheiros.

\- Diretórios como `.git`, `build`, `node\_modules`, `.vs` e `.vscode` são ignorados.

\- A exportação JSON inclui proteções contra determinados destinos com reparse points e utiliza ficheiros temporários.

\- Os hashes SHA-256 permitem detetar alterações, mas não autenticam a origem dos manifestos.

\- Alterações simultâneas aos ficheiros podem afetar a consistência das verificações.

\- A deteção de linguagens baseia-se em extensões reconhecidas.

\- A ferramenta não substitui uma auditoria de segurança especializada.



\## Estrutura do projeto



```text

CodeSentinel/

├── include/codesentinel/    Interfaces dos módulos

├── src/                     Implementação e CLI

├── tests/                   Testes automatizados

├── examples/                Exemplos de utilização

├── docs/                    Documentação

├── CMakeLists.txt            Configuração de compilação

└── README.md                Documentação principal

```



\## Desenvolvimento futuro



O projeto poderá evoluir com reconhecimento de mais linguagens, identificação de tecnologias e frameworks, relatórios de análise mais detalhados e interfaces de integração para outras ferramentas.



\*\*CodeSentinel — Universal Project Analysis \& Integrity Toolkit\*\*



