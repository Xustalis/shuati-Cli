#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "shuati/tui_views.hpp"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

using shuati::tui::AppState;
using shuati::tui::TuiTheme;
using shuati::tui::render_chat_layout;
using shuati::tui::MessageRole;

static void render_smoke(int width, int height) {
    AppState state;
    TuiTheme theme;
    state.view_state = shuati::tui::AppState::ViewState::Chat;
    state.environment_info = "v0.1.0-test";
    state.tips_for_getting_started = "Use /help to view commands";
    state.history.push_back({MessageRole::System, "Welcome to TUI test"});

    auto input_el = ftxui::text("input");
    auto history_el = ftxui::text("history");
    auto autocomplete_el = ftxui::text("autocomplete");
    auto selection_el = ftxui::text("selection");

    auto main_el = render_chat_layout(state, theme, input_el, history_el, autocomplete_el, selection_el, width, height);

    auto screen_main = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen_main, main_el);
    auto s1 = screen_main.ToString();
    assert(!s1.empty());
}

int main() {
    std::cout << "=== TUI Render Smoke Tests ===" << std::endl;
    const std::vector<std::pair<int, int>> sizes = {
        {80, 24},
        {100, 30},
        {120, 40},
        {160, 48},
        {256, 64},
    };
    for (int round = 1; round <= 3; ++round) {
        for (const auto& [w, h] : sizes) {
            render_smoke(w, h);
            std::cout << "  [PASS] round=" << round << " " << w << "x" << h << std::endl;
        }
    }
    std::cout << "All TUI render tests passed!" << std::endl;
    return 0;
}
