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
        std::cout << "[*] 正在拉取题目: " << ctx.pull_url << std::endl;
        svc.pm->pull_problem(ctx.pull_url);
        std::cout << "[+] 拉取完成。" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] 拉取失败: " << e.what() << std::endl;
    }
}

void cmd_new(CommandContext& ctx) {
    try {
        auto root = find_root_or_die();
        auto svc = Services::load(root);
        std::string pid = svc.pm->create_local(ctx.new_title, ctx.new_tags, ctx.new_diff);
        
        // Create directory structure
        fs::path prob_dir = root / ".shuati" / "problems" / "local" / pid;
        fs::create_directories(prob_dir / "data");
        fs::create_directories(prob_dir / "validator");
        fs::create_directories(prob_dir / "debug");
        fs::create_directories(prob_dir / "temp");

        // Statement is already created by create_local in root/.shuati/problems/pid.md
        
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
            if (ctx.is_tui) {
                std::cout << "[用法] /delete <ID> — 请直接提供题目 ID" << std::endl;
                return;
            }
            std::cout << "请输入要删除的题目 ID: ";
            std::cin >> ctx.solve_pid;
            if (ctx.solve_pid.empty()) return;
        }

        // Confirm deletion (skip in TUI — no stdin available)
        if (!ctx.is_tui) {
            std::cout << "确定要删除题目 '" << ctx.solve_pid << "' 吗? [y/N] ";
            char confirm; std::cin >> confirm;
            if (confirm != 'y' && confirm != 'Y') {
                std::cout << "操作取消。" << std::endl;
                return;
            }
        }

        auto prob = svc.pm->get_problem(ctx.solve_pid);
        if (prob.id.empty()) {
            std::cerr << "[!] 未找到题目: " << ctx.solve_pid << std::endl;
            return;
        }
        svc.pm->delete_problem(prob.id);
        std::cout << "[+] 题目已删除。" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}


void cmd_clean(CommandContext& ctx) {
    (void)ctx;
    try {
        fs::path root;
        try {
            root = find_root_or_die();
        } catch (...) {
            // If not in root, maybe just return? Or try current dir?
            // find_root_or_die prints error and throws.
            return;
        }

        fs::path shuati_dir = root / ".shuati";
        int removed_count = 0;
        uintmax_t removed_size = 0;

        auto remove_file = [&](const fs::path& p) {
            std::error_code ec;
            uintmax_t size = fs::file_size(p, ec);
            if (fs::remove(p, ec)) {
                removed_count++;
                if (!ec) removed_size += size;
                // std::cout << "Del: " << p.filename().string() << std::endl;
            }
        };

        // 1. Clean global temp
        fs::path global_temp = shuati_dir / "temp";
        if (fs::exists(global_temp)) {
            for (const auto& entry : fs::directory_iterator(global_temp)) {
                remove_file(entry.path());
            }
        }

        // 2. Clean problem temps
        fs::path problems_dir = shuati_dir / "problems";
        if (fs::exists(problems_dir)) {
            for (const auto& src_entry : fs::directory_iterator(problems_dir)) {
                if (!src_entry.is_directory()) continue;
                for (const auto& p_entry : fs::directory_iterator(src_entry.path())) {
                    if (!p_entry.is_directory()) continue;
                    
                    fs::path p_temp = p_entry.path() / "temp";
                    if (fs::exists(p_temp)) {
                        for (const auto& entry : fs::directory_iterator(p_temp)) {
                            remove_file(entry.path());
                        }
                    }
                }
            }
        }

        // 3. Clean root directory build artifacts only
        // Strict policy: only remove files that are BOTH prefixed with "solution_"
        // AND have a known build artifact extension. This prevents accidental deletion
        // of user files that happen to start with "solution_".
        const std::vector<std::string> artifact_exts = {".exe", ".o", ".obj", ".out"};
        for (const auto& entry : fs::directory_iterator(root)) {
            if (entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            std::string ext = entry.path().extension().string();
            
            // Must start with "solution_" AND have a recognized build artifact extension
            if (name.rfind("solution_", 0) == 0) {
                bool is_artifact = false;
                for (const auto& allowed_ext : artifact_exts) {
                    if (ext == allowed_ext) {
                        is_artifact = true;
                        break;
                    }
                }
                if (is_artifact) {
                    remove_file(entry.path());
                }
            }
        }

        std::cout << "[+] 清理完成。共删除 " << removed_count << " 个文件 (" << (removed_size / 1024.0) << " KB)。" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
