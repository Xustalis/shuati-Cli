#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include <string>
#include <iostream>
#include <fstream>

namespace shuati {
namespace cmd {

using shuati::utils::ensure_utf8;
namespace fs = std::filesystem;

void cmd_pull(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        svc.pm->pull_problem(ctx.pull_url);
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

void cmd_new(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        std::string pid = svc.pm->create_local(ctx.new_title, ctx.new_tags, ctx.new_diff);
        
        // Create directory structure
        fs::path prob_dir = fs::path(".shuati") / "problems" / pid;
        fs::create_directories(prob_dir / "data");
        fs::create_directories(prob_dir / "validator");
        fs::create_directories(prob_dir / "debug");
        fs::create_directories(prob_dir / "temp");

        // Create initial files
        std::ofstream(prob_dir / "problem.md") << "# " << ctx.new_title << "\n\nDescription here.\n";
        
        std::cout << "[+] 本地题目创建成功: " << ctx.new_title << " (" << pid << ")" << std::endl;
        std::cout << "    目录: " << prob_dir.string() << std::endl;
        std::cout << "    请在 data/ 放入测试用例，或在 validator/ 放入 gen.py/sol.py" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

void cmd_delete(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        
        if (ctx.solve_pid.empty()) {
            std::cout << "请输入要删除的题目 ID: ";
            std::cin >> ctx.solve_pid;
            if (ctx.solve_pid.empty()) return;
        }

        // Confirm deletion
        std::cout << "确定要删除题目 '" << ctx.solve_pid << "' 吗? [y/N] ";
        char confirm; std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消。" << std::endl;
            return;
        }

        try {
            int tid = std::stoi(ctx.solve_pid);
            svc.pm->delete_problem(tid);
        } catch (...) {
            svc.pm->delete_problem(ctx.solve_pid);
        }
        std::cout << "[+] 题目已删除。" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
