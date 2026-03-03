# Changelog

All notable changes to this project will be documented in this file.

## [v0.0.5] - 2026-03-03

### 新增特性 (New Features)
* **Linux 编辑器自动检测 (`shuati init`)**：在 Linux/macOS 下初始化项目时，不再硬编码 `code` 为编辑器，而是自动探测 `$VISUAL` / `$EDITOR` 环境变量，或依次尝试路径中的 `nvim` → `vim` → `vi` → `nano` → `emacs`，确保 vim/nano 用户无需额外配置即可开箱即用。
* **REPL 自动启动控制 (`autostart_repl`)**：在 `config.json` 中新增 `autostart_repl` 开关（默认 `true`）。通过 `shuati config --autostart-repl off` 可关闭无参数时自动进入 REPL 的行为，适合 Linux 脚本用户和 CI 场景；通过 `shuati repl` 可随时手动进入交互模式。
* **`repl` 子命令**：新增 `shuati repl` 作为显式进入 REPL 的子命令，无论 `autostart_repl` 为何值都可强制打开。
* **编辑器配置增强 (`config --editor`)**：`shuati config --editor auto` 自动重新检测并保存最优编辑器；`shuati config --editor vim` 可指定任意编辑器命令，支持 vim/nvim/nano/emacs/code/micro 等所有主流编辑器。

### 修复问题 (Bug Fixes)
* **数据库并发死锁修复**：在 SQLite 数据库初始化阶段加入 `PRAGMA journal_mode=WAL;` 和 `PRAGMA synchronous=NORMAL;`，解决了 Companion Server 监听线程并发写入时偶现的 `database is locked` 错误。
* **配置保存重构**：`cmd_config` 现在通过 `Config::load()` + `Config::save()` 合并保存，不会意外覆盖 `version` 等元数据字段。

### 下载安装
- 待 CI 构建完成后从 Releases 页面下载。

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
