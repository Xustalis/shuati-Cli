<div align="center">

```text
  ███████╗██╗  ██╗██╗   ██╗ █████╗ ████████╗██╗
  ██╔════╝██║  ██║██║   ██║██╔══██╗╚══██╔══╝██║
  ███████╗███████║██║   ██║███████║   ██║   ██║
  ╚════██║██╔══██║██║   ██║██╔══██║   ██║   ██║
  ███████║██║  ██║╚██████╔╝██║  ██║   ██║   ██║
  ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝
```

# Shuati CLI

[![CI](https://github.com/Xustalis/shuati-Cli/actions/workflows/ci.yml/badge.svg)](https://github.com/Xustalis/shuati-Cli/actions/workflows/ci.yml)
[![Release](https://github.com/Xustalis/shuati-Cli/actions/workflows/release.yml/badge.svg)](https://github.com/Xustalis/shuati-Cli/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)

**Shuati CLI** 是一个专为oier and coder 打造的命令行算法练习工具。它将"拉题 -> 编码 -> 测试 -> 调试"的完整工作流搬到了本地终端，让你告别浏览器 Web IDE 的低效调试，专注于算法本身。

[安装指南](#安装指南) • [快速开始](#快速开始) • [功能详解](#功能详解) • [配置说明](#配置说明) • [故障排除](#故障排除)

</div>

---

## 1. 项目概述

### 技术栈
- **语言**：C++20
- **核心库**：CLI11 (命令行解析), Replxx (交互式终端), FTXUI (终端界面), nlohmann/json, cpr (网络请求), SQLiteCpp (本地数据库)
- **构建系统**：CMake + vcpkg

### 架构概览

```mermaid
graph TD
    User((用户)) -->|命令| CLI["终端接口 (CLI/Replxx)"]
    CLI --> PM["题目管理器 (ProblemManager)"]
    
    subgraph Core["核心逻辑层"]
        PM -->|拉取| Crawler[多平台爬虫]
        PM -->|管理| DB[("SQLite 数据库")]
        PM -->|生成| Template[代码模板引擎]
        CLI --> Judge[判题引擎]
        CLI --> AI["AI 教练 (LLM)"]
    end
    
    subgraph Execution["执行环境"]
        Judge -->|编译| Compiler[编译器 (g++/python)]
        Judge -->|运行| Sandbox[子进程沙箱]
        Sandbox -->|I/O 重定向| FS[文件系统]
    end
    
    Crawler -->|题目/样例| DB
    AI -->|读取上下文| DB
    Template -->|源文件| FS
```

---

## 2. 安装指南

### 系统要求
- **操作系统**：Windows 10/11 (x64), Linux (x64), macOS (x64)
- **依赖**：
    - **C++ 用户**: `g++` (GCC 10+) 或 `clang` (支持 C++20)
    - **Python 用户**: Python 3.8+
    - **网络**: 能够访问 GitHub 和题目源站（如 LeetCode）

### 安装步骤

#### Windows
我们提供了自动化安装脚本，由 PowerShell 编写。

1. **下载**：前往 [Releases](https://github.com/Xustalis/shuati-Cli/releases) 下载 `shuati-windows-x64-vX.Y.Z.zip`。
2. **解压**：解压压缩包。
3. **安装**：
   右键点击 `install.ps1` 选择 "使用 PowerShell 运行"，或者在终端执行：
   ```powershell
   .\install.ps1
   ```
   > 脚本会自动将 `shuati.exe` 所在目录添加到用户环境变量 `PATH` 中，并进行初始化。
   
4. **验证**：重启终端，输入 `shuati --version`。

#### Linux / macOS
1. **下载**：前往 [Releases](https://github.com/Xustalis/shuati-Cli/releases) 下载对应的 `.tar.gz` 包。
2. **解压与安装**：
   ```bash
   tar -xzvf shuati-linux-x64-vX.Y.Z.tar.gz
   chmod +x shuati
   sudo mv shuati /usr/local/bin/
   ```
3. **验证**：执行 `shuati --version`。

### 验证安装
在终端输入以下命令，如果看到版本号说明安装成功：

```bash
shuati --version
# 输出示例: shuati version 1.4.3
```

---

## 3. 快速使用教程 (5分钟上手)

### 第一步：初始化
在你的工作目录下初始化 Shuati 工作区。

```bash
mkdir my-algorithm-study
cd my-algorithm-study
shuati init
```
*这会创建 `.shuati/` 目录和配置文件。*

### 第二步：配置 (可选)
配置你的 API Key (用于 AI 功能) 和偏好的编辑器。

```bash
shuati config --api-key sk-xxxxxxxxx
shuati config --editor "code"  # 使用 VSCode 打开文件
```

### 第三步：拉取题目
拉取一道 LeetCode 题目（例如两数之和）。

```bash
shuati pull https://leetcode.cn/problems/two-sum/
```

### 第四步：解题
生成代码文件。支持输入 ID 或 DisplayID。

```bash
shuati solve 1
```
*此时会自动创建 `solution_1.cpp` (或 .py) 并用配置的编辑器打开。*

### 第五步：编写代码与测试
编辑代码后，运行本地测试。

```bash
# 编写你的代码...
shuati test 1
```
*系统会运行本地样例，并自动生成额外的边界测试用例。*

---

## 4. 功能详解

### `shuati init`
初始化当前目录为 Shuati 项目。
- 创建 SQLite 数据库用于存储题目和做题记录。
- 创建模板目录。

### `shuati pull <url>`
从支持的 OJ 平台拉取题目。
- **支持平台**：LeetCode (CN/Global), Luogu, Codeforces。
- **效果**：自动抓取标题、描述、测试样例、标签、难度等信息存入数据库。

### `shuati solve <id>`
开始解决某道题目。
- **功能**：
    - 如果未指定 ID，则进入交互式选择菜单。
    - 根据模板生成代码文件（如果文件不存在）。
    - 自动打开编辑器。
- **示例**：`shuati solve 1` 或 `shuati solve` (交互模式)

### `shuati test <id> [options]`
运行本地测试。这是 Shuati CLI 最强大的功能之一。
- **功能**：
    - 编译并运行代码。
    - 运行数据库中的样例。
    - **自动生成测试用例**：根据题目参数推断，自动生成边界值、随机值测试点。
    - **AI 预期生成**：可选请求 AI 生成测试点的预期输出（Oracle）。
- **参数**：
    - `--max <N>`: 最大测试点数量 (默认 30)
    - `--oracle <auto|ai|none>`: 预期输出获取方式
    - `--ui`: 开启交互式界面查看详细测试数据

```bash
shuati test 1 --max 50 --oracle ai --ui
```

### `shuati hint <id>`
卡住时获取 AI 启发式提示。
- **功能**：AI 会读取你的代码和题目描述，给出思路提示，而不是直接给答案。
- **示例**：`shuati hint 1`

### `shuati submit <id>`
本地提交（记录做题状态）。
- **功能**：记录做题结果（掌握程度 0-5），根据艾宾浩斯遗忘曲线计算下次复习时间。
- **参数**：
    - `-q, --quality <0-5>`: 掌握程度。
- **示例**：`shuati submit 1 -q 5` (秒杀)

### `shuati list` & `shuati delete`
- `list`：列出所有已拉取的题目。
- `delete <id>`：删除指定题目及其记录。

### 交互式 REPL 模式
直接输入 `shuati` 不带参数，进入交互式命令行模式，支持命令自动补全。

```text
shuati > list
...
shuati > solve 1
```

---

## 5. 完整配置说明

配置文件位于 `.shuati/config.json`。

| 字段 | 说明 | 示例 |
| :--- | :--- | :--- |
| `api_key` | OpenAI 格式的 API Key (深际/OpenAI等) | `"sk-..."` |
| `api_base` | API 基础 URL | `"https://api.deepseek.com/v1"` |
| `model` | 使用的模型名称 | `"deepseek-chat"` |
| `language` | 默认编程语言 (`cpp` 或 `python`) | `"cpp"` |
| `editor` | 打开代码文件的命令 | `"code"` (VSCode), `"vim"`, `"notepad"` |
| `ai_enabled`| 是否启用 AI 功能 | `true` |

使用命令修改配置：
```bash
shuati config --api-key <KEY>
shuati config --language python
shuati config --show
```

---

## 6. 故障排除手册

**Q1: `list` 命令显示乱码或 "invalid utf8"**
- **原因**：通常是因为 SQLite 数据库中存储了非 UTF-8 字符（如 Windows GBK 编码），或者终端不支持 UTF-8。
- **解决**：
  1. Windows 用户请确保使用 Windows Terminal 或 PowerShell，并设置 `chcp 65001`。
  2. 我们在 v1.3.1+ 版本中增加了数据库读取时的自动 UTF-8 修复，请升级到最新版。

**Q2: 运行 `test` 时提示找不到编译器**
- **原因**：系统环境变量中没有 `g++` 或 `python`。
- **解决**：请安装 GCC (Windows 推荐 MinGW-w64) 或 Python，并确保 `g++ --version` 或 `python --version` 在终端能正常运行。

**Q3: Windows 下运行报错 "缺少 dll"**
- **原因**：Windows 二进制依赖 vcpkg 安装的 DLL 文件。
- **解决**：请确保下载的是完整的 Release zip 包，且解压后 `shuati.exe` 旁边有相关的 `.dll` 文件。如果是源码编译，请将 vcpkg 的 `bin` 目录加入 PATH。

---

## 7. 开发指南

### 环境准备
- CMake 3.20+
- C++20 兼容编译器 (MSVC 2022 / GCC 10+)
- vcpkg (包管理器)

### 构建步骤 (Windows/Linux/macOS)

```bash
# 1. 克隆仓库
git clone https://github.com/Xustalis/shuati-Cli.git
cd shuati-Cli

# 2. 安装 vcpkg (如果已有可跳过)
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh  # Windows 使用 .bat

# 3. 配置 CMake (使用 vcpkg 工具链)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake

# 4. 编译
cmake --build build --config Release
```

### 运行测试
我们提供了内部单元测试：

```bash
cd build
ctest --output-on-failure
```

---

## 8. 贡献指南

欢迎任何形式的贡献！无论是提交 Bug，修复代码，还是改进文档。

1.  **Fork** 本仓库。
2.  **创建分支**: `git checkout -b feat/my-feature`。
3.  **提交代码**: 请遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范 (如 `feat: add new crawler`)。
4.  **提交 PR**: 推送到你的 Fork 并提交 Pull Request。

详细指南请参阅 [CONTRIBUTING.md](CONTRIBUTING.md)。

---

## 9. 附加资源

- **版本日志**: 查看 [CHANGELOG.md](CHANGELOG.md) 了解更新详情。
- **许可证**: 本项目采用 [MIT 许可证](LICENSE)。

感谢所有贡献者！

[⬆ 回到顶部](#shuati-cli)
