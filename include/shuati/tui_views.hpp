#pragma once

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>
#include "../../src/tui/store/app_state.hpp"

namespace shuati {
namespace tui {

struct TuiTheme {
    ftxui::Color bg_color           = ftxui::Color::Default;
    ftxui::Color accent_color       = ftxui::Color::RGB(100, 149, 237);
    ftxui::Color text_color         = ftxui::Color::Default;
    ftxui::Color dim_color          = ftxui::Color::RGB(100, 100, 100);
    ftxui::Color error_color        = ftxui::Color::RGB(220, 80, 60);
    ftxui::Color input_color        = ftxui::Color::RGB(180, 180, 180);
    ftxui::Color system_color       = ftxui::Color::RGB(130, 170, 100);
    ftxui::Color heading_color      = ftxui::Color::RGB(100, 149, 237);
    ftxui::Color prompt_color       = ftxui::Color::RGB(190, 140, 250);
    ftxui::Color success_color      = ftxui::Color::RGB(100, 200, 120);
    ftxui::Color warn_color         = ftxui::Color::RGB(220, 180, 60);
    ftxui::Color table_header_color = ftxui::Color::RGB(160, 160, 220);
    ftxui::Color selected_bg        = ftxui::Color::RGB(50, 50, 80);
};

ftxui::Element render_top_bar(const TuiTheme& theme, const std::string& version);
ftxui::Element render_bottom_bar(const TuiTheme& theme, bool is_running);
ftxui::Element render_buffer(const AppState& state, const TuiTheme& theme);
ftxui::Element render_buffer_line(const BufferLine& line, const TuiTheme& theme);
ftxui::Element render_welcome(const TuiTheme& theme);
ftxui::Element render_config_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_list_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_hint_view(const AppState& state, const TuiTheme& theme);

} // namespace tui
} // namespace shuati
