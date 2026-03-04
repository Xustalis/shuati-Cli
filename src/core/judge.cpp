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
#include "shuati/utils/encoding.hpp"


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
        if (fs::exists(executable)) fs::remove(executable);
    }
}

std::string Judge::compile(const std::string& source_file, const std::string& language) {
    if (language == "python" || language == "py") {
        return "python \"" + source_file + "\""; 
    }

    if (language == "cpp" || language == "c++") {
        // Security check: Reject shell metacharacters to prevent command injection
        // Allow UTF-8 characters for Chinese filenames
        if (source_file.find_first_of("&|;><$`\n\r") != std::string::npos) {
            throw std::runtime_error("Invalid source file path (contains shell metacharacters): " + source_file);
        }

        fs::path src = shuati::utils::utf8_path(source_file);
        std::string exe = shuati::utils::path_to_utf8(src.replace_extension(".exe"));
        
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

    auto sb_res = sb->execute(
        executable,
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
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::InternalError) {
        res.verdict = Verdict::RE;
        res.error_output = "Sandbox Internal Error: " + sb_res.internal_message;
    } else {
        // OK
        res.output = shuati::utils::ensure_utf8_lossy(shuati::read_text_file(out_file.path()));
        res.verdict = check_output(res.output, tc.output);
        
        if (res.verdict == Verdict::WA) {
            res.message = fmt::format("Expected:\n{}\nActual:\n{}", tc.output, res.output);
        } else {
            res.verdict = Verdict::AC;
        }
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
    } else if (sb_res.status == shuati::sandbox::SandboxResultStatus::InternalError) {
        res.verdict = Verdict::RE;
        res.message = "Sandbox Internal Error: " + sb_res.internal_message;
    } else {
        res.verdict = Verdict::AC;
    }

    return res;
}

} // namespace shuati
