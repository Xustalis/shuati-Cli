#include "shuati/judge.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>
#include <system_error>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>
#endif

#include <random>

namespace shuati {

namespace fs = std::filesystem;

// RAII helper for temporary files
class TempFile {
public:
    TempFile(const std::string& extension = ".tmp") {
        auto tmp = fs::temp_directory_path();
        
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<long long> dist(0, 1000000000);
        
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        path_ = tmp / fmt::format("shuati_{}_{}{}", now, dist(rng), extension);
    }
    
    ~TempFile() {
        if (fs::exists(path_)) {
            try { fs::remove(path_); } catch(...) {}
        }
    }

    std::string path() const { return path_.string(); }
    
    void write(const std::string& content) {
        std::ofstream out(path_);
        out << content;
    }

private:
    fs::path path_;
};

// Helper to remove trailing whitespace
std::string trim_right(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string read_text_file(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}



std::string Judge::prepare(const std::string& source_file, const std::string& language) {
    return compile(source_file, language);
}

JudgeResult Judge::run_prepared(const std::string& executable,
                                const TestCase& tc,
                                int time_limit_ms,
                                int memory_limit_kb) {
    return run_case(executable, tc, time_limit_ms, memory_limit_kb);
}

void Judge::cleanup_prepared(const std::string& executable, const std::string& language) {
    if (language == "cpp" || language == "c++") {
        if (fs::exists(executable)) fs::remove(executable);
    }
}

std::string Judge::compile(const std::string& source_file, const std::string& language) {
    if (language == "python" || language == "py") {
        return "python \"" + source_file + "\""; 
    }

    if (language == "cpp" || language == "c++") {
        // Security check: Ensure source_file contains only safe characters
        if (source_file.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/\\") != std::string::npos) {
            throw std::runtime_error("Invalid source file path: " + source_file);
        }

        fs::path src(source_file);
        std::string exe = src.replace_extension(".exe").string();
        
#ifdef _WIN32
        const char* null_dev = "nul";
#else
        const char* null_dev = "/dev/null";
#endif

        const std::vector<std::string> std_flags = {
            "-std=c++20",
            "-std=gnu++20",
            "-std=c++2a",
            "-std=gnu++2a",
            "-std=c++17",
            "-std=gnu++17",
        };

        std::string last_error;
        for (const auto& std_flag : std_flags) {
            TempFile err(".txt");
            std::string cmd = fmt::format(
                "g++ -O2 {} \"{}\" -o \"{}\" 1>{} 2>\"{}\"",
                std_flag, source_file, exe, null_dev, err.path()
            );

            if (fs::exists(exe)) {
                try { fs::remove(exe); } catch(...) {}
            }

            int ret = std::system(cmd.c_str());
            if (ret == 0 && fs::exists(exe)) {
                return exe;
            }

            std::string err_text = read_text_file(err.path());
            if (err_text.empty()) {
                last_error = "Compilation failed";
                continue;
            }

            last_error = err_text;
            if (err_text.find("unrecognized command line option") != std::string::npos &&
                err_text.find(std_flag) != std::string::npos) {
                continue;
            }
            if (err_text.find("invalid argument") != std::string::npos &&
                err_text.find(std_flag) != std::string::npos) {
                continue;
            }

            throw std::runtime_error(err_text);
        }

        throw std::runtime_error(last_error.empty() ? "Compilation failed" : last_error);
    }

    throw std::runtime_error("Unsupported language: " + language);
}

Verdict Judge::check_output(const std::string& user_out, const std::string& expected_out) {
    // Fuzzy compare: Token-based
    std::istringstream iss_u(user_out);
    std::istringstream iss_e(expected_out);
    
    std::string tok_u, tok_e;
    bool has_u = (bool)(iss_u >> tok_u);
    bool has_e = (bool)(iss_e >> tok_e);
    
    while (has_u && has_e) {
        if (tok_u != tok_e) return Verdict::WA;
        has_u = (bool)(iss_u >> tok_u);
        has_e = (bool)(iss_e >> tok_e);
    }
    
    // Both must be exhausted (no extra tokens)
    if (has_u || has_e) return Verdict::WA;
    
    return Verdict::AC;
}

std::vector<JudgeResult> Judge::judge(const std::string& source_file, 
                                      const std::string& language, 
                                      const std::vector<TestCase>& test_cases,
                                      int time_limit_ms,
                                      int memory_limit_kb) {
    std::vector<JudgeResult> results;
    
    // 1. Compile
    std::string executable;
    try {
        executable = prepare(source_file, language);
    } catch (const std::exception& e) {
        JudgeResult r;
        r.verdict = Verdict::CE;
        r.message = e.what();
        return {r};
    }

    // 2. Run cases
    for (const auto& tc : test_cases) {
        auto res = run_prepared(executable, tc, time_limit_ms, memory_limit_kb);
        results.push_back(res);
    }

    // Cleanup executable
    cleanup_prepared(executable, language);

    return results;
}

#ifdef _WIN32
// RAII Wrapper for HANDLE
class ScopedHandle {
public:
    ScopedHandle() : h_(NULL) {}
    ScopedHandle(HANDLE h) : h_(h) {}
    ~ScopedHandle() { close(); }

    void close() {
        if (h_ != NULL && h_ != INVALID_HANDLE_VALUE) {
            CloseHandle(h_);
            h_ = NULL;
        }
    }

    void reset(HANDLE h) {
        close();
        h_ = h;
    }

    HANDLE get() const { return h_; }
    HANDLE* receive() { return &h_; } // For output parameters

    // Non-copyable
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    // Moveable
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = NULL; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            close();
            h_ = other.h_;
            other.h_ = NULL;
        }
        return *this;
    }

private:
    HANDLE h_;
};

JudgeResult Judge::run_case(const std::string& executable, 
                            const TestCase& tc, 
                            int time_limit_ms, 
                            int memory_limit_kb) {
    JudgeResult res;
    res.input = tc.input;
    res.expected = tc.output;

    std::string cmd_line;
    if (executable.find("python") == 0) {
        cmd_line = executable;
    } else {
        cmd_line = "\"" + executable + "\"";
    }

    // Pipes
    ScopedHandle hChildStd_IN_Rd, hChildStd_IN_Wr;
    ScopedHandle hChildStd_OUT_Rd, hChildStd_OUT_Wr;
    ScopedHandle hChildStd_ERR_Rd, hChildStd_ERR_Wr;

    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    if (!CreatePipe(hChildStd_OUT_Rd.receive(), hChildStd_OUT_Wr.receive(), &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_OUT_Rd.get(), HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};
    
    if (!CreatePipe(hChildStd_ERR_Rd.receive(), hChildStd_ERR_Wr.receive(), &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_ERR_Rd.get(), HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};

    if (!CreatePipe(hChildStd_IN_Rd.receive(), hChildStd_IN_Wr.receive(), &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_IN_Wr.get(), HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_ERR_Wr.get();
    si.hStdOutput = hChildStd_OUT_Wr.get();
    si.hStdInput = hChildStd_IN_Rd.get();
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&pi, sizeof(pi));

    auto start_time = std::chrono::high_resolution_clock::now();

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_line.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::string msg = fmt::format("CreateProcess failed: {}", GetLastError());
        return {Verdict::RE, 0, 0, msg, "", "", msg, ""};
    }
    
    // Automatically close process handles when function returns
    ScopedHandle hProcess(pi.hProcess);
    ScopedHandle hThread(pi.hThread);

    // Close handles intended for child in parent
    hChildStd_OUT_Wr.close();
    hChildStd_ERR_Wr.close();
    hChildStd_IN_Rd.close();

    // Write input in a separate thread
    std::thread writer([h = std::move(hChildStd_IN_Wr), data = tc.input]() mutable {
        if (!data.empty()) {
            DWORD dwWritten;
            WriteFile(h.get(), data.c_str(), (DWORD)data.size(), &dwWritten, NULL);
        }
        h.close(); 
    });

    std::string output, error_output;
    const size_t MAX_CAPTURE = 4 * 1024 * 1024; // 4MB limit

    // Drain stdout
    std::thread reader_out([&]() {
        char buffer[4096];
        DWORD dwRead;
        while (ReadFile(hChildStd_OUT_Rd.get(), buffer, sizeof(buffer), &dwRead, NULL) && dwRead > 0) {
            if (output.size() < MAX_CAPTURE) {
                size_t to_copy = std::min((size_t)dwRead, MAX_CAPTURE - output.size());
                output.append(buffer, to_copy);
            }
        }
    });

    // Drain stderr
    std::thread reader_err([&]() {
        char buffer[4096];
        DWORD dwRead;
        while (ReadFile(hChildStd_ERR_Rd.get(), buffer, sizeof(buffer), &dwRead, NULL) && dwRead > 0) {
             if (error_output.size() < MAX_CAPTURE) {
                size_t to_copy = std::min((size_t)dwRead, MAX_CAPTURE - error_output.size());
                error_output.append(buffer, to_copy);
            }
        }
    });

    bool finished = false;
    DWORD exit_code = 0;
    
    if (WaitForSingleObject(hProcess.get(), time_limit_ms) == WAIT_OBJECT_0) {
        finished = true;
        GetExitCodeProcess(hProcess.get(), &exit_code);
    } else {
        TerminateProcess(hProcess.get(), 1);
        finished = false;
    }

    // Wait for IO threads to finish (handles closed by child exit or termination)
    if (writer.joinable()) writer.join();
    if (reader_out.joinable()) reader_out.join();
    if (reader_err.joinable()) reader_err.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    res.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (!finished) {
        res.verdict = Verdict::TLE;
    } else {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess.get(), &pmc, sizeof(pmc))) {
             res.memory_kb = pmc.PeakPagefileUsage / 1024;
             if (res.memory_kb > memory_limit_kb) res.verdict = Verdict::MLE;
        }

        if (res.verdict != Verdict::MLE) {
             res.output = output;
             res.error_output = error_output;
             
             if (exit_code != 0) {
                res.verdict = Verdict::RE;
                res.message = fmt::format("Exit code: {}", exit_code);
            } else {
                res.verdict = Verdict::AC;
                res.verdict = check_output(output, tc.output);
            }
        }
    }
    
    return res;
}
#else
// Linux Implementation
JudgeResult Judge::run_case(const std::string& executable, 
                            const TestCase& tc, 
                            int time_limit_ms, 
                            int memory_limit_kb) {
    JudgeResult res;
    res.input = tc.input;
    res.expected = tc.output;

    int pipe_in[2], pipe_out[2], pipe_err[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0 || pipe(pipe_err) < 0) {
        return {Verdict::SE, 0, 0, "Pipe creation failed"};
    }

    auto start_time = std::chrono::steady_clock::now();
    pid_t pid = fork();

    if (pid < 0) {
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        close(pipe_err[0]); close(pipe_err[1]);
        return {Verdict::SE, 0, 0, "Fork failed"};
    } else if (pid == 0) {
        // Child
        // Redirect stdin
        if (dup2(pipe_in[0], STDIN_FILENO) == -1) exit(1);
        
        // Redirect stdout/stderr
        if (dup2(pipe_out[1], STDOUT_FILENO) == -1) exit(1);
        if (dup2(pipe_err[1], STDERR_FILENO) == -1) exit(1);

        // Close all pipe fds
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        close(pipe_err[0]); close(pipe_err[1]);

        // Execute
        std::vector<std::string> args_vec;
        if (executable.find("python") == 0) {
            args_vec.push_back("python3");
            std::string script = executable.substr(7); 
             if (script.front() == '"') script = script.substr(1, script.length()-2);
            args_vec.push_back(script);
        } else {
            args_vec.push_back(executable);
        }

        std::vector<char*> args;
        for(auto& s : args_vec) args.push_back(&s[0]);
        args.push_back(nullptr);

        execvp(args[0], args.data());
        
        // If exec fails, print to stderr so parent can capture it
        std::cerr << "Exec failed: " << errno << std::endl;
        exit(127); 
    } else {
        // Parent
        close(pipe_in[0]);
        close(pipe_out[1]);
        close(pipe_err[1]);

        // Set non-blocking read
        fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
        fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);

        // Write input (blocking write for simplicity, assuming small input or pipe buffer enough)
        // If input is large, this should be non-blocking too, but to keep it simple and focus on output drain:
        if (!tc.input.empty()) {
            size_t total_written = 0;
            while (total_written < tc.input.size()) {
                ssize_t w = write(pipe_in[1], tc.input.c_str() + total_written, tc.input.size() - total_written);
                if (w < 0) break;
                total_written += w;
            }
        }
        close(pipe_in[1]); 

        std::string output, error_output;
        const size_t MAX_CAPTURE = 4 * 1024 * 1024; // 4MB

        struct pollfd pfd[2];
        pfd[0].fd = pipe_out[0]; pfd[0].events = POLLIN;
        pfd[1].fd = pipe_err[0]; pfd[1].events = POLLIN;

        bool finished = false;
        int status = 0;
        long long max_memory_kb = 0;

        // Loop until both pipes are closed (EOF)
        bool out_closed = false;
        bool err_closed = false;

        while (!out_closed || !err_closed) {
            // Timeout check
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > time_limit_ms) {
                break;
            }

            int ret = poll(pfd, 2, 10); // 10ms timeout
            if (ret < 0 && errno != EINTR) break;

            // Read stdout
            if (!out_closed) {
                if (pfd[0].revents & (POLLIN | POLLHUP | POLLERR)) {
                    char buffer[4096];
                    ssize_t n = read(pipe_out[0], buffer, sizeof(buffer));
                    if (n > 0) {
                        if (output.size() < MAX_CAPTURE) {
                            output.append(buffer, std::min((size_t)n, MAX_CAPTURE - output.size()));
                        }
                    } else if (n == 0) {
                        out_closed = true;
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                         out_closed = true;
                    }
                }
            }

            // Read stderr
            if (!err_closed) {
                if (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) {
                    char buffer[4096];
                    ssize_t n = read(pipe_err[0], buffer, sizeof(buffer));
                    if (n > 0) {
                        if (error_output.size() < MAX_CAPTURE) {
                            error_output.append(buffer, std::min((size_t)n, MAX_CAPTURE - error_output.size()));
                        }
                    } else if (n == 0) {
                        err_closed = true;
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                         err_closed = true;
                    }
                }
            }

            // Check memory usage periodically
             std::string status_path = fmt::format("/proc/{}/status", pid);
            if (fs::exists(status_path)) {
                std::ifstream f(status_path);
                std::string line;
                while (std::getline(f, line)) {
                    if (line.compare(0, 6, "VmPeak") == 0) {
                         std::stringstream ss(line);
                         std::string label, unit;
                         long long val;
                         ss >> label >> val >> unit;
                         if (val > max_memory_kb) max_memory_kb = val;
                         break;
                    }
                }
            }
             if (max_memory_kb > memory_limit_kb) {
                 kill(pid, SIGKILL);
                 res.verdict = Verdict::MLE;
                 res.memory_kb = max_memory_kb;
                 waitpid(pid, &status, 0);
                 finished = true; 
                 out_closed = true; err_closed = true;
            }
        }
        
        close(pipe_out[0]);
        close(pipe_err[0]);

        if (res.verdict == Verdict::MLE) {
            // Already handled
        } else {
             // Check if process finished
             int w = waitpid(pid, &status, WNOHANG);
             if (w == 0) {
                 // Still running, so it must be TLE
                 kill(pid, SIGKILL);
                 waitpid(pid, &status, 0);
                 res.verdict = Verdict::TLE;
                 res.time_ms = time_limit_ms;
             } else {
                 finished = true;
                 auto end_time = std::chrono::steady_clock::now();
                 res.time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                 res.memory_kb = max_memory_kb;
                 
                 res.output = output;
                 res.error_output = error_output;

                 if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    res.verdict = Verdict::RE;
                    res.message = fmt::format("Exit code: {}", WEXITSTATUS(status));
                    // If exec failed (exit 127), ensure output is not empty for tests
                    if (WEXITSTATUS(status) == 127 && res.output.empty() && !res.error_output.empty()) {
                         res.output = "Exec failed: " + res.error_output;
                    }
                } else if (WIFSIGNALED(status)) {
                    res.verdict = Verdict::RE;
                    res.message = fmt::format("Signal: {}", WTERMSIG(status));
                } else {
                    res.verdict = Verdict::AC;
                    res.verdict = check_output(output, tc.output);
                }
             }
        }
    }
    return res;
}
#endif

JudgeResult Judge::run_process_redirect(const std::string& cmd, 
                                        const std::string& input_file, 
                                        const std::string& output_file, 
                                        int time_limit_ms, 
                                        int memory_limit_kb) {
    // Construct command: cmd [ < "input" ] [ > "output" ]
    std::string full_cmd = cmd;
    if (!input_file.empty()) {
        full_cmd += fmt::format(" < \"{}\"", input_file);
    }
    if (!output_file.empty()) {
        full_cmd += fmt::format(" > \"{}\"", output_file);
    }
    
    auto start = std::chrono::steady_clock::now();

#ifdef _WIN32
    // Windows Implementation using CreateProcess for Timeout Control
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide window

    ZeroMemory(&pi, sizeof(pi));

    // Use cmd.exe /C to handle redirection
    std::string shell_cmd = "cmd.exe /C \"" + full_cmd + "\"";

    if (!CreateProcessA(NULL, const_cast<char*>(shell_cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return {Verdict::RE, 0, 0, "CreateProcess failed"};
    }

    bool finished = false;
    DWORD exit_code = 0;

    if (WaitForSingleObject(pi.hProcess, time_limit_ms) == WAIT_OBJECT_0) {
        finished = true;
        GetExitCodeProcess(pi.hProcess, &exit_code);
    } else {
        // Timeout
        TerminateProcess(pi.hProcess, 1);
        finished = false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = std::chrono::steady_clock::now();
    JudgeResult res;
    res.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    res.memory_kb = 0; 

    if (!finished) {
        res.verdict = Verdict::TLE;
    } else if (exit_code != 0) {
        res.verdict = Verdict::RE;
        res.message = fmt::format("Exit code {}", exit_code);
    } else {
        res.verdict = Verdict::AC;
    }
    return res;

#else
    // Fallback or Linux implementation (std::system is simple but blocking)
    // For now keep std::system for non-Windows or implement fork/exec similar to run_case
    int ret = std::system(full_cmd.c_str());
    auto end = std::chrono::steady_clock::now();
    
    JudgeResult res;
    res.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    res.memory_kb = 0;
    
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
        res.verdict = Verdict::RE;
        res.message = fmt::format("Exit code {}", WEXITSTATUS(ret));
    } else if (WIFSIGNALED(ret)) {
        res.verdict = Verdict::RE;
        res.message = fmt::format("Signal {}", WTERMSIG(ret));
    } else {
        if (res.time_ms > time_limit_ms) res.verdict = Verdict::TLE;
        else res.verdict = Verdict::AC;
    }
    return res;
#endif
}

bool Judge::stream_file_diff(const std::string& path1, const std::string& path2) {
    std::ifstream f1(path1), f2(path2);
    if (!f1 || !f2) return false;

    std::string s1, s2;
    // Token-based comparison ignores whitespace/newlines
    while (true) {
        bool b1 = (bool)(f1 >> s1);
        bool b2 = (bool)(f2 >> s2);
        
        if (b1 != b2) return false; // One ended before other
        if (!b1) break; // Both ended
        if (s1 != s2) return false; // Mismatch
    }
    return true;
}

} // namespace shuati
