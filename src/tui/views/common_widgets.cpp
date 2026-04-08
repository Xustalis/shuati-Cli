#include "common_widgets.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

// ── Line classification ───────────────────────────────────────────

LineType classify_output_line(const std::string& text) {
    if (text.empty()) return LineType::Output;
    if (text.starts_with("[Error]") || text.starts_with("[!]") ||
        text.starts_with("Error:"))
        return LineType::Error;
    if (text.starts_with("[+]") || text.starts_with("[*]") ||
        text.starts_with("[Hint]") || text.starts_with("[TUI]") ||
        text.starts_with("[Coach]"))
        return LineType::SystemLog;
    return LineType::Output;
}

// ── Text → lines helpers ──────────────────────────────────────────

void append_text_as_lines(const std::string& raw,
                          std::vector<BufferLine>& buffer) {
    std::string text = raw;
    for (auto& c : text) if (c == '\r') c = '\n';
    std::string::size_type start = 0;
    while (start < text.size()) {
        auto nl = text.find('\n', start);
        std::string seg = (nl == std::string::npos)
                              ? text.substr(start)
                              : text.substr(start, nl - start);
        if (!seg.empty() || nl != std::string::npos)
            buffer.push_back({classify_output_line(seg), seg});
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

void append_text_as_strings(const std::string& raw,
                            std::vector<std::string>& lines) {
    std::string text = raw;
    for (auto& c : text) if (c == '\r') c = '\n';
    std::string::size_type start = 0;
    while (start < text.size()) {
        auto nl = text.find('\n', start);
        if (nl == std::string::npos) {
            if (lines.empty()) lines.push_back("");
            lines.back() += text.substr(start);
            break;
        }
        if (lines.empty()) lines.push_back("");
        lines.back() += text.substr(start, nl - start);
        lines.push_back("");
        start = nl + 1;
    }
}

// ── Render functions ──────────────────────────────────────────────

} // namespace views

using namespace ftxui;

Element render_buffer_line(const BufferLine& line, const TuiTheme& theme) {
    switch (line.type) {
    case LineType::Input:
        return hbox({
            text("  > ") | bold | color(theme.prompt_color),
            text(line.text) | color(theme.input_color),
        });
    case LineType::Output:
        return text("    " + line.text) | color(theme.text_color);
    case LineType::SystemLog:
        return hbox({text("  "),
                     text(line.text) | bold | color(theme.system_color)});
    case LineType::Error:
        return hbox({text("  ! ") | bold | color(theme.error_color),
                     text(line.text) | color(theme.error_color)});
    case LineType::Heading:
        return hbox({text("  "),
                     text(line.text) | bold | color(theme.heading_color)});
    case LineType::Separator:
        return text("    " + line.text) | color(theme.dim_color);
    }
    return text(line.text);
}

Element render_buffer(const AppState& state, const TuiTheme& theme) {
    Elements lines;
    for (const auto& line : state.buffer)
        lines.push_back(render_buffer_line(line, theme));
    if (lines.empty()) return render_welcome(theme);
    return vbox(std::move(lines));
}

Element render_welcome(const TuiTheme& theme) {
    auto accent = theme.accent_color;
    auto dim = theme.dim_color;

    auto left_col = vbox({
        text("     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88     ") | bold | color(accent),
        text("   \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88   ") | bold | color(accent),
        text("   \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88   ") | bold | color(accent),
        text("   \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88   ") | bold | color(accent),
        text("     \xe2\x96\x88\xe2\x96\x88      \xe2\x96\x88\xe2\x96\x88     ") | bold | color(accent),
        text(""),
        text("  SHUATI") | bold | color(accent),
        text(""),
        text("  \xe6\x99\xba\xe8\x83\xbd\xe5\x88\xb7\xe9\xa2\x98\xe5\x8a\xa9\xe6\x89\x8b  \xc2\xb7  for OIer & Coder") | color(dim),
    }) | flex;

    auto right_col = vbox({
        text("\xe2\x9a\xa1 \xe5\xbf\xab\xe9\x80\x9f\xe5\xbc\x80\xe5\xa7\x8b") | bold | color(theme.heading_color),
        separatorLight() | color(dim),
        hbox({ text(" /pull <url> ") | bold | color(accent), text("   \xe4\xbb\x8e URL \xe6\x8b\x89\xe5\x8f\x96\xe9\xa2\x98\xe7\x9b\xae ") | color(dim) }),
        hbox({ text(" /list       ") | bold | color(accent), text("   \xe6\xb5\x8f\xe8\xa7\x88\xe6\x9c\xac\xe5\x9c\xb0\xe9\xa2\x98\xe5\xba\x93 ") | color(dim) }),
        hbox({ text(" /solve <id> ") | bold | color(accent), text("   \xe5\xbc\x80\xe5\xa7\x8b\xe5\x81\x9a\xe9\xa2\x98 ") | color(dim) }),
        text(""),
        text("\xe2\x9a\x99 \xe5\xb8\xb8\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4") | bold | color(theme.heading_color),
        separatorLight() | color(dim),
        hbox({ text(" /hint <id>  ") | bold | color(accent), text("   AI \xe6\x99\xba\xe8\x83\xbd\xe5\x88\x86\xe6\x9e\x90 ") | color(dim) }),
        hbox({ text(" /test <id>  ") | bold | color(accent), text("   \xe7\xbc\x96\xe8\xaf\x91\xe5\xb9\xb6\xe6\xb5\x8b\xe8\xaf\x95 ") | color(dim) }),
        hbox({ text(" /config     ") | bold | color(accent), text("   \xe9\x85\x8d\xe7\xbd\xae\xe8\xae\xbe\xe7\xbd\xae ") | color(dim) }),
        hbox({ text(" /help       ") | bold | color(accent), text("   \xe5\x85\xa8\xe9\x83\xa8\xe5\x91\xbd\xe4\xbb\xa4 ") | color(dim) }),
        text(""),
        text("   \xe8\xbe\x93\xe5\x85\xa5 / \xe5\xbc\x80\xe5\xa4\xb4\xe7\x9a\x84\xe5\x91\xbd\xe4\xbb\xa4, Tab \xe8\xa1\xa5\xe5\x85\xa8, \xe2\x86\x91\xe2\x86\x93 \xe5\x8e\x86\xe5\x8f\xb2\xe8\xae\xb0\xe5\xbd\x95") | color(dim),
    }) | flex;

    return hbox({left_col, separatorEmpty(), right_col});
}

Element render_top_bar(const TuiTheme& theme,
                       const std::string& version,
                       const std::string& project_path) {
    Elements parts;
    parts.push_back(text(" \xe2\x97\x8f") | color(theme.accent_color));
    parts.push_back(text(" shuati") | bold | color(theme.text_color));
    parts.push_back(text(" " + version) | color(theme.dim_color));
    if (!project_path.empty()) {
        parts.push_back(text("  ") | color(theme.dim_color));
        parts.push_back(text(project_path) | color(theme.dim_color));
    }
    parts.push_back(filler());
    return hbox(std::move(parts));
}

Element render_bottom_bar(const TuiTheme& theme,
                          bool is_running,
                          const std::string& active_cmd) {
    Elements parts;
    parts.push_back(text(" Ctrl+C") | bold | color(theme.dim_color));
    parts.push_back(text(" \xe9\x80\x80\xe5\x87\xba") | color(theme.dim_color));
    parts.push_back(text("  /help") | bold | color(theme.dim_color));
    parts.push_back(text(" \xe5\xb8\xae\xe5\x8a\xa9") | color(theme.dim_color));
    parts.push_back(text("  Ctrl+L") | bold | color(theme.dim_color));
    parts.push_back(text(" \xe6\xb8\x85\xe5\xb1\x8f") | color(theme.dim_color));
    parts.push_back(filler());
    if (is_running) {
        std::string label = "\xe2\x8f\xb3 ";
        if (!active_cmd.empty()) label += "/" + active_cmd;
        label += " ";
        parts.push_back(text(label) | bold | color(theme.accent_color));
    } else {
        parts.push_back(
            text("\xe2\x97\x8f \xe5\xb0\xb1\xe7\xbb\xaa ") | color(theme.success_color));
    }
    return hbox(std::move(parts));
}

Element render_sidebar(const AppState& state, const TuiTheme& theme) {
    Elements rows;
    rows.push_back(text("  shuati") | bold | color(theme.heading_color));
    rows.push_back(text(""));
    for (int i = 0;
         i < static_cast<int>(state.sidebar_state.categories.size()); i++) {
        bool selected = (state.sidebar_state.selected == i);
        bool focused  = (state.active_pane == 0);
        auto btn = text(state.sidebar_state.categories[i]);
        if (selected) {
            btn = btn | bold;
            if (focused) btn = btn | inverted;
            else btn = btn | bgcolor(theme.selected_bg) | color(theme.accent_color);
        } else {
            btn = btn | color(theme.dim_color);
        }
        rows.push_back(btn);
        rows.push_back(text(""));
    }
    return vbox(std::move(rows)) | size(WIDTH, LESS_THAN, 20);
}

}  // namespace tui
}  // namespace shuati
