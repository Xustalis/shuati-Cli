#include "commands.hpp"
#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include <string>
// #include <ftxui/component/component.hpp>
// #include <ftxui/component/screen_interactive.hpp>
// #include <ftxui/dom/elements.hpp>
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
            // Interactive Selection
            // ftxui removed for build stability
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
        std::string filename = "solution_" + prob.id + ext;
        if (!fs::exists(filename)) {
            std::ofstream out(filename, std::ios::out | std::ios::binary);
            if (svc.cfg.language == "python") {
                out << "import sys\n\n\ndef main():\n    pass\n\n\nif __name__ == \"__main__\":\n    main()\n";
            } else {
                out << "#include <iostream>\n\nint main() {\n    std::ios::sync_with_stdio(false);\n    std::cin.tie(nullptr);\n\n    return 0;\n}\n";
            }
            std::cout << "[+] 代码文件已就绪: " << filename << std::endl;
        }
        
        if (!svc.cfg.editor.empty()) {
            std::string cmd = svc.cfg.editor + " \"" + filename + "\"";
            if (std::system(cmd.c_str()) != 0) {}
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

void cmd_submit(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        
        if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
            std::cout << "掌握程度 (0=不会, 5=秒杀): ";
            std::string val; std::getline(std::cin, val);
            try { ctx.submit_quality = std::stoi(val); } catch (...) { ctx.submit_quality = 3; }
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
        if (prob.id.empty()) { std::cerr << "[!] 题目不存在。" << std::endl; return; }
        
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
        
        if (ctx.hint_file.empty()) ctx.hint_file = "solution_" + prob.id + (svc.cfg.language == "python" ? ".py" : ".cpp");
        std::string code = "";
        if (fs::exists(ctx.hint_file)) {
            std::ifstream f(ctx.hint_file);
            code.assign(std::istreambuf_iterator<char>(f), {});
        }

        std::cout << "[教练] 思考中 (按 Ctrl+C 中止)..." << std::endl << std::endl;
        
        // Streaming output callback with filtering
        std::string buffer;
        bool suppress = false;
        
        auto print_chunk = [&](std::string chunk) {
            if (suppress) return;
            
            buffer += chunk;
            
            // Check for start marker
            size_t start_pos = buffer.find("<!-- SYSTEM_OP");
            if (start_pos != std::string::npos) {
                // Print pending valid text up to marker
                std::cout << buffer.substr(0, start_pos) << std::flush;
                buffer.clear(); // Clear printed
                suppress = true; // Stop printing subsequent chunks
            } else {
                // To be safe against split markers (e.g. "<!-", "- SY"), 
                // we should keep a small tail. 
                // Simple heuristic: Keep last 20 chars in buffer, print the rest.
                if (buffer.size() > 20) {
                    size_t print_len = buffer.size() - 20;
                    std::cout << buffer.substr(0, print_len) << std::flush;
                    buffer.erase(0, print_len);
                }
            }
        };

        svc.ai->analyze(desc, code, print_chunk);
        
        // Print remaining buffer if not suppressed (and no partial marker found at very end)
        if (!suppress && !buffer.empty()) {
             // Double check if buffer looks like start of marker
             if (buffer.find("<!--") == std::string::npos) {
                 std::cout << buffer << std::flush;
             }
        }
        std::cout << "\n\n";
    } catch (const std::exception& e) {
            std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
