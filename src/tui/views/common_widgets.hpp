#pragma once

#include <string>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include "../store/app_state.hpp"
#include "shuati/tui_views.hpp"  // TuiTheme

namespace shuati {
namespace tui {
namespace views {


// ── Classify output for coloring ──────────────────────────────────

LineType classify_output_line(const std::string& text);

// ── Parse text into lines and append to a buffer ──────────────────

void append_text_as_lines(const std::string& text,
                          std::vector<BufferLine>& buffer);

void append_text_as_strings(const std::string& text,
                            std::vector<std::string>& lines);

}  // namespace views
}  // namespace tui
}  // namespace shuati
