# Changelog

All notable changes to this project will be documented in this file.

## [v0.1.0] - 2026-03-06

> **This is a major architectural release.** v0.1.0 marks the first milestone of the Shuati CLI project, introducing a ground-up TUI redesign, comprehensive CI/CD overhaul, installer refactoring, and deep code cleanup across the entire codebase.

### Highlights

- Brand-new **FTXUI-based TUI** with static main menu, command catalog, auto-complete, and full-screen sub-views.
- **Unified CI/CD pipeline** — merged redundant workflows, fixed critical release bugs, and streamlined three-platform builds.
- **Inno Setup installer refactored** with environment variable overrides, component-based install types, and proper PATH management.
- **Net reduction of ~151 lines** through aggressive dead code removal and architecture consolidation.

### New Features

- **TUI Complete Redesign**: Replaced the previous prototype TUI with a production-ready terminal interface built on FTXUI. The new TUI features a static main menu with an ASCII panda mascot, a scrollable output panel, and a unified command input bar with real-time hint display.
- **Full-Screen Sub-Views**: Three dedicated full-screen views accessible via slash commands:
  - `/config` — Form-based configuration editor for API keys, language, editor preferences.
  - `/list` — Interactive problem browser with table navigation and filtering.
  - `/hint <id>` — Scrollable AI hint display with async loading, PageUp/PageDown/Arrow scroll, and Esc to return.
- **Command Hint System**: Every slash command now displays a real-time usage hint (syntax + description) as the user types. Implemented with an O(1) `unordered_map` lookup table, replacing the previous O(n) if-else chain.
- **Command History & Shortcuts**: Arrow Up/Down to browse command history, Ctrl+L to clear screen, Ctrl+U to clear current input line.
- **Command Auto-Complete**: Tab-triggered auto-complete panel populated from a centralized command catalog (`tui_command_catalog.cpp`).

### Architecture Refactoring

- **New Router Layer** (`src/router/`): Introduced `AppRouter` to cleanly separate CLI and TUI entry points. The main entry (`src/cmd/main.cpp`) now delegates to the router, which decides whether to launch the legacy REPL or the new TUI based on configuration and arguments.
- **Legacy REPL Extraction** (`src/cli/legacy_repl.cpp`): The original replxx-based REPL logic was extracted from `main.cpp` into a dedicated module, preserving backward compatibility while decoupling it from the new TUI path.
- **New TUI Module** (`src/tui/`): A self-contained module with clear separation of concerns:
  - `tui_app.cpp` — Application main loop, component tree, event handling, sub-view routing.
  - `tui_views.cpp` — View rendering (welcome page, config/list/hint sub-views).
  - `tui_command_engine.cpp` — Command parsing, execution, and output capture (including C-level fd redirection).
  - `tui_command_catalog.cpp` — Auto-complete candidate list generation.
  - `command_specs.cpp` — TUI command metadata definitions (slash name, usage, summary).
  - `store/app_state.hpp` — Centralized TUI state management (ViewMode, BufferLine, History, sub-view states).
- **Removed 9 Deprecated Files**: Cleaned up legacy TUI component files that were superseded by the new architecture.

### Bug Fixes

- **Fixed TUI Input Deletion**: Resolved a critical bug where Backspace/Delete keys failed to delete characters in the FTXUI Input component. Root cause was cursor position desynchronization — fixed by explicitly tracking `cursor_position` and syncing it after history recall, auto-complete, and Ctrl+U operations.
- **Fixed /test Output Capture**: The `ScopedStreamCapture` utility now performs C-level file descriptor redirection (`dup2`) in addition to C++ stream redirection. This ensures output from `fmt::print` and other functions that bypass `std::cout`/`std::cerr` is correctly captured in the TUI output panel.
- **Fixed Carriage Return Handling**: `normalize_text` now correctly simulates terminal `\r` (carriage return) behavior, preventing progress bar output from duplicating lines in the TUI scrollback buffer.
- **Fixed Version Display Double-v Bug**: Resolved an issue where the version string was displayed as `vv0.x.x` in certain TUI contexts.

### CI/CD Overhaul

- **Unified CI Pipeline**: Merged the redundant `crawler_ci.yml` into the main `ci.yml`. All crawler tests now run as part of the standard build matrix alongside other unit tests.
- **Fixed Release Notes Generation**: Fixed a critical bug in `release.yml` where the release notes extraction used an undefined `VER` variable (`${VER#v}`) instead of the correct `${GITHUB_REF_NAME#v}`, causing empty release notes on every tag push.
- **Streamlined Build Matrix**: Removed the `build_type` matrix dimension from CI (hardcoded to `Release`), eliminating redundant debug builds in the CI pipeline. Removed unnecessary `fetch-depth: 0` from CI checkout.
- **Added Smoke Tests to Release**: The release workflow now includes a smoke test step that runs `shuati --help` and `shuati init` after build, verifying the binary is functional before packaging.
- **Simplified Checksum Generation**: Replaced platform-specific PowerShell/Bash checksum scripts with a single cross-platform Python one-liner.
- **Corrected CI Permissions**: Changed `ci.yml` permissions from `contents: write` to `contents: read` (CI should never need write access to repository contents).
- **Removed Dead Code**: Removed the optional webhook notification step, verbose file listing steps, and redundant installer verification logic from `release.yml`.

### Installer Refactoring

- **Environment Variable Overrides**: The Inno Setup script (`shuati.iss`) now supports `SHUATI_VERSION`, `SHUATI_OUTPUT_NAME`, and `SHUATI_SOURCE_DIR` environment variables, enabling CI to inject build parameters without modifying the script.
- **Component-Based Installation**: Users can now choose between Full, Compact, and Custom installation types. Components include: Main application (required), Resource files (compiler rules, templates), and Documentation.
- **Proper PATH Management**: The `[Code]` section implements robust Pascal script for adding/removing the install directory from the user PATH, with `WM_SETTINGCHANGE` broadcast to notify other processes of the environment change.
- **Multi-Language Support**: Added English and Simplified Chinese language support for the installer wizard.
- **Upgrade Detection**: `InitializeSetup` logs the previous version when upgrading, enabling future migration logic.
- **Modern Wizard**: Uses `WizardStyle=modern` with 120% size and resizable window.

### Code Quality

- **Deep Code Cleanup**: Merged three separate Windows console configuration lambdas into a single unified `apply` function. Removed unused `awaiting_confirm`/`on_confirm` fields from `AppState`.
- **Optimized Input Hint Lookup**: Replaced the linear O(n) if-else chain in `get_input_hint` with an O(1) `unordered_map` lookup table.
- **Install Script Hardened**: The PowerShell `install.ps1` script now performs SHA-256 verification of downloaded archives, normalizes PATH entries before comparison, and broadcasts `WM_SETTINGCHANGE` after PATH updates.

### Breaking Changes

- None. This release is fully backward compatible with v0.0.x configurations and data.

---

## [v0.0.7] - 2026-03-05

### 新增功能 (New Features)
- **SM2 记忆逻辑增强**：支持记录题目标签 (tags) 和示例 ID (example_id)，使错题分析更精准。
- **Companion Server 端口自动轮询**：支持 3000-3005 端口自动尝试，优化了多实例并发运行时的端口冲突处理。
- **编译诊断规则升级**：新增 GCC 15/C++23 专用诊断规则，支持识别现代 C++ 编译器的新型报错。

### 修复问题 (Bug Fixes)
- 优化了 `solve` 命令的交互式选题 UI，改善了长列表显示的流畅度。
- 改进了解题文件的自动检测逻辑，精准识别旧版 `solution_*.cpp` 与新版 `{序号}_{标题}.cpp`。
- 修复了 Companion Server 在某些环境下跨域资源共享 (CORS) 的预检请求处理。

### 破坏性变更 (Breaking Changes)
- 无。

---

## [v0.0.6] - 2026-03-04

### 新增功能 (New Features)
- **AI 协议标准化 (XML Protocol)**：AI 教练响应解析从 HTML 注释正则升级为结构化 XML 流式状态机（`xml_parser.hpp`）。新增 `<cot>`（思考过程）、`<hint>`（提示）、`<memory_op>`（记忆操作）三种语义标签，支持跨网络分包的安全切割。
- **SM2 自动评分**：`submit` 命令根据判题结果与执行时间自动计算 0–5 分评分（AC 快速→5，WA→2，TLE→1，RE/CE/MLE→0）。用户可按回车接受或手动覆盖。
- **解题文件智能命名**：新建解题文件从无意义的 `solution_lq_3514.cpp` 升级为 `{序号}_{题目标题}.cpp`（如 `10_LQ3512_接龙数列.cpp`）。完全兼容旧格式，无需迁移。

### 修复问题 (Bug Fixes)
- 修复 `shuati config --language` 错误绑定到 `--difficulty` 变量的严重 Bug。
- 删除未使用的 `trim_right()`、`stream_file_diff()` 及大量开发遗留注释。

### 安全修复 (Security Fixes)
修复 Windows 中文环境下的一系列路径编码问题（根因：标准库在 Windows 隐式按系统代码页 CP936 处理字符串，导致含中文的路径乱码）：

| 问题 | 根因 | 修复 |
|---|---|---|
| 编译报错 `g++: no input files` | `fs::path::string()` 按 CP936 编码后传给 `_wsystem()` | 新增 `path_to_utf8()` + `utf8_system()`，统一经由宽字符 API |
| 沙箱无法拉起含中文路径的可执行文件 | 底层使用 `CreateProcessA`/`CreateFileA` | 升级为 `CreateProcessW`/`CreateFileW` |
| 正确代码误判 WA | 临时文件路径含中文（如 `C:\Users\张三\Temp`），`ifstream` 打开失败，输出恒为空 | 全链路改用 `utf8_path()` 构造路径 |
| 测试报告/AI 诊断崩溃 | GBK 编码的代码注释或编译器报错触发 `json::type_error.316` | 序列化出口统一使用 `error_handler_t::replace` 和 `ensure_utf8_lossy` |
| 潜在命令注入 | 编译路径校验为 ASCII 白名单，拦截了合法中文文件名 | 改为 Shell 元字符黑名单（拦截 `&\|;><$\n\r`） |
| 空指针解引用 | `getenv("APPDATA")` 可能返回 `nullptr` | 增加 null 检查，回退至临时目录 |

### 破坏性变更 (Breaking Changes)
- 新建解题文件采用 `{序号}_{题目标题}.ext` 格式。**已有的 `solution_*.cpp/py` 文件不受影响**，系统自动识别两种格式。

### 升级指引 (Upgrade Guide)
1. 从 [Releases](https://github.com/Xustalis/shuati-Cli/releases) 下载最新版本覆盖安装。
2. 已有的 `solution_*.cpp` 文件会继续被 `test`、`hint`、`solve` 命令自动识别，无需重命名。
3. 若需统一文件命名，可删除旧文件后重新执行 `shuati solve <id>` 生成新格式文件。
4. 建议运行 `shuati config --language cpp` 验证 language 配置是否正确。


---

## [v0.0.5] - 2026-03-04

### 新增功能 (New Features)
- **编辑器智能探测与自动配置**：在 `shuati init` 时增加跨平台编辑器自动搜索机制。优先从环境变量 `$VISUAL` 或 `$EDITOR` 获取，若未定义则依次尝试系统 `PATH` 中的 `nvim`, `vim`, `vi`, `nano`, `emacs`, `micro`, `code`，提升 Linux/macOS 用户的初次使用体验。
- **REPL 交互模式增强**：
  - 新增 `autostart_repl` 配置项（默认开启），支持通过 `shuati config --autostart-repl off` 关闭无参数运行时的自动启动。
  - 新增显式启动命令 `shuati repl`，方便脚本用户手动进入。
- **配置管理升级**：`config --editor` 现在支持 `auto` 参数进行重新扫描，也支持直接设置任意命令。

### 修复问题 (Bug Fixes)
- **数据库稳定性瓶颈修复**：为 SQLite 引入了 WAL (Write-Ahead Logging) 模式及 `PRAGMA synchronous=NORMAL` 优化，彻底解决了 Companion Server 指令流并发写入时偶发的 `database is locked` 错误。
- **配置保存机制重构**：重写了配置文件合并写入逻辑。现在使用 `Load-Merge-Save` 模式，确保在 CLI 修改配置时，不会由于 JSON 结构覆盖而导致 `version` 等非结构化元数据丢失。
- **CI/CD 工作流加固**：
  - 修复了 GitHub Actions 环境下 vcpkg 二进制缓存的读写权限问题。
  - 优化了代码静态检查 (`cppcheck`) 与代码风格检查 (`clang-format`) 的判定逻辑，降低了 CI 误报率。
  - 统一了 CMake 版本要求至 3.20+ 以适配更多构建环境。

### 破坏性变更 (Breaking Changes)
- 无。本次更新向后兼容。

### 升级指引 (Upgrade Guide)
- **Windows**: 从 [Releases](https://github.com/Xustalis/shuati-Cli/releases) 页面下载最新的安装包。
- **Linux/macOS**: 重新执行编译构建流程，或使用 CPack 生成的 `.deb` / `.tar.gz` 包进行覆盖。
- **配置迁移**: 建议运行一次 `shuati config --editor auto` 以确保持续使用最优的编辑器配置。


---

## [v0.0.4] - 2026-03-02

### 新增特性与安全升级
* **更新跨平台硬沙箱引擎 (ISandbox)**
  * **Windows**: 底层切换为原生的**作业对象 (Job Objects)** 容器机制。提供内存上限拦截 (防止 `std::bad_alloc` 绕过) 及内核层面的 CPU 超时强杀，抵御死锁攻击及 Fork 炸弹。
  * **Linux**: 引入 **Bubblewrap** (`bwrap`) 沙盒隔离，使用 `--unshare-net` 阻断网络，只读挂载文件系统 `/usr`、`/lib`，从根本上防止 `System()` 读取与系统破坏漏洞。若缺失 `bwrap` 则自动无缝降级为 `setrlimit` + `setpgid` 保障基础运行限制。
* **彻底根除系统命令注入 (RCE)**: 摒弃了早期依赖于 `std::system` 和 `cmd.exe /C` 的脆弱命令中转池。统一使用底层受限且参数分离的 API (`CreateProcessA`, `execvp`)，即使在输入框内填入恶意终止符如 `&`、`;` 也会被完全视作普通参数过滤。
* **内存限额探测 (MLE 判定优化)**: 解决标准 C++ 容器由于 OOM 抛出提前 abort() 导致判定为 Runtime Error 的情况。当触发崩溃时，探查峰值内存若达到限制值的 90% (或临差 5MB 左右) 均会智能裁决为 `MLE`，以输出准确提示信息。

### 下载安装
- **Windows**: [shuati-setup-x64-v0.0.4.exe](https://github.com/Xustalis/shuati-Cli/releases/download/v0.0.4/shuati-setup-x64-v0.0.4.exe)
- **Linux (deb)**: [shuati-cli_0.0.4_amd64.deb](https://github.com/Xustalis/shuati-Cli/releases/download/v0.0.4/shuati-cli_0.0.4_amd64.deb)

---

## [v0.0.3] - 2026-03-02

### 新增功能 (New Features)
- **蓝桥云课交互式登录 (`shuati login`)**：新增了 `login` CLI 命令，用于直接在终端内交互式配置 Cookie。爬虫在拉取数据时会自动携带 Auth 凭据，并在令牌过期或未配置时提供极其友善的错误回退诊断与排查指引 (支持高达 9 种数据字段名的解析与跨层提取，解决蓝桥云课新旧 API 不兼容问题，确保成功抓取)。
- **多平台安装包 (DEB / Tarball / Zip / EXE)**：CI/CD 现已不仅支持 Windows，新增了 Linux 的 `cpack -G DEB` 全自动打 Debian 安装包和 `tar.gz` 封包机制，全面兼容跨平台发行。由于 FHS 文件结构限制，程序已内建动态资源寻址引擎 (`resource_path.hpp`)。

### 修复问题 (Bug Fixes)
- **Linux 沙箱系统级稳定性重构**：修复了在 Linux / macOS / WSL2 下，原生进程沙箱通过 `kill` 命令导致在子进程 (TLE / MLE) 时引发的孤儿僵尸进程问题，全面改为了 `setpgid` (0,0) 的进程组硬隔离管控并以 `killpg` 兜底；此外将 `/proc` 的资源状态轮询间隔从高能耗的 10ms 调整为了 50ms。
- **AI 解析标签泄漏修复**：彻底废弃了原先脆弱的 20 字符固定环形缓冲识别方案，引入了新的状态机标签剔除引擎 (`StreamFilter`) 解析底层 LLM 推理系统的 `<system_op>` 隐藏指令，防止界面 UI 控制台污染。
- **配置文件防崩塌机制**：修复了通过 CLI 临时交互式配置某些零散项时，导致原 `config.json` 的不可见 / 元定义元数据（如 `version`）遭到意外覆盖和丢失的关键 Bug，统一使用了原样载入加软合并写入 (Merge Save)。
- **资源相对定位修复**：修复了由于 `__FILE__` 编译期硬编码导致的生产环境、安装环境 (如 Linux 下的 `/usr/local/share`) 下无法寻找 `CompilerDoctor` 分析诊断规则引擎元数据的重大缺陷。
- **CI 工作流修复与代码静态分析**：集成了 `cppcheck` C/C++ 静态代码分析检测，并在构建脚本中引入了防御性版本锁定策略 (Python Tag Validation)，强制要求 Github Release Tag、代码树内部版本与依赖清单配置严格三字头统一才能继续打包，最大程度规避了暗坑残余。

### 破坏性变更 (Breaking Changes)
- 无

### 升级指引 (Upgrade Guide)
- 请直接在客户端内通过 `update` 命令进行获取，或从本 Release 下载最新安装程序 (`shuati-setup-x64-v0.0.3.exe` 或 Linux `v0.0.3.deb`) 进行覆盖安装。

## [0.0.2] - 2026-03-01

### 新增功能 (New Features)
- **爬虫架构全面升级**：重构了全平台爬虫（Codeforces, LeetCode, 洛谷, 蓝桥云课），现已支持提取完整的题目结构化元数据（包含完整描述文本、测试用例、题目标签及难度等）。
- **LeetCode (GraphQL)**：全面废弃正则/HTML匹配方案，采用官方 GraphQL API 获取精准详细完整的题目内容，全面兼容 `leetcode.com` 与 `leetcode.cn`。
- **洛谷 (Luogu)**：深度适配洛谷最新的 `lentille-context` 内置 JSON 数据源，解析更稳定精准。
- **蓝桥云课 (Lanqiao)**：增加了 API 数据抓取机制，并针对需用户登录才能访问的题目增加了友好的优雅降级与指引提示。
- **Codeforces**：深度集成 Codeforces 官方 API，精准提取题目难度 (Rating) 及专题标签 (Tags)。

### 修复问题 (Bug Fixes)
- 修复了 Luogu 爬虫原有的 `HTTP 0` 网络异常错误判定漏洞，增强了系统底层 `CprHttpClient` 对于网络/代理错误的详细捕获及提示机制。
- 修正了 Codeforces 链接解析的正则表达式，解决了原本 ID 与题目简码提取错误、名称变异的问题。
- 修复了 Git 仓库的跟踪遗留问题，删除了旧有的构建残余、测试脚本、IDE 配置日志等，并深度优化了 `.gitignore`。
- 修复并优化了 GitHub Actions 的 `crawler_ci.yml`，统一使用更稳定高效的 `lukka/run-vcpkg` 缓存机制，解决了 Windows PowerShell 跨平台编译构建 CMAKE 参数解析出错的问题。
- 全面重写并修复了相关适配器的 Mock 拦截单元测试，适配最新爬虫流程。

### 破坏性变更 (Breaking Changes)
- 无

### 升级指引 (Upgrade Guide)
- 请直接在客户端内通过 `update` 命令进行获取，或从本 Release 下载最新安装程序 (`shuati-setup-x64-v0.0.2.exe`) 进行覆盖安装。

## [0.0.1] - 2026-02-26

### 新增

- **引导程序**：初始项目设置，统一版本为 v0.0.1。
- **构建**：修复了 CMake 配置和构建问题。
- **持续集成**：修复了 GitHub Actions CI/CD 工作流程。
- **核心功能**：
- 多平台爬虫：LeetCode (GraphQL)、Codeforces、罗谷、蓝桥
- 本地评判引擎：C++ 和 Python 的沙箱执行
- AI 教练：集成 LLM 用于错误诊断
- 记忆系统：SM2 算法用于间隔重复训练
- 带自动补全功能的交互式 REPL 模式
