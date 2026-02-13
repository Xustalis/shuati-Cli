# Changelog

All notable changes to this project will be documented in this file.

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
