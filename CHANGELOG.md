# Changelog

All notable changes to this project will be documented in this file.

## [1.5.0] - 2026-02-14

### 新功能
- Add GitHub Actions workflow for automated multi-platform builds and releases. (34e6791)

### 修复
- make bump_readme_badge graceful when badge missing, use CRLF-safe _read() everywhere (6f44e72)
- fix InnoSetup paths, output dir mismatch, and CRLF regex issue (c940c8e)

### 其他
- update README (3a08811)
- rewrite README with comprehensive documentation (3ab9d5c)

**对比**: https://github.com/Xustalis/shuati-Cli/compare/v1.4.4...v1.5.0

**贡献者**: Xustalis

## [1.4.4] - 2026-02-14

### 修复
- add vcpkg DLLs to PATH for Windows verification (e48d9fd)

**对比**: https://github.com/Xustalis/shuati-Cli/compare/v1.4.3...v1.4.4

**贡献者**: Xustalis

## [1.4.3] - 2026-02-14

### 修复
- fix indentation and robustify version extraction script (9bc0363)

### 其他
- Establish GitHub Actions for continuous integration, multi-platform builds, and automated releases. (6eb704b)
- Fix installer environment variable handling and update workflow tools (dfc00dc)
- feat-installer-autodownload (de9cca6)
- Merge branch 'main' of https://github.com/Xustalis/shuati-Cli (f5b79fb)

**对比**: https://github.com/Xustalis/shuati-Cli/compare/v1.4.2...v1.4.3

**贡献者**: Xustalis

## [1.4.2] - 2026-02-13

### 修复
- explicitly trigger release workflow from prepare-release (3990e28)
- remove redundant release job to avoid conflicts with release.yml (2330c64)

**对比**: https://github.com/Xustalis/shuati-Cli/compare/v1.4.1...v1.4.2

**贡献者**: Xustalis

## [1.4.1] - 2026-02-13

### Optimized
- **MemoryManager**: Improved performance of mastery updates from O(N) to O(1) using database indexing.

### Fixed
- **CI/CD**: 
  - Fixed `vcpkg` integration on Linux and macOS by correcting `VCPKG_ROOT` environment variable usage.
  - Added automated SHA256 checksum generation for release artifacts.
  - Enabled multi-platform binary releases (Windows, Linux, macOS).

## [1.4.0] - 2026-02-13

### Added
- **User Memory System**: 
  - Tracks recurring mistakes (`memory_mistakes`) and mastered skills (`memory_mastery`).
  - `AICoach` actively injects relevant user history into the context (RAG-Lite).
  - Auto-updates memory base from AI responses without user intervention.
- **Environment Doctor**: `shuati info` now checks for missing `g++` or `python` and warns users.
- **Fuzzy Judge**: `check_output` now ignores trailing whitespace and empty lines for more robust judging.

### Fixed
- **CI/CD**: Fixed build failures on Linux/macOS by conditional linking of `Psapi`.
- **Judge**: Fixed potential TLE/deadlock on Windows by using threaded pipe reading.

## [1.3.1] - 2026-02-12

### Fixed
- `list`: prevent SQLite malformed text from throwing `invalid utf8` and crashing.

## [1.3.0] - 2026-02-12

### Added
- Enhanced `test`: auto-generated boundary/normal/combo test points with optional AI oracle.
- Test report: pass rate, parameter coverage, and per-case resource stats.
- Interactive test viewer (`test --ui`) for input/output details.

### Changed
- CI/CD: faster multi-environment builds, artifact verification, multi-OS release assets, and checksums.
- `solve`: generate minimal code skeleton instead of bloated templates.

### Fixed
- `list`: invalid UTF-8 output crash on Windows terminals.
- `test`: compile standard fallback for older g++ versions.

## [1.2.0] - 2024-02-11

### Added
- **Multi-Platform Crawler**: Added support for LeetCode (GraphQL), Codeforces, Luogu, and Lanqiao.
- **Local Judge Engine**:
  - Implemented sandbox execution for C++ and Python.
  - Added verdicts: AC, WA, TLE, MLE (Windows), RE, CE.
  - Added memory usage monitoring using Windows PSAPI.
- **AI Coach**:
  - Integrated `debug` command for AI-powered error diagnosis.
  - Added smart template generation with `solve` command.
- **CLI Improvements**:
  - Added interactive REPL mode (`run_repl`).
  - Added `init` command for project scaffolding.
  - Added `test` and `add-case` commands for local testing.

### Changed
- **Architecture**: Refactored codebase into Domain-Driven Design (DDD) layers (`core`, `infra`, `adapters`).
- **Build System**: optimized `CMakeLists.txt` and `vcpkg.json` for better dependency management.
- **Documentation**: Rewrote README to professional standards; added Contribution Guide and CI workflows.

### Fixed
- Fixed silent crash issue caused by missing DLL dependencies.
- Fixed `run_repl` crash by adding proper exception handling and initialization.
