# Changelog

All notable changes to this project will be documented in this file.

## [v0.0.6] - 2026-03-04

### 新增功能 (New Features)
- **AI 协议标准化 (XML Protocol)**：AI 教练响应从脆弱的 HTML 注释解析升级为结构化 XML 流式协议。新增 `<cot>` (思考过程/隐藏)、`<hint>` (用户可见提示)、`<memory_op>` (记忆操作) 三种语义标签，支持跨 chunk 安全分裂的状态机解析引擎 (`xml_parser.hpp`)。
- **SM2 自动评分**：`submit` 命令新增自动评分功能。系统会读取最近的测试报告，根据判题结果 (Verdict) 和执行时间自动计算 0-5 分的掌握程度评分：
  - AC 快速 (≤30% 时限) → 5 分
  - AC 中等 (≤70% 时限) → 4 分
  - AC 较慢 (>70% 时限) → 3 分
  - WA → 2 分 / TLE → 1 分 / RE/CE/MLE → 0 分
  - 用户可按回车接受自动评分，也可手动输入覆盖。
- **解题文件智能命名**：解决方案文件从不可读的 `solution_lq_3514.cpp` 格式升级为 `{序号}_{题目标题}.cpp`（如 `3_两数之和.cpp`），大幅提升多题目管理的可辨识度。完全向后兼容旧命名格式的文件发现。

### 修复问题 (Bug Fixes)
- **`--language` 配置冲突**：修复了 `shuati config --language cpp` 命令中 `--language` 选项错误绑定到 `--difficulty` 同一变量导致互相覆盖的严重 Bug。
- **变量遮蔽**：修复 `test_command.cpp` 中局部变量 `fs` 遮蔽 `std::filesystem` 命名空间别名的问题。
- **冗余代码清理**：删除了 3 处重复 `#include`、1 个未实现的 `get_hint()` 死代码声明、1 个未使用的 `trim_right()` 函数、`stream_file_diff()` 未使用实现及声明、20+ 行开发遗留注释。
### 安全与体验修复 (Security & UX Fixes)
- **空指针防护**：修复 `boot_guard.cpp` 中 `getenv("APPDATA")` 返回 `nullptr` 时的空指针解引用风险，增加 null 检查并回退至临时目录。
- **中文路径防注入与白名单优化**：修复 `judge` 编译沙箱的安全检查 (`source_file.find_first_not_of`) 由于严格限制全英文字符导致新版中文题解文件被判定为非法文件（`Invalid source file path`）的错误。将基于 ASCII 的白名单判定重构为更加安全的 Shell 元字符黑名单（拦截 `&|;><$\n\r`），在防御命令注入的同时完美支持带有 UTF-8 字符的路径操作。

### 破坏性变更 (Breaking Changes)
- **解题文件命名格式变更**：新创建的解题文件将使用 `{序号}_{题目标题}.ext` 格式。**已有的 `solution_*.cpp/py` 文件不受影响**，系统会自动识别新旧两种命名格式。无需手动迁移。

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
