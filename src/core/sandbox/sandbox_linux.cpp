#ifndef _WIN32
#include "shuati/sandbox.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace shuati {
namespace sandbox {

class LinuxSandbox : public ISandbox {
private:
    bool has_bwrap() const {
        return system("which bwrap > /dev/null 2>&1") == 0;
    }

public:
    LinuxSandbox() = default;
    ~LinuxSandbox() override = default;

    SandboxResult execute(
        const std::string& executable_path,
        const std::vector<std::string>& args,
        const std::string& input_file,
        const std::string& output_file,
        const std::string& error_file,
        const SandboxLimits& limits
    ) override {
        SandboxResult result;
        result.status = SandboxResultStatus::InternalError;
        result.exit_code = -1;
        result.cpu_time_ms = 0;
        result.memory_mb = 0;

        bool use_bwrap = has_bwrap();

        // Check executable exists
        struct stat st;
        if (stat(executable_path.c_str(), &st) != 0) {
            result.internal_message = "Executable not found: " + executable_path;
            return result;
        }

        pid_t pid = fork();
        if (pid < 0) {
            result.internal_message = "Fork failed";
            return result;
        }

        if (pid == 0) {
            // Child process
            setpgid(0, 0); // Create new process group to enable mass kill

            // I/O Redirection
            if (!input_file.empty()) {
                int fd_in = open(input_file.c_str(), O_RDONLY);
                if (fd_in != -1) { dup2(fd_in, STDIN_FILENO); close(fd_in); }
            }
            if (!output_file.empty()) {
                int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out != -1) { dup2(fd_out, STDOUT_FILENO); close(fd_out); }
            }
            if (!error_file.empty()) {
                int fd_err = open(error_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_err != -1) { dup2(fd_err, STDERR_FILENO); close(fd_err); }
            }

            // Set basic resource limits (CPU time and Virtual Memory)
            // Memory Limit
            if (limits.memory_mb > 0) {
                struct rlimit rl_mem;
                rl_mem.rlim_cur = limits.memory_mb * 1024 * 1024;
                rl_mem.rlim_max = rl_mem.rlim_cur;
                setrlimit(RLIMIT_AS, &rl_mem); // Address space limit
            }

            // Time Limit
            if (limits.cpu_time_ms > 0) {
                struct rlimit rl_cpu;
                rlim_t cpu_sec = (limits.cpu_time_ms + 1999) / 1000; // ceil seconds
                rl_cpu.rlim_cur = cpu_sec;
                rl_cpu.rlim_max = cpu_sec;
                setrlimit(RLIMIT_CPU, &rl_cpu);
            }

            // Prepare arguments
            std::vector<const char*> c_args;
            if (use_bwrap) {
                c_args.push_back("bwrap");
                c_args.push_back("--ro-bind"); c_args.push_back("/usr"); c_args.push_back("/usr");
                c_args.push_back("--ro-bind"); c_args.push_back("/lib"); c_args.push_back("/lib");
                c_args.push_back("--ro-bind"); c_args.push_back("/lib64"); c_args.push_back("/lib64");
                c_args.push_back("--ro-bind"); c_args.push_back("/bin"); c_args.push_back("/bin");
                c_args.push_back("--proc"); c_args.push_back("/proc");
                c_args.push_back("--dev"); c_args.push_back("/dev");
                c_args.push_back("--unshare-net"); // Disable network
                c_args.push_back("--unshare-pid"); // Isolate PIDs
                // Bind current dir
                char cwd[1024];
                if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                    c_args.push_back("--bind"); c_args.push_back(cwd); c_args.push_back(cwd);
                }
                c_args.push_back("--"); // End of bwrap args
            }

            c_args.push_back(executable_path.c_str());
            for (const auto& a : args) {
                c_args.push_back(a.c_str());
            }
            c_args.push_back(nullptr);

            if (use_bwrap) {
                execvp("bwrap", const_cast<char* const*>(c_args.data()));
            } else {
                execv(executable_path.c_str(), const_cast<char* const*>(c_args.data()));
            }
            
            // If exec fails
            exit(127);
        } else {
            // Parent process
            auto start_time = std::chrono::steady_clock::now();
            int wstatus;
            struct rusage usage;

            while (true) {
                int ret = wait4(pid, &wstatus, WNOHANG, &usage);
                if (ret == pid) {
                    // Process exited
                    result.cpu_time_ms = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000 +
                                         usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
                    result.memory_mb = usage.ru_maxrss / 1024; // ru_maxrss is in KB on Linux

                    if (WIFEXITED(wstatus)) {
                        result.exit_code = WEXITSTATUS(wstatus);
                        if (result.exit_code == 0) {
                            result.status = SandboxResultStatus::OK;
                        } else {
                            result.status = SandboxResultStatus::RuntimeError;
                        }
                    } else if (WIFSIGNALED(wstatus)) {
                        int sig = WTERMSIG(wstatus);
                        result.exit_code = 128 + sig;
                        if (sig == SIGXCPU || sig == SIGKILL) {
                            // SIGXCPU triggered by RLIMIT_CPU.
                            // SIGKILL often triggered by OOM killer or us manually killing.
                            // We need to distinguish based on elapsed time vs limit.
                            auto now = std::chrono::steady_clock::now();
                            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                            if (elapsed_ms >= limits.cpu_time_ms) {
                                result.status = SandboxResultStatus::TimeLimitExceeded;
                            } else {
                                // Likely OOM or hard crash
                                result.status = SandboxResultStatus::RuntimeError;
                                int threshold = limits.memory_mb - (limits.memory_mb / 10);
                                if (threshold < limits.memory_mb - 5) threshold = limits.memory_mb - 5;
                                if (result.memory_mb >= threshold) {
                                     result.status = SandboxResultStatus::MemoryLimitExceeded;
                                }
                            }
                        } else if (sig == SIGSEGV || sig == SIGABRT) {
                            result.status = SandboxResultStatus::RuntimeError;
                            // Check for MLE disguised as SEGFAULT
                            int threshold = limits.memory_mb - (limits.memory_mb / 10);
                            if (threshold < limits.memory_mb - 5) threshold = limits.memory_mb - 5;
                            if (result.memory_mb >= threshold) {
                                result.status = SandboxResultStatus::MemoryLimitExceeded;
                            }
                        } else {
                            result.status = SandboxResultStatus::RuntimeError;
                        }
                    }
                    break;
                } else if (ret == 0) {
                    // Still running, check time
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                    
                    if (elapsed_ms > limits.cpu_time_ms + 100) { // Slight padding
                        killpg(pid, SIGKILL); // Kill entire process group
                        result.status = SandboxResultStatus::TimeLimitExceeded;
                        result.cpu_time_ms = limits.cpu_time_ms;
                        waitpid(pid, &wstatus, 0); // Cleanup zombie
                        break;
                    }

                    // Poll every 50ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } else {
                    result.internal_message = "wait4 failed";
                    break;
                }
            }

            // Cleanup any stray processes in the group just in case
            killpg(pid, SIGKILL);
        }

        return result;
    }
};

std::unique_ptr<ISandbox> create_sandbox() {
    return std::make_unique<LinuxSandbox>();
}

} // namespace sandbox
} // namespace shuati
#endif // !_WIN32
