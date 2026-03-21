#include "shuati/tui_app.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "shuati/version.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "commands.hpp"

#include "tui_command_engine.hpp"
#include "command_specs.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <ctime>
#include <memory>
#include <unordered_set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace tui {

using namespace ftxui;

namespace {

#ifdef _WIN32
bool configure_windows_console_for_tui() {
    auto apply = [](DWORD handle_type, DWORD add, DWORD remove) -> bool {
        HANDLE h = GetStdHandle(handle_type);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        return !!SetConsoleMode(h, (mode | add) & ~remove);
    };
    bool ok = apply(STD_OUTPUT_HANDLE, ENABLE_VIRTUAL_TERMINAL_PROCESSING, 0);
    // NOTE: Do NOT re-enable ENABLE_VIRTUAL_TERMINAL_INPUT here — main.cpp disables it
    // for replxx CLI mode. TUI uses ftxui's own raw input; it does not rely on VT INPUT.
    ok = apply(STD_INPUT_HANDLE,
               ENABLE_EXTENDED_FLAGS,
               ENABLE_QUICK_EDIT_MODE | ENABLE_VIRTUAL_TERMINAL_INPUT) && ok;
    return ok;
}
#endif

std::string render_help_text() {
    auto specs = tui_command_specs();
    std::string out;
    out += "\xe5\x85\xa8\xe9\x83\xa8\xe6\x8c\x87\xe4\xbb\xa4\n\n";
    for (const auto& spec : specs) {
        out += "  " + spec.usage + "  --  " + spec.summary + "\n";
    }
    out += "\n\xe5\x86\x85\xe7\xbd\xae\xe5\x91\xbd\xe4\xbb\xa4: ls, cd, pwd, clear\n";
    return out;
}

void update_autocomplete(AppState& state, const std::vector<CommandSpec>& specs) {
    if (state.input.empty() || state.input[0] != '/') {
        state.is_autocomplete_open = false;
        return;
    }
    std::string prefix = state.input;
    auto space_pos = prefix.find(' ');
    if (space_pos != std::string::npos) {
        state.is_autocomplete_open = false;
        return;
    }

    state.autocomplete_labels.clear();
    state.autocomplete_commands.clear();
    bool exact = false;
    for (const auto& spec : specs) {
        if (spec.slash.starts_with(prefix)) {
            if (spec.slash == prefix) exact = true;
            state.autocomplete_commands.push_back(spec.slash);
            state.autocomplete_labels.push_back(spec.slash + "  " + spec.summary);
        }
    }

    if (exact && state.autocomplete_labels.size() == 1) {
        state.is_autocomplete_open = false;
    } else if (!state.autocomplete_labels.empty()) {
        state.is_autocomplete_open = true;
        state.autocomplete_selected = std::clamp(state.autocomplete_selected,
            0, static_cast<int>(state.autocomplete_labels.size()) - 1);
    } else {
        state.is_autocomplete_open = false;
    }
}

struct CmdHint {
    const char* no_args;
    const char* with_args;
};

const std::unordered_map<std::string, CmdHint>& hint_table() {
    static const std::unordered_map<std::string, CmdHint> table = {
        {"/pull",   {"\xe7\x94\xa8\xe6\xb3\x95: /pull <url>  \xe4\xbe\x8b: /pull https://www.luogu.com.cn/problem/P1000",
                     "\xe2\x9c\x93 Enter \xe6\x89\xa7\xe8\xa1\x8c\xe6\x8b\x89\xe5\x8f\x96"}},
        {"/solve",  {"\xe7\x94\xa8\xe6\xb3\x95: /solve <\xe9\xa2\x98\xe5\x8f\xb7>  \xe4\xbe\x8b: /solve 1",
                     "\xe2\x9c\x93 Enter \xe5\xbc\x80\xe5\xa7\x8b\xe5\x81\x9a\xe9\xa2\x98"}},
        {"/test",   {"\xe7\x94\xa8\xe6\xb3\x95: /test <\xe9\xa2\x98\xe5\x8f\xb7>  \xe7\xbc\x96\xe8\xaf\x91\xe5\xb9\xb6\xe6\xb5\x8b\xe8\xaf\x95\xe4\xbd\xa0\xe7\x9a\x84\xe4\xbb\xa3\xe7\xa0\x81",
                     "\xe2\x9c\x93 Enter \xe8\xbf\x90\xe8\xa1\x8c\xe6\xb5\x8b\xe8\xaf\x95"}},
        {"/hint",   {"\xe7\x94\xa8\xe6\xb3\x95: /hint <\xe9\xa2\x98\xe5\x8f\xb7>  AI \xe5\x88\x86\xe6\x9e\x90\xe9\xa2\x98\xe7\x9b\xae\xe5\xb9\xb6\xe7\xbb\x99\xe5\x87\xba\xe8\xa7\xa3\xe9\xa2\x98\xe6\x80\x9d\xe8\xb7\xaf",
                     "\xe2\x9c\x93 Enter \xe8\x8e\xb7\xe5\x8f\x96 AI \xe6\x8f\x90\xe7\xa4\xba"}},
        {"/view",   {"\xe7\x94\xa8\xe6\xb3\x95: /view <\xe9\xa2\x98\xe5\x8f\xb7>  \xe6\x9f\xa5\xe7\x9c\x8b\xe6\xb5\x8b\xe8\xaf\x95\xe7\x94\xa8\xe4\xbe\x8b\xe5\x92\x8c\xe9\xa2\x98\xe7\x9b\xae\xe4\xbf\xa1\xe6\x81\xaf",
                     "\xe2\x9c\x93 Enter \xe6\x9f\xa5\xe7\x9c\x8b\xe8\xaf\xa6\xe6\x83\x85"}},
        {"/delete", {"\xe7\x94\xa8\xe6\xb3\x95: /delete <\xe9\xa2\x98\xe5\x8f\xb7>  \xe4\xbb\x8e\xe6\x9c\xac\xe5\x9c\xb0\xe9\xa2\x98\xe5\xba\x93\xe5\x88\xa0\xe9\x99\xa4\xe9\xa2\x98\xe7\x9b\xae",
                     "\xe2\x9c\x93 Enter \xe5\x88\xa0\xe9\x99\xa4\xe9\xa2\x98\xe7\x9b\xae"}},
        {"/record", {"\xe7\x94\xa8\xe6\xb3\x95: /record <\xe9\xa2\x98\xe5\x8f\xb7>  \xe5\xa4\x8d\xe4\xb9\xa0\xe6\x8e\xa8\xe8\x8d\x90\xe6\xa3\x80\xe6\x9f\xa5\xe5\xae\x8c\xe6\x88\x90\xe5\xb9\xb6\xe8\xae\xb0\xe5\xbd\x95",
                     "\xe2\x9c\x93 Enter \xe8\xae\xb0\xe5\xbd\x95\xe5\xae\x8c\xe6\x88\x90\xe5\xba\xa6"}},
        {"/new",    {"\xe7\x94\xa8\xe6\xb3\x95: /new <\xe6\xa0\x87\xe9\xa2\x98>  \xe5\x88\x9b\xe5\xbb\xba\xe4\xb8\x80\xe9\x81\x93\xe6\x9c\xac\xe5\x9c\xb0\xe9\xa2\x98\xe7\x9b\xae",
                     "\xe2\x9c\x93 Enter \xe5\x88\x9b\xe5\xbb\xba\xe9\xa2\x98\xe7\x9b\xae"}},
        {"/login",  {"\xe7\x94\xa8\xe6\xb3\x95: /login <\xe5\xb9\xb3\xe5\x8f\xb0>  \xe4\xbe\x8b: /login lanqiao",
                     "\xe2\x9c\x93 Enter \xe9\x85\x8d\xe7\xbd\xae\xe7\x99\xbb\xe5\xbd\x95"}},
        {"/list",   {"Enter \xe6\x89\x93\xe5\xbc\x80\xe9\xa2\x98\xe5\xba\x93\xe6\xb5\x8f\xe8\xa7\x88\xe5\x99\xa8, \xe6\x94\xaf\xe6\x8c\x81\xe7\xad\x9b\xe9\x80\x89\xe5\x92\x8c\xe5\xbf\xab\xe6\x8d\xb7\xe6\x93\x8d\xe4\xbd\x9c", nullptr}},
        {"/config", {"Enter \xe6\x89\x93\xe5\xbc\x80\xe9\x85\x8d\xe7\xbd\xae\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8", nullptr}},
        {"/help",   {"Enter \xe6\x9f\xa5\xe7\x9c\x8b\xe5\x85\xa8\xe9\x83\xa8\xe5\x8f\xaf\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4\xe5\x8f\x8a\xe7\x94\xa8\xe6\xb3\x95", nullptr}},
        {"/?",      {"Enter \xe6\x9f\xa5\xe7\x9c\x8b\xe5\x85\xa8\xe9\x83\xa8\xe5\x8f\xaf\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4\xe5\x8f\x8a\xe7\x94\xa8\xe6\xb3\x95", nullptr}},
        {"/init",   {"Enter \xe5\x9c\xa8\xe5\xbd\x93\xe5\x89\x8d\xe7\x9b\xae\xe5\xbd\x95\xe5\x88\x9d\xe5\xa7\x8b\xe5\x8c\x96 .shuati \xe9\xa1\xb9\xe7\x9b\xae\xe7\xbb\x93\xe6\x9e\x84", nullptr}},
        {"/uninstall", {"Enter 查看并清除所有相关记录及 .shuati 文件夹", nullptr}},
        {"/info",   {"Enter \xe6\x98\xbe\xe7\xa4\xba\xe5\xbd\x93\xe5\x89\x8d\xe7\x8e\xaf\xe5\xa2\x83\xe5\x92\x8c\xe7\x89\x88\xe6\x9c\xac\xe4\xbf\xa1\xe6\x81\xaf", nullptr}},
        {"/clean",  {"Enter \xe6\xb8\x85\xe7\x90\x86\xe4\xb8\xb4\xe6\x97\xb6\xe6\x96\x87\xe4\xbb\xb6\xe5\x92\x8c\xe7\xbc\x93\xe5\xad\x98", nullptr}},
        {"/exit",   {"Enter \xe9\x80\x80\xe5\x87\xba\xe7\xa8\x8b\xe5\xba\x8f", nullptr}},
        {"/repl",   {"Enter \xe8\xbf\x9b\xe5\x85\xa5\xe7\xbb\x8f\xe5\x85\xb8\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c\xe6\xa8\xa1\xe5\xbc\x8f", nullptr}},
    };
    return table;
}

std::string get_input_hint(const std::string& input) {
    if (input.empty()) {
        return "\xe8\xbe\x93\xe5\x85\xa5 / \xe5\xbc\x80\xe5\xa4\xb4\xe7\x9a\x84\xe5\x91\xbd\xe4\xbb\xa4, \xe4\xbe\x8b\xe5\xa6\x82 /help \xe6\x9f\xa5\xe7\x9c\x8b\xe5\xb8\xae\xe5\x8a\xa9";
    }

    auto start = input.find_first_not_of(' ');
    if (start == std::string::npos || input[start] != '/') {
        return "\xe5\x91\xbd\xe4\xbb\xa4\xe9\x9c\x80\xe4\xbb\xa5 / \xe5\xbc\x80\xe5\xa4\xb4, \xe4\xbe\x8b\xe5\xa6\x82 /help";
    }

    auto space_pos = input.find(' ', start);
    std::string cmd = (space_pos != std::string::npos) ? input.substr(start, space_pos - start) : input.substr(start);
    bool has_args = (space_pos != std::string::npos);

    auto& table = hint_table();
    auto it = table.find(cmd);
    if (it == table.end()) return "";

    const auto& h = it->second;
    if (has_args && h.with_args) return h.with_args;
    return h.no_args;
}

void load_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x8c\xe8\xaf\xb7\xe5\x85\x88\xe8\xbf\x90\xe8\xa1\x8c /init";
        state.config_state.loaded = true;
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    state.config_state.language       = cfg.language;
    state.config_state.editor         = cfg.editor;
    state.config_state.api_key        = cfg.api_key;
    state.config_state.model          = cfg.model;
    state.config_state.ui_mode        = cfg.ui_mode;
    state.config_state.autostart_repl = cfg.autostart_repl;
    state.config_state.max_tokens     = cfg.max_tokens;
    state.config_state.ai_enabled     = cfg.ai_enabled;
    state.config_state.template_enabled = cfg.template_enabled;
    state.config_state.lanqiao_cookie = cfg.lanqiao_cookie;
    state.config_state.focused_field  = 0;
    state.config_state.status_msg.clear();
    state.config_state.loaded         = true;
}

void save_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x8c\xe8\xaf\xb7\xe5\x85\x88\xe8\xbf\x90\xe8\xa1\x8c /init";
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    cfg.language       = state.config_state.language;
    cfg.editor         = state.config_state.editor;
    cfg.api_key        = state.config_state.api_key;
    cfg.model          = state.config_state.model;
    cfg.ui_mode        = state.config_state.ui_mode;
    cfg.autostart_repl = state.config_state.autostart_repl;
    cfg.max_tokens     = state.config_state.max_tokens;
    cfg.ai_enabled     = state.config_state.ai_enabled;
    cfg.template_enabled = state.config_state.template_enabled;
    cfg.lanqiao_cookie = state.config_state.lanqiao_cookie;
    cfg.save(Config::config_path(root));
    state.config_state.status_msg = "\xe9\x85\x8d\xe7\xbd\xae\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe3\x80\x82";
}



std::string next_status_filter(const std::string& filter) {
    if (filter == "all") return "ac";
    if (filter == "ac") return "failed";
    if (filter == "failed") return "unaudited";
    if (filter == "unaudited") return "review";
    return "all";
}

std::string next_source_filter(const std::string& source) {
    if (source == "all") return "leetcode";
    if (source == "leetcode") return "codeforces";
    if (source == "codeforces") return "luogu";
    if (source == "luogu") return "lanqiao";
    if (source == "lanqiao") return "local";
    return "all";
}

std::string next_difficulty_filter(const std::string& diff) {
    if (diff == "all") return "easy";
    if (diff == "easy") return "medium";
    if (diff == "medium") return "hard";
    return "all";
}

std::string normalize_list_filter(const std::string& filter) {
    if (filter == "all" || filter == "ac" || filter == "failed" ||
        filter == "unaudited" || filter == "review") {
        return filter;
    }
    return {};
}

void load_list_state(AppState& state, const std::string& status_filter = "all", const std::string& difficulty_filter = "all", const std::string& source_filter = "all") {
    auto root = Config::find_root();
    state.list_state.status_filter = status_filter;
    state.list_state.difficulty_filter = difficulty_filter;
    state.list_state.source_filter = source_filter;
    if (root.empty()) {
        state.list_state.error = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x8c\xe8\xaf\xb7\xe5\x85\x88\xe8\xbf\x90\xe8\xa1\x8c /init";
        state.list_state.rows.clear();
        state.list_state.loaded = true;
        return;
    }
    try {
        auto svc = cmd::Services::load(root);
        auto problems = svc.pm->filter_problems(svc.pm->list_problems(), status_filter, difficulty_filter, source_filter);
        
        auto current_time = std::time(nullptr);
        auto reviews = svc.db->get_due_reviews(current_time);
        std::unordered_set<std::string> due_ids;
        for (const auto& r : reviews) due_ids.insert(r.problem_id);

        // Preserve previous selection: find the same problem ID in the new list
        std::string prev_id;
        if (state.list_state.selected < static_cast<int>(state.list_state.rows.size())) {
            prev_id = state.list_state.rows[state.list_state.selected].id;
        }
        state.list_state.rows.clear();
        for (const auto& p : problems) {
            ListState::Row row;
            row.tid        = p.display_id;
            row.id         = p.id;
            row.title      = p.title;
            row.difficulty = p.difficulty;
            row.source     = cmd::canonical_source(p.source);
            row.status     = p.last_verdict.empty() ? "-" : p.last_verdict;
            row.passed     = (p.last_verdict == "AC");
            row.review_due = (due_ids.find(p.id) != due_ids.end());
            {
                char buf[16] = {};
                std::time_t t = static_cast<std::time_t>(p.created_at);
                struct std::tm tm_val{};
#ifdef _WIN32
                localtime_s(&tm_val, &t);
#else
                localtime_r(&t, &tm_val);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
                row.date = buf;
            }
            state.list_state.rows.push_back(std::move(row));
        }
        // Restore selection to previous problem if still visible
        state.list_state.selected = 0;
        if (!prev_id.empty()) {
            for (int i = 0; i < static_cast<int>(state.list_state.rows.size()); ++i) {
                if (state.list_state.rows[i].id == prev_id) {
                    state.list_state.selected = i;
                    break;
                }
            }
        }
        state.list_state.error.clear();
    } catch (const std::exception& e) {
        state.list_state.rows.clear();
        state.list_state.error = std::string("\xe6\x95\xb0\xe6\x8d\xae\xe5\xba\x93\xe9\x94\x99\xe8\xaf\xaf: ") + e.what();
    }
    state.list_state.loaded = true;
}

bool is_error_output(const std::string& text) {
    if (text.empty()) return false;
    return text.starts_with("[Error]") ||
           text.starts_with("[!]") ||
           text.starts_with("Error:");
}

LineType classify_output_line(const std::string& text) {
    if (text.empty()) return LineType::Output;
    if (is_error_output(text)) return LineType::Error;
    if (text.starts_with("[+]") || text.starts_with("[*]") ||
        text.starts_with("[Hint]") || text.starts_with("[TUI]") ||
        text.starts_with("[Coach]")) {
        return LineType::System;
    }
    return LineType::Output;
}

} // namespace

int TuiApp::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    configure_windows_console_for_tui();
#endif

    auto screen = ScreenInteractive::Fullscreen();
    TuiTheme theme;
    AppState state;

    // current_version().to_string() already returns "v0.1.0" — do NOT add another 'v'.
    std::string version = current_version().to_string();
    auto specs = tui_command_specs();

    std::string project_path;
    {
        auto root = Config::find_root();
        if (!root.empty()) project_path = root.string();
    }

    auto alive = std::make_shared<std::atomic_bool>(true);
    std::vector<std::thread> workers;

    auto quit = [&] {
        alive->store(false);
        state.pending_commands.clear();
        state.is_running = false;
        state.active_command.clear();
        screen.ExitLoopClosure()();
    };

    state.post_task = [&screen](std::function<void()> task) {
        screen.Post(std::move(task));
    };

    auto run_command_ptr = std::make_shared<std::function<void(const std::string&, bool)>>();
    *run_command_ptr = [&](const std::string& line, bool record_input) {
        std::string trimmed = line;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) return;

        if (record_input) {
            state.push_history(trimmed);
            state.append(LineType::Input, trimmed);
        }

        std::string normalized = trimmed;
        if (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());

        auto args = parse_command_line(normalized);
        if (args.size() <= 1) return;

        std::string base_cmd = args[1];

        if (base_cmd == "?" || base_cmd == "help") {
            state.append(LineType::Heading, "\xe5\xb8\xae\xe5\x8a\xa9");
            state.append_output_lines(render_help_text());
            return;
        }
        if (base_cmd == "clear" || base_cmd == "cls") {
            state.buffer.clear();
            state.scroll_offset = 0;
            return;
        }
        if (base_cmd == "exit" || base_cmd == "quit") {
            quit();
            return;
        }

        if (base_cmd == "config" && args.size() == 2) {
            load_config_state(state);
            state.push_view(ViewMode::ConfigView);
            return;
        }

        if (base_cmd == "list") {
            std::string requested_status_filter = state.list_state.status_filter.empty() ? "all" : state.list_state.status_filter;
            std::string requested_diff_filter = state.list_state.difficulty_filter.empty() ? "all" : state.list_state.difficulty_filter;
            if (args.size() == 4 && (args[2] == "--filter" || args[2] == "-f")) {
                auto normalized_filter = normalize_list_filter(args[3]);
                if (normalized_filter.empty()) {
                    state.append(LineType::Error,
                        "Invalid list filter. Use all/ac/failed/unaudited/review.");
                    return;
                }
                requested_status_filter = normalized_filter;
            } else if (args.size() != 2) {
                state.append(LineType::Error,
                    "Usage: /list [--filter all|ac|failed|unaudited|review]");
                return;
            }

            load_list_state(state, requested_status_filter, requested_diff_filter);
            state.push_view(ViewMode::ListView);
            return;
        }

        if (base_cmd == "hint") {
            if (state.is_running) {
                state.pending_commands.push_back(trimmed);
                return;
            }
            state.hint_state = HintState{};
            state.hint_state.loading = true;
            state.push_view(ViewMode::HintView);
            state.is_running = true;
            state.active_command = base_cmd;

            workers.emplace_back([alive, run_command_ptr, &screen, &state, args = std::move(args), base_cmd]() mutable {
                auto cb = [alive, &screen, &state](const std::string& chunk) {
                    if (!alive->load()) return;
                    screen.Post([&state, chunk]() {
                        std::string lines = chunk;
                        for (auto& c : lines) {
                            if (c == '\r') c = '\n';
                        }
                        state.hint_state.loading = false;
                        std::string::size_type start = 0;
                        while (start < lines.size()) {
                            auto nl = lines.find('\n', start);
                            if (nl == std::string::npos) {
                                if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                                state.hint_state.lines.back() += lines.substr(start);
                                break;
                            }
                            if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                            state.hint_state.lines.back() += lines.substr(start, nl - start);
                            state.hint_state.lines.push_back("");
                            start = nl + 1;
                        }
                        int max_scroll = std::max(0, static_cast<int>(state.hint_state.lines.size()) - 10);
                        state.hint_state.scroll_offset = max_scroll;
                    });
                };
                tui_execute_command_stream(args, base_cmd, cb);
                if (!alive->load()) return;
                auto run_command_ptr_local = run_command_ptr;
                screen.Post([&state, run_command_ptr_local] {
                    state.hint_state.loading = false;
                    state.is_running = false;
                    state.active_command.clear();
                    if (state.hint_state.lines.empty() && state.hint_state.error.empty()) {
                        state.hint_state.error = "\xe6\x9c\xaa\xe8\x8e\xb7\xe5\x8f\x96\xe5\x88\xb0\xe6\x8f\x90\xe7\xa4\xba\xe5\x86\x85\xe5\xae\xb9\xe3\x80\x82";
                    }
                    if (!state.pending_commands.empty()) {
                        auto next = state.pending_commands.front();
                        state.pending_commands.pop_front();
                        (*run_command_ptr_local)(next, false);
                    }
                });
            });
            return;
        }

        // Block commands that would spawn a nested TUI or REPL inside the TUI.
        if (base_cmd == "tui" || base_cmd == "repl") {
            state.append(LineType::System,
                base_cmd == "tui"
                    ? "\xe5\xbd\x93\xe5\x89\x8d\xe5\xb7\xb2\xe5\x9c\xa8 TUI 模\xe5\xbc\x8f\xe4\xb8\xad\xef\xbc\x9b\xe6\x97\xa0\xe9\x9c\x80\xe9\x87\x8d\xe5\xa4\x8d\xe5\x90\xaf\xe5\x8a\xa8\xe3\x80\x82"
                    : "\xe5\xbd\x93\xe5\x89\x8d\xe5\xb7\xb2\xe5\x9c\xa8 TUI 模\xe5\xbc\x8f\xe4\xb8\xad\xef\xbc\x9b\xe8\xaf\xb7\xe7\x9b\xb4\xe6\x8e\xa5\xe8\xbe\x93\xe5\x85\xa5\xe6\x8c\x87\xe4\xbb\xa4\xe3\x80\x82");
            return;
        }

        if (state.is_running) {
            state.pending_commands.push_back(trimmed);
            return;
        }
        state.is_running = true;
        state.active_command = base_cmd;

        workers.emplace_back([alive, run_command_ptr, &screen, &state, args = std::move(args), base_cmd]() mutable {
            // Use streaming for all real commands so output appears incrementally.
            std::string accumulated;
            auto cb = [alive, &screen, &state, &accumulated](const std::string& chunk) {
                accumulated += chunk;
                // Post each chunk to TUI for live update
                if (!alive->load()) return;
                screen.Post([&state, chunk]() {
                    std::string lines = chunk;
                    for (auto& c : lines) {
                        if (c == '\r') c = '\n';
                    }
                    // Split and append each line
                    std::string::size_type start = 0;
                    while (start < lines.size()) {
                        auto nl = lines.find('\n', start);
                        std::string seg = (nl == std::string::npos)
                            ? lines.substr(start)
                            : lines.substr(start, nl - start);
                        if (!seg.empty() || nl != std::string::npos) {
                            state.buffer.push_back({classify_output_line(seg), seg});
                        }
                        if (nl == std::string::npos) break;
                        start = nl + 1;
                    }
                    state.scroll_offset = 0;
                });
            };

            tui_execute_command_stream(args, base_cmd, cb);

            if (!alive->load()) return;
            auto run_command_ptr_local = run_command_ptr;
            screen.Post([&state, run_command_ptr_local, base_cmd]() {
                state.is_running = false;
                state.active_command.clear();

                if (base_cmd == "record" || base_cmd == "delete") {
                    load_list_state(state, state.list_state.status_filter, state.list_state.difficulty_filter, state.list_state.source_filter);
                }

                if (!state.pending_commands.empty()) {
                    auto next = state.pending_commands.front();
                    state.pending_commands.pop_front();
                    (*run_command_ptr_local)(next, false);
                }
            });
        });
    };

    // --- Component tree ---
    // input_comp is the BASE component so it always has focus.
    // CatchEvent wraps it to intercept our custom keys.
    // Renderer wraps CatchEvent for custom UI layout.

    int cursor_pos = 0;

    InputOption input_opt;
    input_opt.cursor_position = &cursor_pos;
    input_opt.on_change = [&] { update_autocomplete(state, specs); };
    input_opt.on_enter = [&] {
        if (state.input.empty()) return;
        std::string cmd = std::move(state.input);
        state.input.clear();
        cursor_pos = 0;
        state.is_autocomplete_open = false;
        state.history_index = -1;
        (*run_command_ptr)(cmd, true);
    };

    auto input_comp = Input(&state.input, "", input_opt);

    auto event_handler = CatchEvent(input_comp, [&](Event event) -> bool {
        // Ctrl+C: quit
        if (event == Event::Character('\x03')) {
            quit();
            return true;
        }

        // --- Subview: ConfigView ---
        if (state.view_mode == ViewMode::ConfigView) {
            auto& cs = state.config_state;
            if (event == Event::Escape) {
                state.pop_view();
                return true;
            }
            if (event == Event::ArrowUp) {
                cs.focused_field = std::max(0, cs.focused_field - 1);
                return true;
            }
            if (event == Event::ArrowDown) {
                cs.focused_field = std::min(ConfigState::TOTAL_ITEMS - 1, cs.focused_field + 1);
                return true;
            }
            if (event == Event::Return) {
                if (cs.focused_field == ConfigState::SAVE_INDEX) {
                    save_config_state(state);
                    return true;
                }
                if (cs.focused_field == ConfigState::CANCEL_INDEX) {
                    state.pop_view();
                    return true;
                }
                if (cs.focused_field == 0) {
                    cs.language = (cs.language == "cpp") ? "python" : "cpp";
                    return true;
                }
                if (cs.focused_field == 4) {
                    cs.ui_mode = (cs.ui_mode == "tui") ? "legacy" : "tui";
                    return true;
                }
                if (cs.focused_field == 5) {
                    cs.autostart_repl = !cs.autostart_repl;
                    return true;
                }
                if (cs.focused_field == 7) {
                    cs.ai_enabled = !cs.ai_enabled;
                    return true;
                }
                if (cs.focused_field == 8) {
                    cs.template_enabled = !cs.template_enabled;
                    return true;
                }
                return true;
            }
            if (cs.focused_field >= 1 && cs.focused_field <= 3) {
                auto target = [&]() -> std::string* {
                    switch (cs.focused_field) {
                    case 1: return &cs.editor;
                    case 2: return &cs.api_key;
                    case 3: return &cs.model;
                    default: return nullptr;
                    }
                }();
                if (target) {
                    if (event == Event::Backspace) {
                        if (!target->empty()) target->pop_back();
                        return true;
                    }
                    if (event.is_character()) {
                        *target += event.character();
                        return true;
                    }
                }
            }
            if (cs.focused_field == 6) { // max_tokens
                if (event == Event::Backspace) {
                    cs.max_tokens /= 10;
                    return true;
                }
                if (event.is_character() && isdigit(event.character()[0])) {
                    cs.max_tokens = cs.max_tokens * 10 + (event.character()[0] - '0');
                    return true;
                }
            }
            if (cs.focused_field == 9) { // lanqiao_cookie
                if (event == Event::Backspace) {
                    if (!cs.lanqiao_cookie.empty()) cs.lanqiao_cookie.pop_back();
                    return true;
                }
                if (event.is_character()) {
                    cs.lanqiao_cookie += event.character();
                    return true;
                }
            }
            return true;
        }

        // --- Subview: ListView ---
        if (state.view_mode == ViewMode::ListView) {
            auto& ls = state.list_state;
            if (event == Event::Escape) {
                state.pop_view();
                return true;
            }
            if (event == Event::ArrowUp || event == Event::Character('k')) {
                ls.selected = std::max(0, ls.selected - 1);
                return true;
            }
            if (event == Event::ArrowDown || event == Event::Character('j')) {
                ls.selected = std::min(static_cast<int>(ls.rows.size()) - 1, ls.selected + 1);
                return true;
            }
            if (event == Event::Character('f')) {
                load_list_state(state, next_status_filter(ls.status_filter), ls.difficulty_filter);
                return true;
            }
            if (event == Event::Character('F')) {
                load_list_state(state, ls.status_filter, next_difficulty_filter(ls.difficulty_filter), ls.source_filter);
                return true;
            }
            if (event == Event::Character('s')) {
                load_list_state(state, ls.status_filter, ls.difficulty_filter, next_source_filter(ls.source_filter));
                return true;
            }
            auto get_selected_tid = [&]() -> std::string {
                if (ls.selected >= 0 && ls.selected < static_cast<int>(ls.rows.size()))
                    return std::to_string(ls.rows[ls.selected].tid);
                return "";
            };
            if (event == Event::Return && !ls.rows.empty()) {
                state.pop_view();
                (*run_command_ptr)("/solve " + get_selected_tid(), true);
                return true;
            }
            if (event == Event::Character('t') && !ls.rows.empty()) {
                state.pop_view();
                (*run_command_ptr)("/test " + get_selected_tid(), true);
                return true;
            }
            if (event == Event::Character('v') && !ls.rows.empty()) {
                state.pop_view();
                (*run_command_ptr)("/view " + get_selected_tid(), true);
                return true;
            }
            if (event == Event::Character('d') && !ls.rows.empty()) {
                state.pop_view();
                (*run_command_ptr)("/delete " + get_selected_tid(), true);
                return true;
            }
            if (event == Event::Character('h') && !ls.rows.empty()) {
                auto tid = get_selected_tid();
                state.pop_view();
                (*run_command_ptr)("/hint " + tid, true);
                return true;
            }
            if (event == Event::Character('r') && !ls.rows.empty()) {
                auto tid = get_selected_tid();
                state.pop_view();
                (*run_command_ptr)("/record " + tid, true);
                return true;
            }
            return true;
        }

        // --- Subview: HintView ---
        if (state.view_mode == ViewMode::HintView) {
            auto& hs = state.hint_state;
            if (event == Event::Escape) {
                state.pop_view();
                return true;
            }
            int max_scroll = std::max(0, static_cast<int>(hs.lines.size()) - 10);
            if (event == Event::ArrowUp || event == Event::Character('k')) {
                hs.scroll_offset = std::max(0, hs.scroll_offset - 1);
                return true;
            }
            if (event == Event::ArrowDown || event == Event::Character('j')) {
                hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 1);
                return true;
            }
            if (event == Event::PageUp || event == Event::Special("\x1B[5~")) {
                hs.scroll_offset = std::max(0, hs.scroll_offset - 10);
                return true;
            }
            if (event == Event::PageDown || event == Event::Special("\x1B[6~")) {
                hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 10);
                return true;
            }
            return true;
        }

        // --- Main view ---

        // Ctrl+L: clear screen
        if (event == Event::Character('\x0C')) {
            state.buffer.clear();
            state.scroll_offset = 0;
            return true;
        }

        // Ctrl+U: clear input line
        if (event == Event::Character('\x15')) {
            state.input.clear();
            cursor_pos = 0;
            state.is_autocomplete_open = false;
            return true;
        }

        // Autocomplete navigation (intercept before input gets arrows)
        if (state.is_autocomplete_open) {
            if (event == Event::ArrowDown) {
                int max_idx = static_cast<int>(state.autocomplete_labels.size()) - 1;
                state.autocomplete_selected = std::min(max_idx, state.autocomplete_selected + 1);
                return true;
            }
            if (event == Event::ArrowUp) {
                state.autocomplete_selected = std::max(0, state.autocomplete_selected - 1);
                return true;
            }
            if (event == Event::Tab) {
                if (state.autocomplete_selected >= 0 &&
                    state.autocomplete_selected < static_cast<int>(state.autocomplete_commands.size())) {
                    state.input = state.autocomplete_commands[state.autocomplete_selected] + " ";
                    cursor_pos = static_cast<int>(state.input.size());
                    update_autocomplete(state, specs);
                }
                return true;
            }
            if (event == Event::Escape) {
                state.is_autocomplete_open = false;
                return true;
            }
            // Let other keys (characters, backspace) pass to input_comp
        }

        // Command history (only when autocomplete is closed and using arrows)
        if (!state.is_autocomplete_open && !state.command_history.empty()) {
            if (event == Event::ArrowUp) {
                if (state.history_index == -1) {
                    state.history_stash = state.input;
                    state.history_index = static_cast<int>(state.command_history.size()) - 1;
                } else if (state.history_index > 0) {
                    state.history_index--;
                }
                state.input = state.command_history[state.history_index];
                cursor_pos = static_cast<int>(state.input.size());
                return true;
            }
            if (event == Event::ArrowDown && state.history_index >= 0) {
                state.history_index++;
                if (state.history_index >= static_cast<int>(state.command_history.size())) {
                    state.history_index = -1;
                    state.input = state.history_stash;
                    state.history_stash.clear();
                } else {
                    state.input = state.command_history[state.history_index];
                }
                cursor_pos = static_cast<int>(state.input.size());
                return true;
            }
        }

        // Buffer scroll
        if (event == Event::PageUp || event == Event::Special("\x1B[5~")) {
            int max_scroll = std::max(0, static_cast<int>(state.buffer.size()) - 20);
            state.scroll_offset = std::min(state.scroll_offset + 5, max_scroll);
            return true;
        }
        if (event == Event::PageDown || event == Event::Special("\x1B[6~")) {
            state.scroll_offset = std::max(state.scroll_offset - 5, 0);
            return true;
        }

        // All other events (characters, Backspace, Delete, ArrowLeft, ArrowRight, Home, End)
        // pass through to input_comp which is the base component.
        return false;
    });

    auto renderer = Renderer(event_handler, [&] {
        auto top_bar = render_top_bar(theme, version, project_path);
        auto bottom_bar = render_bottom_bar(theme, state.is_running, state.active_command);

        Element separator_line = separatorLight() | color(theme.border_color);

        // --- Subview rendering ---
        if (state.view_mode == ViewMode::ConfigView) {
            return vbox({
                top_bar,
                separator_line,
                render_config_view(state, theme) | flex,
                separator_line,
                bottom_bar,
            }) | borderStyled(ROUNDED);
        }
        if (state.view_mode == ViewMode::ListView) {
            return vbox({
                top_bar,
                separator_line,
                render_list_view(state, theme) | flex,
                separator_line,
                bottom_bar,
            }) | borderStyled(ROUNDED);
        }
        if (state.view_mode == ViewMode::HintView) {
            return vbox({
                top_bar,
                separator_line,
                render_hint_view(state, theme) | flex,
                separator_line,
                bottom_bar,
            }) | borderStyled(ROUNDED);
        }

        // --- Main view ---
        auto buffer_content = render_buffer(state, theme);
        auto buffer_area = buffer_content | focusPositionRelative(0, 1) | frame | flex;

        // Autocomplete popup
        Element ac_element = emptyElement();
        if (state.is_autocomplete_open && !state.autocomplete_labels.empty()) {
            Elements ac_items;
            for (int i = 0; i < static_cast<int>(state.autocomplete_labels.size()); i++) {
                auto item = text("  " + state.autocomplete_labels[i] + "  ");
                if (i == state.autocomplete_selected) {
                    item = item | bold | bgcolor(theme.selected_bg) | color(theme.accent_color);
                } else {
                    item = item | color(theme.dim_color);
                }
                ac_items.push_back(item);
            }
            ac_element = vbox(std::move(ac_items))
                | size(HEIGHT, LESS_THAN, 8)
                | borderStyled(ROUNDED)
                | color(theme.border_color);
        }

        // Contextual hint line
        std::string hint = get_input_hint(state.input);
        Element hint_line = emptyElement();
        if (!hint.empty()) {
            hint_line = text("  " + hint) | color(theme.dim_color);
        }

        auto prompt_char = state.is_running ? "  " : "> ";
        auto input_line = hbox({
            text(" ") | color(theme.dim_color),
            text(prompt_char) | bold | color(theme.prompt_color),
            input_comp->Render() | flex,
        });

        return vbox({
            top_bar,
            separator_line,
            buffer_area,
            ac_element,
            separator_line,
            hint_line,
            input_line,
            separator_line,
            bottom_bar,
        }) | borderStyled(ROUNDED);
    });

    screen.Loop(renderer);
    alive->store(false);
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    return 0;
}

} // namespace tui
} // namespace shuati
