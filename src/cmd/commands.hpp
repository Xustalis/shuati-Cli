#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <CLI/CLI.hpp>

// Include service headers
#include "shuati/config.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "shuati/mistake_analyzer.hpp"
#include "shuati/ai_coach.hpp"
#include "shuati/memory_manager.hpp"
#include "shuati/sm2_algorithm.hpp"
#include "shuati/judge.hpp"
#include "shuati/adapters/companion_server.hpp"

namespace shuati {
namespace cmd {

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
    int test_max_cases = 30;
    std::string test_oracle = "auto";
    bool test_ui = false;
    std::string list_filter; // "all", "ac", "failed", "unaudited"
    std::string view_export_dir; // Directory to save test cases
};

// ─── Services ─────────────────────────────────────────

struct Services {
    std::shared_ptr<Database>       db;
    std::shared_ptr<ProblemManager> pm;
    std::shared_ptr<MistakeAnalyzer> ma;
    std::unique_ptr<MemoryManager>  mm;
    std::unique_ptr<AICoach>        ai;
    std::unique_ptr<CompanionServer> companion;
    std::unique_ptr<Judge>          judge;
    Config                          cfg;

    static Services load(const std::filesystem::path& root);
};

// ─── Utils ────────────────────────────────────────────

std::filesystem::path find_root_or_die();
std::string executable_path_utf8();

// ─── Command Declarations ─────────────────────────────

void cmd_test(CommandContext& ctx);
void cmd_list(CommandContext& ctx);
void cmd_init(CommandContext& ctx);
void cmd_info(CommandContext& ctx);
void cmd_config(CommandContext& ctx);
void cmd_pull(CommandContext& ctx);
void cmd_new(CommandContext& ctx);
void cmd_solve(CommandContext& ctx);
void cmd_delete(CommandContext& ctx);
void cmd_submit(CommandContext& ctx);
void cmd_hint(CommandContext& ctx);
void cmd_clean(CommandContext& ctx);
void cmd_view(CommandContext& ctx);

// Registers all commands to the App
void setup_commands(CLI::App& app, CommandContext& ctx);

} // namespace cmd
} // namespace shuati
