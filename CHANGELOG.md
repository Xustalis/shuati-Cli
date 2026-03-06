# Changelog

All notable changes to this project will be documented in this file.

## [v0.1.0] - 2026-03-06 🎉 首个里程碑版本

> **里程碑版本。** v0.1.0 标志着 Shuati CLI 从纯命令行工具正式迈入**终端图形界面时代**。本版本首次引入基于 FTXUI 的 TUI 设计界面，实现了沉浸式的全屏交互体验；同时对 CI/CD 管线、Windows 安装程序和整体代码架构进行了全面重构与深度清理。

### 核心亮点

- **🖥️ 首次引入 TUI 终端界面** — 基于 [FTXUI](https://github.com/ArthurSonzogni/FTXUI) 从零构建的生产级终端 UI，支持静态主菜单、命令自动补全、实时输入提示和三个全屏子视图（配置编辑器 / 题目浏览器 / AI 提示页），这是项目架构层面最大的一次跨越。
- **🔀 路由层架构引入** — 新增 `AppRouter` 路由层，实现 CLI（传统 REPL）与 TUI 双入口解耦，为后续多模式扩展奠定基础。
- **⚙️ CI/CD 管线统一** — 合并冗余工作流、修复关键发布 Bug、精简三平台构建矩阵。
- **📦 Inno Setup 安装程序重构** — 支持环境变量注入、组件化安装和规范 PATH 管理。
- **🧹 净减约 151 行代码** — 通过激进的死代码移除、废弃文件清理和架构整合完成。

### 新增功能

#### TUI 终端界面

- **全新 TUI 主界面**：以基于 FTXUI 的生产级终端界面替换了此前的原型 TUI。主界面包含带 ASCII 熊猫吉祥物的欢迎页、可滚动输出面板以及带实时提示的统一命令输入栏。运行 `shuati tui` 即可进入。
- **三个全屏子视图**，通过斜杠命令进入：
  - `/config` — 表单式配置编辑器，可设置 API Key、语言、编辑器偏好等。
  - `/list` — 交互式题目浏览器，支持表格导航和 `f` 键切换筛选。
  - `/hint <id>` — 可滚动的 AI 提示展示，支持异步加载、PageUp/PageDown/方向键滚动、Esc 返回。
- **命令实时提示**：所有斜杠命令在用户输入时实时显示用法提示（语法 + 描述）。采用 O(1) `unordered_map` 查找表替代了原有的 O(n) if-else 链。
- **命令历史与快捷键**：`↑↓` 浏览命令历史、`Ctrl+L` 清屏、`Ctrl+U` 清除当前输入行。
- **Tab 自动补全**：Tab 键触发自动补全面板，候选项来自集中式命令目录 (`tui_command_catalog.cpp`)。

### 架构重构

- **新增路由层** (`src/router/`)：引入 `AppRouter`，将 CLI 和 TUI 入口进行清晰解耦。程序主入口 (`src/cmd/main.cpp`) 现委托路由层根据配置和参数决定启动传统 REPL 还是新 TUI。
- **传统 REPL 抽取** (`src/cli/legacy_repl.cpp`)：将原有基于 replxx 的 REPL 逻辑从 `main.cpp` 抽取为独立模块，保持向后兼容的同时与新 TUI 路径解耦。
- **新增 TUI 模块** (`src/tui/`)：自包含模块，职责分离清晰：
  - `tui_app.cpp` — 应用主循环、组件树、事件处理、子视图路由。
  - `tui_views.cpp` — 视图渲染（欢迎页、Config/List/Hint 子视图）。
  - `tui_command_engine.cpp` — 命令解析、执行、输出捕获（含 C 层 fd 重定向）。
  - `tui_command_catalog.cpp` — 自动补全候选列表生成。
  - `command_specs.cpp` — TUI 命令元数据定义（斜杠名称、用法、摘要）。
  - `store/app_state.hpp` — 集中式 TUI 状态管理（ViewMode、BufferLine、History、子视图状态）。
- **移除 9 个废弃文件**：清理了被新架构取代的旧 TUI 组件文件。

### 问题修复

- **修复 TUI 输入删除失效**：解决了 FTXUI Input 组件中 Backspace/Delete 键无法删除字符的严重 Bug。根因是光标位置失步 — 通过显式追踪 `cursor_position` 并在历史回溯、自动补全和 Ctrl+U 操作后同步光标位置来修复。
- **修复 /test 输出捕获**：`ScopedStreamCapture` 工具类现在除 C++ 流重定向外还执行 C 层文件描述符重定向 (`dup2`)，确保 `fmt::print` 等绕过 `std::cout`/`std::cerr` 的函数输出能正确显示在 TUI 输出面板中。
- **修复回车符处理**：`normalize_text` 现在正确模拟终端 `\r`（回车）行为，防止进度条输出在 TUI 滚动缓冲区中产生重复行。
- **修复版本号双 v Bug**：解决了在某些 TUI 上下文中版本字符串显示为 `vv0.x.x` 的问题。

### CI/CD 全面检修

- **统一 CI 管线**：将冗余的 `crawler_ci.yml` 合并到主 `ci.yml` 中。所有爬虫测试现在与其他单元测试一起在标准构建矩阵中运行。
- **修复发布说明生成**：修复了 `release.yml` 中发布说明提取使用未定义变量 `VER`（`${VER#v}`）而非正确的 `${GITHUB_REF_NAME#v}` 导致每次标签推送时发布说明为空的严重 Bug。
- **精简构建矩阵**：从 CI 中移除 `build_type` 矩阵维度（硬编码为 `Release`），消除冗余的 debug 构建。移除不必要的 `fetch-depth: 0`。
- **发布添加冒烟测试**：发布工作流现包含冒烟测试步骤，在打包前运行 `shuati --help` 和 `shuati init` 验证二进制可用性。
- **简化校验和生成**：用单行跨平台 Python 脚本替换了平台特定的 PowerShell/Bash 校验和脚本。
- **纠正 CI 权限**：将 `ci.yml` 权限从 `contents: write` 改为 `contents: read`（CI 不应需要仓库内容写权限）。
- **移除死代码**：从 `release.yml` 中移除可选的 Webhook 通知步骤、冗长的文件列表步骤以及冗余的安装程序验证逻辑。

### 安装程序重构

- **环境变量覆盖**：Inno Setup 脚本 (`shuati.iss`) 现支持 `SHUATI_VERSION`、`SHUATI_OUTPUT_NAME` 和 `SHUATI_SOURCE_DIR` 环境变量，使 CI 无需修改脚本即可注入构建参数。
- **组件化安装**：用户现可选择完整安装、精简安装和自定义安装。组件包括：主程序（必选）、资源文件（编译器规则、模板）和文档。
- **规范 PATH 管理**：`[Code]` 段实现了健壮的 Pascal 脚本，用于在用户 PATH 中添加/移除安装目录，并广播 `WM_SETTINGCHANGE` 通知其他进程环境变量变更。
- **多语言支持**：为安装向导新增英文和简体中文语言支持。
- **升级检测**：`InitializeSetup` 在升级时记录前一版本号，为后续迁移逻辑预留接口。
- **现代向导**：使用 `WizardStyle=modern`，120% 尺寸且窗口可调整大小。

### 代码质量

- **深度代码清理**：将三个独立的 Windows 控制台配置 lambda 合并为一个统一的 `apply` 函数。移除 `AppState` 中未使用的 `awaiting_confirm`/`on_confirm` 字段。
- **优化输入提示查找**：用 O(1) `unordered_map` 查找表替换了 `get_input_hint` 中的线性 O(n) if-else 链。
- **安装脚本加固**：PowerShell `install.ps1` 脚本现对下载的压缩包执行 SHA-256 校验，在比较前规范化 PATH 条目，并在 PATH 更新后广播 `WM_SETTINGCHANGE`。

### 破坏性变更

- 无。本版本与 v0.0.x 的配置和数据完全向后兼容。

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

---

## [v0.0.2] - 2026-03-01

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

---

## [v0.0.1] - 2026-02-26

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
