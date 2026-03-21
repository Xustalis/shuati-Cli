#include "legacy_repl.hpp"
#include "commands.hpp"
#include "shuati/version.hpp"
#include "shuati/logger.hpp"
#include "shuati/boot_guard.hpp"
#include "shuati/utils/encoding.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <iostream>
#include <vector>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <replxx.hxx>

namespace shuati {
namespace cli {

void run_legacy_repl() {
    using Replxx = replxx::Replxx;
    Replxx rx;

    // Load global service for completion context
    auto root = Config::find_root();
    std::unique_ptr<cmd::Services> global_svc;
    if (!root.empty()) {
        try {
            global_svc = std::make_unique<cmd::Services>(cmd::Services::load(root));
            if (global_svc->companion) global_svc->companion->start();
        } catch (...) {}
    }

    // Set up completion
    rx.set_completion_callback([&](std::string const& input, int& contextLen) {
        std::vector<Replxx::Completion> completions;
        std::vector<std::string> cmds = {"init", "info", "pull", "new", "solve", "list", "delete", "record", "test", "hint", "config", "login", "repl", "exit", "view"};
        
        // Command completion
        size_t last_space = input.rfind(' ');
        if (last_space == std::string::npos) {
            for (auto& c : cmds) if (c.find(input) == 0) completions.push_back({c});
            contextLen = static_cast<int>(input.length());
        } else {
            // Context-aware completion
            std::string cmd = input.substr(0, last_space);
            std::string prefix = input.substr(last_space + 1);
            
            if (cmd == "solve" || cmd == "delete" || cmd == "record" || cmd == "hint" || cmd == "test" || cmd == "view") {
                if (global_svc && global_svc->pm) {
                    auto problems = global_svc->pm->list_problems();
                    for (const auto& p : problems) {
                        std::string label = std::to_string(p.display_id);
                        // Match against ID or Title
                        if (std::to_string(p.display_id).find(prefix) == 0 || p.title.find(prefix) == 0) {
                             completions.push_back({label});
                        }
                    }
                }
            }
            contextLen = prefix.length();
        }
        return completions;
    });

    fmt::print(fg(fmt::color::cyan), 
        R"(
  ███████╗██╗  ██╗██╗   ██╗ █████╗ ████████╗██╗
  ██╔════╝██║  ██║██║   ██║██╔══██╗╚══██╔══╝██║
  ███████╗███████║██║   ██║███████║   ██║   ██║
  ╚════██║██╔══██║██║   ██║██╔══██║   ██║   ██║
  ███████║██║  ██║╚██████╔╝██║  ██║   ██║   ██║
  ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝
        )" "\n");
    fmt::print("  输入 'help' 查看命令, 'exit' 退出\n\n");

    while (true) {
        const char* input = rx.input("shuati > ");
        if (input == nullptr) break;
        std::string line(input);
        if (line.empty()) continue;
        
        rx.history_add(line);

        std::stringstream ss(line);
        std::string word;
        std::vector<std::string> args;
        args.push_back("shuati"); 
        while (ss >> word) args.push_back(word);

        if (args.size() == 1) continue;
        if (args[1] == "exit" || args[1] == "quit") break;
        
        // ─── Shell Command Emulation ───
        if (args[1] == "ls" || args[1] == "dir") {
            try {
                auto cwd = std::filesystem::current_path();
                fmt::print(fg(fmt::color::cyan), "目录: {}\n\n", cwd.string().c_str());
                
                std::vector<std::filesystem::directory_entry> entries;
                for (const auto& entry : std::filesystem::directory_iterator(cwd)) {
                    entries.push_back(entry);
                }
                
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                    if (a.is_directory() != b.is_directory()) return a.is_directory();
                    return a.path().filename().string() < b.path().filename().string();
                });
                
                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory()) {
                        fmt::print(fg(fmt::color::blue), "  [DIR]  {}/\n", shuati::utils::ensure_utf8(name).c_str());
                    } else {
                        auto size = entry.file_size();
                        std::string size_str;
                        if (size < 1024) size_str = fmt::format("{} B", size);
                        else if (size < 1024 * 1024) size_str = fmt::format("{:.1f} KB", size / 1024.0);
                        else size_str = fmt::format("{:.1f} MB", size / (1024.0 * 1024.0));
                        fmt::print("  [FILE] {:<40} {}\n", shuati::utils::ensure_utf8(name).c_str(), size_str.c_str());
                    }
                }
                fmt::print("\n");
            } catch (const std::exception& e) {
                fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
            }
            continue;
        }
        
        if (args[1] == "cd") {
            if (args.size() < 3) {
                fmt::print(fg(fmt::color::yellow), "用法: cd <目录>\n");
                continue;
            }
            try {
                std::filesystem::path target = args[2];
                if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
                    std::filesystem::current_path(target);
                    fmt::print(fg(fmt::color::green), "[+] 已切换到: {}\n", std::filesystem::current_path().string());
                    BootGuard::record_history(target.string());
                    
                    // Reload services if we're in a valid project directory
                    auto new_root = Config::find_root();
                    if (!new_root.empty() && global_svc) {
                        try {
                            *global_svc = cmd::Services::load(new_root);
                            fmt::print(fg(fmt::color::cyan), "[*] 已重新加载项目上下文\n");
                        } catch (...) {}
                    }
                } else {
                    fmt::print(fg(fmt::color::red), "[!] 目录不存在: {}\n", target.string());
                }
            } catch (const std::exception& e) {
                fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
            }
            continue;
        }
        
        if (args[1] == "pwd") {
            fmt::print("{}\n", std::filesystem::current_path().string());
            continue;
        }

        if (args[1] == "cls" || args[1] == "clear") {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            continue;
        }
        
        if (args[1] == "help") { 
             fmt::print(fg(fmt::color::white), "{:<10} {:<35} {}\n", "命令", "说明", "用法");
             fmt::print("{}\n", std::string(80, '-'));
             
             // Shell commands
             fmt::print(fg(fmt::color::yellow), "Shell 命令:\n");
             fmt::print("{:<10} {:<35} {}\n", "ls/dir", "列出当前目录文件", "ls");
             fmt::print("{:<10} {:<35} {}\n", "cd", "切换目录 (自动重载项目)", "cd <目录>");
             fmt::print("{:<10} {:<35} {}\n", "pwd", "显示当前目录", "pwd");
             fmt::print("{:<10} {:<35} {}\n", "cls/clear", "清屏", "cls");
             fmt::print("\n");
             // Shuati commands
             fmt::print(fg(fmt::color::cyan), "Shuati 命令:\n");
             fmt::print("{:<10} {:<35} {}\n", "init", "初始化项目结构", "init");
             fmt::print("{:<10} {:<35} {}\n", "info", "显示可执行文件与环境信息", "info");
             fmt::print("{:<10} {:<35} {}\n", "pull", "从 URL 拉取题目", "pull <url>");
             fmt::print("{:<10} {:<35} {}\n", "new", "创建本地题目", "new <title>");
             fmt::print("{:<10} {:<35} {}\n", "solve", "开始做题 (支持交互选择)", "solve [id]");
             fmt::print("{:<10} {:<35} {}\n", "list", "列出所有题目", "list");
             fmt::print("{:<10} {:<35} {}\n", "view", "查看测试详情", "view <id>");
             fmt::print("{:<10} {:<35} {}\n", "delete", "删除题目", "delete <id>");
             fmt::print("{:<10} {:<35} {}\n", "record", "提交记录与心得", "record <id>");
             fmt::print("{:<10} {:<35} {}\n", "test", "运行测试用例", "test <id>");
             fmt::print("{:<10} {:<35} {}\n", "hint", "获取 AI 提示", "hint <id>");
             fmt::print("{:<10} {:<35} {}\n", "config", "配置工具", "config [--show]");
             fmt::print("{:<10} {:<40} {}\n", "", "  设置编辑器", "config --editor <cmd|auto>");
             fmt::print("{:<10} {:<40} {}\n", "", "  自动启动 REPL 开关", "config --autostart-repl <on|off>");
             fmt::print("{:<10} {:<40} {}\n", "", "  启动 UI 模式", "config --ui-mode <tui|legacy>");
             fmt::print("{:<10} {:<35} {}\n", "login", "配置平台登录 Cookie", "login lanqiao");
             fmt::print("{:<10} {:<35} {}\n", "repl", "手动进入交互模式", "repl");
             fmt::print("{:<10} {:<35} {}\n", "tui", "进入全屏 TUI", "tui");
             fmt::print("{:<10} {:<35} {}\n", "exit", "退出", "exit");
             fmt::print("\n");
             continue; 
        }

        CLI::App app{"REPL"};
        app.allow_extras();
        cmd::CommandContext ctx;
        cmd::setup_commands(app, ctx);

        std::vector<const char*> argv;
        for (const auto& a : args) argv.push_back(a.c_str());

        try {
            app.parse(argv.size(), const_cast<char**>(argv.data()));
        } catch (const CLI::ParseError& e) {
             app.exit(e);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 运行时错误: {}\n", e.what());
        }
    }
    
    if (global_svc && global_svc->companion) global_svc->companion->stop();
}

} // namespace cli
} // namespace shuati
