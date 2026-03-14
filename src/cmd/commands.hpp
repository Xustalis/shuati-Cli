#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <functional>
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

struct CommandContext {
    std::string pull_url;
    std::string new_title, new_tags, new_diff = "medium";
    std::string solve_pid;
    std::string submit_pid, submit_file;
    int submit_quality = -1;
    std::string hint_pid, hint_file;
    std::string cfg_key, cfg_model;
    std::string cfg_language;         // --language flag for config command
    std::string cfg_editor;          // --editor flag for config command
    std::string cfg_autostart_repl;  // "on" or "off" for --autostart-repl
    std::string cfg_ui_mode;         // "tui" or "legacy" for --ui-mode
    bool cfg_show = false;
    int test_max_cases = 30;
    std::string test_oracle = "auto";
    bool test_ui = false;
    std::string list_filter; // "all", "ac", "failed", "unaudited", "review"
    std::string view_export_dir; // Directory to save test cases
    std::string login_platform;  // Platform for login command (e.g., "lanqiao")
    std::function<void(const std::string&)> stream_cb; // Callback for streaming outputs
    // Set to true when command is dispatched from the TUI - suppresses stdin reads,
    // FTXUI interactive menus, and editor launch to avoid TUI corruption.
    bool is_tui = false;
};


struct Services {
    std::shared_ptr<Database>       db;
    std::shared_ptr<ProblemManager> pm;
    std::shared_ptr<MistakeAnalyzer> ma;
    std::unique_ptr<MemoryManager>  mm;
    std::unique_ptr<AICoach>        ai;
    std::unique_ptr<CompanionServer> companion;
    std::unique_ptr<Judge>          judge;
    Config                          cfg;

    static Services load(const std::filesystem::path& root, bool skip_doctor = false);
};


std::filesystem::path find_root_or_die();
std::string executable_path_utf8();

/**
 * @brief Generate human-readable solution filename
 * @param prob The problem to generate filename for
 * @param language "cpp" or "python"
 * @return Generated filename
 */
std::string make_solution_filename(const Problem& prob, const std::string& language);

/**
 * @brief Find existing solution file (supports both old and new naming)
 * @param prob The problem to find solution for
 * @param language "cpp" or "python"
 * @return Path if found, empty string if not
 */
std::string find_solution_file(const Problem& prob, const std::string& language);

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
void cmd_login(CommandContext& ctx);

// Registers all commands to the App
void setup_commands(CLI::App& app, CommandContext& ctx);

// Run interactive REPL
void run_repl();

} // namespace cmd
} // namespace shuati
