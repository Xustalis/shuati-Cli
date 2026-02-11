#include "shuati/judge.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace shuati {

namespace fs = std::filesystem;

// Helper to remove trailing whitespace
std::string trim_right(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
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
        return {r}; // Return single CE result
    }

    // 2. Run cases
    for (const auto& tc : test_cases) {
        auto res = run_case(executable, tc, time_limit_ms, memory_limit_kb);
        results.push_back(res);
        
        // Optional: stop on first failure? usually OJ runs all
    }

    // Cleanup executable
    if (language == "cpp" && fs::exists(executable)) {
        fs::remove(fs::path(executable));
    }

    return results;
}

std::string Judge::compile(const std::string& source_file, const std::string& language) {
    if (language == "python" || language == "py") {
        return "python \"" + source_file + "\""; 
    }

    if (language == "cpp" || language == "c++") {
        fs::path src(source_file);
        std::string exe = src.replace_extension(".exe").string();
        
        // Command: g++ -O2 -std=c++17 <src> -o <exe>
        std::string cmd = fmt::format("g++ -O2 -std=c++17 \"{}\" -o \"{}\"", source_file, exe);
        
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

    // Create pipes for stdin/stdout
    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};

    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return {Verdict::SE, 0, 0, "Pipe Error"};
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return {Verdict::SE, 0, 0, "Handle Error"};

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_OUT_Wr; // Redirect stderr to stdout for simplicity
    si.hStdOutput = hChildStd_OUT_Wr;
    si.hStdInput = hChildStd_IN_Rd;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    // Write input text to pipe
    DWORD dwWritten;
    WriteFile(hChildStd_IN_Wr, tc.input.c_str(), tc.input.size(), &dwWritten, NULL);
    CloseHandle(hChildStd_IN_Wr); // Close write end so child sees EOF

    auto start_time = std::chrono::high_resolution_clock::now();

    if (!CreateProcessA(NULL, 
        const_cast<char*>(cmd_line.c_str()), 
        NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return {Verdict::RE, 0, 0, "CreateProcess failed"};
    }

    // Wait with timeout
    DWORD wait_res = WaitForSingleObject(pi.hProcess, time_limit_ms);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    res.time_ms = (int)duration;

    if (wait_res == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        res.verdict = Verdict::TLE;
    } else {
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        if (exit_code != 0) {
            res.verdict = Verdict::RE;
            res.message = fmt::format("Exit code: {}", exit_code);
        } else {
            // Read output
            std::string output;
            DWORD dwRead;
            CHAR chBuf[4096];
            BOOL bSuccess = FALSE;
            
            // Close write end of stdout so we can read
            CloseHandle(hChildStd_OUT_Wr);

            for (;;) { 
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
    CloseHandle(hChildStd_IN_Rd);

    return res;
}
#else
JudgeResult Judge::run_case(const std::string& executable, 
                            const TestCase& tc, 
                            int time_limit_ms, 
                            int memory_limit_kb) {
    // POSIX Implementation placeholder
    return {Verdict::SE, 0, 0, "Not implemented on non-Windows yet"};
}
#endif

} // namespace shuati
