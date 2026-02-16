#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace shuati {
namespace cmd {

using shuati::utils::ensure_utf8;

namespace {
    struct LoadedReport {
        TestReport r;
        bool ok;
    };

    LoadedReport load_report(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return {{}, false};
        try {
            std::ifstream i(path);
            nlohmann::json j;
            i >> j;
            TestReport r;
            r.problem_id = j.value("problem_id", "");
            r.timestamp = j.value("timestamp", 0LL);
            r.verdict = j.value("verdict", "");
            r.pass_count = j.value("pass_count", 0);
            r.total_count = j.value("total_count", 0);
            
            if (j.contains("cases")) {
                for (const auto& cj : j["cases"]) {
                    JudgeResult jr;
                    std::string v = cj.value("verdict", "");
                    if (v == "AC") jr.verdict = Verdict::AC;
                    else if (v == "WA") jr.verdict = Verdict::WA;
                    else if (v == "TLE") jr.verdict = Verdict::TLE;
                    else if (v == "MLE") jr.verdict = Verdict::MLE;
                    else if (v == "RE") jr.verdict = Verdict::RE;
                    else if (v == "CE") jr.verdict = Verdict::CE;
                    else jr.verdict = Verdict::SE;

                    jr.time_ms = cj.value("time_ms", 0);
                    jr.memory_kb = cj.value("memory_kb", 0);
                    jr.message = cj.value("message", "");
                    jr.input = cj.value("input", "");
                    jr.output = cj.value("output", "");
                    jr.expected = cj.value("expected", "");
                    r.cases.push_back(jr);
                }
            }
            return {r, true};
        } catch (...) {
            return {{}, false};
        }
    }
}

void cmd_view(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        Problem prob;
        if (std::all_of(ctx.solve_pid.begin(), ctx.solve_pid.end(), ::isdigit)) {
             prob = svc.db->get_problem_by_display_id(std::stoi(ctx.solve_pid));
        }
        if (prob.id.empty()) prob = svc.db->get_problem(ctx.solve_pid);

        if (prob.id.empty()) { std::cerr << "[!] 题目不存在。" << std::endl; return; }
        
        std::filesystem::path prob_dir = std::filesystem::path(".shuati") / "problems" / prob.id;
        std::filesystem::path report_path = prob_dir / "test_report.json";
        
        auto [report, ok] = load_report(report_path);
        if (!ok) {
            std::cout << "[!] 未找到测试报告 (请先运行 test 命令)" << std::endl;
            return;
        }

        // Export logic
        if (!ctx.view_export_dir.empty()) {
            std::filesystem::path export_dir = ctx.view_export_dir;
            if (!std::filesystem::exists(export_dir)) {
                std::filesystem::create_directories(export_dir);
            }
            std::cout << "[*] 正在导出测试数据到: " << export_dir.string() << std::endl;
            
            for (size_t i = 0; i < report.cases.size(); ++i) {
                const auto& c = report.cases[i];
                std::string base = "case_" + std::to_string(i + 1);
                
                std::ofstream in(export_dir / (base + ".in"));
                in << c.input;
                
                std::ofstream out(export_dir / (base + ".out"));
                out << c.output;
                
                std::ofstream ans(export_dir / (base + ".ans"));
                ans << c.expected;
            }
            std::cout << "[+] 成功导出 " << report.cases.size() << " 个测试点。" << std::endl;
            return;
        }
        
        std::cout << "=== 测试报告: " << ensure_utf8(prob.title).c_str() << " ===" << std::endl;
        std::cout << "Verdict: " << report.verdict.c_str() << std::endl;
        std::cout << "Passed:  " << report.pass_count << "/" << report.total_count << std::endl;
        std::cout << std::endl;

        for (size_t i = 0; i < report.cases.size(); ++i) {
            const auto& c = report.cases[i];
            
            // Colorize verdict
            std::string v = c.verdict_str();
            if (c.verdict == Verdict::AC) v = "\033[32m" + v + "\033[0m";
            else if (c.verdict == Verdict::WA) v = "\033[31m" + v + "\033[0m";
            else v = "\033[33m" + v + "\033[0m";

            std::cout << "Case #" << (i+1) << ": ";
            std::cout << v << " (" << c.time_ms << "ms, " << c.memory_kb << "KB)";
            if (c.verdict != Verdict::AC) {
                 std::cout << std::endl;
                 std::cout << "  Input:    " << (c.input.substr(0, 100) + (c.input.size()>100?"...":"")) << std::endl;
                 std::cout << "  Expected: " << (c.expected.substr(0, 100) + (c.expected.size()>100?"...":"")) << std::endl;
                 std::cout << "  Actual:   " << (c.output.substr(0, 100) + (c.output.size()>100?"...":"")) << std::endl;
            }
            std::cout << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
