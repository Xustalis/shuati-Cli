<p align="center">
  <img src="https://raw.githubusercontent.com/yhy/shuati-cli/master/docs/logo.png" alt="Shuati CLI Logo" width="200">
</p>

<h1 align="center">Shuati CLI (刷题 CLI)</h1>

<p align="center">
  <strong>A hacker's way to ace algorithm contests.</strong>
</p>

<p align="center">
  <a href="https://github.com/yhy/shuati-cli/actions"><img src="https://img.shields.io/github/actions/workflow/status/yhy/shuati-cli/build.yml?style=flat-square" alt="Build Status"></a>
  <a href="https://github.com/yhy/shuati-cli/releases"><img src="https://img.shields.io/github/v/release/yhy/shuati-cli?style=flat-square" alt="Release"></a>
  <a href="https://github.com/yhy/shuati-cli/blob/master/LICENSE"><img src="https://img.shields.io/github/license/yhy/shuati-cli?style=flat-square" alt="License"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square" alt="C++20">
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#usage">Usage</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#contributing">Contributing</a>
</p>

---

**Shuati CLI** is a powerful, cross-platform command-line tool designed for competitive programmers and algorithm enthusiasts. It integrates problem crawling, local coding, testing, and AI coaching into a seamless workflow.

## ✨ Features

- **Polyglot Crawler**: Automatically fetch problems from LeetCode, Codeforces, Luogu, and Lanqiao.
- **Local Judge**: Compile and test your code locally with customizable test cases.
- **AI Coach**: Built-in AI assistant to provide non-spoiler hints, code analysis, and template generation.
- **Smart Workflow**: One command to pull, generate template, open editor, and test.
- **REPL Mode**: Interactive shell with autocompletion and history for efficient usage.
- **Memory Curve**: Spaced repetition tracking to help you review problems at optimal intervals.

## 🚀 Installation

### Windows (Pre-built)
Download the latest `.exe` from the [Releases](https://github.com/yhy/shuati-cli/releases) page and add it to your PATH.

### Build from Source

Requirements:
- CMake 3.20+
- C++20 Compiler (MSVC, GCC, Clang)
- `vcpkg` (optional, for dependency management)

```bash
git clone https://github.com/yhy/shuati-cli.git
cd shuati-cli
cmake -B build
cmake --build build --config Release
```

## 📖 Usage

### Interactive Mode (Recommended)
Simply type `shuati` to enter the interactive REPL.

```bash
$ shuati
shuati> help
```

### Basic Commands

1.  **Pull a problem**:
    ```bash
    shuati pull https://leetcode.com/problems/two-sum/
    ```

2.  **Start solving** (Generates file, opens editor, fetches API template):
    ```bash
    shuati solve 1
    ```

3.  **Run tests**:
    ```bash
    shuati test 1
    ```

4.  **Get AI Hint**:
    ```bash
    shuati hint 1
    ```

5.  **Submit & Record**:
    ```bash
    shuati submit 1
    ```

## ⚙️ Configuration

Global config is stored in `~/.shuati/config.json`.

```bash
shuati config --api-key "sk-..." --model "deepseek-coder"
```

| Key | Description | Default |
| :--- | :--- | :--- |
| `api_key` | OpenAI-compatible API Key | "" |
| `api_base` | API Base URL | "https://api.deepseek.com/v1" |
| `editor` | Command to open your editor | "code" |
| `language` | Preferred language | "cpp" |

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## 📄 License

MIT © [YHY](https://github.com/yhy)
