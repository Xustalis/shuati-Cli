#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>

namespace shuati {
namespace tui {

enum class ViewMode { Main, ConfigView, ListView, HintView };

enum class LineType {
    Input,
    Output,
    System,
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

    int focused_field = 0;
    bool loaded = false;
    std::string status_msg;

    static constexpr int FIELD_COUNT = 6;
    static constexpr int SAVE_INDEX = 6;
    static constexpr int CANCEL_INDEX = 7;
    static constexpr int TOTAL_ITEMS = 8;
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
