#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include "shuati/stream_filter.hpp"
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <string>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>

namespace shuati {
namespace cmd {

using shuati::utils::ensure_utf8;
namespace fs = std::filesystem;

void cmd_solve(CommandContext& ctx) {
    try {
        auto root = find_root_or_die();
        auto svc = Services::load(root);
        
        Problem prob;
        if (ctx.solve_pid.empty()) {
            // In TUI mode: cannot launch nested FTXUI menu or read stdin.
            if (ctx.is_tui) {
                std::cout << "[提示] 请通过 /list 选择题目后使用 /solve <ID> 开始练习。\n"
                          << "      例如: /solve 1" << std::endl;
                return;
            }

            // Interactive Selection via ftxui Menu (CLI-only)
            auto problems = svc.pm->list_problems();
            if (problems.empty()) {
                std::cout << "题库为空，请先使用 pull 或 new 添加题目。" << std::endl;
                return;
            }

            try {
                using namespace ftxui;
                std::vector<std::string> options;
                for (const auto& p : problems) {
                    std::string status = p.last_verdict.empty() ? "-" : p.last_verdict;
                    options.push_back(fmt::format("{:>3}  {:<24}  [{}]  {}",
                        p.display_id,
                        ensure_utf8(p.title).substr(0, 24),
                        ensure_utf8(p.difficulty),
                        status));
                }

                int selected = 0;
                auto menu = Menu(&options, &selected);
                auto screen = ScreenInteractive::TerminalOutput();

                auto component = CatchEvent(menu, [&](Event event) {
                    if (event == Event::Return) {
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    if (event == Event::Escape || event == Event::Character('q')) {
                        selected = -1;
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    return false;
                });

                auto renderer = Renderer(component, [&] {
                    return vbox({
                        text("选择要练习的题目:") | ftxui::bold,
                        separator(),
                        menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 15),
                        separator(),
                        text("↑/↓ 选择  Enter 确认  q/Esc 取消") | dim,
                    }) | border;
                });

                screen.Loop(renderer);

                if (selected < 0 || selected >= (int)problems.size()) {
                    std::cout << "操作取消。" << std::endl;
                    return;
                }
                prob = problems[selected];

            } catch (...) {
                if (ctx.is_tui) {
                    std::cout << "[!] 请直接提供题目 ID: /solve <ID>" << std::endl;
                    return;
                }
                // Fallback to plain stdin
                std::cout << "请输入题目 ID: ";
                std::cin >> ctx.solve_pid;
                try {
                    if (std::all_of(ctx.solve_pid.begin(), ctx.solve_pid.end(), ::isdigit)) {
                        int tid = std::stoi(ctx.solve_pid);
                        prob = svc.db->get_problem_by_display_id(tid);
                    } else {
                        prob = svc.db->get_problem(ctx.solve_pid);
                    }
                } catch (...) {}
            }

        } else {
            try {
                int tid = std::stoi(ctx.solve_pid);
                prob = svc.db->get_problem_by_display_id(tid);
            } catch (...) {
                prob = svc.db->get_problem(ctx.solve_pid);
            }
        }

        if (prob.id.empty()) {
            std::cerr << "[!] 未找到题目: " << ctx.solve_pid << std::endl;
            return;
        }

        std::cout << "=== 开始练习: " << ensure_utf8(prob.title).c_str() << " ===" << std::endl;
        if (fs::exists(prob.content_path)) {
            std::cout << "题目描述: " << prob.content_path.c_str() << std::endl;
        }

        std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
        // Try to find existing solution file (supports old + new naming)
        std::string filename = find_solution_file(prob, svc.cfg.language);
        if (filename.empty()) {
            filename = make_solution_filename(prob, svc.cfg.language);
        }
        if (!fs::exists(shuati::utils::utf8_path(filename))) {
            std::ofstream out(shuati::utils::utf8_path(filename), std::ios::out | std::ios::binary);
            if (svc.cfg.language == "python") {
                out << "import sys\n\n\ndef main():\n    pass\n\n\nif __name__ == \"__main__\":\n    main()\n";
            } else {
                out << "#include <iostream>\n\nint main() {\n    std::ios::sync_with_stdio(false);\n    std::cin.tie(nullptr);\n\n    return 0;\n}\n";
            }
            std::cout << "[+] 代码文件已就绪: " << filename << std::endl;
        }
        
        // Do not launch editor from TUI — it would overwrite the full-screen interface.
        if (!ctx.is_tui && !svc.cfg.editor.empty()) {
            std::string cmd = svc.cfg.editor + " \"" + filename + "\"";
            if (shuati::utils::utf8_system(cmd) != 0) {}
        } else if (ctx.is_tui) {
            std::cout << "[提示] 代码文件: " << filename << "\n"
                      << "      编辑器未在 TUI 内自动打开，请另开终端编辑。" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

void cmd_submit(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        
        // V0.0.6: Auto-quality from recent test report
        if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
            // Try to auto-compute from test report
            fs::path report_path = fs::path(".shuati") / "problems" / ctx.submit_pid / "test_report.json";
            int auto_q = -1;
            if (fs::exists(report_path)) {
                try {
                    std::ifstream rf(report_path);
                    auto j = nlohmann::json::parse(rf);
                    std::string verdict = j.value("verdict", "");
                    int time_ms = 0;
                    if (j.contains("cases") && !j["cases"].empty()) {
                        time_ms = j["cases"][0].value("time_ms", 0);
                    }
                    auto_q = SM2Algorithm::auto_quality(verdict, time_ms, 2000);
                    std::cout << "[*] 根据最近测试结果自动评分: " << auto_q 
                              << " (verdict=" << verdict << ", time=" << time_ms << "ms)" << std::endl;
                } catch (...) {}
            }

            if (ctx.is_tui) {
                // In TUI mode: cannot use stdin; just accept auto_q or default to 3.
                ctx.submit_quality = (auto_q >= 0) ? auto_q : 3;
                std::cout << "[TUI] 自动采用评分: " << ctx.submit_quality
                          << "。如需覆盖请使用 /submit <id> -q <0-5>" << std::endl;
            } else if (auto_q >= 0) {
                std::cout << "按回车接受自动评分 [" << auto_q << "]，或输入 0-5 覆盖: ";
                std::string val; std::getline(std::cin, val);
                if (val.empty()) {
                    ctx.submit_quality = auto_q;
                } else {
                    try { ctx.submit_quality = std::stoi(val); } catch (...) { ctx.submit_quality = auto_q; }
                }
            } else {
                std::cout << "掌握程度 (0=不会, 5=秒杀): ";
                std::string val; std::getline(std::cin, val);
                try { ctx.submit_quality = std::stoi(val); } catch (...) { ctx.submit_quality = 3; }
            }
        }

        if (ctx.submit_quality < 3) {
                std::cout << "记录一个错误类型 (逻辑/细节/算法/TLE): ";
                std::string type, desc;
                std::getline(std::cin, type);
                std::cout << "详细描述 (或按空格跳过): ";
                std::getline(std::cin, desc);
                svc.ma->log_mistake(ctx.submit_pid, type, desc);
        }

        auto review = svc.db->get_review(ctx.submit_pid);
        review = SM2Algorithm::update(review, ctx.submit_quality);
        svc.db->upsert_review(review);
        std::cout << "[+] 记录成功。下次复习间隔: " << review.interval << " 天。" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

void cmd_hint(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        auto prob = svc.pm->get_problem(ctx.hint_pid);
        if (prob.id.empty()) {
            if (ctx.stream_cb) ctx.stream_cb("[!] 题目不存在。\n");
            else std::cerr << "[!] 题目不存在。" << std::endl;
            return;
        }
        
        std::string desc;
        desc += "题目: " + ensure_utf8(prob.title) + "\n";
        desc += "ID: " + prob.id + "\n";
        desc += "来源: " + ensure_utf8(prob.source) + "\n";
        desc += "难度: " + ensure_utf8(prob.difficulty) + "\n";
        desc += "URL: " + ensure_utf8(prob.url) + "\n\n";

        if (fs::exists(prob.content_path)) {
            std::ifstream f(prob.content_path);
            std::string body(std::istreambuf_iterator<char>(f), {});
            if (!body.empty()) {
                desc += "题面/笔记文件内容:\n";
                desc += body.substr(0, 6000);
                desc += "\n\n";
            }
        }

        auto cases = svc.db->get_test_cases(prob.id);
        if (!cases.empty()) {
            desc += "测试用例(最多展示 3 个):\n";
            for (size_t i = 0; i < cases.size() && i < 3; i++) {
                desc += "Case " + std::to_string(i + 1) + " Input:\n" + cases[i].first.substr(0, 800) + "\n";
                desc += "Case " + std::to_string(i + 1) + " Expected:\n" + cases[i].second.substr(0, 800) + "\n\n";
            }
        }
        
        if (ctx.hint_file.empty()) {
            ctx.hint_file = find_solution_file(prob, svc.cfg.language);
            if (ctx.hint_file.empty()) ctx.hint_file = make_solution_filename(prob, svc.cfg.language);
        }
        std::string code = "";
        if (fs::exists(shuati::utils::utf8_path(ctx.hint_file))) {
            std::ifstream f(shuati::utils::utf8_path(ctx.hint_file));
            code.assign(std::istreambuf_iterator<char>(f), {});
        }

        if (ctx.stream_cb) {
            ctx.stream_cb("[教练] 思考中...\n\n");
        } else {
            std::cout << "[教练] 思考中 (按 Ctrl+C 中止)..." << std::endl << std::endl;
        }
        
        // Streaming output with robust tag filtering
        StreamFilter filter([&ctx](const std::string& text) {
            if (ctx.stream_cb) ctx.stream_cb(text);
            else std::cout << text << std::flush;
        });

        svc.ai->analyze(desc, code, [&filter](std::string chunk) {
            filter.feed(chunk);
        });
        
        filter.flush();
        if (!ctx.stream_cb) std::cout << "\n\n";
    } catch (const std::exception& e) {
        if (ctx.stream_cb) ctx.stream_cb(std::string("[!] 错误: ") + e.what() + "\n");
        else std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
