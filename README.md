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
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Version](https://img.shields.io/badge/version-1.2.0-green.svg)](https://github.com/Xustalis/shuati-Cli/releases)

**Shuati CLI** 是一个基于命令行的算法练习工具：拉题、生成本地代码文件、运行本地测评、记录复习进度，并提供 AI 启发式提示。

本项目适合习惯使用 CLI 和 Vim/VSCode 等本地编辑器，且希望脱离浏览器 Web IDE 低效调试环节的开发者。

[快速开始](#快速开始) • [安装指南](#安装指南) • [配置说明](#配置说明) • [故障排除](#故障排除) • [开发指南](#开发指南)

</div>

---

## 🏗 架构概览

Shuati CLI 采用分层架构设计，核心包括爬虫适配器、本地判题沙箱和 AI 诊断模块。

```mermaid
graph TD
    User((用户)) -->|命令| CLI["命令行接口 (CLI11/Replxx)"]
    CLI --> PM["题目管理器 ProblemManager"]
    
    subgraph Core Logic
        PM -->|拉取| Crawler[多平台爬虫]
        PM -->|生成| Template[代码模板引擎]
        PM -->|测试| Judge[本地判题引擎]
        PM -->|诊断| AI["AI Coach (LLM)"]
    end
    
    subgraph Data Layer
        DB[("SQLite 数据库")]
        FS[文件系统]
    end
    
    Crawler -->|题目/样例| DB
    Template -->|源文件| FS
    Judge -->|读取| FS
    Judge -->|执行/监控| Sandbox[子进程沙箱]
    AI -->|读取上下文| DB
```

## 快速开始

### 系统要求

- **Windows**: Windows 10/11（x64）
- **Linux/macOS**: x64
- **运行时**:
  - C++ 题目本地测评：需要 `g++`（Windows 推荐 MinGW-w64/GCC 10+；Linux/macOS 使用系统 GCC/Clang）
  - Python 题目本地测评：需要 `python`（3.x）

### 方式 A：下载 Release（二进制）

1. 前往 [Releases](https://github.com/Xustalis/shuati-Cli/releases) 下载对应平台的压缩包
2. 解压后将 `shuati`（或 `shuati.exe`）加入 `PATH`
3. 验证：

```bash
shuati --help
```

Windows 也可以在解压目录运行安装脚本（可选）：

```powershell
.\install.ps1
```

### 方式 B：源码构建（开发者）

#### 依赖

- Git
- CMake 3.20+
- 编译器：MSVC（VS2022）/ GCC 10+ / Clang 12+
- vcpkg（本项目使用 `vcpkg.json` 管理依赖）

```bash
git clone https://github.com/Xustalis/shuati-Cli.git
cd shuati-CLI

git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release

.\build\Release\shuati.exe --help
```

## 核心工作流

Shuati CLI 的设计理念是 **"Pull -> Solve -> Test -> Debug"** 闭环。

### 1. 初始化项目
在任意工作目录下初始化结构，生成 `.shuati/`、SQLite 数据库和配置文件。

```bash
shuati init
```

### 2. 拉取题目 (Pull)
支持 LeetCode (CN/US), Luogu, Codeforces 等平台。

```bash
shuati pull https://leetcode.cn/problems/two-sum/
```

### 3. 解题 (Solve)
生成本地代码文件（最小骨架）

```bash
shuati solve 1
```

### 4. 本地测试 (Test)
运行样例，并自动生成边界/正常/组合测试点（可选生成预期输出）。

```bash
shuati test 1
```

常用参数：

```bash
shuati test 1 --max 30 --oracle auto
shuati test 1 --oracle none
shuati test 1 --ui
```

### 5. 获取提示 (Hint)
结合题目、样例与当前代码，输出启发式提示（不直接给完整解法）。

```bash
shuati hint lg_P5728
shuati hint lg_P5728 -f solution_lg_P5728.cpp
```

## 配置说明

配置文件位于项目根目录：`.shuati/config.json`。

```json
{
  "api_key": "",
  "api_base": "https://api.deepseek.com/v1",
  "model": "deepseek-chat",
  "language": "cpp",
  "max_tokens": 300,
  "editor": "code",
  "ai_enabled": true,
  "template_enabled": true
}
```

常用配置命令：

```bash
shuati config --show
shuati config --api-key <YOUR_KEY>
shuati config --model deepseek-chat
shuati config --language cpp
```

## 安装指南

### Windows

1. 下载 Release 压缩包并解压
2. 将解压目录加入 `PATH`
3. 验证：

```powershell
shuati --help
```

### Linux

1. 下载 `.tar.gz` 解压（或源码构建）
2. 放到 `~/.local/bin` 并确保在 `PATH` 中

```bash
mkdir -p ~/.local/bin
tar -xzf shuati-Linux-x64-vX.Y.Z.tar.gz
cp -f shuati ~/.local/bin/shuati
chmod +x ~/.local/bin/shuati
shuati --help
```

### macOS

流程与 Linux 类似（或源码构建）：

```bash
chmod +x shuati
./shuati --help
```

## 本地判题引擎

本项目内置了一个轻量级 OJ 判题器 (`src/core/judge.cpp`)，实现标准 ACM 判题逻辑。

| Verdict | 含义 | 判定逻辑 |
| :--- | :--- | :--- |
| **AC** | Accepted | Stdout 与 Expected 一致 (忽略行末空格) |
| **WA** | Wrong Answer | Stdout 与 Expected 不一致 |
| **TLE** | Time Limit Exceeded | 运行时间超过限制 (默认 1s)，子进程被 Kill |
| **MLE** | Memory Limit Exceeded | 峰值内存超过限制 (Windows下使用 PSAPI 监控) |
| **RE** | Runtime Error | 进程退出码非 0 |
| **CE** | Compile Error | 编译器 (g++) 返回非 0 |

## 故障排除

### 1) `-std=c++20` 不支持 / 编译器过旧

- 升级 MinGW-w64 / GCC（推荐 GCC 10+）
- 或在项目配置中使用更现代的编译器（MSVC/Clang）

### 2) Windows 运行时报缺少 DLL

- 使用 Release 压缩包自带的 DLL 一起部署
- 或确保 `vcpkg_installed/.../bin` 里的依赖 DLL 与 `shuati.exe` 同目录

### 3) `list` 输出 `invalid utf8`

- 这通常是终端编码或抓取内容编码导致。项目已在 Windows 下对输出做了 UTF-8 兜底处理；
- 若仍遇到，建议使用 Windows Terminal 并将编码设置为 UTF-8。

### 4) `test` 没有测试用例/无法校验

- 部分平台可能抓不到完整样例；可使用 `--oracle ai` 生成预期输出，或 `--oracle none` 仅做运行检查。

## 开发指南

### 本地构建

```bash
git clone https://github.com/Xustalis/shuati-Cli.git
cd shuati-CLI

git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Debug
.\build\Debug\shuati.exe --help
```

### 贡献

- Issue/PR 模板在 `.github/`
- 贡献指南见 [CONTRIBUTING.md](CONTRIBUTING.md) 与 [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md)

我们非常欢迎社区贡献。为了保持代码质量，请遵循以下规范：

- Bug 提交：优先用 Issue 模板
- 功能建议：先提 Issue 再开 PR
- 代码提交：确保 CI 通过
