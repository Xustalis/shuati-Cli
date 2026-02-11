# Changelog

All notable changes to this project will be documented in this file.

## [v3.0.0] - 2026-02-11

### 🚀 New Features

- **Polyglot Crawler**: Added support for LeetCode, Codeforces, Luogu, and Lanqiao.
- **Local Judge**: Implemented a standalone judge engine with AC, WA, TLE, MLE, RE, CE verdicts.
- **AI Coach**: Integrated AI for hints, code analysis, and smart template generation.
- **Workflow**: Auto-open editor, `test` command, `add-case` command.
- **REPL**: Enhanced interactive shell with autocompletion for commands and problem IDs.
- **Companion Server**: Support for competitive-companion browser extension.

### 🛠 Improvements

- **Architecture**: Refactored to Domain-Driven Design (DDD) directory structure.
- **Config**: Enhanced configuration system for user preferences.
- **Dependencies**: Migrated to `vcpkg` manifest mode.
- **Templates**: Added support for customizable templates with `{{DATE}}`, `{{TAGS}}` placeholders.

### 🐛 Fixes

- Fixed build issues with `fmt` library.
- Resolved service lifetime issues in REPL mode.
- Fixed UI text encoding for Windows console.
