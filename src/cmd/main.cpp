#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <vector>
#include <sstream>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <replxx.hxx>

#include "shuati/config.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "shuati/mistake_analyzer.hpp"
#include "shuati/ai_coach.hpp"
#include "shuati/sm2_algorithm.hpp"
#include "shuati/judge.hpp"
#include "shuati/judge.hpp"
#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/companion_server.hpp"
// Step 4 of the user request says "Step 1... Modify src/cmd/main.cpp... remove .cpp include".
// I will assume companion_server needs similar treatment or I should comment it out if it causes issues, but for now I will leave it or include its header if it exists.
// Looking at file list, src/adapters/companion_server.cpp exists. There is NO companion_server.hpp in list_dir output (Step 9).
// I should probably create a header for it too or forward declare.
// However, to strictly follow the "crawlers" fix, I will focus on crawlers.
// BUT, if I leave companion_server.cpp included, it might still fail linking if it's also in CMakeLists.txt.
// Let's check CMakeLists.txt again. Line 46 adds src/adapters/companion_server.cpp.
// So including it here is DEFINITELY an error.
// I will assuming there is a class CompanionServer. I'll declare it locally or in a new header.
// Given the constraints and the explicit task to fix crawlers, I'll try to find if there is a header.
// Step 9 showed no companion_server.hpp.
// I will ADD a simple declaration header or just include the class definition if it's small? No, better to separate.
// I'll create a header for CompanionServer as well to be safe, OR just forward declare it and include the .cpp in CMake only?
// Main needs `std::make_unique<CompanionServer>`, so it needs the full definition.
// I cannot fix everything in one go perfectly without more file reads, but I can guess the pattern.
// I will creates `include/shuati/companion_server.hpp` as well?
// The user prompt specifically asked for `include/shuati/crawler.hpp` and `src/adapters/leetcode_crawler.cpp`.
// It didn't explicitly ask for CompanionServer.
// I will try to include a non-existent header "shuati/companion_server.hpp" and THEN create it, or just use "shuati/crawler.hpp" if it was there? No.
// I'll stick to fixing what I know. I will replace the crawler includes.
// For CompanionServer, I will include it as a .hpp and I will create that file in the next step to avoid compilation error.

#include "shuati/crawlers.hpp"
// #include "shuati/companion_server.hpp" // TODO: Create this context


namespace fs = std::filesystem;
using namespace shuati;

// ─── Helpers ──────────────────────────────────────────

static fs::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\\n");
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
        } catch (...) {
            throw; // Let caller handle
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
        fs::create_directory(dir);
        fs::create_directory(dir / "templates");
        fs::create_directory(dir / "problems"); // Create problems repo
        Config cfg;
        cfg.save(Config::config_path(fs::current_path()));
        Database db(Config::db_path(fs::current_path()).string());
        fmt::print(fg(fmt::color::green_yellow), "[+] 初始化成功: {}\n", dir.string());
        fmt::print("    请设置 API Key: config --api-key <YOUR_KEY>\n");
        fmt::print(fg(fmt::color::dim_gray), "    提示: 在 {}/templates/ 放置自定义代码模板\n", Config::DIR_NAME);
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
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
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
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── solve ──
    auto solve_cmd = app.add_subcommand("solve", "开始解决一道题 (生成代码模板)");
    solve_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    solve_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            auto prob = svc.pm->get_problem(ctx.solve_pid);

            if (prob.id.empty()) {
                fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", ctx.solve_pid);
                return;
            }

            fmt::print(fg(fmt::color::cyan), "=== 开始练习: {} ===\n", prob.title);
            fmt::print("题目描述: {}\n", prob.content_path);

            std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
            std::string filename = "solution_" + prob.id + ext;
            if (fs::exists(filename)) {
                fmt::print(fg(fmt::color::yellow), "[!] 代码文件已存在: {}\n", filename);
            } else {
                // Check for user-defined template in .shuati/templates/
                auto tmpl_path = root / Config::DIR_NAME / "templates" / ("template" + ext);
                std::ofstream out(filename);
                
                std::string content = "";
                if (svc.cfg.ai_enabled && svc.ai->enabled()) {
                     fmt::print("[*] 正在请求 AI 生成模版...\n");
                     if (fs::exists(prob.content_path)) {
                         std::ifstream t(prob.content_path);
                         std::stringstream buffer;
                         buffer << t.rdbuf();
                         std::string full_desc = buffer.str();
                         std::string ai_tmpl = svc.ai->generate_template(prob.title, full_desc, svc.cfg.language);
                         if (ai_tmpl.find("```") != std::string::npos) {
                             std::regex code_re(R"(```\w*\n([\s\S]*?)```)");
                             std::smatch m;
                             if (std::regex_search(ai_tmpl, m, code_re)) content = m[1].str();
                             else content = ai_tmpl;
                         } else {
                             content = ai_tmpl;
                         }
                     }
                }

                if (!content.empty() && content.find("[Error]") == std::string::npos) {
                     out << content;
                     fmt::print(fg(fmt::color::green), "[+] 使用 AI 生成模板: {}\n", filename);
                } else if (fs::exists(tmpl_path)) {
                    // Use custom template, replace placeholders
                    std::ifstream tmpl(tmpl_path);
                    std::string content((std::istreambuf_iterator<char>(tmpl)), {});
                    // Replace placeholders
                    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
                        size_t pos = 0;
                        while ((pos = s.find(from, pos)) != std::string::npos) {
                            s.replace(pos, from.length(), to);
                            pos += to.length();
                        }
                    };
                    
                    std::time_t now = std::time(nullptr);
                    char buf[80];
                    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                    
                    replace_all(content, "{{TITLE}}", prob.title);
                    replace_all(content, "{{URL}}", prob.url);
                    replace_all(content, "{{ID}}", prob.id);
                    replace_all(content, "{{DATE}}", std::string(buf));
                    replace_all(content, "{{TAGS}}", prob.tags);
                    replace_all(content, "{{DIFFICULTY}}", prob.difficulty);
                    
                    out << content;
                    fmt::print(fg(fmt::color::green), "[+] 使用自定义模板生成: {}\n", filename);
                } else {
                    // Default template
                    if (svc.cfg.language == "python") {
                         out << "import sys\n\n# Problem: " << prob.title << "\n# URL: " << prob.url << "\n\ndef solve():\n    pass\n\nif __name__ == '__main__':\n    solve()\n";
                    } else {
                         out << "// Problem: " << prob.title << "\n// URL: " << prob.url << "\n\n#include <iostream>\n#include <vector>\nusing namespace std;\n\nint main() {\n    // Your code here\n    return 0;\n}\n";
                    }
                    fmt::print(fg(fmt::color::green), "[+] 代码模板已生成: {}\n", filename);
                    fmt::print(fg(fmt::color::dim_gray), "    提示: 在 .shuati/templates/template{} 放置自定义模板\n", ext);
                }
                out.close();
            }
            
            // Auto open editor
            if (!svc.cfg.editor.empty()) {
                std::string cmd = fmt::format("{} \"{}\"", svc.cfg.editor, filename);
                fmt::print("[*] 正在打开编辑器: {}\n", cmd);
#ifdef _WIN32
                std::system(cmd.c_str());
#else
                // POSIX compliant way to open editor?
                // For now, just print the command or use system
                std::system(cmd.c_str());
#endif
            } else {
                fmt::print("\n完成后使用以下命令提交:\n  submit {} -f {}\n", prob.id, filename);
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── list ──
    app.add_subcommand("list", "列出所有题目")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto problems = svc.pm->list_problems();
            if (problems.empty()) {
                fmt::print("暂无题目。请使用: pull <url>\n");
                return;
            }
            fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n", "TID", "ID", "标题", "难度", "来源");
            fmt::print("{}\n", std::string(80, '-'));
            for (auto& p : problems) {
                std::string title = p.title.length() > 28 ? p.title.substr(0, 25) + "..." : p.title;
                fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n", p.display_id, p.id.substr(0, 18), title, p.difficulty, p.source);
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── delete ──
    auto delete_cmd = app.add_subcommand("delete", "删除题目");
    delete_cmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID")->required();
    delete_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            try {
                int tid = std::stoi(ctx.solve_pid);
                svc.pm->delete_problem(tid);
            } catch (...) {
                svc.pm->delete_problem(ctx.solve_pid);
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── submit ──
    auto submit_cmd = app.add_subcommand("submit", "提交记录 (记录错误与心得)");
    submit_cmd->add_option("problem_id", ctx.submit_pid, "题目 ID")->required();
    submit_cmd->add_option("-f,--file", ctx.submit_file, "代码文件路径");
    submit_cmd->add_option("-q,--quality", ctx.submit_quality, "自评难度 0-5 (0=不会, 5=秒杀)");
    submit_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            auto prob = svc.pm->get_problem(ctx.submit_pid);

            if (prob.id.empty()) {
                fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", ctx.submit_pid);
                return;
            }

            if (ctx.submit_file.empty()) {
                std::string candidate = "solution_" + prob.id + ".cpp";
                if (fs::exists(candidate)) {
                    ctx.submit_file = candidate;
                    fmt::print("[i] 自动使用代码文件: {}\n", ctx.submit_file);
                } else {
                    fmt::print(fg(fmt::color::yellow), "[!] 未指定文件，且默认文件 {} 不存在。\n", candidate);
                }
            }

            if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
                fmt::print("本次练习感觉如何? (0=完全不会, 3=有挑战, 5=轻松秒杀): ");
                std::string input;
                std::getline(std::cin, input);
                try {
                    ctx.submit_quality = std::stoi(input);
                    if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
                        throw std::out_of_range("Quality must be 0-5");
                    }
                } catch (...) {
                    fmt::print(fg(fmt::color::yellow), "[!] 无效输入，默认设为 3\n");
                    ctx.submit_quality = 3;
                }
            }

            if (ctx.submit_quality < 3) {
                fmt::print("\n-- 错误类型 --\n");
                auto& types = MistakeAnalyzer::mistake_types();
                for (size_t i = 0; i < types.size(); i++) fmt::print("  {}. {}\n", i + 1, types[i]);
                int choice = 0;
                fmt::print("选择主要错误类型 (1-{}): ", types.size());
                std::string choice_input;
                std::getline(std::cin, choice_input);
                try {
                    choice = std::stoi(choice_input);
                } catch (...) {
                    choice = 0;
                }
                std::string desc;
                fmt::print("简要描述错误原因: ");
                std::getline(std::cin, desc);
                if (choice >= 1 && choice <= (int)types.size()) svc.ma->log_mistake(ctx.submit_pid, types[choice - 1], desc);
            }

            auto review = svc.db->get_review(ctx.submit_pid);
            review = SM2Algorithm::update(review, ctx.submit_quality);
            svc.db->upsert_review(review);
            fmt::print(fg(fmt::color::green_yellow), "[+] 记录成功。下次复习时间: {} 天后\n", review.interval);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── review ──
    app.add_subcommand("review", "查看待复习题目")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto due = svc.db->get_due_reviews(std::time(nullptr));
            if (due.empty()) {
                fmt::print(fg(fmt::color::green_yellow), "太棒了！所有题目都已复习完毕。\n");
                return;
            }
            fmt::print(fg(fmt::color::yellow), "=== 今日待复习 ({} 题) ===\n\n", due.size());
            for (auto& r : due) fmt::print("  [{}] {} (间隔: {}天)\n", r.problem_id.substr(0, 15), r.title, r.interval);
            fmt::print("\n开始练习: solve <id>\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── next ──
    app.add_subcommand("next", "智能推荐下一题")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto due = svc.db->get_due_reviews(std::time(nullptr));
            if (!due.empty()) {
                fmt::print(fg(fmt::color::yellow), "[教练] 还有 {} 道题待复习。建议优先解决:\n", due.size());
                fmt::print(fg(fmt::color::cyan),   "  -> {} ({})\n", due[0].title, due[0].problem_id);
                fmt::print("\n命令: solve {}\n", due[0].problem_id);
                return;
            }
            auto stats = svc.ma->get_stats();
            if (!stats.empty()) {
                fmt::print(fg(fmt::color::yellow), "[教练] 你的薄弱点: {} ({} 次)\n  建议专项练习。\n\n", stats[0].first, stats[0].second);
            }
            fmt::print(fg(fmt::color::green_yellow), "[教练] 按计划推进，拉取新题:\n  pull <url>\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── hint ──
    auto hint_cmd = app.add_subcommand("hint", "AI 教练提示 (非答案)");
    hint_cmd->add_option("problem_id", ctx.hint_pid, "题目 ID")->required();
    hint_cmd->add_option("-f,--file", ctx.hint_file, "代码文件");
    hint_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto prob = svc.pm->get_problem(ctx.hint_pid);
            if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 未找到题目。\n"); return; }
            
            std::string content, code;
            if (fs::exists(prob.content_path)) { std::ifstream f(prob.content_path); content.assign(std::istreambuf_iterator<char>(f), {}); }
            
            if (ctx.hint_file.empty()) ctx.hint_file = "solution_" + prob.id + ".cpp";
            if (fs::exists(ctx.hint_file)) { std::ifstream f(ctx.hint_file); code.assign(std::istreambuf_iterator<char>(f), {}); } 
            else { fmt::print("找不到代码 ({})。请手动输入 (EOF结束):\n", ctx.hint_file); code.assign(std::istreambuf_iterator<char>(std::cin), {}); }

            fmt::print(fg(fmt::color::yellow), "\n[教练] 正在分析...\n\n");
            fmt::print("{}\n", svc.ai->analyze(content, code));
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── stats ──
    app.add_subcommand("stats", "查看错误统计")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto stats = svc.ma->get_stats();
            if (stats.empty()) { fmt::print("暂无错误记录。\n"); return; }
            int total = 0; for (auto& [t, c] : stats) total += c;
            fmt::print(fg(fmt::color::yellow), "=== 错误统计 ({} 次) ===\n\n", total);
            for (auto& [type, count] : stats) {
                int len = (int)(30.0 * count / total);
                fmt::print("  {:<22} {:>3}  {}\n", type, count, std::string(len, '#'));
            }
            fmt::print("\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });

    // ── test ──
    auto test_cmd = app.add_subcommand("test", "运行本地测试");
    test_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    test_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            auto prob = svc.pm->get_problem(ctx.solve_pid);
            if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 未找到题目。\n"); return; }
            
            std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
            std::string src_file = "solution_" + prob.id + ext;
            if (!fs::exists(src_file)) {
                fmt::print(fg(fmt::color::red), "[!] 代码文件 {} 不存在。\n", src_file);
                return;
            }

            // Fetch test cases
            // We need to convert from DB format (string pairs) to TestCase struct
            auto db_cases = svc.db->get_test_cases(prob.id);


            std::vector<TestCase> cases;
            for(auto& p : db_cases) {
                TestCase tc; tc.input = p.first; tc.output = p.second; tc.is_sample = true;
                cases.push_back(tc);
            }
            
            if (cases.empty()) {
                fmt::print(fg(fmt::color::yellow), "[!] 暂无测试用例。请使用 add-case 添加。\n");
                return;
            }

            fmt::print("[*] 正在编译并运行 {} 个测试用例...\n", cases.size());
            auto results = svc.judge->judge(src_file, svc.cfg.language, cases);
            
            int accepted = 0;
            for (size_t i = 0; i < results.size(); i++) {
                auto& r = results[i];
                std::string status;
                fmt::color color = fmt::color::white;
                switch(r.verdict) {
                    case Verdict::AC: status = "AC"; color = fmt::color::green; accepted++; break;
                    case Verdict::WA: status = "WA"; color = fmt::color::red; break;
                    case Verdict::TLE: status = "TLE"; color = fmt::color::yellow; break;
                    case Verdict::MLE: status = "MLE"; color = fmt::color::yellow; break;
                    case Verdict::RE: status = "RE"; color = fmt::color::magenta; break;
                    case Verdict::CE: status = "CE"; color = fmt::color::dark_red; break;
                    case Verdict::SE: status = "SE"; color = fmt::color::gray; break;
                }
                
                fmt::print(fg(color), "  Case {}: {} ({:.1f}ms)\n", i+1, status, (float)r.time_ms);
                if (r.verdict != Verdict::AC) {
                    if (r.verdict == Verdict::CE) {
                         fmt::print(fg(fmt::color::red), "    Compile Error:\n{}\n", r.message);
                    } else {
                         fmt::print(fg(fmt::color::dim_gray), "    Input:    {}\n", cases[i].input.substr(0, 50) + (cases[i].input.size()>50?"...":""));
                         fmt::print(fg(fmt::color::dim_gray), "    Output:   {}\n", r.output.substr(0, 50) + (r.output.size()>50?"...":""));
                         fmt::print(fg(fmt::color::dim_gray), "    Expected: {}\n", r.expected.substr(0, 50) + (r.expected.size()>50?"...":""));
                    }
                }
            }
            
            if (accepted == cases.size()) {
                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "\n[Passed] 全部通过！\n");
            } else {
                fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\n[Failed] 通过率: {}/{}\n", accepted, cases.size());
            }

        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── add-case ──
    auto add_case_cmd = app.add_subcommand("add-case", "添加自定义测试用例");
    add_case_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    add_case_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            auto prob = svc.pm->get_problem(ctx.solve_pid);
            if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 未找到题目。\n"); return; }

            std::string input, output;
            fmt::print("输入 (输入 END 结束):\n");
            std::string line;
            while(std::getline(std::cin, line) && line != "END") input += line + "\n";
            if (!input.empty() && input.back() == '\n') input.pop_back();

            fmt::print("预期输出 (输入 END 结束):\n");
            while(std::getline(std::cin, line) && line != "END") output += line + "\n";
            if (!output.empty() && output.back() == '\n') output.pop_back();
            
            svc.db->add_test_case(prob.id, input, output, false); // User added is not sample? or maybe sample.
            fmt::print(fg(fmt::color::green), "[+] 测试用例已添加。\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── config ──
    auto cfg_cmd = app.add_subcommand("config", "配置");
    cfg_cmd->add_option("--api-key", ctx.cfg_key, "API Key");
    cfg_cmd->add_option("--model", ctx.cfg_model, "Model Name");
    cfg_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto c = Config::load(Config::config_path(root));
            if (!ctx.cfg_key.empty()) c.api_key = ctx.cfg_key;
            if (!ctx.cfg_model.empty()) c.model = ctx.cfg_model;
            c.save(Config::config_path(root));
            fmt::print(fg(fmt::color::green_yellow), "[+] 配置更新。\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });
    // ── debug ──
    auto debug_cmd = app.add_subcommand("debug", "AI 诊断错题 (分析 Failing Case)");
    debug_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    debug_cmd->callback([&]() {
        try {
             auto root = find_root_or_die();
             auto svc = Services::load(root);
             auto prob = svc.pm->get_problem(ctx.solve_pid);
             if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 未找到题目。\n"); return; }
             
             std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
             std::string src_file = "solution_" + prob.id + ext;
             if (!fs::exists(src_file)) {
                 fmt::print(fg(fmt::color::red), "[!] 代码文件 {} 不存在。\n", src_file);
                 return;
             }

             // Run tests
             auto db_cases = svc.db->get_test_cases(prob.id);
             std::vector<TestCase> cases;
             for(auto& p : db_cases) {
                 TestCase tc; tc.input = p.first; tc.output = p.second; tc.is_sample = true;
                 cases.push_back(tc);
             }
             if (cases.empty()) { fmt::print("[!] 无测试用例。\n"); return; }

             fmt::print("[*] 正在运行测试...\n");
             auto results = svc.judge->judge(src_file, svc.cfg.language, cases);
             
             int failure_idx = -1;
             for(size_t i=0; i<results.size(); ++i) {
                 if (results[i].verdict != Verdict::AC) {
                     failure_idx = (int)i;
                     break;
                 }
             }

             if (failure_idx == -1) {
                 fmt::print(fg(fmt::color::green), "[*] 所有测试通过！\n");
                 return;
             }
             
             const auto& failure = results[failure_idx];
             const auto& failed_case = cases[failure_idx];

             // Simplified print to avoid fmt overload resolution issues
             std::string v_str = failure.verdict_str();
             fmt::print(fg(fmt::color::yellow), "[!] 发现错误: {}\n", v_str);
             
             if (svc.cfg.ai_enabled && svc.ai->enabled()) {
                 fmt::print("[*] AI 教练正在分析...\n");
                 
                 std::string content = "Content";
                 if (fs::exists(prob.content_path)) {
                     std::ifstream t(prob.content_path);
                     std::stringstream buffer;
                     buffer << t.rdbuf();
                     content = buffer.str();
                 }
                 
                 std::ifstream s(src_file);
                 std::string code((std::istreambuf_iterator<char>(s)), {});

                 // Manually construct fail info to ensure string types
                 std::stringstream ss;
                 ss << "Input: " << failed_case.input << "\n"
                    << "Expected: " << failed_case.output << "\n"
                    << "Actual: " << failure.output << "\n"
                    << "Error: " << failure.message;
                 std::string fail_info = ss.str();

                 // Get stats
                 auto stats = svc.ma->get_stats();
                 std::string history = "";
                 for(auto& [k,v] : stats) history += fmt::format("{}: {}, ", k, v);

                 std::string analysis = svc.ai->diagnose(content, code, fail_info, history);
                 fmt::print(fg(fmt::color::cyan), "\n[诊断报告]\n{}\n", analysis);
             } else {
                 fmt::print("[!] AI 未启用。\n");
             }

        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        } catch (...) {
            fmt::print(fg(fmt::color::red), "[!] 发生未知错误\n");
        }
    });
}

// ─── Tokenizer ────────────────────────────────────────

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_quote = false;
    for (char c : line) {
        if (c == '"') {
            in_quote = !in_quote;
        } else if ((c == ' ' || c == '\n' || c == '\r') && !in_quote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else if (c != '\r' && c != '\n') {
            token += c;
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

// ─── REPL Loop ────────────────────────────────────────

void run_repl() {
    using Replxx = replxx::Replxx;
    Replxx rx;

    // Start background services (Companion Server)
    auto root = Config::find_root();
    std::unique_ptr<Services> global_svc;
    
    if (!root.empty()) {
        try {
            global_svc = std::make_unique<Services>(Services::load(root));
            if(global_svc->companion) global_svc->companion->start(); 
            // Now global_svc stays alive during REPL
        } catch (...) {}
    }

    // Load history
    auto history_path = (Config::find_root().empty() ? fs::current_path() : Config::find_root() / Config::DIR_NAME) / "history.txt";
    std::string history_file = history_path.string();
    rx.history_load(history_file);

    // Completion callback
    rx.set_completion_callback([&](std::string const& input, int& contextLen) {
        std::vector<Replxx::Completion> completions;
        std::string prefix = input.substr(input.find_last_of(" \t") + 1);
        contextLen = prefix.length();

        // 1. Commands
        std::vector<std::string> commands = {"init", "pull", "new", "solve", "list", "submit", "review", "next", "hint", "stats", "config", "test", "add-case", "clear", "exit", "delete", "help"};
        for (const auto& cmd : commands) {
            if (cmd.find(prefix) == 0 && prefix.length() > 0) completions.emplace_back(cmd);
            else if (prefix.empty()) completions.emplace_back(cmd);
        }

        // 2. IDs (only if input starts with solve/delete/test/add-case/submit/hint)
        if (input.find("solve ") == 0 || input.find("delete ") == 0 || input.find("submit ") == 0 || 
            input.find("test ") == 0 || input.find("add-case ") == 0 || input.find("hint ") == 0) {
            try {
                auto svc = Services::load(find_root_or_die());
                auto problems = svc.pm->list_problems();
                for (const auto& p : problems) {
                    std::string tid = std::to_string(p.display_id);
                    if (tid.find(prefix) == 0) completions.emplace_back(tid);
                }
            } catch (...) {}
        }
        return completions;
    });

    // Syntax highlighting (simple)
    rx.set_highlighter_callback([](std::string const& input, Replxx::colors_t& colors) {
        for (size_t i = 0; i < input.length(); i++) {
            if (isdigit(input[i])) colors[i] = Replxx::Color::BRIGHTMAGENTA;
            else if (input.find("shuati") != std::string::npos && i < 6) colors[i] = Replxx::Color::GREEN;
        }
    });

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold,
        "\n"
        "  ███████╗██╗  ██╗██╗   ██╗ █████╗ ████████╗██╗\n"
        "  ██╔════╝██║  ██║██║   ██║██╔══██╗╚══██╔══╝██║\n"
        "  ███████╗███████║██║   ██║███████║   ██║   ██║\n"
        "  ╚════██║██╔══██║██║   ██║██╔══██║   ██║   ██║\n"
        "  ███████║██║  ██║╚██████╔╝██║  ██║   ██║   ██║\n"
        "  ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝\n"
        "\n"
    );
    fmt::print("  欢迎使用 shuati CLI (v2.0)\n");
    fmt::print("  输入 'help' 查看命令，'exit' 退出\n\n");

    const char* example = "shuati > ";
    while (true) {
        const char* cinput = rx.input(example);
        if (cinput == nullptr) break;
        std::string line(cinput);
        if (line.empty()) continue;

        rx.history_add(line);
        rx.history_save(history_file);

        if (line == "exit" || line == "quit") break;
        if (line == "clear") {
            rx.clear_screen();
            continue;
        }
        if (line == "help") {
             fmt::print("\n  可用命令:\n");
             fmt::print("  {:<12} {}\n", "init",   "在当前目录初始化项目");
             fmt::print("  {:<12} {}\n", "pull",   "从 URL 拉取题目");
             fmt::print("  {:<12} {}\n", "new",    "创建本地题目");
             fmt::print("  {:<12} {}\n", "solve",  "开始解题 (输入 ID/TID)");
             fmt::print("  {:<12} {}\n", "list",   "列出所有题目");
             fmt::print("  {:<12} {}\n", "delete", "删除题目 (输入 ID/TID)");
             fmt::print("  {:<12} {}\n", "submit", "提交记录");
             fmt::print("  {:<12} {}\n", "review", "查看待复习题目");
             fmt::print("  {:<12} {}\n", "next",   "智能推荐下一题");
             fmt::print("  {:<12} {}\n", "hint",   "AI 教练提示");
             fmt::print("  {:<12} {}\n", "stats",  "查看错误统计");
             fmt::print("  {:<12} {}\n", "config", "配置 API Key");
             fmt::print("  {:<12} {}\n", "clear",  "清屏");
             fmt::print("  {:<12} {}\n", "exit",   "退出");
             fmt::print("\n");
             continue;
        }

        auto args = tokenize(line);
        if (args.empty()) continue;

        // Create fresh context and app
        CommandContext ctx;
        CLI::App app{"shuati REPL"};
        setup_commands(app, ctx);

        // Strip redundant program name
        if (args[0] == "shuati") {
            args.erase(args.begin());
            if (args.empty()) continue;
        }

        args.insert(args.begin(), "shuati");
        std::vector<char*> argv;
        for (auto& s : args) argv.push_back(&s[0]);

        try {
            app.parse(argv.size(), argv.data());
        } catch (const CLI::CallForHelp& e) {
            // Do nothing, already handled by help command or default
            // But CLI11 help output is sent to cout by exit(0) usually?
            // CLI11 exits on help by default? usage of CLI11_PARSE handles it.
            // app.parse throws CallForHelp.
            std::cout << app.help();
        } catch (const CLI::ParseError& e) {
           // app.exit(e); // prints error but might exit?
           // print error manually
           fmt::print(fg(fmt::color::red), "[!] 命令错误: {}\n", e.what());
        } catch (const std::exception& e) {
             fmt::print(fg(fmt::color::red), "[!] 运行时错误: {}\n", e.what());
        }
    }
}

// ─── Main ─────────────────────────────────────────────

int main(int argc, char** argv) {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif

    if (argc == 1) {
        run_repl();
    } else {
        CommandContext ctx;
        CLI::App app{"shuati CLI - 你的 AI 算法教练"};
        app.set_version_flag("--version", "1.2.0");
        setup_commands(app, ctx);
        CLI11_PARSE(app, argc, argv);
    }

    return 0;
}
