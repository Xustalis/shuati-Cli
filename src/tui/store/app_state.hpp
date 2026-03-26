#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <atomic>
#include <memory>

namespace shuati {
namespace tui {

enum class ViewMode { Main, ConfigView, ListView, HintView, PullView, SolveView, StatusView, MenuView };

struct StatusState {
    int total_problems = 0;
    int ac_problems = 0;
    int pending_reviews = 0;
    std::string last_activity;
    bool loaded = false;
};

enum class LineType {
    Input,
    Output,
    SystemLog,
    Error,
    Heading,
    Separator
};

struct BufferLine {
    LineType type;
    std::string text;
};

struct ConfigState {
    std::string language;
    std::string editor;
    std::string api_key;
    std::string model;
    std::string ui_mode;
    bool autostart_repl = true;
    int max_tokens = 4096;
    bool ai_enabled = true;
    bool template_enabled = true;
    std::string lanqiao_cookie;

    int focused_field = 0;
    bool loaded = false;
    std::string status_msg;

    static constexpr int FIELD_COUNT = 10;
    static constexpr int SAVE_INDEX = 10;
    static constexpr int CANCEL_INDEX = 11;
    static constexpr int TOTAL_ITEMS = 12;
};

struct ListState {
    struct Row {
        int tid = 0;
        std::string id;
        std::string title;
        std::string difficulty;
        std::string source;
        std::string status;
        std::string date;
        bool passed = false;
        bool review_due = false;
    };
    std::vector<Row> rows;
    int selected = 0;
    std::string status_filter = "all";
    std::string difficulty_filter = "all";
    std::string source_filter = "all";
    bool loaded = false;
    std::string error;
};

struct HintState {
    std::vector<std::string> lines;
    int scroll_offset = 0;
    bool loading = false;
    std::string error;

    // Quick actions for secondary menu selection
    std::vector<std::string> actions = {"\xe7\xbb\xa7\xe7\xbb\xad\xe5\x81\x9a\xe9\xa2\x98 (Solve)", "\xe8\xbf\x90\xe8\xa1\x8c\xe6\xb5\x8b\xe8\xaf\x95 (Test)", "\xe8\xbf\x94\xe5\x9b\x9e\xe5\x88\x97\xe8\xa1\x88 (List)"};
    int selected_action = 0;
    bool show_actions = false;
};

struct PullState {
    std::string url;
    bool loading = false;
    float progress = 0.0f;
    std::string status_msg;
    std::string error;
    bool finished = false;
    int countdown = 3;
    
    struct Result {
        int tid = 0;
        std::string title;
        std::string path;
    } result;
};

struct SolveState {
    std::vector<std::string> sources = {"luogu", "leetcode", "codeforces", "lanqiao"};
    std::vector<std::string> selected_sources = {"luogu", "leetcode", "codeforces", "lanqiao"};
    std::vector<std::string> selected_difficulties = {"easy", "medium", "hard"};

    std::string search_query;

    struct Row {
        int tid = 0;
        std::string id;
        std::string title;
        std::string difficulty;
        std::string source;
    };
    std::vector<Row> filtered_rows;
    int selected_idx = 0;

    bool show_preview = false;
    std::string preview_content;
    bool has_template = false;
    int preview_scroll = 0;
};

struct MenuState {
    struct MenuItem {
        std::string label;
        std::string description;
        std::string command;
        std::string route_id;
        std::string params;
        bool requires_args = false;
        std::string args_placeholder;
    };
    struct MenuCategory {
        std::string name;
        std::vector<MenuItem> items;
    };
    std::vector<MenuCategory> categories;
    int selected_category = 0;
    int selected_item = 0;

    void init_from_specs();
};

struct SidebarState {
    int selected = 0;
    std::vector<std::string> categories = {
        " \xe5\xb7\xa5\xe4\xbd\x9c\xe5\x8f\xb0 ", " \xe9\xa2\x98\xe5\xba\x93\xe6\xb5\x8b\xe8\xaf\x95 ", " AI \xe6\x95\x99\xe7\xbb\x83 ", " \xe7\x8a\xb6\xe6\x80\x81\xe7\x9b\x91\xe6\x8e\xa7 ", " \xe8\xae\xbe\xe7\xbd\xae\xe4\xb8\x8e\xe5\xb8\xae\xe5\x8a\xa9 "
    };
};

struct AppState {
    ViewMode view_mode = ViewMode::Main;
    std::vector<ViewMode> view_stack;

    std::vector<BufferLine> buffer;
    int scroll_offset = 0;

    std::string input;

    bool is_autocomplete_open = false;
    std::vector<std::string> autocomplete_labels;
    std::vector<std::string> autocomplete_commands;
    int autocomplete_selected = 0;

    bool is_running = false;
    std::string active_command;

    // Commands waiting for the currently running command to finish.
    // Only the TUI main thread mutates this; worker threads update UI via screen.Post.
    std::deque<std::string> pending_commands;

    std::function<void(std::function<void()>)> post_task = [](std::function<void()>) {};

    std::vector<std::string> command_history;
    int history_index = -1;
    std::string history_stash;

    ConfigState config_state;
    ListState list_state;
    HintState hint_state;
    PullState pull_state;
    SolveState solve_state;
    StatusState status_state;
    MenuState menu_state;
    SidebarState sidebar_state;

    int active_pane = 1; // 0 = Sidebar, 1 = Main Workspace
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

    void append(LineType type, const std::string& text) {
        buffer.push_back({type, text});
        scroll_offset = 0;
    }

    void push_view(ViewMode mode) {
        if (view_mode != mode) {
            view_stack.push_back(view_mode);
            view_mode = mode;
        }
    }

    bool pop_view() {
        if (!view_stack.empty()) {
            view_mode = view_stack.back();
            view_stack.pop_back();
            return true;
        }
        view_mode = ViewMode::Main;
        return false;
    }

    void append_output_lines(const std::string& text) {
        if (text.empty()) return;
        std::string::size_type start = 0;
        while (start < text.size()) {
            auto nl = text.find('\n', start);
            if (nl == std::string::npos) {
                buffer.push_back({LineType::Output, text.substr(start)});
                break;
            }
            buffer.push_back({LineType::Output, text.substr(start, nl - start)});
            start = nl + 1;
        }
        scroll_offset = 0;
    }

    void push_history(const std::string& cmd) {
        if (!cmd.empty() && (command_history.empty() || command_history.back() != cmd)) {
            command_history.push_back(cmd);
        }
        history_index = -1;
        history_stash.clear();
    }
};

} // namespace tui
} // namespace shuati
