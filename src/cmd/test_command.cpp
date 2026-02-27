#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <random>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <cstdlib>

// Assuming these headers are available or we need to fix includes
#include "shuati/utils/encoding.hpp"

namespace shuati {
namespace cmd {

namespace fs = std::filesystem;
using shuati::utils::ensure_utf8;

// ─── JSON Serialization for TestReport ────────────────

namespace {
    // Helper to serialize JudgeResult
    nlohmann::json to_json(const JudgeResult& r) {
        return {
            {"verdict", r.verdict_str()},
            {"time_ms", r.time_ms},
            {"memory_kb", r.memory_kb},
            {"message", ensure_utf8(r.message)},
            {"input", ensure_utf8(r.input)},
            {"output", ensure_utf8(r.output)},
            {"expected", ensure_utf8(r.expected)}
        };
    }

    void save_report(const fs::path& path, const TestReport& report) {
        nlohmann::json j;
        j["problem_id"] = report.problem_id;
        j["timestamp"] = report.timestamp;
        j["verdict"] = report.verdict;
        j["pass_count"] = report.pass_count;
        j["total_count"] = report.total_count;
        j["cases"] = nlohmann::json::array();
        for (const auto& c : report.cases) {
            j["cases"].push_back(to_json(c));
        }
        std::ofstream o(path);
        o << j.dump(2);
    }
}

// ─── Helper Structs for Input Generation ──────────────
// (Copied from main.cpp, we might want to move these to a shared util if reused)

enum class SimpleType { Int, Float, String };

struct ParamSpec {
    std::string name;
    SimpleType type = SimpleType::String;
    std::optional<long long> min_i;
    std::optional<long long> max_i;
    std::vector<std::string> normals;
};

struct InputSpec {
    std::vector<ParamSpec> params;
};

struct PlannedCase {
    TestCase tc;
    std::string name;
    std::string kind;
    int priority = 0;
    bool has_oracle = false;
    std::vector<std::pair<std::string, std::string>> coverage_marks;
};

static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    for (std::string tok; iss >> tok;) out.push_back(tok);
    return out;
}

static bool parse_i64(const std::string& s, long long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0) return false;
    if (end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

static std::string join_tokens(const std::vector<std::string>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i) out.push_back(' ');
        out += tokens[i];
    }
    out.push_back('\n');
    return out;
}

static std::string trim_copy(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// ... (We need the implementation of infer_spec_from_db_cases and others from main.cpp if we fully implement dynamic testing)
// For brevity and to ensure compilation, I will implement essential parts and reuse logic.

static std::string build_problem_text(const Problem& prob) {
    std::string desc;
    desc += "题目: " + ensure_utf8(prob.title) + "\n";
    desc += "ID: " + prob.id + "\n";
    desc += "来源: " + ensure_utf8(prob.source) + "\n";
    desc += "难度: " + ensure_utf8(prob.difficulty) + "\n";
    desc += "URL: " + ensure_utf8(prob.url) + "\n\n";

    if (!prob.content_path.empty() && fs::exists(prob.content_path)) {
        std::ifstream f(prob.content_path, std::ios::in | std::ios::binary);
        std::string body(std::istreambuf_iterator<char>(f), {});
        if (!body.empty()) {
            desc += body.substr(0, 9000);
        }
    }
    return desc;
}

// ─── cmd_test Implementation ──────────────────────────

void cmd_test(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        
        // 1. Resolve Problem
        // Support ID or TID
        Problem prob;
        if (std::all_of(ctx.solve_pid.begin(), ctx.solve_pid.end(), ::isdigit)) {
             prob = svc.db->get_problem_by_display_id(std::stoi(ctx.solve_pid));
        }
        if (prob.id.empty()) prob = svc.db->get_problem(ctx.solve_pid);

        if (prob.id.empty()) { std::cerr << "[!] 题目不存在。" << std::endl; return; }
        
        // 2. Prepare Environment
        fs::path prob_dir = fs::path(".shuati") / "problems" / prob.id;
        fs::create_directories(prob_dir / "data");
        fs::create_directories(prob_dir / "validator");
        fs::create_directories(prob_dir / "debug");
        fs::create_directories(prob_dir / "temp");

        std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
        std::string src_file = "solution_" + prob.id + ext; 
        if (!fs::exists(src_file)) { std::cerr << "[!] 找不到代码文件: " << src_file << std::endl; return; }

        // Compile User Solution
        std::cout << "[*] 正在编译用户代码..." << std::endl;
        std::string user_exe;
        try {
            user_exe = svc.judge->prepare(src_file, svc.cfg.language);
        } catch (const std::exception& e) {
            std::cerr << "[Compile Error]\n" << e.what() << std::endl;
            return;
        }

        TestReport report;
        report.problem_id = prob.id;
        report.timestamp = std::time(nullptr);
        report.pass_count = 0;
        report.total_count = 0;

        // 3. Collect Test Cases
        // A. Static Cases from data directory
        std::vector<TestCase> cases;
        if (fs::exists(prob_dir / "data")) {
            // Find all .in files
            std::vector<fs::path> in_files;
            for (const auto& entry : fs::directory_iterator(prob_dir / "data")) {
                if (entry.path().extension() == ".in") {
                    in_files.push_back(entry.path());
                }
            }
            std::sort(in_files.begin(), in_files.end()); // Ensure stable order

            for (const auto& in_path : in_files) {
                 TestCase tc;
                 std::ifstream f(in_path, std::ios::binary);
                 tc.input.assign(std::istreambuf_iterator<char>(f), {});
                 
                 fs::path out_path = in_path;
                 out_path.replace_extension(".out");
                 if (fs::exists(out_path)) {
                     std::ifstream fo(out_path, std::ios::binary);
                     tc.output.assign(std::istreambuf_iterator<char>(fo), {});
                 }
                 tc.is_sample = false; // Could be sample but treat as static file case
                 cases.push_back(tc);
            }
        }
        
        // B. DB Cases (Samples mostly)
        // If no static files, use DB cases
        if (cases.empty()) {
            auto db_cases = svc.db->get_test_cases(prob.id);
            for (const auto& p : db_cases) {
                TestCase tc;
                tc.input = p.first;
                tc.output = p.second;
                tc.is_sample = true;
                cases.push_back(tc);
            }
        }
        
        // C. Dynamic Generation (Only if requested or essentially needed for checking?)
        // The prompt asks for "Enhanced list function" and "view details".
        // It says "Detailed progress info: Passed X/Y test points".
        // We should run the collected cases. 
        // Note: The original cmd_test had "Stress Mode" (infinite loop) vs "Static Mode".
        // The requirement implies a finite set of tests mainly. 
        // If user wants stress test, maybe `test --stress`? 
        // For now, let's implement the standard check logic first (Static + DB).
        // If cases is empty, try to generate if allowed?
        // Let's stick to what we have. If no cases, warn.

        if (cases.empty()) {
             // Check if we should/can generate
             fs::path validator_dir = prob_dir / "validator";
             if (!fs::exists(validator_dir)) fs::create_directories(validator_dir);
             
             fs::path gen_py = validator_dir / "gen.py";
             fs::path sol_py = validator_dir / "sol.py";
             
             bool has_scripts = fs::exists(gen_py) && fs::exists(sol_py);

             if (has_scripts) {
                 // Silent entry, maybe just a header
                 std::cout << "=== 动态对拍模式 (Dynamic Test Mode) ===" << std::endl;
             } else if (svc.ai->enabled()) {
                 std::cout << "[*] 未找到静态用例，正在请求 AI 生成脚本..." << std::endl;
             } else {
                 std::cout << "[!] 未找到静态测试用例，且未启用 AI。" << std::endl;
             }
             
             if (!has_scripts) {
                 if (svc.ai->enabled()) {
                     std::cout << "[*] 正在调用 AI 生成对拍脚本 (gen.py / sol.py)..." << std::endl;
                     try {
                         // Need problem description. 
                         // We have limited info in 'prob' struct usually (title, etc).
                         // We might need to read the content file or fetch.
                         // For now use build_problem_text or just title + tags?
                         // Ideally we read the md file.
                         std::string desc = build_problem_text(prob); 
                         auto scripts = svc.ai->generate_test_scripts(desc);
                         
                         if (scripts.first.empty() || scripts.second.empty()) {
                             std::cerr << "[!] AI 生成脚本失败或格式无法解析。" << std::endl;
                         } else {
                             std::ofstream fg(gen_py); fg << scripts.first;
                             std::ofstream fs(sol_py); fs << scripts.second;
                             std::cout << "[+] 脚本已保存至 " << validator_dir.string() << std::endl;
                             has_scripts = true;
                         }
                     } catch (const std::exception& e) {
                         std::cerr << "[!] AI 调用失败: " << e.what() << std::endl;
                     }
                 } else {
                     std::cout << "[!] AI 未启用 (请配置 api_key)，跳过脚本生成。" << std::endl;
                 }
             }

             if (has_scripts) {
                 std::cout << "[*] 正在生成临时测试用例 (5 sets)..." << std::endl;
                 int gen_count = 5;
                 for (int i=1; i<=gen_count; ++i) {
                     // Run gen.py -> temp/gen_i.in
                     fs::path in_path = prob_dir / "temp" / ("gen_" + std::to_string(i) + ".in");
                     fs::path ans_path = prob_dir / "temp" / ("gen_" + std::to_string(i) + ".ans");
                     
                     // Run Generator
                     // python "gen_py" > in_path
                     // We use run_process_redirect which handles redirection internally if supported, 
                     // or we pass the command. 
                     // Judge::run_process_redirect(cmd, input_file, output_file, ...)
                     std::string cmd_gen = "python \"" + gen_py.string() + "\"";
                     auto res_gen = svc.judge->run_process_redirect(cmd_gen, "", in_path.string(), 5000, 256*1024);
                     
                     if (res_gen.verdict != Verdict::AC) {
                         std::string msg = res_gen.message.empty() ? res_gen.verdict_str() : res_gen.message;
                         std::cerr << "[!] 生成器运行失败 (Case " << i << "): " << msg << std::endl;
                         continue;
                     }
                     
                     // Run Solver
                     // python "sol_py" < in_path > ans_path
                     std::string cmd_sol = "python \"" + sol_py.string() + "\"";
                     auto res_sol = svc.judge->run_process_redirect(cmd_sol, in_path.string(), ans_path.string(), 5000, 256*1024);
                     
                     if (res_sol.verdict != Verdict::AC) {
                         std::string msg = res_sol.message.empty() ? res_sol.verdict_str() : res_sol.message;
                         std::cerr << "[!] 标程运行失败 (Case " << i << "): " << msg << std::endl;
                         continue;
                     }
                     
                     // Load as case
                     TestCase tc;
                     {
                        std::ifstream f(in_path, std::ios::binary);
                        tc.input.assign(std::istreambuf_iterator<char>(f), {});
                     }
                     {
                        std::ifstream f(ans_path, std::ios::binary);
                        tc.output.assign(std::istreambuf_iterator<char>(f), {});
                     }
                     tc.is_sample = false;
                     cases.push_back(tc);
                 }
             }
        }

        if (cases.empty()) {
             std::cout << "[!] 无法获取任何测试用例 (静态/数据库/AI生成)。" << std::endl;
        }

        report.total_count = cases.size();
        std::cout << "=== 开始测试 (共 " << cases.size() << " 个测试点) ===" << std::endl;

        int passed = 0;
        bool all_ac = true;
        
        for (size_t i = 0; i < cases.size(); i++) {
            const auto& tc = cases[i];
            
            // Simple Output: "Case 1: Running..."
            std::cout << "Case " << (i + 1) << ": Running...\r" << std::flush;

            // Run
            auto res = svc.judge->run_prepared(user_exe, tc, 2000, 256*1024);
            res.expected = tc.output;
            res.input = tc.input; // Ensure input is captured
            
            report.cases.push_back(res);

            // Overwrite line
            std::cout << "Case " << (i + 1) << ": ";
            if (res.verdict == Verdict::AC) {
                std::cout << "AC";
                passed++;
            } else {
                std::cout << res.verdict_str().c_str();
                all_ac = false;
            }
            std::cout << " (" << res.time_ms << "ms, " << res.memory_kb << "KB)   " << std::endl; // Extra spaces to clear "Running..."
        }

        report.pass_count = passed;
        report.verdict = all_ac ? "AC" : (passed > 0 ? "WA" : "WA"); // WA if any failed
        // Refine verdict if TLE/RE?
        // Just use "WA" or specific if all failed same way?
        // Let's iterate report.cases to find first non-AC for the summary verdict if not AC
        if (!all_ac) {
             for (const auto& c : report.cases) {
                 if (c.verdict != Verdict::AC) {
                     report.verdict = c.verdict_str();
                     break;
                 }
             }
        }
        if (cases.empty()) report.verdict = "SKIPPED";

        // Summary Line
        if (cases.empty()) {
            std::cout << "\n[Result] SKIPPED (No test cases found)." << std::endl;
        } else if (all_ac) {
            std::cout << "\n[Result] All Accepted! (" << passed << "/" << report.total_count << ")" << std::endl;
        } else {
             std::cout << "\n[Result] Failed. Passed " << passed << "/" << report.total_count << std::endl;
        }

        // Save Report
        fs::path report_path = prob_dir / "test_report.json";
        save_report(report_path, report);
        std::cout << "Report saved to " << report_path.string() << std::endl;

        // Update DB
        svc.db->update_problem_status(prob.id, report.verdict, report.pass_count, report.total_count);
        
        svc.judge->cleanup_prepared(user_exe, svc.cfg.language);

        // AI Diagnosis
        // std::cout << "[DEBUG] AI Enabled: " << svc.ai->enabled() << ", All AC: " << all_ac << std::endl;
        if (!all_ac && svc.ai->enabled()) {
             std::cout << std::endl << "[*] 测试未通过，正在进行AI分析..." << std::endl;
             
             // Gather info
             std::string code;
             if (fs::exists(src_file)) {
                 std::ifstream f(src_file);
                 code.assign(std::istreambuf_iterator<char>(f), {});
             }

             // Find first failed
             std::string failure_info;
             for (const auto& c : report.cases) {
                 if (c.verdict != Verdict::AC) {
                     failure_info = fmt::format("Verdict: {}\nInput:\n{}\nOutput:\n{}\nExpected:\n{}", 
                         c.verdict_str(), 
                         c.input.substr(0, 200),
                         c.output.substr(0, 200), 
                         c.expected.substr(0, 200));
                     break;
                 }
             }

             std::string desc = build_problem_text(prob);
             
             // Streaming output callback with filtering (Reuse logic? Copy for now to be safe)
             std::string buffer;
             bool suppress = false;
             auto print_chunk = [&](std::string chunk) {
                if (suppress) return;
                buffer += chunk;
                size_t start_pos = buffer.find("<!-- SYSTEM_OP");
                if (start_pos != std::string::npos) {
                    std::cout << buffer.substr(0, start_pos) << std::flush;
                    buffer.clear();
                    suppress = true;
                } else {
                    if (buffer.size() > 20) {
                        size_t print_len = buffer.size() - 20;
                        std::cout << buffer.substr(0, print_len) << std::flush;
                        buffer.erase(0, print_len);
                    }
                }
             };

             svc.ai->diagnose(desc, code, failure_info, "", print_chunk);
             
             if (!suppress && !buffer.empty() && buffer.find("<!--") == std::string::npos) {
                 std::cout << buffer << std::flush;
             }
             std::cout << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
