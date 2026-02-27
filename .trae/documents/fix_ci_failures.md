# Fix CI Failures Plan

This plan addresses two main issues in the CI pipeline: `test_judge_complex` failure due to truncated output returning 0 bytes, and Windows checksum failure due to directory hashing.

## 1. Fix `test_judge_complex` (src/core/judge.cpp)

The goal is to ensure `test_judge_complex` passes on all platforms (Windows, macOS, Linux) by reliably capturing output even when truncated or when process execution fails.

### Requirements
- **Concurrent Drain**: Read stdout and stderr simultaneously to prevent deadlocks.
- **Truncation Logic**: Keep the first N bytes (e.g., 4MB) when output exceeds the limit; never clear the buffer.
- **Error Propagation**: If `exec` or process creation fails, the error message must be present in the `output` field (not just `error_output`) to satisfy test assertions.
- **Platform Compatibility**: Ensure `poll` is used correctly on Linux/macOS and `ReadFile` threads on Windows.

### Implementation Details

#### Windows (`#ifdef _WIN32`)
- **Process Creation Failure**:
  - If `CreateProcessA` fails, construct a `JudgeResult` where `output` contains the error message (e.g., "CreateProcess failed: ...").
  - Current implementation returns `{Verdict::RE, ...}` but `output` is empty. Change to populate `output`.
- **Output Capturing**:
  - Verify `reader_out` and `reader_err` threads correctly append to `output`/`error_output` up to `MAX_CAPTURE`.
  - Ensure no logic clears `output` on truncation.

#### Linux/macOS (`#else`)
- **Headers**: Verify `#include <poll.h>` is strictly within `#ifndef _WIN32` blocks.
- **Process Execution**:
  - In the child process, if `execvp` fails (returns), write the `errno` to stderr and exit with code 127.
- **Output Capturing (Parent Process)**:
  - Use `poll()` to monitor `pipe_out` and `pipe_err`.
  - **Loop Condition**: Continue loop until both stdout and stderr pipes are closed (`POLLHUP` or `read` returns 0).
  - **Truncation**: If `output.size() >= MAX_CAPTURE`, continue reading from pipe (to drain it) but stop appending to `output`.
  - **Exec Failure Handling**: After the process finishes, if `exit_code == 127` (command not found/exec failed) and `output` is empty, append the content of `error_output` (which contains the exec error from child) to `output`. This ensures `res.output` is not empty, satisfying the "Got 0 bytes" check in tests.

## 2. Fix Windows Checksum (ci.yml)

The goal is to fix the `sha256sum: resources: Is a directory` error on Windows.

### Implementation Details
- Modify the `Generate Checksum` step in `.github/workflows/ci.yml`.
- **Action**: Before running `sha256sum`, check if `resources` is a directory.
- **Packaging**: If it is, package it into `resources.tar.gz` (using `tar`, available in Git Bash) and remove the original directory.
- **Hashing**: Run `sha256sum * > checksums.sha256`. Since `resources` is now a file, this will succeed.

## 3. Verification
- **Local**:
  - Compile and run `ctest --test-dir build -C Release --output-on-failure -VV` on Windows.
  - Verify `test_judge_complex` passes and captures > 0 bytes in the deadlock check.
- **CI (Simulated)**:
  - The changes to `ci.yml` will be tested when pushed.

## Todo List
- [ ] Modify `src/core/judge.cpp`:
    - [ ] Update Windows `CreateProcess` failure return to include error in `output`.
    - [ ] Update Linux `poll` loop to robustly handle EOF and truncation.
    - [ ] Update Linux `exec` failure handling to propagate stderr to `output` if needed.
- [ ] Modify `.github/workflows/ci.yml`:
    - [ ] Update `Generate Checksum` step to package `resources` directory before hashing.
