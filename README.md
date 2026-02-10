# 🧠 shuati — AI-Powered Algorithm Training Coach

> **刷题不是为了 AC，而是为了不再犯同样的错。**

A CLI-first intelligent algorithm practice coach. It tracks your mistakes, gives low-token AI hints (never full solutions), and tells you exactly what to practice next.

## Features

| Command | Description |
|:---|:---|
| `shuati init` | Initialize project in current directory |
| `shuati pull <url>` | Import a problem from any URL |
| `shuati new <title>` | Create a local problem |
| `shuati list` | List all problems |
| `shuati submit <id> -q <0-5>` | Record submission with self-rating |
| `shuati review` | Show problems due for review |
| `shuati next` | Get recommendation on what to practice |
| `shuati hint <id> -f <file>` | Get AI coaching hint (not the answer!) |
| `shuati stats` | View mistake statistics |
| `shuati config --api-key <key>` | Set DeepSeek API key |

## Quick Start

```bash
# Initialize in your practice folder
shuati init

# Set your DeepSeek API key
shuati config --api-key sk-xxx

# Pull a problem
shuati pull https://leetcode.com/problems/two-sum/

# After solving, record your attempt
shuati submit web_xxx -q 3

# Get AI hint if stuck
shuati hint web_xxx -f solution.cpp

# Check what to review
shuati next
```

## Core Philosophy

- **Coach, not answer machine** — AI gives hints, never full solutions
- **Mistake-driven learning** — Track and eliminate recurring errors
- **Spaced repetition (SM-2)** — Review at optimal intervals
- **Low token cost** — Each AI call < 300 tokens

## Build

```bash
# Prerequisites: C++20 compiler, CMake 3.20+, vcpkg

# Clone and bootstrap vcpkg
git clone --depth 1 https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.bat

# Install dependencies
./vcpkg/vcpkg.exe install

# Build
cmake -B build -S . -G "Visual Studio 17 2022" \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Tech Stack

- **C++20** / CMake / vcpkg
- **CLI11** — Argument parsing
- **SQLiteCpp** — Local database
- **cpr** — HTTP client
- **nlohmann/json** — JSON handling
- **fmt** — Formatting
- **DeepSeek API** — AI coaching

## License

MIT
