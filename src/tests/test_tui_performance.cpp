#include <cassert>
#include <chrono>
#include <cstdlib>
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
    state.append(shuati::tui::LineType::System, "bench");

    auto begin = std::chrono::steady_clock::now();
    auto top = shuati::tui::render_top_bar(theme, "v0.1.0-bench");
    auto buf = shuati::tui::render_buffer(state, theme);
    auto bot = shuati::tui::render_bottom_bar(theme, false);
    auto layout = ftxui::vbox({top, ftxui::separator(), buf | ftxui::flex, ftxui::separator(), bot});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, layout);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
}

static long long benchmark_welcome_render_ms() {
    shuati::tui::TuiTheme theme;
    auto begin = std::chrono::steady_clock::now();
    auto el = shuati::tui::render_welcome(theme);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, el);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
}

static long long benchmark_config_view_ms() {
    shuati::tui::AppState state;
    shuati::tui::TuiTheme theme;
    state.config_state.language = "cpp";
    state.config_state.editor   = "code";
    state.config_state.api_key  = "sk-abcdefg1234567890";
    state.config_state.model    = "deepseek-chat";
    state.config_state.ui_mode  = "tui";
    state.config_state.autostart_repl = true;
    state.config_state.loaded   = true;

    auto begin = std::chrono::steady_clock::now();
    auto el = shuati::tui::render_config_view(state, theme);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, el);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
}

static long long benchmark_list_view_ms() {
    shuati::tui::AppState state;
    shuati::tui::TuiTheme theme;
    state.list_state.status_filter = "all";
    state.list_state.difficulty_filter = "all";
    for (int i = 0; i < 100; i++) {
        state.list_state.rows.push_back({
            i + 1,
            "problem-" + std::to_string(i),
            "Problem Title #" + std::to_string(i),
            (i % 3 == 0) ? "easy" : (i % 3 == 1) ? "medium" : "hard",
            "leetcode",
            (i % 4 == 0) ? "AC" : (i % 4 == 1) ? "WA 1/3" : "-",
            "2025-01-" + std::string(i < 9 ? "0" : "") + std::to_string(i + 1),
            (i % 4 == 0),
            false,
        });
    }
    state.list_state.selected = 50;
    state.list_state.loaded = true;

    auto begin = std::chrono::steady_clock::now();
    auto el = shuati::tui::render_list_view(state, theme);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, el | ftxui::flex);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
}

static long long benchmark_hint_view_ms() {
    shuati::tui::AppState state;
    shuati::tui::TuiTheme theme;
    for (int i = 0; i < 500; i++) {
        state.hint_state.lines.push_back(
            "Hint line " + std::to_string(i) + ": Consider using dynamic programming with memoization for this subproblem.");
    }
    state.hint_state.scroll_offset = 0;

    auto begin = std::chrono::steady_clock::now();
    auto el = shuati::tui::render_hint_view(state, theme);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, el | ftxui::flex);
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
    auto first_render      = benchmark_first_render_ms();
    auto welcome_render    = benchmark_welcome_render_ms();
    auto config_render     = benchmark_config_view_ms();
    auto list_render       = benchmark_list_view_ms();
    auto hint_render       = benchmark_hint_view_ms();
    auto input_feedback    = benchmark_input_feedback_us();

    std::cout << "first_render_ms="   << first_render   << "\n";
    std::cout << "welcome_render_ms=" << welcome_render  << "\n";
    std::cout << "config_render_ms="  << config_render   << "\n";
    std::cout << "list_100_render_ms=" << list_render    << "\n";
    std::cout << "hint_500_render_ms=" << hint_render    << "\n";
    std::cout << "input_feedback_us=" << input_feedback  << "\n";

    // These are micro-bench style assertions. CI runners can have different CPU scheduling
    // and graphics/terminal overhead. If we're on CI, use much more lenient limits so the
    // pipeline doesn't fail due to transient noise.
    const bool is_ci = std::getenv("CI") != nullptr;
    const long long first_limit   = is_ci ? 2000 : 600;
    const long long welcome_limit = is_ci ? 2000 : 250;
    const long long config_limit  = is_ci ? 2000 : 250;
    const long long list_limit    = is_ci ? 2000 : 600;
    const long long hint_limit    = is_ci ? 2000 : 600;
    const long long input_limit   = is_ci ? 200000 : 100000;

    assert(first_render   <= first_limit);
    assert(welcome_render <= welcome_limit);
    assert(config_render  <= config_limit);
    assert(list_render    <= list_limit);
    assert(hint_render    <= hint_limit);
    assert(input_feedback <= input_limit);

    std::cout << "All performance benchmarks passed.\n";
    return 0;
}
