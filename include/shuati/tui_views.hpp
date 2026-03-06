#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include "../../src/tui/store/app_state.hpp"

namespace shuati {
namespace tui {
struct TuiTheme {
    ftxui::Color bg_color = ftxui::Color::Default;
    ftxui::Color accent_color = ftxui::Color::RGB(230, 110, 30); // Orange
    ftxui::Color text_color = ftxui::Color::Default;
    ftxui::Color dim_color = ftxui::Color::RGB(120, 120, 120);
    ftxui::Color error_color = ftxui::Color::RGB(220, 50, 50);
};

// Renders the overall shuatiCLI style terminal layout.
ftxui::Element render_chat_layout(
    const tui::AppState& state,
    const TuiTheme& theme,
    ftxui::Element input_element,
    ftxui::Element history_element,
    ftxui::Element autocomplete_element,
    ftxui::Element selection_element,
    int width,
    int height
);

// Render a single message in the history
ftxui::Element render_chat_message(const tui::ChatMessage& msg, const TuiTheme& theme);

// Draw the SHUATI ASCII Logo
ftxui::Element render_logo(const TuiTheme& theme);

}
}
