#include "shuati/judge.hpp"
#include "shuati/sandbox.hpp"
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
#include <random>
#include <cctype>
#include "shuati/utils/encoding.hpp"


namespace shuati {

namespace fs = std::filesystem;

static std::string resolve_executable_in_path(const std::string& name) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return {};

    std::string path_str(path_env);
#ifdef _WIN32
    char delimiter = ';';
    std::vector<std::string> candidates = {name, name + ".exe"};
#else
    char delimiter = ':';
    std::vector<std::string> candidates = {name};
#endif

    std::istringstream iss(path_str);
    std::string dir;
    while (std::getline(iss, dir, delimiter)) {
        if (dir.empty()) continue;
        for (const auto& cand : candidates) {
            fs::path full = fs::path(dir) / cand;
            std::error_code ec;
            if (fs::exists(full, ec) && !fs::is_directory(full, ec)) {
                return shuati::utils::path_to_utf8(full);
            }
        }
    }
    return {};
}

static std::string resolve_python_executable() {
    auto p = resolve_executable_in_path("python");
    if (!p.empty()) return p;
    return resolve_executable_in_path("python3");
}

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

    std::string path() const { return shuati::utils::path_to_utf8(path_); }
    
    void write(const std::string& content) {
        std::ofstream out(path_);
        out << content;
    }

private:
    fs::path path_;
};

static std::string read_text_file(const std::string& path) {
    std::ifstream in(shuati::utils::utf8_path(path), std::ios::in | std::ios::binary);
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
        fs::path exe_path = shuati::utils::utf8_path(executable);
        if (fs::exists(exe_path)) fs::remove(exe_path);
    }
}

std::string Judge::compile(const std::string& source_file, const std::string& language) {
    if (language == "python" || language == "py") {
        // For sandbox compatibility, return a marker.
        // The runner will resolve `python` executable from PATH and pass the script as argv[0]/arg0.
        // The runner will set executable to `python` and pass the script as argv[0]/arg0.
        if (source_file.find_first_of("&|;><$`\n\r\"") != std::string::npos) {
            throw std::runtime_error("Invalid source file path (contains shell metacharacters): " + source_file);
        }
        return std::string("PYTHON:") + source_file;
    }

    if (language == "cpp" || language == "c++") {
        // Security check: Reject shell metacharacters to prevent command injection
        // Allow UTF-8 characters for Chinese filenames
        if (source_file.find_first_of("&|;><$`\n\r") != std::string::npos) {
            throw std::runtime_error("Invalid source file path (contains shell metacharacters): " + source_file);
        }

        fs::path src = shuati::utils::utf8_path(source_file);
        // Use absolute path so execvp() on Linux can locate the binary
        // (execvp only searches PATH for bare names without '/'; CWD is not searched).
        fs::path abs_src = fs::absolute(src);
#ifdef _WIN32
        // On Windows, MinGW g++ / ld.exe via _wsystem struggles with Unicode characters in the output path.
        // We override the executable name to be ASCII-safe using a timestamp.
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::string safe_name = fmt::format("shuati_build_{}.exe", now);
        std::string exe = shuati::utils::path_to_utf8(abs_src.replace_filename(safe_name));
#else
        std::string exe = shuati::utils::path_to_utf8(abs_src.replace_extension(""));
#endif
        
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

            if (fs::exists(shuati::utils::utf8_path(exe))) {
                try { fs::remove(shuati::utils::utf8_path(exe)); } catch(...) {}
            }

            int ret = shuati::utils::utf8_system(cmd);
            if (ret == 0 && fs::exists(shuati::utils::utf8_path(exe))) {
                return exe;
            }

            std::string err_text = shuati::utils::ensure_utf8_lossy(shuati::read_text_file(err.path()));
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
// unified run_case using ISandbox
JudgeResult Judge::run_case(const std::string& executable, 
                            const TestCase& tc, 
                            int time_limit_ms, 
                            int memory_limit_kb) {
    JudgeResult res;
    res.input = tc.input;
    res.expected = tc.output;

    shuati::TempFile in_file(".in");
    in_file.write(tc.input);

    shuati::TempFile out_file(".out");
    shuati::TempFile err_file(".err");

    auto sb = shuati::sandbox::create_sandbox();
    shuati::sandbox::SandboxLimits limits;
    limits.cpu_time_ms = time_limit_ms;
    limits.memory_mb = memory_limit_kb / 1024;

    std::vector<std::string> args;
    std::string executable_program = executable;

    // Interpreted languages marker-based execution.
    constexpr const char* py_prefix = "PYTHON:";
    std::string python_script;
    if (executable.rfind(py_prefix, 0) == 0) {
        python_script = executable.substr(std::string(py_prefix).size());
    } else {
        auto ext = fs::path(executable).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (ext == ".py" || ext == ".pyw") {
            python_script = executable;
        }
    }

    if (!python_script.empty()) {
        std::string py_exe = resolve_python_executable();
        if (py_exe.empty()) {
            res.verdict = Verdict::SE;
            res.message = "python executable not found in PATH";
            return res;
        }
        executable_program = py_exe;
        args.push_back(python_script); // script path
    }

    auto sb_res = sb->execute(
        executable_program,
        args,
        in_file.path(),
        out_file.path(),
        err_file.path(),
        limits
    );

    res.time_ms = sb_res.cpu_time_ms;
    res.memory_kb = sb_res.memory_mb * 1024;

    if (sb_res.status == shuati::sandbox::SandboxResultStatus::TimeLimitExceeded) {
        res.verdict = Verdict::TLE;
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::MemoryLimitExceeded) {
        res.verdict = Verdict::MLE;
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::RuntimeError) {
        res.verdict = Verdict::RE;
        res.error_output = shuati::utils::ensure_utf8_lossy(shuati::read_text_file(err_file.path()));
        if (res.error_output.empty()) {
            res.error_output = fmt::format("Process exited with code {}", sb_res.exit_code);
        }
        
        // Fallback for MLE detection if stderr mentions bad_alloc
        if (res.error_output.find("std::bad_alloc") != std::string::npos ||
            res.error_output.find("Memory limit exceeded") != std::string::npos) {
            res.verdict = Verdict::MLE;
            if (res.memory_kb == 0 && limits.memory_mb > 0) {
                res.memory_kb = limits.memory_mb * 1024;
            }
        }
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::InternalError) {
        res.verdict = Verdict::RE;
        res.error_output = "Sandbox Internal Error: " + sb_res.internal_message;
    } else {
        // OK
        res.output = shuati::utils::ensure_utf8_lossy(shuati::read_text_file(out_file.path()));
        res.verdict = check_output(res.output, tc.output);
        
        if (res.verdict == Verdict::WA) {
            res.message = fmt::format("Expected:\n{}\nActual:\n{}", tc.output, res.output);
        }
        // verdict is already set correctly by check_output (AC or WA)
    }

    return res;
}

JudgeResult Judge::run_process_redirect(const std::string& cmd, 
                                        const std::string& input_file, 
                                        const std::string& output_file, 
                                        int time_limit_ms, 
                                        int memory_limit_kb) {
    auto start = std::chrono::steady_clock::now();
    
    // Naively parse cmd into executable and args for Sandbox compatibility
    std::string executable;
    std::vector<std::string> args;
    size_t first_space = cmd.find(' ');
    if (first_space == std::string::npos) {
        executable = cmd;
    } else {
        executable = cmd.substr(0, first_space);
        std::string rest = cmd.substr(first_space + 1);
        
        // Very basic quote un-escaping since test generators pass `python "file"`
        if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"') {
            rest = rest.substr(1, rest.size() - 2);
        }
        args.push_back(rest);
    }

    if (executable == "python" || executable == "python3") {
        std::string resolved = resolve_python_executable();
        if (!resolved.empty()) executable = resolved;
    }

    shuati::TempFile err_file(".err");

    auto sb = shuati::sandbox::create_sandbox();
    shuati::sandbox::SandboxLimits limits;
    limits.cpu_time_ms = time_limit_ms;
    limits.memory_mb = memory_limit_kb / 1024;

    auto sb_res = sb->execute(
        executable,
        args,
        input_file,
        output_file,
        err_file.path(),
        limits
    );

    JudgeResult res;
    res.time_ms = sb_res.cpu_time_ms;
    res.memory_kb = sb_res.memory_mb * 1024;

    if (sb_res.status == shuati::sandbox::SandboxResultStatus::TimeLimitExceeded) {
        res.verdict = Verdict::TLE;
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::MemoryLimitExceeded) {
        res.verdict = Verdict::MLE;
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::RuntimeError) {
        res.verdict = Verdict::RE;
        std::string err_output = shuati::utils::ensure_utf8_lossy(shuati::read_text_file(err_file.path()));
        res.message = err_output.empty() ? fmt::format("Exit code {}", sb_res.exit_code) : err_output;
        
        // Fallback checking for MLE
        if (res.message.find("std::bad_alloc") != std::string::npos ||
            res.message.find("Memory limit exceeded") != std::string::npos) {
            res.verdict = Verdict::MLE;
            if (res.memory_kb == 0 && limits.memory_mb > 0) {
                res.memory_kb = limits.memory_mb * 1024;
            }
        }
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::InternalError) {
        res.verdict = Verdict::RE;
        res.message = "Sandbox Internal Error: " + sb_res.internal_message;
    } else {
        res.verdict = Verdict::AC;
    }

    return res;
}

} // namespace shuati
