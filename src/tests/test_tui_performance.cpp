#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "shuati/tui_views.hpp"
#include "../tui/tui_command_engine.hpp"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

static long long benchmark_first_render_ms() {
    shuati::tui::AppState state;
    shuati::tui::TuiTheme theme;
    state.view_state = shuati::tui::AppState::ViewState::Chat;
    state.environment_info = "v0.1.0-bench";
    state.tips_for_getting_started = "Use /help";
    state.history.push_back({shuati::tui::MessageRole::System, "bench"});
    auto input_el = ftxui::text("input");
    auto history_el = ftxui::text("history");
    auto autocomplete_el = ftxui::text("autocomplete");
    auto selection_el = ftxui::text("selection");

    auto begin = std::chrono::steady_clock::now();
    auto main_el = shuati::tui::render_chat_layout(
        state, theme, input_el, history_el, autocomplete_el, selection_el, 120, 40
    );
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, main_el);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
}

static long long benchmark_input_feedback_us() {
    auto begin = std::chrono::steady_clock::now();
    auto all = shuati::tui::tui_command_candidates();
    std::vector<std::string> matched;
    matched.reserve(all.size());
    for (const auto& cmd : all) {
        if (cmd.starts_with("/so")) {
            matched.push_back(cmd);
        }
    }
    auto end = std::chrono::steady_clock::now();
    assert(!matched.empty());
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
}

int main() {
    auto first_render_ms = benchmark_first_render_ms();
    auto input_feedback_us = benchmark_input_feedback_us();

    std::cout << "first_render_ms=" << first_render_ms << "\n";
    std::cout << "input_feedback_us=" << input_feedback_us << "\n";

    assert(first_render_ms <= 300);
    assert(input_feedback_us <= 50000);
    return 0;
}
