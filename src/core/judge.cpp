#include "shuati/judge.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>

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
            std::error_code ec;
            fs::remove(path_, ec);
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

std::string JudgeResult::verdict_str() const {
    switch (verdict) {
        case Verdict::AC: return "AC";
        case Verdict::WA: return "WA";
        case Verdict::TLE: return "TLE";
        case Verdict::MLE: return "MLE";
        case Verdict::RE: return "RE";
        case Verdict::CE: return "CE";
        case Verdict::SE: return "SE";
        default: return "UNKNOWN";
    }
}

std::string Judge::compile(const std::string& source_file, const std::string& language) {
    if (language == "python" || language == "py") {
        return "python \"" + source_file + "\""; 
    }

    if (language == "cpp" || language == "c++") {
        fs::path src(source_file);
        std::string exe = src.replace_extension(".exe").string();
        
        // Quote paths to prevent shell injection in simple system() calls
        std::string cmd = fmt::format("g++ -O2 -std=c++20 \"{}\" -o \"{}\"", source_file, exe);
        
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            throw std::runtime_error("Compilation failed");
        }
        return exe;
    }

    throw std::runtime_error("Unsupported language: " + language);
}

Verdict Judge::check_output(const std::string& user_out, const std::string& expected_out) {
    std::string u = trim_right(user_out);
    std::string e = trim_right(expected_out);
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
        executable = compile(source_file, language);
    } catch (const std::exception& e) {
        JudgeResult r;
        r.verdict = Verdict::CE;
        r.message = e.what();
        return {r};
    }

    // 2. Run cases
    for (const auto& tc : test_cases) {
        auto res = run_case(executable, tc, time_limit_ms, memory_limit_kb);
        results.push_back(res);
    }

    // Cleanup executable
    if (language == "cpp") {
        // Need to be careful about deleting checking if it exists
        // But logic is fine-ish.
        if (fs::exists(executable)) fs::remove(executable);
    }

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

    // Close write end of output pipe
    CloseHandle(hChildStd_OUT_Wr);

    // Wait
    DWORD wait_res = WaitForSingleObject(pi.hProcess, time_limit_ms);
    auto end_time = std::chrono::high_resolution_clock::now();
    res.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (wait_res == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        res.verdict = Verdict::TLE;
    } else {
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);

        // Check Memory
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
             res.memory_kb = pmc.PeakPagefileUsage / 1024;
             if (res.memory_kb > memory_limit_kb) res.verdict = Verdict::MLE;
        }

        if (exit_code != 0) {
            res.verdict = Verdict::RE;
            res.message = fmt::format("Exit code: {}", exit_code);
        } else if (res.verdict != Verdict::MLE) {
            std::string output;
            DWORD dwRead;
            CHAR chBuf[4096];
            BOOL bSuccess = FALSE;
            while(true) {
                bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL);
                if (!bSuccess || dwRead == 0) break;
                output.append(chBuf, dwRead);
            }
            res.output = output;
            res.verdict = check_output(output, tc.output);
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);
    // hChildStd_IN_Rd was passed to child, need to be closed? 
    // Actually yes, clean up handles
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
