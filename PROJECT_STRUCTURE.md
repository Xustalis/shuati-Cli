# Shuati CLI 项目结构说明

> 更新日期: 2026-03-06
> 项目版本: v0.1.0
> 架构目标: 高内聚、低耦合、跨平台、易扩展

## 目录结构概览

```
shuati-Cli/
├── .github/                 # GitHub 配置 (CI/CD, Issue/PR 模板)
├── include/                 # 公共头文件
├── installer/               # 安装程序配置
├── resources/               # 资源文件 (编译器错误规则等)
├── src/
│   ├── cmd/                 # CLI 命令层 (main, 各子命令)
│   ├── core/                # 核心业务逻辑 (判题、版本、记忆等)
│   │   └── sandbox/         # 沙箱执行 (Windows/Linux)
│   ├── adapters/            # 外部适配器 (爬虫、AI 教练、Companion)
│   ├── infra/               # 基础设施 (数据库、日志、HTTP)
│   ├── utils/               # 工具函数 (编码转换)
│   ├── tui/                 # TUI 终端界面 (FTXUI)
│   │   └── store/           # TUI 状态管理
│   ├── cli/                 # CLI 交互 (Legacy REPL)
│   ├── router/              # 路由层 (CLI/TUI 入口分发)
│   └── tests/               # 单元测试
├── tools/                   # 工具脚本 (发布流程)
├── scripts/                 # CI 辅助脚本
├── CMakeLists.txt           # CMake 构建配置
├── vcpkg.json               # vcpkg 依赖配置
├── README.md                # 项目说明
├── CHANGELOG.md             # 变更日志
├── CONTRIBUTING.md          # 贡献指南
├── install.ps1              # Windows 安装脚本
└── install.sh               # Linux/macOS 安装脚本
```

---

## 源码文件 (.cpp)

### src/cmd/ - 命令行接口层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/cmd/main.cpp](src/cmd/main.cpp) | 程序入口，REPL 主循环，命令解析 | commands, version, logger, boot_guard, encoding |
| [src/cmd/commands.cpp](src/cmd/commands.cpp) | CLI 命令注册与绑定 | version, CLI11 |
| [src/cmd/commands.hpp](src/cmd/commands.hpp) | 命令上下文与函数声明头文件 | CLI11, version, database, problem_manager, judge, ai_coach |
| [src/cmd/basic_commands.cpp](src/cmd/basic_commands.cpp) | 基础命令实现 (init, info, config) | version, database, config |
| [src/cmd/manage_commands.cpp](src/cmd/manage_commands.cpp) | 题目管理命令 (pull, new, delete, submit) | problem_manager, crawler, database |
| [src/cmd/solve_command.cpp](src/cmd/solve_command.cpp) | 解题命令实现 (solve, hint) | problem_manager, judge, ai_coach |
| [src/cmd/list_command.cpp](src/cmd/list_command.cpp) | 列表命令实现 | problem_manager, database |
| [src/cmd/view_command.cpp](src/cmd/view_command.cpp) | 查看测试详情命令 | problem_manager, judge |
| [src/cmd/test_command.cpp](src/cmd/test_command.cpp) | 测试命令实现 | problem_manager, judge, ai_coach |
| [src/cmd/services.cpp](src/cmd/services.cpp) | 服务层封装，整合各模块 | problem_manager, judge, ai_coach, crawler, companion_server |

### src/core/ - 核心业务逻辑层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/core/version.cpp](src/core/version.cpp) | 版本号解析与比较 | fmt |
| [src/core/problem_manager.cpp](src/core/problem_manager.cpp) | 题目管理器，CRUD 操作 | database, crawler |
| [src/core/judge.cpp](src/core/judge.cpp) | 本地判题引擎，沙箱执行 | database, logger, fmt, Threads |
| [src/core/compiler_doctor.cpp](src/core/compiler_doctor.cpp) | 编译器诊断工具 | fmt, nlohmann_json |
| [src/core/boot_guard.cpp](src/core/boot_guard.cpp) | 启动检查与历史记录 | fmt, filesystem |
| [src/core/memory_manager.cpp](src/core/memory_manager.cpp) | 记忆曲线管理 (SM2 算法) | database |
| [src/core/mistake_analyzer.cpp](src/core/mistake_analyzer.cpp) | 错误分析器 | database |
| [src/core/sm2_algorithm.cpp](src/core/sm2_algorithm.cpp) | SM2 间隔重复算法实现 | - |

### src/core/sandbox/ - 沙箱执行层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/core/sandbox/sandbox_windows.cpp](src/core/sandbox/sandbox_windows.cpp) | Windows 沙箱 (Job Object 隔离) | Win32 API |
| [src/core/sandbox/sandbox_linux.cpp](src/core/sandbox/sandbox_linux.cpp) | Linux 沙箱 (seccomp/rlimit 隔离) | POSIX |

### src/infra/ - 基础设施层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/infra/database.cpp](src/infra/database.cpp) | SQLite 数据库封装 | SQLiteCpp, nlohmann_json |
| [src/infra/logger.cpp](src/infra/logger.cpp) | 日志系统 | fmt, filesystem |
| [src/infra/http_client.cpp](src/infra/http_client.cpp) | HTTP 客户端封装 | cpr |

### src/adapters/ - 适配器层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/adapters/leetcode_crawler.cpp](src/adapters/leetcode_crawler.cpp) | LeetCode 爬虫 | cpr, nlohmann_json |
| [src/adapters/codeforces_crawler.cpp](src/adapters/codeforces_crawler.cpp) | Codeforces 爬虫 | cpr, nlohmann_json |
| [src/adapters/luogu_crawler.cpp](src/adapters/luogu_crawler.cpp) | 洛谷爬虫 | cpr, nlohmann_json |
| [src/adapters/lanqiao_crawler.cpp](src/adapters/lanqiao_crawler.cpp) | 蓝桥杯爬虫 | cpr, nlohmann_json |
| [src/adapters/companion_server.cpp](src/adapters/companion_server.cpp) | Companion Server (浏览器插件通信) | httplib, nlohmann_json |
| [src/adapters/ai_coach.cpp](src/adapters/ai_coach.cpp) | AI 教练 (LLM 接口) | cpr, nlohmann_json, database |

### src/utils/ - 工具层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/utils/encoding.cpp](src/utils/encoding.cpp) | 编码转换工具 (UTF-8/GBK) | - |

### src/tui/ - TUI 终端界面层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/tui/tui_app.cpp](src/tui/tui_app.cpp) | TUI 应用主循环，组件树，事件处理，子视图路由 | ftxui, config, database, version |
| [src/tui/tui_views.cpp](src/tui/tui_views.cpp) | 视图渲染 (欢迎页/Config/List/Hint 子视图) | ftxui |
| [src/tui/tui_command_engine.cpp](src/tui/tui_command_engine.cpp) | 命令解析、执行、输出捕获 (含 C 级 fd 重定向) | CLI11 |
| [src/tui/tui_command_catalog.cpp](src/tui/tui_command_catalog.cpp) | 命令自动补全候选列表 | - |
| [src/tui/command_specs.cpp](src/tui/command_specs.cpp) | TUI 命令元数据定义 (slash/usage/summary) | - |
| [src/tui/store/app_state.hpp](src/tui/store/app_state.hpp) | TUI 状态 (ViewMode, BufferLine, Config/List/HintState, History) | - |
| [src/tui/tui_command_engine.hpp](src/tui/tui_command_engine.hpp) | 命令引擎接口 | - |
| [src/tui/command_specs.hpp](src/tui/command_specs.hpp) | CommandSpec 结构定义 | - |

### src/cli/ - CLI 交互层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/cli/legacy_repl.cpp](src/cli/legacy_repl.cpp) | Legacy REPL 交互模式 | replxx |
| [src/cli/legacy_repl.hpp](src/cli/legacy_repl.hpp) | Legacy REPL 接口 | - |

### src/router/ - 路由层

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/router/app_router.cpp](src/router/app_router.cpp) | CLI/TUI 入口路由分发 | config |
| [src/router/app_router.hpp](src/router/app_router.hpp) | 路由接口 | - |

### src/tests/ - 测试文件

| 文件路径 | 功能说明 | 依赖模块 |
|---------|---------|---------|
| [src/tests/test_version_logic.cpp](src/tests/test_version_logic.cpp) | 版本逻辑测试 | version, fmt |
| [src/tests/test_judge_complex.cpp](src/tests/test_judge_complex.cpp) | 判题引擎复杂测试 | judge, fmt, SQLiteCpp |
| [src/tests/test_judge_security.cpp](src/tests/test_judge_security.cpp) | 判题引擎安全测试 | judge, fmt |
| [src/tests/test_memory.cpp](src/tests/test_memory.cpp) | 记忆管理测试 | memory_manager, database |
| [src/tests/test_sm2_auto_quality.cpp](src/tests/test_sm2_auto_quality.cpp) | SM2 算法质量测试 | sm2_algorithm |
| [src/tests/test_crawlers.cpp](src/tests/test_crawlers.cpp) | 爬虫测试 | crawlers, mock_http_client |
| [src/tests/test_tui_render.cpp](src/tests/test_tui_render.cpp) | TUI 渲染烟雾测试 | tui_views |
| [src/tests/test_tui_performance.cpp](src/tests/test_tui_performance.cpp) | TUI 渲染性能基准 | tui_views |
| [src/tests/test_tui_cli_parity.cpp](src/tests/test_tui_cli_parity.cpp) | TUI/CLI 命令一致性测试 | tui_command_catalog |
| [src/tests/test_tui_command_specs.cpp](src/tests/test_tui_command_specs.cpp) | TUI 命令元数据测试 | command_specs |
| [src/tests/test_tui_menu_flow.cpp](src/tests/test_tui_menu_flow.cpp) | TUI 菜单流程测试 | app_state |
| [src/tests/mock_http_client.hpp](src/tests/mock_http_client.hpp) | HTTP Mock 测试辅助 | - |

---

## 头文件 (.hpp/.h)

### include/shuati/ - 核心头文件

| 文件路径 | 功能说明 |
|---------|---------|
| [include/shuati/version.hpp](include/shuati/version.hpp) | 版本结构体定义 |
| [include/shuati/types.hpp](include/shuati/types.hpp) | 公共类型定义 (Problem, ReviewItem, etc.) |
| [include/shuati/config.hpp](include/shuati/config.hpp) | 配置管理 |
| [include/shuati/database.hpp](include/shuati/database.hpp) | 数据库接口 |
| [include/shuati/logger.hpp](include/shuati/logger.hpp) | 日志接口 |
| [include/shuati/crawler.hpp](include/shuati/crawler.hpp) | 爬虫基类 |
| [include/shuati/judge.hpp](include/shuati/judge.hpp) | 判题引擎接口 |
| [include/shuati/problem_manager.hpp](include/shuati/problem_manager.hpp) | 题目管理器接口 |
| [include/shuati/ai_coach.hpp](include/shuati/ai_coach.hpp) | AI 教练接口 |
| [include/shuati/compiler_doctor.hpp](include/shuati/compiler_doctor.hpp) | 编译器诊断接口 |
| [include/shuati/boot_guard.hpp](include/shuati/boot_guard.hpp) | 启动检查接口 |
| [include/shuati/memory_manager.hpp](include/shuati/memory_manager.hpp) | 记忆管理接口 |
| [include/shuati/mistake_analyzer.hpp](include/shuati/mistake_analyzer.hpp) | 错误分析接口 |
| [include/shuati/sm2_algorithm.hpp](include/shuati/sm2_algorithm.hpp) | SM2 算法接口 |
| [include/shuati/tui_app.hpp](include/shuati/tui_app.hpp) | TUI 应用入口接口 |
| [include/shuati/tui_views.hpp](include/shuati/tui_views.hpp) | TUI 视图渲染接口 |

### include/shuati/adapters/ - 适配器头文件

| 文件路径 | 功能说明 |
|---------|---------|
| [include/shuati/adapters/leetcode_crawler.hpp](include/shuati/adapters/leetcode_crawler.hpp) | LeetCode 爬虫接口 |
| [include/shuati/adapters/codeforces_crawler.hpp](include/shuati/adapters/codeforces_crawler.hpp) | Codeforces 爬虫接口 |
| [include/shuati/adapters/luogu_crawler.hpp](include/shuati/adapters/luogu_crawler.hpp) | 洛谷爬虫接口 |
| [include/shuati/adapters/lanqiao_crawler.hpp](include/shuati/adapters/lanqiao_crawler.hpp) | 蓝桥杯爬虫接口 |
| [include/shuati/adapters/companion_server.hpp](include/shuati/adapters/companion_server.hpp) | Companion Server 接口 |

### include/shuati/utils/ - 工具头文件

| 文件路径 | 功能说明 |
|---------|---------|
| [include/shuati/utils/encoding.hpp](include/shuati/utils/encoding.hpp) | 编码工具接口 |

---

## 架构说明

项目采用**分层架构 (Domain-Driven Design)**：

1. **CLI 层** (`src/cmd/`): 命令行接口，处理用户输入
2. **TUI 层** (`src/tui/`): 终端交互界面，基于 FTXUI 的单面板 + 全屏子视图架构
   - **Main 视图**: 命令行滚动面板 + 自动补全 + 命令提示 + ASCII 欢迎页
   - **ConfigView**: 全屏配置编辑器 (表单式)
   - **ListView**: 全屏题目浏览器 (交互式表格)
   - **HintView**: 全屏 AI 提示滚动页 (异步加载)
3. **核心层** (`src/core/`): 业务逻辑 (判题、题目管理、记忆曲线)
4. **基础设施层** (`src/infra/`): 数据库、日志、HTTP 客户端
5. **适配器层** (`src/adapters/`): 外部系统集成 (爬虫、AI 接口、浏览器插件)
6. **工具层** (`src/utils/`): 通用工具函数
7. **路由层** (`src/router/`): CLI/TUI 入口分发

---

## 配置文件

| 文件路径 | 功能说明 |
|---------|---------|
| [CMakeLists.txt](CMakeLists.txt) | 主 CMake 配置，定义项目、依赖、目标 |
| [vcpkg.json](vcpkg.json) | vcpkg 依赖清单 |
| [install.ps1](install.ps1) | Windows PowerShell 安装脚本 |
| [install.sh](install.sh) | Linux/macOS Bash 安装脚本 |
| [installer/shuati.iss](installer/shuati.iss) | Inno Setup Windows 安装程序配置 |
| [.gitignore](.gitignore) | Git 忽略规则 |
| [resources/rules/compiler_errors.json](resources/rules/compiler_errors.json) | 编译器错误规则配置 |

---

## CI/CD 工作流

| 文件路径 | 功能说明 |
|---------|---------|
| [.github/workflows/ci.yml](.github/workflows/ci.yml) | 持续集成 (质量检查 + 三平台构建 + 测试) |
| [.github/workflows/release.yml](.github/workflows/release.yml) | 发布 (构建 + 打包 + 安装包 + GitHub Release) |
| [.github/workflows/prepare-release.yml](.github/workflows/prepare-release.yml) | 发布准备 (版本更新 + 标签 + 触发发布) |
| [.github/workflows/rollback-release.yml](.github/workflows/rollback-release.yml) | 发布回滚 |
| [.github/workflows/workflow-tests.yml](.github/workflows/workflow-tests.yml) | 工作流自检 (actionlint + 发布工具验证) |

---

## 第三方依赖 (vcpkg)

| 依赖名称 | 用途 |
|---------|------|
| CLI11 | 命令行解析 |
| nlohmann-json | JSON 处理 |
| cpr | HTTP 客户端 |
| SQLiteCpp | SQLite 数据库封装 |
| fmt | 格式化库 |
| replxx | REPL 交互 |
| ftxui | 终端 UI |
| cpp-httplib | HTTP 服务器 |

---

## 工具脚本

| 文件路径 | 功能说明 |
|---------|---------|
| [scripts/check_version.py](scripts/check_version.py) | CI 版本一致性校验 |
| [scripts/benchmark_tui.ps1](scripts/benchmark_tui.ps1) | TUI 渲染性能基准测试 |
| [scripts/run_tui_regression.ps1](scripts/run_tui_regression.ps1) | TUI 回归测试运行脚本 |
| [tools/release/plan_release.py](tools/release/plan_release.py) | 发布计划生成脚本 |
| [tools/release/update_versions.py](tools/release/update_versions.py) | 版本号更新脚本 |
| [tools/release/verify_release_logic.py](tools/release/verify_release_logic.py) | 发布逻辑验证脚本 |
| [tools/release/__init__.py](tools/release/__init__.py) | Python 包初始化 |

---

*此文件由自动化工具生成，如有更新请重新生成。*
