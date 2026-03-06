#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "shuati/tui_views.hpp"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

using shuati::tui::AppState;
using shuati::tui::TuiTheme;
using shuati::tui::LineType;
using shuati::tui::render_buffer;
using shuati::tui::render_top_bar;
using shuati::tui::render_bottom_bar;
using shuati::tui::render_welcome;
using shuati::tui::render_config_view;
using shuati::tui::render_list_view;
using shuati::tui::render_hint_view;

static void render_to_string(ftxui::Element layout, int w, int h) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(w), ftxui::Dimension::Fixed(h));
    ftxui::Render(screen, layout);
    auto out = screen.ToString();
    assert(!out.empty());
}

static void test_main_view(int width, int height) {
    AppState state;
    TuiTheme theme;

    state.append(LineType::System, "Welcome to TUI test");
    state.append(LineType::Input, "/help");
    state.append(LineType::Heading, "Help");
    state.append(LineType::Output, "  /pull <url>  --  Pull a problem");
    state.append(LineType::Error, "Some error");
    state.append(LineType::Separator, "--- test " + std::string(48, '-'));

    auto top    = render_top_bar(theme, "v0.1.0-test");
    auto buffer = render_buffer(state, theme);
    auto bottom = render_bottom_bar(theme, false);

    auto layout = ftxui::vbox({
        top,
        ftxui::separator(),
        buffer | ftxui::flex,
        ftxui::separator(),
        bottom,
    });

    render_to_string(layout, width, height);
}

static void test_welcome() {
    TuiTheme theme;
    auto welcome = render_welcome(theme);
    render_to_string(welcome, 80, 24);
    render_to_string(welcome, 120, 40);
}

static void test_empty_buffer_shows_welcome() {
    AppState state;
    TuiTheme theme;
    auto buf = render_buffer(state, theme);
    render_to_string(buf, 80, 24);
}

static void test_config_view() {
    AppState state;
    TuiTheme theme;

    state.config_state.language       = "cpp";
    state.config_state.editor         = "code";
    state.config_state.api_key        = "sk-test1234567890";
    state.config_state.model          = "deepseek-chat";
    state.config_state.ui_mode        = "tui";
    state.config_state.autostart_repl = true;
    state.config_state.loaded         = true;

    for (int field = 0; field < shuati::tui::ConfigState::TOTAL_ITEMS; field++) {
        state.config_state.focused_field = field;
        auto view = render_config_view(state, theme);
        render_to_string(view, 100, 30);
    }

    state.config_state.status_msg = "Configuration saved.";
    auto view = render_config_view(state, theme);
    render_to_string(view, 100, 30);
}

static void test_list_view() {
    AppState state;
    TuiTheme theme;

    state.list_state.filter = "all";
    state.list_state.rows = {
        {1, "two-sum",         "Two Sum",                "easy",   "AC",     "2025-03-01"},
        {2, "add-two-numbers", "Add Two Numbers",        "medium", "WA 1/3", "2025-03-02"},
        {3, "longest-substr",  "Longest Substring",      "hard",   "-",      "-"},
    };
    state.list_state.loaded = true;

    for (int sel = 0; sel < 3; sel++) {
        state.list_state.selected = sel;
        auto view = render_list_view(state, theme);
        render_to_string(view | ftxui::flex, 100, 30);
    }
}

static void test_hint_view() {
    AppState state;
    TuiTheme theme;

    // Loading state
    state.hint_state.loading = true;
    auto view = render_hint_view(state, theme);
    render_to_string(view, 100, 30);

    // With content
    state.hint_state.loading = false;
    for (int i = 0; i < 50; i++) {
        state.hint_state.lines.push_back("Line " + std::to_string(i) + ": This is a hint about the problem.");
    }
    view = render_hint_view(state, theme);
    render_to_string(view | ftxui::flex, 100, 30);

    // Error state
    shuati::tui::AppState err_state;
    err_state.hint_state.error = "Failed to get hint.";
    view = render_hint_view(err_state, theme);
    render_to_string(view, 100, 20);

    // Empty state
    shuati::tui::AppState empty_state;
    view = render_hint_view(empty_state, theme);
    render_to_string(view, 100, 20);
}

static void test_list_view_empty() {
    AppState state;
    TuiTheme theme;
    state.list_state.error = "Not in a shuati project.";
    auto view = render_list_view(state, theme);
    render_to_string(view, 100, 20);
}

int main() {
    std::cout << "=== TUI Render Tests ===" << std::endl;

    const std::vector<std::pair<int, int>> sizes = {
        {80, 24}, {100, 30}, {120, 40}, {160, 48}, {256, 64},
    };
    for (const auto& [w, h] : sizes) {
        test_main_view(w, h);
        std::cout << "  [PASS] main_view " << w << "x" << h << std::endl;
    }

    test_welcome();
    std::cout << "  [PASS] welcome" << std::endl;

    test_empty_buffer_shows_welcome();
    std::cout << "  [PASS] empty_buffer_welcome" << std::endl;

    test_config_view();
    std::cout << "  [PASS] config_view" << std::endl;

    test_list_view();
    std::cout << "  [PASS] list_view" << std::endl;

    test_list_view_empty();
    std::cout << "  [PASS] list_view_empty" << std::endl;

    test_hint_view();
    std::cout << "  [PASS] hint_view" << std::endl;

    std::cout << "All TUI render tests passed!" << std::endl;
    return 0;
}
