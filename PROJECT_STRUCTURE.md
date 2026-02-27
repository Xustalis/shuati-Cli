# Shuati CLI 项目结构说明

> 生成日期: 2026-02-26
> 项目版本: v0.0.1

## 目录结构概览

```
shuati-Cli/
├── .github/                 # GitHub 配置
├── include/                 # 头文件
├── installer/               # 安装程序配置
├── resources/               # 资源文件
├── src/                     # 源代码
├── tools/                   # 工具脚本
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

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/cmd/main.cpp](src/cmd/main.cpp) | 程序入口，REPL 主循环，命令解析 | 是 | commands, version, logger, boot_guard, encoding |
| [src/cmd/commands.cpp](src/cmd/commands.cpp) | CLI 命令注册与绑定 | 是 | version, CLI11 |
| [src/cmd/commands.hpp](src/cmd/commands.hpp) | 命令上下文与函数声明头文件 | 是 | CLI11, version, database, problem_manager, judge, ai_coach |
| [src/cmd/basic_commands.cpp](src/cmd/basic_commands.cpp) | 基础命令实现 (init, info, config) | 是 | version, database, config |
| [src/cmd/manage_commands.cpp](src/cmd/manage_commands.cpp) | 题目管理命令 (pull, new, delete, submit) | 是 | problem_manager, crawler, database |
| [src/cmd/solve_command.cpp](src/cmd/solve_command.cpp) | 解题命令实现 | 是 | problem_manager, judge, ai_coach |
| [src/cmd/list_command.cpp](src/cmd/list_command.cpp) | 列表命令实现 | 是 | problem_manager, database |
| [src/cmd/view_command.cpp](src/cmd/view_command.cpp) | 查看测试详情命令 | 是 | problem_manager, judge |
| [src/cmd/test_command.cpp](src/cmd/test_command.cpp) | 测试命令实现 | 是 | problem_manager, judge, ai_coach |
| [src/cmd/services.cpp](src/cmd/services.cpp) | 服务层封装，整合各模块 | 是 | problem_manager, judge, ai_coach, crawler, companion_server |

### src/core/ - 核心业务逻辑层

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/core/version.cpp](src/core/version.cpp) | 版本号解析与比较 | 是 | fmt |
| [src/core/problem_manager.cpp](src/core/problem_manager.cpp) | 题目管理器，CRUD 操作 | 是 | database, crawler |
| [src/core/judge.cpp](src/core/judge.cpp) | 本地判题引擎，沙箱执行 | 是 | database, logger, fmt, Threads |
| [src/core/compiler_doctor.cpp](src/core/compiler_doctor.cpp) | 编译器诊断工具 | 是 | fmt, nlohmann_json |
| [src/core/boot_guard.cpp](src/core/boot_guard.cpp) | 启动检查与历史记录 | 是 | fmt, filesystem |
| [src/core/memory_manager.cpp](src/core/memory_manager.cpp) | 记忆曲线管理 (SM2 算法) | 是 | database |
| [src/core/mistake_analyzer.cpp](src/core/mistake_analyzer.cpp) | 错误分析器 | 是 | database |
| [src/core/sm2_algorithm.cpp](src/core/sm2_algorithm.cpp) | SM2 间隔重复算法实现 | 是 | - |

### src/infra/ - 基础设施层

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/infra/database.cpp](src/infra/database.cpp) | SQLite 数据库封装 | 是 | SQLiteCpp, nlohmann_json |
| [src/infra/logger.cpp](src/infra/logger.cpp) | 日志系统 | 是 | fmt, filesystem |

### src/adapters/ - 适配器层

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/adapters/leetcode_crawler.cpp](src/adapters/leetcode_crawler.cpp) | LeetCode 爬虫 | 是 | cpr, nlohmann_json |
| [src/adapters/codeforces_crawler.cpp](src/adapters/codeforces_crawler.cpp) | Codeforces 爬虫 | 是 | cpr, nlohmann_json |
| [src/adapters/luogu_crawler.cpp](src/adapters/luogu_crawler.cpp) | 洛谷爬虫 | 是 | cpr, nlohmann_json |
| [src/adapters/lanqiao_crawler.cpp](src/adapters/lanqiao_crawler.cpp) | 蓝桥杯爬虫 | 是 | cpr, nlohmann_json |
| [src/adapters/companion_server.cpp](src/adapters/companion_server.cpp) | Companion Server (浏览器插件通信) | 是 | httplib, nlohmann_json |
| [src/adapters/ai_coach.cpp](src/adapters/ai_coach.cpp) | AI 教练 (LLM 接口) | 是 | cpr, nlohmann_json, database |

### src/utils/ - 工具层

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/utils/encoding.cpp](src/utils/encoding.cpp) | 编码转换工具 (UTF-8/GBK) | 是 | - |

### src/tests/ - 测试文件

| 文件路径 | 功能说明 | 核心模块 | 依赖模块 |
|---------|---------|---------|---------|
| [src/tests/test_version_logic.cpp](src/tests/test_version_logic.cpp) | 版本逻辑测试 | 否 | version, fmt |
| [src/tests/test_judge_complex.cpp](src/tests/test_judge_complex.cpp) | 判题引擎复杂测试 | 否 | judge, fmt, SQLiteCpp |
| [src/tests/test_judge_security.cpp](src/tests/test_judge_security.cpp) | 判题引擎安全测试 | 否 | judge, fmt |
| [src/tests/test_memory.cpp](src/tests/test_memory.cpp) | 记忆管理测试 | 否 | memory_manager, database |

---

## 头文件 (.hpp/.h)

### include/shuati/ - 核心头文件

| 文件路径 | 功能说明 | 核心模块 |
|---------|---------|---------|
| [include/shuati/version.hpp](include/shuati/version.hpp) | 版本结构体定义 | 是 |
| [include/shuati/types.hpp](include/shuati/types.hpp) | 公共类型定义 | 是 |
| [include/shuati/config.hpp](include/shuati/config.hpp) | 配置管理 | 是 |
| [include/shuati/database.hpp](include/shuati/database.hpp) | 数据库接口 | 是 |
| [include/shuati/logger.hpp](include/shuati/logger.hpp) | 日志接口 | 是 |
| [include/shuati/crawler.hpp](include/shuati/crawler.hpp) | 爬虫基类 | 是 |
| [include/shuati/judge.hpp](include/shuati/judge.hpp) | 判题引擎接口 | 是 |
| [include/shuati/problem_manager.hpp](include/shuati/problem_manager.hpp) | 题目管理器接口 | 是 |
| [include/shuati/ai_coach.hpp](include/shuati/ai_coach.hpp) | AI 教练接口 | 是 |
| [include/shuati/compiler_doctor.hpp](include/shuati/compiler_doctor.hpp) | 编译器诊断接口 | 是 |
| [include/shuati/boot_guard.hpp](include/shuati/boot_guard.hpp) | 启动检查接口 | 是 |
| [include/shuati/memory_manager.hpp](include/shuati/memory_manager.hpp) | 记忆管理接口 | 是 |
| [include/shuati/mistake_analyzer.hpp](include/shuati/mistake_analyzer.hpp) | 错误分析接口 | 是 |
| [include/shuati/sm2_algorithm.hpp](include/shuati/sm2_algorithm.hpp) | SM2 算法接口 | 是 |

### include/shuati/adapters/ - 适配器头文件

| 文件路径 | 功能说明 | 核心模块 |
|---------|---------|---------|
| [include/shuati/adapters/leetcode_crawler.hpp](include/shuati/adapters/leetcode_crawler.hpp) | LeetCode 爬虫接口 | 是 |
| [include/shuati/adapters/codeforces_crawler.hpp](include/shuati/adapters/codeforces_crawler.hpp) | Codeforces 爬虫接口 | 是 |
| [include/shuati/adapters/luogu_crawler.hpp](include/shuati/adapters/luogu_crawler.hpp) | 洛谷爬虫接口 | 是 |
| [include/shuati/adapters/lanqiao_crawler.hpp](include/shuati/adapters/lanqiao_crawler.hpp) | 蓝桥杯爬虫接口 | 是 |
| [include/shuati/adapters/companion_server.hpp](include/shuati/adapters/companion_server.hpp) | Companion Server 接口 | 是 |

### include/shuati/utils/ - 工具头文件

| 文件路径 | 功能说明 | 核心模块 |
|---------|---------|---------|
| [include/shuati/utils/encoding.hpp](include/shuati/utils/encoding.hpp) | 编码工具接口 | 是 |

---

## CMake 文件

| 文件路径 | 功能说明 |
|---------|---------|
| [CMakeLists.txt](CMakeLists.txt) | 主 CMake 配置，定义项目、依赖、目标 |

---

## 配置文件

| 文件路径 | 功能说明 |
|---------|---------|
| [vcpkg.json](vcpkg.json) | vcpkg 依赖清单 |
| [install.ps1](install.ps1) | Windows PowerShell 安装脚本 |
| [install.sh](install.sh) | Linux/macOS Bash 安装脚本 |
| [installer/shuati.iss](installer/shuati.iss) | Inno Setup Windows 安装程序配置 |
| [.gitignore](.gitignore) | Git 忽略规则 |
| [resources/rules/compiler_errors.json](resources/rules/compiler_errors.json) | 编译器错误规则配置 |

---

## Markdown 文档

| 文件路径 | 功能说明 |
|---------|---------|
| [README.md](README.md) | 项目主文档，介绍、安装、使用说明 |
| [CHANGELOG.md](CHANGELOG.md) | 版本变更日志 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 贡献指南 |
| [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) | GitHub 贡献指南 |
| [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md) | PR 模板 |
| [.github/ISSUE_TEMPLATE/bug_report.md](.github/ISSUE_TEMPLATE/bug_report.md) | Bug 报告模板 |

---

## CI/CD 工作流

| 文件路径 | 功能说明 |
|---------|---------|
| [.github/workflows/ci.yml](.github/workflows/ci.yml) | 持续集成工作流 (构建、测试) |
| [.github/workflows/release.yml](.github/workflows/release.yml) | 发布工作流 (构建、打包、发布) |
| [.github/workflows/prepare-release.yml](.github/workflows/prepare-release.yml) | 发布准备工作流 (版本更新、标签) |
| [.github/workflows/rollback-release.yml](.github/workflows/rollback-release.yml) | 发布回滚工作流 |
| [.github/workflows/workflow-tests.yml](.github/workflows/workflow-tests.yml) | 工作流测试 |

---

## 工具脚本

| 文件路径 | 功能说明 |
|---------|---------|
| [tools/release/plan_release.py](tools/release/plan_release.py) | 发布计划生成脚本 |
| [tools/release/update_versions.py](tools/release/update_versions.py) | 版本号更新脚本 |
| [tools/release/verify_release_logic.py](tools/release/verify_release_logic.py) | 发布逻辑验证脚本 |
| [tools/release/__init__.py](tools/release/__init__.py) | Python 包初始化 |

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

## 架构说明

项目采用**分层架构 (Domain-Driven Design)**：

1. **CLI 层** (`src/cmd/`): 命令行接口，处理用户输入
2. **核心层** (`src/core/`): 业务逻辑，如判题、题目管理
3. **基础设施层** (`src/infra/`): 数据库、日志等基础设施
4. **适配器层** (`src/adapters/`): 外部系统集成，如爬虫、AI 接口
5. **工具层** (`src/utils/`): 通用工具函数

---

*此文件由自动化工具生成，如有更新请重新生成。*
