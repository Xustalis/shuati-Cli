#pragma once

#include <string>
#include <vector>
#include <functional>

namespace shuati {
namespace tui {

enum class PageID {
    Welcome,
    SolveWorkspace,
    ProblemList,
    ReportView,
    Settings
};

enum class MessageRole {
    User,
    Assistant,
    System,
    Error
};

struct ChatMessage {
    MessageRole role;
    std::string text;
};

// UI Dialog State
struct DialogState {
    bool is_open = false;
    std::string title;
    std::string message;
    std::vector<std::string> buttons;
    std::function<void(int)> on_resolve;
};

// Application State Store (Single Source of Truth)
struct AppState {
    // Navigation routing
    std::vector<PageID> page_stack = { PageID::SolveWorkspace };
    int current_page_index = 1; // index matching PageID for Tab container
    
    // --- Global Workspace / Chat State (Migrated from ChatLayoutModel) ---
    std::vector<ChatMessage> history;
    std::string environment_info = "v0.1.0";
    std::string tips_for_getting_started = "试试运行 /help 查看所有命令。使用 /pull 拉取题目，/solve 进入做题模式。";
    std::vector<std::string> recent_activity;

    // View State Overrides (within Workspace Page)
    enum class ViewState { Chat, Selection, Report } view_state = ViewState::Chat;
    
    bool is_autocomplete_open = false;
    std::vector<std::string> autocomplete_options;
    int autocomplete_selected = 0;
    
    std::vector<std::string> selection_options;
    int selection_selected = 0;
    std::string selection_title = "请选择";
    std::string selection_pending_command = "";

    std::string report_text;
    std::string captured_output; // Real-time command output
    bool is_running = false;
    
    // UI Dispatcher for thread-safe updates & re-rendering
    std::function<void(std::function<void()>)> post_task = [](std::function<void()>) {};
    // ---------------------------------------------------------------------

    // Global Overlays
    DialogState current_dialog;
    std::string toast_message;
    int toast_timer_ms = 0;
    
    // Global terminal buffer (for executing commands safely without tearing TUI)
    std::string terminal_buffer;
    bool is_command_running = false;

    // Helper functions for Redux-like state dispatching
    void push_page(PageID id) {
        page_stack.push_back(id);
        current_page_index = static_cast<int>(id);
    }
    
    void pop_page() {
        if (page_stack.size() > 1) {
            page_stack.pop_back();
            current_page_index = static_cast<int>(page_stack.back());
        }
    }

    void show_dialog(const std::string& title, const std::string& msg, const std::vector<std::string>& btns, std::function<void(int)> on_res) {
        current_dialog.is_open = true;
        current_dialog.title = title;
        current_dialog.message = msg;
        current_dialog.buttons = btns;
        current_dialog.on_resolve = [this, on_res](int index) {
            current_dialog.is_open = false;
            if (on_res) on_res(index);
        };
    }
    
    void show_toast(const std::string& msg, int fade_ms = 3000) {
        toast_message = msg;
        toast_timer_ms = fade_ms;
    }
};

} // namespace tui
} // namespace shuati
