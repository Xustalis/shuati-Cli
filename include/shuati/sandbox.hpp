#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace shuati {
namespace sandbox {

enum class SandboxResultStatus {
    OK,                 // Execution completed normally
    TimeLimitExceeded,  // Execution exceeded time limit
    MemoryLimitExceeded,// Execution exceeded memory limit
    RuntimeError,       // Execution crashed/exited with non-zero
    InternalError       // Sandbox failed to initialize or monitor process
};

struct SandboxLimits {
    long long cpu_time_ms; // CPU time limit in milliseconds
    long long memory_mb;   // Memory limit in megabytes
};

struct SandboxResult {
    SandboxResultStatus status;
    int exit_code;
    long long cpu_time_ms;
    long long memory_mb;
    std::string internal_message; // Error details if InternalError
};

/**
 * Interface for isolated process execution.
 */
class ISandbox {
public:
    virtual ~ISandbox() = default;

    /**
     * Executes the given executable in an isolated environment.
     * 
     * @param executable_path Full path to the compiled binary.
     * @param args Command line arguments (excluding the executable itself).
     * @param input_file Path to standard input file.
     * @param output_file Path to standard output file.
     * @param error_file Path to standard error file.
     * @param limits Resource limits (memory, time).
     * @return Result containing execution stats and status.
     */
    virtual SandboxResult execute(
        const std::string& executable_path,
        const std::vector<std::string>& args,
        const std::string& input_file,
        const std::string& output_file,
        const std::string& error_file,
        const SandboxLimits& limits
    ) = 0;
};

// Factory method to create the appropriate sandbox instance for the current OS.
std::unique_ptr<ISandbox> create_sandbox();

} // namespace sandbox
} // namespace shuati
