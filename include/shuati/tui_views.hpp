#pragma once

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>
#include "../../src/tui/store/app_state.hpp"

namespace shuati {
namespace tui {

struct TuiTheme {
    ftxui::Color bg_color           = ftxui::Color::Default;
    ftxui::Color accent_color       = ftxui::Color::RGB(230, 140, 60);
    ftxui::Color text_color         = ftxui::Color::Default;
    ftxui::Color dim_color          = ftxui::Color::RGB(100, 100, 100);
    ftxui::Color error_color        = ftxui::Color::RGB(220, 80, 60);
    ftxui::Color input_color        = ftxui::Color::RGB(200, 200, 200);
    ftxui::Color system_color       = ftxui::Color::RGB(100, 180, 140);
    ftxui::Color heading_color      = ftxui::Color::RGB(230, 180, 80);
    ftxui::Color prompt_color       = ftxui::Color::RGB(230, 160, 80);
    ftxui::Color success_color      = ftxui::Color::RGB(100, 200, 120);
    ftxui::Color warn_color         = ftxui::Color::RGB(230, 180, 60);
    ftxui::Color table_header_color = ftxui::Color::RGB(180, 160, 130);
    ftxui::Color selected_bg        = ftxui::Color::RGB(60, 50, 40);
    ftxui::Color border_color       = ftxui::Color::RGB(60, 60, 60);
};

ftxui::Element render_top_bar(const TuiTheme& theme, const std::string& version, const std::string& project_path = "");
ftxui::Element render_bottom_bar(const TuiTheme& theme, bool is_running, const std::string& active_cmd = "");
ftxui::Element render_buffer(const AppState& state, const TuiTheme& theme);
ftxui::Element render_buffer_line(const BufferLine& line, const TuiTheme& theme);
ftxui::Element render_welcome(const TuiTheme& theme);
ftxui::Element render_config_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_list_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_hint_view(const AppState& state, const TuiTheme& theme);

} // namespace tui
} // namespace shuati
