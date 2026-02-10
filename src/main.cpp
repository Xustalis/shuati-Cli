#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>

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
        fmt::print(fg(fmt::color::red), "[!] Not a shuati project. Run: shuati init\n");
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

// SM-2 algorithm update
static ReviewItem sm2_update(ReviewItem r, int quality) {
    // quality: 0-5 (0=blackout, 5=perfect)
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
    CLI::App app{"shuati - AI-powered algorithm training coach"};
    app.require_subcommand(1);

    // ── init ──
    app.add_subcommand("init", "Initialize shuati in current directory")->callback([]() {
        if (fs::exists(Config::DIR_NAME)) {
            fmt::print("[!] Already initialized.\n");
            return;
        }
        auto dir = fs::current_path() / Config::DIR_NAME;
        fs::create_directory(dir);

        // Create default config
        Config cfg;
        cfg.save(Config::config_path(fs::current_path()));

        // Init DB
        Database db(Config::db_path(fs::current_path()).string());

        fmt::print(fg(fmt::color::green_yellow), "[+] shuati initialized in {}\n", dir.string());
        fmt::print("    Set your API key: shuati config --api-key <KEY>\n");
    });

    // ── pull ──
    std::string pull_url;
    auto pull_cmd = app.add_subcommand("pull", "Pull a problem from URL");
    pull_cmd->add_option("url", pull_url, "Problem URL")->required();
    pull_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        svc.pm->pull_problem(pull_url);
    });

    // ── new ──
    std::string new_title, new_tags = "", new_diff = "medium";
    auto new_cmd = app.add_subcommand("new", "Create a local problem");
    new_cmd->add_option("title", new_title, "Problem title")->required();
    new_cmd->add_option("-t,--tags", new_tags, "Tags (comma-separated)");
    new_cmd->add_option("-d,--difficulty", new_diff, "Difficulty (easy/medium/hard)");
    new_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        svc.pm->create_local(new_title, new_tags, new_diff);
    });

    // ── list ──
    app.add_subcommand("list", "List all problems")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto problems = svc.pm->list_problems();
        if (problems.empty()) {
            fmt::print("No problems yet. Use: shuati pull <url>\n");
            return;
        }
        fmt::print("{:<20} {:<30} {:<8} {}\n", "ID", "TITLE", "DIFF", "SOURCE");
        fmt::print("{}\n", std::string(70, '-'));
        for (auto& p : problems) {
            fmt::print("{:<20} {:<30} {:<8} {}\n",
                p.id.substr(0, 18), p.title.substr(0, 28), p.difficulty, p.source);
        }
    });

    // ── submit ──
    std::string submit_file, submit_pid;
    int submit_quality = -1;
    auto submit_cmd = app.add_subcommand("submit", "Record a submission attempt");
    submit_cmd->add_option("problem_id", submit_pid, "Problem ID")->required();
    submit_cmd->add_option("-f,--file", submit_file, "Solution file path");
    submit_cmd->add_option("-q,--quality", submit_quality, "Self-rating 0-5 (0=fail, 5=perfect)");
    submit_cmd->callback([&]() {
        auto root = find_root_or_die();
        auto svc = Services::load(root);

        // Validate problem exists
        auto prob = svc.pm->get_problem(submit_pid);
        if (prob.id.empty()) {
            fmt::print(fg(fmt::color::red), "[!] Problem '{}' not found.\n", submit_pid);
            return;
        }

        // Get quality rating if not provided
        if (submit_quality < 0 || submit_quality > 5) {
            fmt::print("How did it go? (0=blackout, 3=hard, 5=perfect): ");
            std::cin >> submit_quality;
            std::cin.ignore();
        }

        // Log mistake if quality < 3
        if (submit_quality < 3) {
            fmt::print("\n-- Mistake Types --\n");
            auto& types = MistakeAnalyzer::mistake_types();
            for (size_t i = 0; i < types.size(); i++)
                fmt::print("  {}. {}\n", i + 1, types[i]);

            int choice = 0;
            fmt::print("Select type (1-{}): ", types.size());
            std::cin >> choice;
            std::cin.ignore();

            std::string desc;
            fmt::print("Brief description: ");
            std::getline(std::cin, desc);

            if (choice >= 1 && choice <= (int)types.size())
                svc.ma->log_mistake(submit_pid, types[choice - 1], desc);
        }

        // Update spaced repetition
        auto review = svc.db->get_review(submit_pid);
        review = sm2_update(review, submit_quality);
        svc.db->upsert_review(review);

        fmt::print(fg(fmt::color::green_yellow),
            "[+] Recorded. Next review in {} day(s).\n", review.interval);
    });

    // ── review ──
    app.add_subcommand("review", "Show problems due for review")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto due = svc.db->get_due_reviews(std::time(nullptr));

        if (due.empty()) {
            fmt::print(fg(fmt::color::green_yellow), "All clear! No reviews due.\n");
            return;
        }

        fmt::print(fg(fmt::color::yellow), "=== {} problem(s) due for review ===\n\n", due.size());
        for (auto& r : due) {
            fmt::print("  [{}] {}\n", r.problem_id.substr(0, 15), r.title);
        }
        fmt::print("\nUse: shuati submit <problem_id> -q <0-5>\n");
    });

    // ── next ──
    app.add_subcommand("next", "Recommend what to practice next")->callback([]() {
        auto svc = Services::load(find_root_or_die());

        // Priority 1: Due reviews
        auto due = svc.db->get_due_reviews(std::time(nullptr));
        if (!due.empty()) {
            fmt::print(fg(fmt::color::yellow), "[Coach] You have {} review(s) due. Start with:\n", due.size());
            fmt::print(fg(fmt::color::cyan),   "  -> {} ({})\n\n", due[0].title, due[0].problem_id);
            return;
        }

        // Priority 2: Weakest mistake type
        auto stats = svc.ma->get_stats();
        if (!stats.empty()) {
            fmt::print(fg(fmt::color::yellow),
                "[Coach] Your most common mistake: {} ({} times)\n", stats[0].first, stats[0].second);
            fmt::print("  Recommend practicing problems targeting this weakness.\n\n");
        }

        fmt::print(fg(fmt::color::green_yellow), "[Coach] All caught up! Pull a new problem:\n");
        fmt::print("  shuati pull <url>\n");
    });

    // ── hint ──
    std::string hint_pid, hint_file;
    auto hint_cmd = app.add_subcommand("hint", "Get an AI coaching hint (not the answer!)");
    hint_cmd->add_option("problem_id", hint_pid, "Problem ID")->required();
    hint_cmd->add_option("-f,--file", hint_file, "Your solution file");
    hint_cmd->callback([&]() {
        auto svc = Services::load(find_root_or_die());
        auto prob = svc.pm->get_problem(hint_pid);
        if (prob.id.empty()) {
            fmt::print(fg(fmt::color::red), "[!] Problem not found.\n");
            return;
        }

        // Read problem content
        std::string prob_content;
        if (fs::exists(prob.content_path)) {
            std::ifstream f(prob.content_path);
            prob_content.assign(std::istreambuf_iterator<char>(f), {});
        }

        // Read user code
        std::string code;
        if (!hint_file.empty() && fs::exists(hint_file)) {
            std::ifstream f(hint_file);
            code.assign(std::istreambuf_iterator<char>(f), {});
        } else {
            fmt::print("Paste your code (end with EOF / Ctrl+Z):\n");
            code.assign(std::istreambuf_iterator<char>(std::cin), {});
        }

        fmt::print(fg(fmt::color::yellow), "\n[Coach] Analyzing...\n\n");
        auto result = svc.ai->analyze(prob_content, code);
        fmt::print("{}\n", result);
    });

    // ── stats ──
    app.add_subcommand("stats", "Show your mistake statistics")->callback([]() {
        auto svc = Services::load(find_root_or_die());
        auto stats = svc.ma->get_stats();

        if (stats.empty()) {
            fmt::print("No mistakes recorded yet. Keep going!\n");
            return;
        }

        int total = 0;
        for (auto& [t, c] : stats) total += c;

        fmt::print(fg(fmt::color::yellow), "=== Mistake Statistics ({} total) ===\n\n", total);
        for (auto& [type, count] : stats) {
            int bar_len = (int)(30.0 * count / total);
            fmt::print("  {:<22} {:>3}  {}\n", type, count, std::string(bar_len, '#'));
        }
        fmt::print("\n");
    });

    // ── config ──
    std::string cfg_key, cfg_model, cfg_lang;
    auto cfg_cmd = app.add_subcommand("config", "Configure shuati settings");
    cfg_cmd->add_option("--api-key", cfg_key, "DeepSeek API key");
    cfg_cmd->add_option("--model", cfg_model, "AI model name");
    cfg_cmd->add_option("--language", cfg_lang, "Default solution language");
    cfg_cmd->callback([&]() {
        auto root = find_root_or_die();
        auto cfg = Config::load(Config::config_path(root));
        if (!cfg_key.empty())   cfg.api_key = cfg_key;
        if (!cfg_model.empty()) cfg.model   = cfg_model;
        if (!cfg_lang.empty())  cfg.language = cfg_lang;
        cfg.save(Config::config_path(root));
        fmt::print(fg(fmt::color::green_yellow), "[+] Config updated.\n");
    });

    try {
        CLI11_PARSE(app, argc, argv);
    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "[!] {}\n", e.what());
        return 1;
    }
    return 0;
}
