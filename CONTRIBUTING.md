# Contributing to Shuati CLI

Thank you for your interest in contributing to Shuati CLI! We welcome contributions from everyone.

## Development Setup

1.  **Fork** the repository on GitHub.
2.  **Clone** your fork locally.
3.  **Install Dependencies**: We use `vcpkg` (manifest mode).
4.  **Build**:
    ```bash
    cmake -B build
    cmake --build build
    ```

## Code Style

- We use **C++20** standard.
- Follow standard C++ naming conventions (snake_case for variables/functions, PascalCase for classes).
- Use `fmt::print` instead of `std::cout` for formatted output.

## Pull Requests

1.  Create a new branch for your feature or fix: `git checkout -b feat/my-feature`.
2.  Commit your changes with clear messages.
3.  Push to your fork and submit a Pull Request.
4.  Ensure all tests pass (if applicable).

## Conventional Commits

Release automation relies on Conventional Commits:

- `feat: ...` / `feat(scope): ...` for features
- `fix: ...` / `fix(scope): ...` for bug fixes
- Add `!` or a `BREAKING CHANGE:` footer for breaking changes

Examples:

- `feat(test): add boundary case generation`
- `fix(list): prevent invalid utf8 crash`
- `feat!: change config format`

## Reporting Issues

If you find a bug or have a feature request, please open an [Issue](https://github.com/yhy/shuati-cli/issues).
