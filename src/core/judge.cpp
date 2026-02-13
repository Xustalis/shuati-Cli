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

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#endif

namespace shuati {

namespace fs = std::filesystem;

// RAII helper for temporary files
class TempFile {
public:
    TempFile(const std::string& extension = ".tmp") {
        auto tmp = fs::temp_directory_path();
        // Simple random name generation
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        path_ = tmp / fmt::format("shuati_{}_{}{}", now, rand() % 10000, extension);
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
    std::string u = trim_right(user_out);
    std::string e = trim_right(expected_out);
    if (e.empty()) return Verdict::AC;
    return (u == e) ? Verdict::AC : Verdict::WA;
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
    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_OUT_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.hStdInput = hChildStd_IN_Rd;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&pi, sizeof(pi));

    // Write input
    DWORD dwWritten;
    WriteFile(hChildStd_IN_Wr, tc.input.c_str(), tc.input.size(), &dwWritten, NULL);
    CloseHandle(hChildStd_IN_Wr);

    auto start_time = std::chrono::high_resolution_clock::now();

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_line.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return {Verdict::RE, 0, 0, "CreateProcess failed"};
    }

    // Close write end of output pipe so we don't block reading
    CloseHandle(hChildStd_OUT_Wr);

    std::string output;
    bool finished = false;
    DWORD exit_code = 0;
    
    while (true) {
        // Check if process has exited
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            finished = true;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            break;
        }

        // Check timeout
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > time_limit_ms) {
            finished = false; // TLE
            break;
        }

        // Read available data
        DWORD dwRead, dwAvail;
        if (PeekNamedPipe(hChildStd_OUT_Rd, NULL, 0, NULL, &dwAvail, NULL) && dwAvail > 0) {
            char buffer[4096];
            if (ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer), &dwRead, NULL) && dwRead > 0) {
                output.append(buffer, dwRead);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    res.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Read any remaining output after exit
    DWORD dwRead;
    char buffer[4096];
    while (ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer), &dwRead, NULL) && dwRead > 0) {
        output.append(buffer, dwRead);
    }

    if (!finished) {
        TerminateProcess(pi.hProcess, 1);
        res.verdict = Verdict::TLE;
    } else {
        // Check Memory
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
             res.memory_kb = pmc.PeakPagefileUsage / 1024;
             if (res.memory_kb > memory_limit_kb) res.verdict = Verdict::MLE;
        }

        if (res.verdict != Verdict::MLE) {
             if (exit_code != 0) {
                res.verdict = Verdict::RE;
                res.message = fmt::format("Exit code: {}", exit_code);
            } else {
                res.verdict = Verdict::AC;
                res.output = output; 
                // Only now check output
                res.verdict = check_output(output, tc.output);
            }
        }
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);
    CloseHandle(hChildStd_IN_Rd); 

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

    // Use TempFile for input/output buffering if pipes are tricky with large data, 
    // but pipes are generally fine for stdio
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) return {Verdict::SE, 0, 0, "Pipe failed"};

    auto start_time = std::chrono::steady_clock::now();
    pid_t pid = fork();

    if (pid < 0) {
        return {Verdict::SE, 0, 0, "Fork failed"};
    } else if (pid == 0) {
        // Child
        // Redirect stdin
        dup2(pipe_in[0], STDIN_FILENO);
        close(pipe_in[0]);
        close(pipe_in[1]);

        // Redirect stdout
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);
        close(pipe_out[0]);

        // Execute
        // Split executable for python cases: "python script.py"
        std::vector<std::string> args_vec;
        if (executable.find("python") == 0) {
            args_vec.push_back("python3");
            // Primitive parsing, assuming "python script.py"
            std::string script = executable.substr(7); 
            // remove quotes
             if (script.front() == '"') script = script.substr(1, script.length()-2);
            args_vec.push_back(script);
        } else {
            args_vec.push_back(executable);
        }

        std::vector<char*> args;
        for(auto& s : args_vec) args.push_back(&s[0]);
        args.push_back(nullptr);

        execvp(args[0], args.data());
        exit(127); // Command not found
    } else {
        // Parent
        close(pipe_in[0]);
        close(pipe_out[1]);

        // Write input
        if (::write(pipe_in[1], tc.input.c_str(), tc.input.size()) < 0) {}
        close(pipe_in[1]); 

        // Wait loop
        int status;
        bool finished = false;
        auto start_wait = std::chrono::steady_clock::now();

        long long max_memory_kb = 0;

        while(true) {
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) {
                finished = true;
                break;
            }
            // Timeout check
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_wait).count() > time_limit_ms) {
                break;
            }

            // Check memory usage via /proc/[pid]/status
            std::string status_path = fmt::format("/proc/{}/status", pid);
            if (fs::exists(status_path)) {
                std::ifstream f(status_path);
                std::string line;
                while (std::getline(f, line)) {
                    if (line.compare(0, 6, "VmPeak") == 0) { // Or VmHWM
                         std::stringstream ss(line);
                         std::string label;
                         long long val;
                         ss >> label >> val;
                         if (val > max_memory_kb) max_memory_kb = val;
                         break;
                    }
                }
            }
            if (max_memory_kb > memory_limit_kb) {
                 // MLE detected, kill process immediately
                 kill(pid, SIGKILL);
                 res.verdict = Verdict::MLE;
                 res.memory_kb = max_memory_kb;
                 finished = true; // technically finished by our kill
                 waitpid(pid, &status, 0); // reap
                 break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (res.verdict == Verdict::MLE) {
            // Already handled above
        } else if (!finished) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            res.verdict = Verdict::TLE;
             res.time_ms = time_limit_ms;
        } else {
            auto end_time = std::chrono::steady_clock::now();
            res.time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_wait).count();
            res.memory_kb = max_memory_kb;

             if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                res.verdict = Verdict::RE;
                res.message = fmt::format("Exit code: {}", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                res.verdict = Verdict::RE;
                res.message = fmt::format("Signal: {}", WTERMSIG(status));
            } else {
                // Read output
                std::vector<char> buffer(4096);
                std::string output;
                ssize_t bytes;
                while((bytes = read(pipe_out[0], buffer.data(), buffer.size())) > 0) {
                    output.append(buffer.data(), bytes);
                }
                res.output = output;
                res.verdict = check_output(output, tc.output);
            }
        }
        close(pipe_out[0]);
    }
    return res;
}
#endif

} // namespace shuati
