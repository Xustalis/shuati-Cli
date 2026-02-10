#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <vector>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

#include "config.hpp"
#include "database.hpp"
#include "problem_manager.hpp"
#include "mistake_analyzer.hpp"
#include "ai_coach.hpp"

namespace fs = std::filesystem;
using namespace shuati;

// ─── Helpers ──────────────────────────────────────────

static fs::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\n");
        std::exit(1);
    }
    return root;
}

struct Services {
    std::shared_ptr<Database>       db;
    std::shared_ptr<ProblemManager> pm;
    std::shared_ptr<MistakeAnalyzer> ma;
    std::unique_ptr<AICoach>        ai;
    Config                          cfg;

    static Services load(const fs::path& root) {
        Services s;
        s.cfg = Config::load(Config::config_path(root));
        s.db  = std::make_shared<Database>(Config::db_path(root).string());
        s.pm  = std::make_shared<ProblemManager>(s.db);
        s.ma  = std::make_shared<MistakeAnalyzer>(s.db);
        s.ai  = std::make_unique<AICoach>(s.cfg);
        return s;
    }
};

static ReviewItem sm2_update(ReviewItem r, int quality) {
    if (quality < 3) {
        r.repetitions = 0;
        r.interval = 1;
    } else {
        if (r.repetitions == 0)      r.interval = 1;
        else if (r.repetitions == 1) r.interval = 6;
        else                         r.interval = (int)(r.interval * r.ease_factor);
        r.repetitions++;
    }
    r.ease_factor += 0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02);
    if (r.ease_factor < 1.3) r.ease_factor = 1.3;
    r.next_review = std::time(nullptr) + r.interval * 86400;
    return r;
}

// ─── Main ─────────────────────────────────────────────

int main(int argc, char** argv) {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif

    CLI::App app{"shuati CLI - 你的 AI 算法教练"};
    app.require_subcommand(1);

    // ── init ──
    app.add_subcommand("init", "在当前目录初始化项目")->callback([]() {
        if (fs::exists(Config::DIR_NAME)) {
            fmt::print("[!] 项目已初始化。\n");
            return;
        }
        auto dir = fs::current_path() / Config::DIR_NAME;
        fs::create_directory(dir);
        Config cfg;
        cfg.save(Config::config_path(fs::current_path()));
        Database db(Config::db_path(fs::current_path()).string());
        fmt::print(fg(fmt::color::green_yellow), "[+] 初始化成功: {}\n", dir.string());
        fmt::print("    请设置 API Key: shuati config --api-key <YOUR_KEY>\n");
    });

    // ── pull ──
    std::string pull_url;
    auto pull_cmd = app.add_subcommand("pull", "从 URL 拉取题目");
    pull_cmd->add_option("url", pull_url, "题目链接")->required();
    pull_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        svc.pm->pull_problem(pull_url);
    });

    // ── new ──
    std::string new_title, new_tags = "", new_diff = "medium";
    auto new_cmd = app.add_subcommand("new", "创建本地题目");
    new_cmd->add_option("title", new_title, "标题")->required();
    new_cmd->add_option("-t,--tags", new_tags, "标签 (逗号分隔)");
    new_cmd->add_option("-d,--difficulty", new_diff, "难度 (easy/medium/hard)");
    new_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        svc.pm->create_local(new_title, new_tags, new_diff);
    });

    // ── solve ──
    std::string solve_pid;
    auto solve_cmd = app.add_subcommand("solve", "开始解决一道题 (生成代码模板)");
    solve_cmd->add_option("id", solve_pid, "题目 ID")->required();
    solve_cmd->callback([&]() {
        auto root = find_root_or_die();
        auto svc = Services::load(root);
        auto prob = svc.pm->get_problem(solve_pid);

        if (prob.id.empty()) {
            fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", solve_pid);
            return;
        }

        fmt::print(fg(fmt::color::cyan), "=== 开始练习: {} ===\n", prob.title);
        fmt::print("题目描述: {}\n", prob.content_path);

        std::string filename = "solution_" + prob.id + ".cpp";
        if (fs::exists(filename)) {
            fmt::print(fg(fmt::color::yellow), "[!] 代码文件已存在: {}\n", filename);
        } else {
            std::ofstream out(filename);
            out << "// Problem: " << prob.title << "\n// URL: " << prob.url << "\n\n#include <iostream>\n#include <vector>\nusing namespace std;\n\nint main() {\n    cout << \"Hello Shuati!\" << endl;\n    return 0;\n}\n";
            out.close();
            fmt::print(fg(fmt::color::green), "[+] 代码模板已生成: {}\n", filename);
        }
        fmt::print("\n完成后使用以下命令提交:\n  shuati submit {} -f {}\n", prob.id, filename);
    });

    // ── list ──
    app.add_subcommand("list", "列出所有题目")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto problems = svc.pm->list_problems();
        if (problems.empty()) {
            fmt::print("暂无题目。请使用: shuati pull <url>\n");
            return;
        }
        fmt::print("{:<20} {:<30} {:<8} {}\n", "ID", "标题", "难度", "来源");
        fmt::print("{}\n", std::string(70, '-'));
        for (auto& p : problems) {
            std::string title = p.title.length() > 28 ? p.title.substr(0, 25) + "..." : p.title;
            fmt::print("{:<20} {:<30} {:<8} {}\n", p.id.substr(0, 18), title, p.difficulty, p.source);
        }
    });

    // ── submit ──
    std::string submit_file, submit_pid;
    int submit_quality = -1;
    auto submit_cmd = app.add_subcommand("submit", "提交记录 (记录错误与心得)");
    submit_cmd->add_option("problem_id", submit_pid, "题目 ID")->required();
    submit_cmd->add_option("-f,--file", submit_file, "代码文件路径");
    submit_cmd->add_option("-q,--quality", submit_quality, "自评难度 0-5 (0=不会, 5=秒杀)");
    submit_cmd->callback([&]() {
        auto root = find_root_or_die();
        auto svc = Services::load(root);
        auto prob = svc.pm->get_problem(submit_pid);

        if (prob.id.empty()) {
            fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", submit_pid);
            return;
        }

        if (submit_file.empty()) {
            std::string candidate = "solution_" + prob.id + ".cpp";
            if (fs::exists(candidate)) {
                submit_file = candidate;
                fmt::print("[i] 自动使用代码文件: {}\n", submit_file);
            } else {
                fmt::print(fg(fmt::color::yellow), "[!] 未指定文件，且默认文件 {} 不存在。\n", candidate);
            }
        }

        if (submit_quality < 0 || submit_quality > 5) {
            fmt::print("本次练习感觉如何? (0=完全不会, 3=有挑战, 5=轻松秒杀): ");
            std::cin >> submit_quality;
            std::cin.ignore();
        }

        if (submit_quality < 3) {
            fmt::print("\n-- 错误类型 --\n");
            auto& types = MistakeAnalyzer::mistake_types();
            for (size_t i = 0; i < types.size(); i++) fmt::print("  {}. {}\n", i + 1, types[i]);
            int choice = 0;
            fmt::print("选择主要错误类型 (1-{}): ", types.size());
            std::cin >> choice;
            std::cin.ignore();
            std::string desc;
            fmt::print("简要描述错误原因: ");
            std::getline(std::cin, desc);
            if (choice >= 1 && choice <= (int)types.size()) svc.ma->log_mistake(submit_pid, types[choice - 1], desc);
        }

        auto review = svc.db->get_review(submit_pid);
        review = sm2_update(review, submit_quality);
        svc.db->upsert_review(review);
        fmt::print(fg(fmt::color::green_yellow), "[+] 记录成功。下次复习时间: {} 天后\n", review.interval);
    });

    // ── review ──
    app.add_subcommand("review", "查看待复习题目")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto due = svc.db->get_due_reviews(std::time(nullptr));
        if (due.empty()) {
            fmt::print(fg(fmt::color::green_yellow), "太棒了！所有题目都已复习完毕。\n");
            return;
        }
        fmt::print(fg(fmt::color::yellow), "=== 今日待复习 ({} 题) ===\n\n", due.size());
        for (auto& r : due) fmt::print("  [{}] {} (间隔: {}天)\n", r.problem_id.substr(0, 15), r.title, r.interval);
        fmt::print("\n开始练习: shuati solve <id>\n");
    });

    // ── next ──
    app.add_subcommand("next", "智能推荐下一题")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto due = svc.db->get_due_reviews(std::time(nullptr));
        if (!due.empty()) {
            fmt::print(fg(fmt::color::yellow), "[教练] 还有 {} 道题待复习。建议优先解决:\n", due.size());
            fmt::print(fg(fmt::color::cyan),   "  -> {} ({})\n", due[0].title, due[0].problem_id);
            fmt::print("\n命令: shuati solve {}\n", due[0].problem_id);
            return;
        }
        auto stats = svc.ma->get_stats();
        if (!stats.empty()) {
            fmt::print(fg(fmt::color::yellow), "[教练] 你的薄弱点: {} ({} 次)\n  建议专项练习。\n\n", stats[0].first, stats[0].second);
        }
        fmt::print(fg(fmt::color::green_yellow), "[教练] 按计划推进，拉取新题:\n  shuati pull <url>\n");
    });

    // ── hint ──
    std::string hint_pid, hint_file;
    auto hint_cmd = app.add_subcommand("hint", "AI 教练提示 (非答案)");
    hint_cmd->add_option("problem_id", hint_pid, "题目 ID")->required();
    hint_cmd->add_option("-f,--file", hint_file, "代码文件");
    hint_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        auto prob = svc.pm->get_problem(hint_pid);
        if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 未找到题目。\n"); return; }
        
        std::string content, code;
        if (fs::exists(prob.content_path)) { std::ifstream f(prob.content_path); content.assign(std::istreambuf_iterator<char>(f), {}); }
        if (hint_file.empty()) hint_file = "solution_" + prob.id + ".cpp";
        if (fs::exists(hint_file)) { std::ifstream f(hint_file); code.assign(std::istreambuf_iterator<char>(f), {}); } 
        else { fmt::print("找不到代码 ({})。请手动输入 (EOF结束):\n", hint_file); code.assign(std::istreambuf_iterator<char>(std::cin), {}); }

        fmt::print(fg(fmt::color::yellow), "\n[教练] 正在分析...\n\n");
        fmt::print("{}\n", svc.ai->analyze(content, code));
    });

    // ── stats ──
    app.add_subcommand("stats", "查看错误统计")->callback([]() {
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
    });

    // ── config ──
    std::string cfg_key, cfg_model;
    auto cfg_cmd = app.add_subcommand("config", "配置");
    cfg_cmd->add_option("--api-key", cfg_key, "API Key");
    cfg_cmd->add_option("--model", cfg_model, "Model Name");
    cfg_cmd->callback([&]() {
        auto root = find_root_or_die();
        auto c = Config::load(Config::config_path(root));
        if (!cfg_key.empty()) c.api_key = cfg_key;
        if (!cfg_model.empty()) c.model = cfg_model;
        c.save(Config::config_path(root));
        fmt::print(fg(fmt::color::green_yellow), "[+] 配置更新。\n");
    });

    try { CLI11_PARSE(app, argc, argv); }
    catch (const std::exception& e) { fmt::print(fg(fmt::color::red), "[!] Error: {}\n", e.what()); return 1; }
    return 0;
}
