# shuati CLI - 你的 AI 智能算法教练

> **刷题不是为了 AC，而是为了不再犯同样的错。**

**shuati CLI** 是一个以命令行 (CLI) 优先的智能刷题辅助工具。它不直接提供答案，而是像教练一样，追踪你的错误，提供基于 AI 的低成本提示，并利用间隔重复算法（SM-2）安排复习计划。

## 功能特性 (Phase 1)

| 命令 | 说明 |
|:---|:---|
| `shuati init` | 在当前目录初始化项目（创建 `.shuati` 数据库） |
| `shuati pull <url>` | 从 URL 拉取题目 (自动生成 Markdown) |
| `shuati solve <id>` | **开始做题**：生成代码模板 `solution_<id>.cpp` |
| `shuati list` | 列出所有题目 |
| `shuati submit <id>` | **提交记录**：自评难度 (0-5)，记录错误类型 |
| `shuati review` | 查看今日需要复习的题目 |
| `shuati next` | **智能推荐**：基于复习计划和薄弱项推荐下一题 |
| `shuati hint <id>` | **AI 提示**：发送代码给 AI (DeepSeek)，获取思路指引，绝不直接给答案 |
| `shuati stats` | 查看错误类型统计图表 |
| `shuati config` | 配置 API Key 和模型参数 |

## 快速开始

```bash
# 1. 初始化项目
shuati init

# 2. 配置 DeepSeek API
shuati config --api-key sk-xxxxxx

# 3. 拉取一道题目 (例如 LeetCode)
shuati pull https://leetcode.com/problems/two-sum/

# 4. 开始做题 (生成 solution_web_xxx.cpp)
shuati solve web_17707

# 5. (编写代码...)

# 6. 遇到困难？寻求 AI 提示 (不消耗大量 Token，仅分析逻辑)
shuati hint web_17707

# 7. 完成后提交记录
shuati submit web_17707 -q 3
# (如果难度 < 3，系统会追问错误类型，如 "Off-by-one" 或 "贪心误用")

# 8. 明天再来复习
shuati review
```

## 核心理念

1.  **教练 (Coach) 而非题库**：我们不存储题库，我们管理你的练习过程。
2.  **错误驱动学习**：记录每一次犯错的原因（边界条件？状态转移？），针对性强化。
3.  **间隔重复 (Spaced Repetition)**：集成 SM-2 算法，在遗忘临界点提醒复习。
4.  **低 Token 消耗**：AI 仅作为“纠错员”，Prompt 经过高度优化，且支持国产 DeepSeek 模型。

## 构建指南

本项目基于 C++20，依赖 `vcpkg` 管理库。

**前置要求**:
* C++20 编译器 (MSVC 2022 / GCC 10+ / Clang 11+)
* CMake 3.20+
* Git

**构建步骤**:

```bash
# 1. 克隆项目 & vcpkg
git clone ...
cd "shuati CLI"
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.bat

# 2. 安装依赖
./vcpkg/vcpkg.exe install

# 3. 编译
cmake -B build -S . -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# 4. 运行
./build/Release/shuati.exe init
```

## 技术栈

*   **语言**: C++20
*   **构建**: CMake, vcpkg
*   **CLI 框架**: CLI11
*   **数据库**: SQLiteCpp (SQLite3)
*   **网络**: cpr (libcurl)
*   **JSON**: nlohmann/json
*   **AI**: DeepSeek API

## License

MIT
