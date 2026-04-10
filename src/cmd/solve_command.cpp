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
            if (ctx.is_tui) {
                std::cout << "[Hint] 请先使用 /list 选题，然后运行 /solve <题号>。\n"
                          << "       例如: /solve 1" << std::endl;
                return;
            }

            auto problems = svc.pm->list_problems();
            if (problems.empty()) {
                std::cout << "Problem set is empty. Use pull or new first." << std::endl;
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
                        text("Choose a problem:") | ftxui::bold,
                        separator(),
                        menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 15),
                        separator(),
                        text("Up/Down to move, Enter to confirm, q/Esc to cancel") | dim,
                    }) | border;
                });

                screen.Loop(renderer);

                if (selected < 0 || selected >= static_cast<int>(problems.size())) {
                    std::cout << "Cancelled." << std::endl;
                    return;
                }
                prob = problems[selected];

            } catch (...) {
                std::cout << "Input problem ID: ";
                std::cin >> ctx.solve_pid;
                prob = svc.pm->get_problem(ctx.solve_pid);
            }

        } else {
            prob = svc.pm->get_problem(ctx.solve_pid);
        }

        if (prob.id.empty()) {
            std::cerr << "[!] Problem not found: " << ctx.solve_pid << std::endl;
            return;
        }

        std::cout << "=== Start solving: " << ensure_utf8(prob.title).c_str() << " ===" << std::endl;
        if (fs::exists(prob.content_path)) {
            std::cout << "Statement: " << prob.content_path.c_str() << std::endl;
        }

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
            std::cout << "[+] Solution file is ready: " << filename << std::endl;
        }

        if (!ctx.is_tui && !svc.cfg.editor.empty()) {
            std::string cmd = svc.cfg.editor + " \"" + filename + "\"";
            if (shuati::utils::utf8_system(cmd) != 0) {}
        } else if (ctx.is_tui) {
            std::cout << "[Hint] Solution file: " << filename << "\n"
                      << "       Editor launch is disabled inside TUI. Open it in another terminal." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

void cmd_record(CommandContext& ctx) {
    try {
        auto root = find_root_or_die();
        auto svc = Services::load(root);
        auto prob = svc.pm->get_problem(ctx.record_pid);

        if (prob.id.empty()) {
            std::cerr << "[!] Problem not found: " << ctx.record_pid << std::endl;
            return;
        }

        if (ctx.record_quality < 0 || ctx.record_quality > 5) {
            fs::path report_path = root / ".shuati" / "problems" / canonical_source(prob.source) / prob.id / "test_report.json";
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
                    std::cout << "[*] Auto quality from latest test: " << auto_q
                              << " (verdict=" << verdict << ", time=" << time_ms << "ms)" << std::endl;
                } catch (...) {
                }
            }

            if (ctx.is_tui) {
                ctx.record_quality = (auto_q >= 0) ? auto_q : 3;
                std::cout << "[TUI] Auto-selected quality: " << ctx.record_quality
                          << ". Override with /record <id> -q <0-5> if needed." << std::endl;
            } else if (auto_q >= 0) {
                std::cout << "Press Enter to accept [" << auto_q << "], or input 0-5 to override: ";
                std::string val;
                std::getline(std::cin, val);
                if (val.empty()) {
                    ctx.record_quality = auto_q;
                } else {
                    try { ctx.record_quality = std::stoi(val); } catch (...) { ctx.record_quality = auto_q; }
                }
            } else {
                std::cout << "Mastery (0=not yet, 5=fully mastered): ";
                std::string val;
                std::getline(std::cin, val);
                try { ctx.record_quality = std::stoi(val); } catch (...) { ctx.record_quality = 3; }
            }
        }

        if (ctx.record_quality < 3) {
            if (ctx.is_tui) {
                svc.ma->log_mistake(prob.id, "manual-review", "Recorded from TUI with low confidence");
                std::cout << "[TUI] Logged a low-confidence record marker."
                          << " Add details later with shuati record " << prob.display_id
                          << " -q " << ctx.record_quality << std::endl;
            } else {
                std::cout << "Mistake type (logic/detail/algorithm/TLE): ";
                std::string type, desc;
                std::getline(std::cin, type);
                std::cout << "Details (or leave empty): ";
                std::getline(std::cin, desc);
                svc.ma->log_mistake(prob.id, type, desc);
            }
        }

        auto review = svc.db->get_review(prob.id);
        review = SM2Algorithm::update(review, ctx.record_quality);
        svc.db->upsert_review(review);
        std::cout << "[+] Recorded. Next review interval: " << review.interval << " days." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

void cmd_hint(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        auto prob = svc.pm->get_problem(ctx.hint_pid);
        if (prob.id.empty()) {
            if (ctx.stream_cb) ctx.stream_cb("[!] Problem not found.\n");
            else std::cerr << "[!] Problem not found." << std::endl;
            return;
        }

        std::string desc;
        desc += "Problem: " + ensure_utf8(prob.title) + "\n";
        desc += "ID: " + prob.id + "\n";
        desc += "Source: " + ensure_utf8(prob.source) + "\n";
        desc += "Difficulty: " + ensure_utf8(prob.difficulty) + "\n";
        desc += "URL: " + ensure_utf8(prob.url) + "\n\n";

        if (fs::exists(prob.content_path)) {
            std::ifstream f(prob.content_path);
            std::string body(std::istreambuf_iterator<char>(f), {});
            if (!body.empty()) {
                desc += "Statement / notes:\n";
                desc += body.substr(0, 6000);
                desc += "\n\n";
            }
        }

        auto cases = svc.db->get_test_cases(prob.id);
        if (!cases.empty()) {
            desc += "Test cases (up to 3):\n";
            for (size_t i = 0; i < cases.size() && i < 3; i++) {
                desc += "Case " + std::to_string(i + 1) + " Input:\n" + cases[i].first.substr(0, 800) + "\n";
                desc += "Case " + std::to_string(i + 1) + " Expected:\n" + cases[i].second.substr(0, 800) + "\n\n";
            }
        }

        if (ctx.hint_file.empty()) {
            ctx.hint_file = find_solution_file(prob, svc.cfg.language);
            if (ctx.hint_file.empty()) ctx.hint_file = make_solution_filename(prob, svc.cfg.language);
        }
        std::string code;
        if (fs::exists(shuati::utils::utf8_path(ctx.hint_file))) {
            std::ifstream f(shuati::utils::utf8_path(ctx.hint_file));
            code.assign(std::istreambuf_iterator<char>(f), {});
        }

        if (ctx.stream_cb) {
            ctx.stream_cb("[Coach] Thinking...\n\n");
        } else {
            std::cout << "[Coach] Thinking... (Ctrl+C to stop)" << std::endl << std::endl;
        }

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
        if (ctx.stream_cb) ctx.stream_cb(std::string("[!] Error: ") + e.what() + "\n");
        else std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
