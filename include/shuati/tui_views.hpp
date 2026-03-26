#pragma once

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include "../../src/tui/store/app_state.hpp"

namespace shuati {
namespace tui {

struct TuiTheme {
    ftxui::Color bg_color           = ftxui::Color::Default;
    ftxui::Color accent_color       = ftxui::Color::CyanLight;
    ftxui::Color text_color         = ftxui::Color::Default;
    ftxui::Color dim_color          = ftxui::Color::GrayDark;
    ftxui::Color error_color        = ftxui::Color::RedLight;
    ftxui::Color input_color        = ftxui::Color::Default;
    ftxui::Color system_color       = ftxui::Color::GreenLight;
    ftxui::Color heading_color      = ftxui::Color::CyanLight;
    ftxui::Color prompt_color       = ftxui::Color::YellowLight;
    ftxui::Color success_color      = ftxui::Color::GreenLight;
    ftxui::Color warn_color         = ftxui::Color::YellowLight;
    ftxui::Color table_header_color = ftxui::Color::CyanLight;
    ftxui::Color selected_bg        = ftxui::Color::GrayDark;
    ftxui::Color border_color       = ftxui::Color::GrayDark;
};

ftxui::Element render_top_bar(const TuiTheme& theme, const std::string& version, const std::string& project_path = "");
ftxui::Element render_bottom_bar(const TuiTheme& theme, bool is_running, const std::string& active_cmd = "");
ftxui::Element render_sidebar(const AppState& state, const TuiTheme& theme);
ftxui::Element render_buffer(const AppState& state, const TuiTheme& theme);
ftxui::Element render_buffer_line(const BufferLine& line, const TuiTheme& theme);
ftxui::Element render_welcome(const TuiTheme& theme);
ftxui::Element render_config_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_list_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_hint_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_pull_view(const AppState& state, const TuiTheme& theme, ftxui::Component input_comp);
ftxui::Element render_solve_view(const AppState& state, const TuiTheme& theme, ftxui::Component search_comp);
ftxui::Element render_status_view(const AppState& state, const TuiTheme& theme);
ftxui::Element render_menu_view(const AppState& state, const TuiTheme& theme);

} // namespace tui
} // namespace shuati
