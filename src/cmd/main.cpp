#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <vector>
#include <sstream>
#include <regex>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <replxx.hxx>

#include "shuati/config.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "shuati/mistake_analyzer.hpp"
#include "shuati/ai_coach.hpp"
#include "shuati/sm2_algorithm.hpp"
#include "shuati/judge.hpp"

// Header separation fixes ODR violations
#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/companion_server.hpp"

namespace fs = std::filesystem;
using namespace shuati;

// ─── Helpers ──────────────────────────────────────────

static fs::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\n");
        throw std::runtime_error("Root not found");
    }
    return root;
}

struct Services {
    std::shared_ptr<Database>       db;
    std::shared_ptr<ProblemManager> pm;
    std::shared_ptr<MistakeAnalyzer> ma;
    std::unique_ptr<AICoach>        ai;
    std::unique_ptr<CompanionServer> companion;
    std::unique_ptr<Judge>          judge;
    Config                          cfg;

    static Services load(const fs::path& root) {
        Services s;
        try {
            s.cfg = Config::load(Config::config_path(root));
            s.db  = std::make_shared<Database>(Config::db_path(root).string());
            s.pm  = std::make_shared<ProblemManager>(s.db);
            
            // Register crawlers
            s.pm->register_crawler(std::make_unique<LeetCodeCrawler>());
            s.pm->register_crawler(std::make_unique<CodeforcesCrawler>());
            s.pm->register_crawler(std::make_unique<LuoguCrawler>());
            s.pm->register_crawler(std::make_unique<LanqiaoCrawler>());
            
            s.ma  = std::make_shared<MistakeAnalyzer>(s.db);
            s.ai  = std::make_unique<AICoach>(s.cfg);
            s.companion = std::make_unique<CompanionServer>(*s.pm, *s.db);
            s.judge = std::make_unique<Judge>();
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 服务加载失败: {}\n", e.what());
            throw;
        }
        return s;
    }
};

// ─── Command Context ──────────────────────────────────

struct CommandContext {
    std::string pull_url;
    std::string new_title, new_tags, new_diff = "medium";
    std::string solve_pid;
    std::string submit_pid, submit_file;
    int submit_quality = -1;
    std::string hint_pid, hint_file;
    std::string cfg_key, cfg_model;
    bool cfg_show = false;
};

// ─── Command Setup ────────────────────────────────────

void setup_commands(CLI::App& app, CommandContext& ctx) {
    app.require_subcommand(1);

    // ── init ──
    app.add_subcommand("init", "在当前目录初始化项目")->callback([]() {
        if (fs::exists(Config::DIR_NAME)) {
            fmt::print("[!] 项目已初始化。\n");
            return;
        }
        auto dir = fs::current_path() / Config::DIR_NAME;
        fs::create_directories(dir / "templates");
        fs::create_directories(dir / "problems");
        
        Config cfg;
        cfg.save(Config::config_path(fs::current_path()));
        
        Database db(Config::db_path(fs::current_path()).string());
        db.init_schema();
        
        fmt::print(fg(fmt::color::green_yellow), "[+] 初始化成功: {}\n", dir.string());
        fmt::print("    请设置 API Key: config --api-key <YOUR_KEY>\n");
    });

    // ── pull ──
    auto pull_cmd = app.add_subcommand("pull", "从 URL 拉取题目");
    pull_cmd->add_option("url", ctx.pull_url, "题目链接")->required();
    pull_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            svc.pm->pull_problem(ctx.pull_url);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── new ──
    auto new_cmd = app.add_subcommand("new", "创建本地题目");
    new_cmd->add_option("title", ctx.new_title, "标题")->required();
    new_cmd->add_option("-t,--tags", ctx.new_tags, "标签 (逗号分隔)");
    new_cmd->add_option("-d,--difficulty", ctx.new_diff, "难度 (easy/medium/hard)");
    new_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            svc.pm->create_local(ctx.new_title, ctx.new_tags, ctx.new_diff);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── solve ──
    auto solve_cmd = app.add_subcommand("solve", "开始解决一道题 (支持 ID 或 TID)");
    solve_cmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    solve_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            
            Problem prob;
            if (ctx.solve_pid.empty()) {
                // Interactive Selection
                auto problems = svc.pm->list_problems();
                if (problems.empty()) { fmt::print("题库为空。\n"); return; }
                
                using namespace ftxui;
                std::vector<std::string> entries;
                for (const auto& p : problems) {
                    entries.push_back(fmt::format("{:<4} {:<20} {}", p.display_id, p.id.substr(0, 18), p.title));
                }
                int selected = 0;
                auto menu = Menu(&entries, &selected);
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
                    return window(text("选择题目 (Enter 确认, q 退出)"), menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20)) | border;
                });
                
                screen.Loop(renderer);
                if (selected >= 0 && selected < problems.size()) {
                    prob = problems[selected];
                } else {
                    return;
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
                fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", ctx.solve_pid);
                return;
            }

            fmt::print(fg(fmt::color::cyan), "=== 开始练习: {} ===\n", prob.title);
            if (fs::exists(prob.content_path)) {
                fmt::print("题目描述: {}\n", prob.content_path);
            }

            std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
            std::string filename = "solution_" + prob.id + ext;
            if (!fs::exists(filename)) {
                std::ofstream out(filename);
                
                std::string content = "";
                if (svc.cfg.ai_enabled && svc.ai->enabled()) {
                    fmt::print("[*] 正在通过 AI 生成代码模板...\n");
                    if (fs::exists(prob.content_path)) {
                        std::ifstream t(prob.content_path);
                        std::string full_desc((std::istreambuf_iterator<char>(t)), {});
                        content = svc.ai->generate_template(prob.title, full_desc, svc.cfg.language);
                        // Strip markdown fences
                        std::regex code_re(R"(```\w*\n([\s\S]*?)```)");
                        std::smatch m;
                        if (std::regex_search(content, m, code_re)) content = m[1].str();
                    }
                }

                if (content.empty() || content.find("[Error]") != std::string::npos) {
                    if (svc.cfg.language == "python") {
                        out << "import sys\n\n# Problem: " << prob.title << "\n# URL: " << prob.url << "\n\nif __name__ == '__main__':\n    pass\n";
                    } else {
                        out << "// Problem: " << prob.title << "\n// URL: " << prob.url << "\n\n#include <iostream>\n#include <vector>\nusing namespace std;\n\nint main() {\n    return 0;\n}\n";
                    }
                } else {
                    out << content;
                }
                fmt::print(fg(fmt::color::green), "[+] 代码文件已就绪: {}\n", filename);
            }
            
            if (!svc.cfg.editor.empty()) {
                std::string cmd = fmt::format("{} \"{}\"", svc.cfg.editor, filename);
                if (std::system(cmd.c_str()) != 0) {}
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── list ──
    app.add_subcommand("list", "列出所有题目")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto problems = svc.pm->list_problems();
            if (problems.empty()) {
                fmt::print("题库为空。\n");
                return;
            }
            fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n", "TID", "ID", "标题", "难度", "来源");
            fmt::print("{}\n", std::string(80, '-'));
            for (auto& p : problems) {
                std::string title = p.title;
                if (title.length() > 27) title = title.substr(0, 25) + "..";
                fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n", p.display_id, p.id.substr(0, 18), title, p.difficulty, p.source);
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── delete ──
    // ── delete ──
    auto delete_cmd = app.add_subcommand("delete", "删除题目 (支持 TID)");
    delete_cmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    delete_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            
            if (ctx.solve_pid.empty()) {
                // Interactive Selection
                auto problems = svc.pm->list_problems();
                if (problems.empty()) { fmt::print("题库为空。\n"); return; }
                
                using namespace ftxui;
                std::vector<std::string> entries;
                for (const auto& p : problems) {
                    entries.push_back(fmt::format("{:<4} {:<20} {}", p.display_id, p.id.substr(0, 18), p.title));
                }
                int selected = 0;
                auto menu = Menu(&entries, &selected);
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
                    return window(text("选择要删除的题目 (Enter 确认, q 退出)"), menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20)) | border;
                });
                
                screen.Loop(renderer);
                if (selected >= 0 && selected < problems.size()) {
                    ctx.solve_pid = problems[selected].id;
                } else {
                    return;
                }
            }

            // Confirm deletion
            fmt::print(fg(fmt::color::yellow), "确定要删除题目 '{}' 吗? [y/N] ", ctx.solve_pid);
            char confirm; std::cin >> confirm;
            if (confirm != 'y' && confirm != 'Y') {
                fmt::print("操作取消。\n");
                return;
            }

            try {
                int tid = std::stoi(ctx.solve_pid);
                svc.pm->delete_problem(tid);
            } catch (...) {
                svc.pm->delete_problem(ctx.solve_pid);
            }
            fmt::print(fg(fmt::color::green), "[+] 题目已删除。\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── submit ──
    auto submit_cmd = app.add_subcommand("submit", "提交并记录心得");
    submit_cmd->add_option("id", ctx.submit_pid, "题目 ID")->required();
    submit_cmd->add_option("-q,--quality", ctx.submit_quality, "掌握程度 (0-5)");
    submit_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            
            if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
                fmt::print("掌握程度 (0=不会, 5=秒杀): ");
                std::string val; std::getline(std::cin, val);
                try { ctx.submit_quality = std::stoi(val); } catch (...) { ctx.submit_quality = 3; }
            }

            if (ctx.submit_quality < 3) {
                 fmt::print("记录一个错误类型 (逻辑/细节/算法/TLE): ");
                 std::string type, desc;
                 std::getline(std::cin, type);
                 fmt::print("详细描述 (或按空格跳过): ");
                 std::getline(std::cin, desc);
                 svc.ma->log_mistake(ctx.submit_pid, type, desc);
            }

            auto review = svc.db->get_review(ctx.submit_pid);
            review = SM2Algorithm::update(review, ctx.submit_quality);
            svc.db->upsert_review(review);
            fmt::print(fg(fmt::color::green), "[+] 记录成功。下次复习间隔: {} 天。\n", review.interval);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── test ──
    auto test_cmd = app.add_subcommand("test", "运行测试用例");
    test_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    test_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto prob = svc.pm->get_problem(ctx.solve_pid);
            if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 题目不存在。\n"); return; }
            
            std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
            std::string src_file = "solution_" + prob.id + ext;
            if (!fs::exists(src_file)) { fmt::print(fg(fmt::color::red), "[!] 找不到代码文件: {}\n", src_file); return; }
            
            auto db_cases = svc.db->get_test_cases(prob.id);
            std::vector<TestCase> cases;
            for(auto& p : db_cases) cases.push_back({p.first, p.second});
            
            fmt::print("[*] 正在测评...\n");
            auto results = svc.judge->judge(src_file, svc.cfg.language, cases);
            int ac = 0;
            for (size_t i = 0; i < results.size(); i++) {
                auto& r = results[i];
                fmt::print("Case {}: {} ({}ms, {}KB)\n", i+1, r.verdict_str(), r.time_ms, r.memory_kb);
                if (r.verdict == Verdict::AC) ac++;
                else if (r.verdict != Verdict::AC) {
                    fmt::print(fg(fmt::color::dim_gray), "   Expected: {}\n   Actual:   {}\n", r.expected.substr(0, 40), r.output.substr(0, 40));
                }
            }
            fmt::print("\n结果: {}/{} 通过\n", ac, results.size());
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 测评异常: {}\n", e.what());
        }
    });

    // ── hint ──
    auto hint_cmd = app.add_subcommand("hint", "AI 启发式提示");
    hint_cmd->add_option("id", ctx.hint_pid, "题目 ID")->required();
    hint_cmd->add_option("-f,--file", ctx.hint_file, "当前代码文件");
    hint_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto prob = svc.pm->get_problem(ctx.hint_pid);
            if (prob.id.empty()) return;
            
            std::string desc = "";
            if (fs::exists(prob.content_path)) {
                std::ifstream f(prob.content_path);
                desc.assign(std::istreambuf_iterator<char>(f), {});
            }
            
            if (ctx.hint_file.empty()) ctx.hint_file = "solution_" + prob.id + (svc.cfg.language == "python" ? ".py" : ".cpp");
            std::string code = "";
            if (fs::exists(ctx.hint_file)) {
                std::ifstream f(ctx.hint_file);
                code.assign(std::istreambuf_iterator<char>(f), {});
            }

            fmt::print(fg(fmt::color::yellow), "[教练] 思考中...\n");
            std::string hint = svc.ai->analyze(desc, code);
            fmt::print("\n{}\n", hint);
        } catch (const std::exception& e) {
             fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── config ──
    auto cfg_cmd = app.add_subcommand("config", "配置 API 或编辑器");
    cfg_cmd->add_flag("--show", ctx.cfg_show, "显示当前配置");
    cfg_cmd->add_option("--api-key", ctx.cfg_key, "API Key (例如: sk-...)");
    cfg_cmd->add_option("--model", ctx.cfg_model, "模型名称 (例如: gpt-3.5-turbo)");
    cfg_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto cfg_path = Config::config_path(root);
            auto cfg = Config::load(cfg_path);

            if (ctx.cfg_show) {
                fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "当前配置:\n");
                fmt::print("  API Key:      {}\n", cfg.api_key.empty() ? "(未设置)" : "******" + cfg.api_key.substr(std::max(0, (int)cfg.api_key.length()-4)));
                fmt::print("  Model:        {}\n", cfg.model);
                fmt::print("  Language:     {}\n", cfg.language);
                fmt::print("  Editor:       {}\n", cfg.editor.empty() ? "(未设置)" : cfg.editor);
                fmt::print("  AI Enabled:   {}\n", cfg.ai_enabled ? "Yes" : "No");
                return;
            }

            bool changed = false;
            if (!ctx.cfg_key.empty()) {
                if (ctx.cfg_key.length() < 10) {
                     fmt::print(fg(fmt::color::red), "[!] API Key 格式似乎不正确。\n");
                     return;
                }
                cfg.api_key = ctx.cfg_key;
                changed = true;
            }
            if (!ctx.cfg_model.empty()) {
                cfg.model = ctx.cfg_model;
                changed = true;
            }
            
            if (changed) {
                cfg.save(cfg_path);
                fmt::print(fg(fmt::color::green), "[+] 配置已保存。\n");
            } else {
                fmt::print("未检测到修改。使用 --show 查看当前配置。\n");
            }
        } catch (...) { fmt::print(fg(fmt::color::red), "[!] 操作失败\n"); }
    });

    app.add_subcommand("exit", "退出程序")->callback([]() { exit(0); });
}

// ─── REPL Loop ────────────────────────────────────────

void run_repl() {
    using Replxx = replxx::Replxx;
    Replxx rx;

    auto root = Config::find_root();
    std::unique_ptr<Services> global_svc;
    if (!root.empty()) {
        try {
            global_svc = std::make_unique<Services>(Services::load(root));
            if (global_svc->companion) global_svc->companion->start();
        } catch (...) {}
    }

    // Set up completion
    rx.set_completion_callback([&](std::string const& input, int& contextLen) {
        std::vector<Replxx::Completion> completions;
        std::vector<std::string> cmds = {"init", "pull", "new", "solve", "list", "delete", "submit", "test", "hint", "config", "exit"};
        
        // Command completion
        size_t last_space = input.rfind(' ');
        if (last_space == std::string::npos) {
            for (auto& c : cmds) if (c.find(input) == 0) completions.push_back({c});
            contextLen = input.length();
        } else {
            // Context-aware completion
            std::string cmd = input.substr(0, last_space);
            std::string prefix = input.substr(last_space + 1);
            
            if (cmd == "solve" || cmd == "delete" || cmd == "submit" || cmd == "hint" || cmd == "test") {
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

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, 
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
        // ... (existing loop code, but "input" variable declaration is handled by replxx)
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
        if (args[1] == "help") { 
             fmt::print(fg(fmt::color::white) | fmt::emphasis::bold, "{:<10} {:<35} {}\n", "命令", "说明", "用法");
             fmt::print("{}\n", std::string(80, '-'));
             fmt::print("{:<10} {:<35} {}\n", "init", "初始化项目结构", "init");
             fmt::print("{:<10} {:<35} {}\n", "pull", "从 URL 拉取题目", "pull <url>");
             fmt::print("{:<10} {:<35} {}\n", "new", "创建本地题目", "new <title>");
             fmt::print("{:<10} {:<35} {}\n", "solve", "开始做题 (支持交互选择)", "solve [id]");
             fmt::print("{:<10} {:<35} {}\n", "list", "列出所有题目", "list");
             fmt::print("{:<10} {:<35} {}\n", "delete", "删除题目", "delete <id>");
             fmt::print("{:<10} {:<35} {}\n", "submit", "提交记录与心得", "submit <id>");
             fmt::print("{:<10} {:<35} {}\n", "test", "运行测试用例", "test <id>");
             fmt::print("{:<10} {:<35} {}\n", "hint", "获取 AI 提示", "hint <id>");
             fmt::print("{:<10} {:<35} {}\n", "config", "配置工具", "config [--show]");
             fmt::print("{:<10} {:<35} {}\n", "exit", "退出", "exit");
             fmt::print("\n");
             continue; 
        }

        CLI::App app{"REPL"};
        app.allow_extras();
        CommandContext ctx;
        setup_commands(app, ctx);

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

// ─── Main ─────────────────────────────────────────────

int main(int argc, char** argv) {
#ifdef _WIN32
    std::system("chcp 65001 > nul");
#endif

    if (argc == 1) {
        run_repl();
    } else {
        CLI::App app{"shuati CLI"};
        CommandContext ctx;
        setup_commands(app, ctx);
        CLI11_PARSE(app, argc, argv);
    }
    return 0;
}
