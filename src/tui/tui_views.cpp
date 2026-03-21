#include "shuati/tui_views.hpp"
#include <string>
#include <algorithm>

namespace shuati {
namespace tui {

using namespace ftxui;

namespace {

std::string filter_label(const std::string& filter) {
    if (filter == "ac") return "AC";
    if (filter == "failed") return "Failed";
    if (filter == "unaudited") return "Unaudited";
    if (filter == "review") return "Review";
    return "All Status";
}

std::string diff_filter_label(const std::string& diff) {
    if (diff == "easy") return "Easy";
    if (diff == "medium") return "Medium";
    if (diff == "hard") return "Hard";
    return "All Diffs";
}

} // namespace

Element render_buffer_line(const BufferLine& line, const TuiTheme& theme) {
    switch (line.type) {
    case LineType::Input:
        return hbox({
            text("  > ") | bold | color(theme.prompt_color),
            text(line.text) | color(theme.input_color),
        });
    case LineType::Output:
        return text("    " + line.text) | color(theme.text_color);
    case LineType::System:
        return hbox({
            text("    ") | color(theme.dim_color),
            text(line.text) | color(theme.system_color),
        });
    case LineType::Error:
        return hbox({
            text("  ! ") | bold | color(theme.error_color),
            text(line.text) | color(theme.error_color),
        });
    case LineType::Heading:
        return hbox({
            text("  "),
            text(line.text) | bold | color(theme.heading_color),
        });
    case LineType::Separator:
        return text("    " + line.text) | color(theme.dim_color);
    }
    return text(line.text);
}

Element render_buffer(const AppState& state, const TuiTheme& theme) {
    // Buffer is rendered as a "window" driven by AppState.scroll_offset.
    // This makes PageUp/PageDown functional (otherwise scroll_offset had no effect).
    constexpr int VISIBLE_BUFFER_LINES = 20;

    Elements lines;
    const int start = std::max(0, state.scroll_offset);
    const int end_exclusive = std::min(
        static_cast<int>(state.buffer.size()),
        start + VISIBLE_BUFFER_LINES
    );
    if (start >= end_exclusive) {
        // Avoid rendering an empty buffer area with non-empty buffer.
        // Fallback to showing from 0.
        lines.reserve(std::min(static_cast<int>(state.buffer.size()), VISIBLE_BUFFER_LINES));
        const int fallback_end = std::min(static_cast<int>(state.buffer.size()), VISIBLE_BUFFER_LINES);
        for (int i = 0; i < fallback_end; i++) {
            lines.push_back(render_buffer_line(state.buffer[i], theme));
        }
    } else {
        lines.reserve(static_cast<size_t>(end_exclusive - start));
        for (int i = start; i < end_exclusive; i++) {
            lines.push_back(render_buffer_line(state.buffer[static_cast<size_t>(i)], theme));
        }
    }

    if (lines.empty()) {
        return render_welcome(theme);
    }
    return vbox(std::move(lines));
}

Element render_welcome(const TuiTheme& theme) {
    Elements lines;

    lines.push_back(text(""));
    lines.push_back(text(""));

    auto panda_white = ftxui::Color::RGB(220, 220, 220);

    // Panda mascot using box-drawing: ears + face with eye patches
    lines.push_back(hbox({
        text("        ") | color(theme.dim_color),
        text("\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97") | bold | color(theme.text_color),
        text("    ") | color(panda_white),
        text("\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97") | bold | color(theme.text_color),
    }));
    lines.push_back(hbox({
        text("        ") | color(theme.dim_color),
        text("\xe2\x95\x9a\xe2\x95\x97") | bold | color(theme.text_color),
        text(" \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d ") | color(panda_white),
        text("\xe2\x95\x94\xe2\x95\x9d") | bold | color(theme.text_color),
    }));
    lines.push_back(hbox({
        text("         ") | color(theme.dim_color),
        text("\xe2\x95\x91") | color(panda_white),
        text(" \xe2\x96\x88\xc2\xb7") | bold | color(theme.text_color),
        text("    ") | color(panda_white),
        text("\xc2\xb7\xe2\x96\x88 ") | bold | color(theme.text_color),
        text("\xe2\x95\x91") | color(panda_white),
    }));
    lines.push_back(hbox({
        text("         ") | color(theme.dim_color),
        text("\xe2\x95\x91") | color(panda_white),
        text("    \xe2\x96\xbd    ") | color(theme.text_color),
        text("\xe2\x95\x91") | color(panda_white),
    }));
    lines.push_back(hbox({
        text("         ") | color(theme.dim_color),
        text("\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d") | color(panda_white),
    }));

    lines.push_back(text(""));

    // Compact shuati logo using box-drawing characters
    lines.push_back(text("       \xe2\x95\x94\xe2\x95\x90\xe2\x95\x97 \xe2\x95\xa6 \xe2\x95\xa6 \xe2\x95\xa6 \xe2\x95\xa6 \xe2\x95\x94\xe2\x95\x90\xe2\x95\x97 \xe2\x95\x94\xe2\x95\xa6\xe2\x95\x97 \xe2\x95\xa6") | bold | color(theme.accent_color));
    lines.push_back(text("       \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x97 \xe2\x95\xa0\xe2\x95\x90\xe2\x95\xa3 \xe2\x95\x91 \xe2\x95\x91 \xe2\x95\xa0\xe2\x95\x90\xe2\x95\xa3  \xe2\x95\x91  \xe2\x95\x91") | bold | color(theme.accent_color));
    lines.push_back(text("       \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\xa9 \xe2\x95\xa9 \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\xa9 \xe2\x95\xa9  \xe2\x95\xa9  \xe2\x95\xa9") | bold | color(theme.accent_color));

    lines.push_back(text(""));
    lines.push_back(text("       \xe6\x99\xba\xe8\x83\xbd\xe5\x88\xb7\xe9\xa2\x98\xe5\x8a\xa9\xe6\x89\x8b \xc2\xb7 for OIer & Coder") | color(theme.dim_color));

    lines.push_back(text(""));

    // Quick start section
    lines.push_back(text("   \xe5\xbf\xab\xe9\x80\x9f\xe5\xbc\x80\xe5\xa7\x8b") | bold | color(theme.heading_color));
    lines.push_back(text(""));
    lines.push_back(hbox({
        text("     /pull <url>") | bold | color(theme.prompt_color),
        text("       \xe4\xbb\x8e URL \xe6\x8b\x89\xe5\x8f\x96\xe9\xa2\x98\xe7\x9b\xae\xe5\x88\xb0\xe6\x9c\xac\xe5\x9c\xb0\xe9\xa2\x98\xe5\xba\x93") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("     /list") | bold | color(theme.prompt_color),
        text("             \xe6\xb5\x8f\xe8\xa7\x88\xe6\x9c\xac\xe5\x9c\xb0\xe9\xa2\x98\xe5\xba\x93\xef\xbc\x8c\xe6\x94\xaf\xe6\x8c\x81 --filter \xe5\x92\x8c\xe5\xbf\xab\xe6\x8d\xb7\xe6\x93\x8d\xe4\xbd\x9c") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("     /solve <id>") | bold | color(theme.prompt_color),
        text("       \xe8\xbf\x9b\xe5\x85\xa5\xe5\x81\x9a\xe9\xa2\x98\xe5\xb7\xa5\xe4\xbd\x9c\xe6\xb5\x81\xef\xbc\x8c\xe8\x87\xaa\xe5\x8a\xa8\xe6\x89\x93\xe5\xbc\x80\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8") | color(theme.dim_color),
    }));

    lines.push_back(text(""));
    lines.push_back(text("   \xe6\x9b\xb4\xe5\xa4\x9a\xe5\x91\xbd\xe4\xbb\xa4") | bold | color(theme.heading_color));
    lines.push_back(text(""));
    lines.push_back(hbox({
        text("     /hint <id>") | bold | color(theme.prompt_color),
        text("        AI \xe6\x99\xba\xe8\x83\xbd\xe5\x88\x86\xe6\x9e\x90\xe9\xa2\x98\xe7\x9b\xae\xef\xbc\x8c\xe7\xbb\x99\xe5\x87\xba\xe8\xa7\xa3\xe9\xa2\x98\xe6\x80\x9d\xe8\xb7\xaf\xe6\x8f\x90\xe7\xa4\xba") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("     /test <id>") | bold | color(theme.prompt_color),
        text("        \xe7\xbc\x96\xe8\xaf\x91\xe5\xb9\xb6\xe8\xbf\x90\xe8\xa1\x8c\xe6\xb5\x8b\xe8\xaf\x95\xe7\x94\xa8\xe4\xbe\x8b\xef\xbc\x8c\xe5\xaf\xb9\xe6\xaf\x94\xe9\xa2\x84\xe6\x9c\x9f\xe8\xbe\x93\xe5\x87\xba") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("     /config") | bold | color(theme.prompt_color),
        text("           \xe7\xbc\x96\xe8\xbe\x91\xe8\xaf\xad\xe8\xa8\x80\xe3\x80\x81\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe3\x80\x81" "API Key \xe7\xad\x89\xe9\x85\x8d\xe7\xbd\xae") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("     /help") | bold | color(theme.prompt_color),
        text("             \xe6\x9f\xa5\xe7\x9c\x8b\xe5\x85\xa8\xe9\x83\xa8\xe5\x8f\xaf\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4\xe5\x8f\x8a\xe8\xaf\xa6\xe7\xbb\x86\xe7\x94\xa8\xe6\xb3\x95") | color(theme.dim_color),
    }));

    lines.push_back(text(""));
    lines.push_back(text("   \xe8\xbe\x93\xe5\x85\xa5 / \xe5\xbc\x80\xe5\xa4\xb4\xe7\x9a\x84\xe5\x91\xbd\xe4\xbb\xa4, Tab \xe8\xa1\xa5\xe5\x85\xa8, \xe2\x86\x91\xe2\x86\x93 \xe5\x8e\x86\xe5\x8f\xb2\xe8\xae\xb0\xe5\xbd\x95") | color(theme.dim_color));
    lines.push_back(text(""));

    return vbox(std::move(lines));
}

Element render_top_bar(const TuiTheme& theme, const std::string& version, const std::string& project_path) {
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

Element render_bottom_bar(const TuiTheme& theme, bool is_running, const std::string& active_cmd) {
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
        parts.push_back(text("\xe2\x97\x8f \xe5\xb0\xb1\xe7\xbb\xaa ") | color(theme.success_color));
    }
    return hbox(std::move(parts));
}

Element render_config_view(const AppState& state, const TuiTheme& theme) {
    const auto& cs = state.config_state;
    Elements rows;

    rows.push_back(text(""));
    rows.push_back(hbox({
        text("   \xe2\x9a\x99 \xe9\x85\x8d\xe7\xbd\xae") | bold | color(theme.heading_color),
        filler(),
        text("\xe2\x86\x91\xe2\x86\x93 \xe5\x88\x87\xe6\x8d\xa2  Enter \xe7\xa1\xae\xe8\xae\xa4  Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color),
        text("  "),
    }));
    rows.push_back(text(""));

    auto pad_label = [](const std::string& s, size_t w = 18) {
        std::string r = s;
        while (r.size() < w) r += ' ';
        return r;
    };

    auto render_toggle = [&](int idx, const std::string& label,
                              const std::string& val,
                              const std::string& opt1,
                              const std::string& opt2,
                              const std::string& hint_text = "") -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? "  > " : "    ";

        auto o1 = text(" " + opt1 + " ");
        auto o2 = text(" " + opt2 + " ");
        if (val == opt1) {
            o1 = o1 | bold | color(theme.accent_color);
            o2 = o2 | color(theme.dim_color);
        } else {
            o1 = o1 | color(theme.dim_color);
            o2 = o2 | bold | color(theme.accent_color);
        }

        Elements row_parts = {
            text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
            text(pad_label(label)) | color(focused ? theme.text_color : theme.dim_color),
            o1, text(" / ") | color(theme.dim_color), o2,
        };
        if (focused && !hint_text.empty()) {
            row_parts.push_back(text("  " + hint_text) | color(theme.dim_color));
        }
        return hbox(std::move(row_parts));
    };

    auto render_text_field = [&](int idx, const std::string& label,
                                  const std::string& val,
                                  bool masked = false,
                                  const std::string& hint_text = "") -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? "  > " : "    ";

        std::string display = val;
        if (masked && display.size() > 4) {
            display = display.substr(0, 4) + std::string(display.size() - 4, '*');
        }
        if (focused) display += "_";

        Elements row_parts = {
            text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
            text(pad_label(label)) | color(focused ? theme.text_color : theme.dim_color),
            text(display) | color(focused ? theme.input_color : theme.dim_color),
        };
        if (focused && !hint_text.empty()) {
            row_parts.push_back(text("  " + hint_text) | color(theme.dim_color));
        }
        return hbox(std::move(row_parts));
    };

    rows.push_back(render_toggle(0, "\xe7\xbc\x96\xe7\xa8\x8b\xe8\xaf\xad\xe8\xa8\x80", cs.language, "cpp", "python",
                                 "Enter \xe5\x88\x87\xe6\x8d\xa2\xe8\xaf\xad\xe8\xa8\x80"));
    rows.push_back(text(""));
    rows.push_back(render_text_field(1, "\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8", cs.editor, false,
                                     "\xe8\xbe\x93\xe5\x85\xa5\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8\xe5\x91\xbd\xe4\xbb\xa4, \xe5\xa6\x82 code / vim"));
    rows.push_back(text(""));
    rows.push_back(render_text_field(2, "API \xe5\xaf\x86\xe9\x92\xa5", cs.api_key, true,
                                     "\xe8\xbe\x93\xe5\x85\xa5 AI \xe6\x9c\x8d\xe5\x8a\xa1 API Key"));
    rows.push_back(text(""));
    rows.push_back(render_text_field(3, "AI \xe6\xa8\xa1\xe5\x9e\x8b", cs.model, false,
                                     "\xe5\xa6\x82 deepseek-chat / gpt-4o"));
    rows.push_back(text(""));
    rows.push_back(render_toggle(4, "\xe7\x95\x8c\xe9\x9d\xa2\xe6\xa8\xa1\xe5\xbc\x8f", cs.ui_mode, "tui", "legacy",
                                 "tui=\xe5\x85\xa8\xe5\xb1\x8f, legacy=\xe7\xbb\x8f\xe5\x85\xb8\xe5\x91\xbd\xe4\xbb\xa4\xe8\xa1\x8c"));
    rows.push_back(text(""));
    rows.push_back(render_toggle(5, "\xe8\x87\xaa\xe5\x8a\xa8\xe5\x90\xaf\xe5\x8a\xa8 REPL",
                                  cs.autostart_repl ? "on" : "off", "on", "off",
                                  "\xe5\x90\xaf\xe5\x8a\xa8\xe6\x97\xb6\xe8\x87\xaa\xe5\x8a\xa8\xe8\xbf\x9b\xe5\x85\xa5\xe4\xba\xa4\xe4\xba\x92\xe6\xa8\xa1\xe5\xbc\x8f"));

    rows.push_back(text(""));
    rows.push_back(text(""));

    {
        bool sf = (cs.focused_field == ConfigState::SAVE_INDEX);
        bool cf = (cs.focused_field == ConfigState::CANCEL_INDEX);

        auto save_btn = text("  \xe4\xbf\x9d\xe5\xad\x98  ");
        if (sf) save_btn = save_btn | bold | bgcolor(theme.accent_color)
                                            | color(Color::RGB(30, 20, 10));
        else    save_btn = save_btn | color(theme.dim_color);

        auto cancel_btn = text("  \xe5\x8f\x96\xe6\xb6\x88  ");
        if (cf) cancel_btn = cancel_btn | bold | bgcolor(theme.error_color)
                                                | color(Color::RGB(255, 240, 240));
        else    cancel_btn = cancel_btn | color(theme.dim_color);

        std::string btn_hint;
        if (sf) btn_hint = "Enter \xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae\xe5\x88\xb0\xe6\x96\x87\xe4\xbb\xb6";
        if (cf) btn_hint = "Enter \xe6\x94\xbe\xe5\xbc\x83\xe4\xbf\xae\xe6\x94\xb9\xe5\xb9\xb6\xe8\xbf\x94\xe5\x9b\x9e";

        rows.push_back(hbox({
            text("    "),
            save_btn,
            text("    "),
            cancel_btn,
            text("  "),
            text(btn_hint) | color(theme.dim_color),
        }));
    }

    if (!cs.status_msg.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("    " + cs.status_msg) | color(theme.system_color));
    }

    return vbox(std::move(rows));
}

Element render_list_view(const AppState& state, const TuiTheme& theme) {
    const auto& ls = state.list_state;
    Elements rows;

    rows.push_back(text(""));
    rows.push_back(hbox({
        text("   \xe9\xa2\x98\xe5\xba\x93") | bold | color(theme.heading_color),
        filler(),
        text(" f ") | color(theme.prompt_color) | bold,
        text("Status: [" + filter_label(ls.status_filter) + "] ") | color(theme.system_color),
        text(" F ") | color(theme.prompt_color) | bold,
        text("Difficulty: [" + diff_filter_label(ls.difficulty_filter) + "] ") | color(theme.system_color),
        text(" s ") | color(theme.prompt_color) | bold,
        text("Source: [" + ls.source_filter + "] ") | color(theme.system_color),
        text("  Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color),
        text("  "),
    }));
    rows.push_back(text(""));

    if (!ls.error.empty()) {
        rows.push_back(text("    " + ls.error) | color(theme.dim_color));
        rows.push_back(text(""));
        rows.push_back(text("    \xe6\x8f\x90\xe7\xa4\xba: \xe4\xbd\xbf\xe7\x94\xa8 /init \xe5\x88\x9d\xe5\xa7\x8b\xe5\x8c\x96\xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x8c\xe7\x84\xb6\xe5\x90\x8e\xe7\x94\xa8 /pull <url> \xe6\x8b\x89\xe5\x8f\x96\xe9\xa2\x98\xe7\x9b\xae") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    if (ls.rows.empty()) {
        rows.push_back(text("    \xe6\x9a\x82\xe6\x97\xa0\xe9\xa2\x98\xe7\x9b\xae\xe3\x80\x82") | color(theme.dim_color));
        rows.push_back(text(""));
        rows.push_back(text("    \xe6\x8f\x90\xe7\xa4\xba: \xe4\xbd\xbf\xe7\x94\xa8 /pull <url> \xe4\xbb\x8e\xe5\x9c\xa8\xe7\xba\xbf\xe9\xa2\x98\xe5\xba\x93\xe6\x8b\x89\xe5\x8f\x96\xe4\xbd\xa0\xe7\x9a\x84\xe7\xac\xac\xe4\xb8\x80\xe9\x81\x93\xe9\xa2\x98\xe5\x90\xa7\xef\xbc\x81") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    rows.push_back(hbox({
        text("     ") | size(WIDTH, EQUAL, 5),
        text("#") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 5),
        text("\xe9\xa2\x98\xe7\x9b\xae\xe5\x90\x8d\xe7\xa7\xb0") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 25),
        text("\xe9\x9a\xbe\xe5\xba\xa6") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 8),
        text("\xe6\x9d\xa5\xe6\xba\x90") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 10),
        text("P") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 3),
        text("R") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 3),
        text("\xe7\x8a\xb6\xe6\x80\x81") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 10),
        text("\xe6\x97\xa5\xe6\x9c\x9f") | bold | color(theme.table_header_color),
    }));

    Elements table_rows;
    for (int i = 0; i < static_cast<int>(ls.rows.size()); i++) {
        const auto& r = ls.rows[i];
        bool sel = (i == ls.selected);
        auto rc = sel ? theme.text_color : theme.dim_color;

        auto diff_c = theme.dim_color;
        if (r.difficulty == "easy")        diff_c = theme.success_color;
        else if (r.difficulty == "medium") diff_c = theme.warn_color;
        else if (r.difficulty == "hard")   diff_c = theme.error_color;

        auto stat_c = theme.dim_color;
        if (r.status == "AC")         stat_c = theme.success_color;
        else if (r.status != "-")     stat_c = theme.error_color;

        auto row_el = hbox({
            text(sel ? "  >  " : "     ") | color(sel ? theme.prompt_color : theme.dim_color)
                                           | size(WIDTH, EQUAL, 5),
            text(std::to_string(r.tid)) | color(rc) | size(WIDTH, EQUAL, 5),
            text(r.title)               | color(rc) | size(WIDTH, EQUAL, 25),
            text(r.difficulty)          | color(diff_c) | size(WIDTH, EQUAL, 8),
            text(r.source)              | color(theme.dim_color) | size(WIDTH, EQUAL, 10),
            text(r.passed ? "Y" : " ")  | color(r.passed ? theme.success_color : theme.dim_color) | bold | size(WIDTH, EQUAL, 3),
            text(r.review_due ? "!" : " ") | color(r.review_due ? theme.error_color : theme.dim_color) | bold | size(WIDTH, EQUAL, 3),
            text(r.status)              | color(stat_c) | size(WIDTH, EQUAL, 10),
            text(r.date)                | color(rc),
        });

        if (sel) {
            row_el = row_el | bold | bgcolor(theme.selected_bg) | focus;
        }

        table_rows.push_back(row_el);
    }

    auto table_body = vbox(std::move(table_rows))
                      | vscroll_indicator | frame | flex;
    rows.push_back(table_body);

    rows.push_back(text(""));
    rows.push_back(hbox({
        text("   Enter") | bold | color(theme.dim_color),
        text(" \xe5\x81\x9a\xe9\xa2\x98") | color(theme.dim_color),
        text("  t") | bold | color(theme.dim_color),
        text(" \xe6\xb5\x8b\xe8\xaf\x95") | color(theme.dim_color),
        text("  v") | bold | color(theme.dim_color),
        text(" \xe6\x9f\xa5\xe7\x9c\x8b") | color(theme.dim_color),
        text("  h") | bold | color(theme.dim_color),
        text(" \xe6\x8f\x90\xe7\xa4\xba") | color(theme.dim_color),
        text("  r") | bold | color(theme.dim_color),
        text(" \xe8\xae\xb0\xe5\xbd\x95") | color(theme.dim_color),
        text("  d") | bold | color(theme.dim_color),
        text(" \xe5\x88\xa0\xe9\x99\xa4") | color(theme.dim_color),
        text("  f/F/s") | bold | color(theme.dim_color),
        text(" \xe5\x88\x87\xe6\x8d\xa2\xe7\xad\x9b\xe9\x80\x89") | color(theme.dim_color),
        filler(),
        text(std::to_string(ls.selected + 1) + "/" + std::to_string(ls.rows.size()) + " ") | color(theme.dim_color),
    }));

    return vbox(std::move(rows));
}

Element render_hint_view(const AppState& state, const TuiTheme& theme) {
    const auto& hs = state.hint_state;
    Elements rows;

    rows.push_back(text(""));
    rows.push_back(hbox({
        text("   AI \xe8\xa7\xa3\xe9\xa2\x98\xe6\x8f\x90\xe7\xa4\xba") | bold | color(theme.heading_color),
        filler(),
        text("\xe2\x86\x91\xe2\x86\x93/PgUp/PgDn \xe6\xbb\x9a\xe5\x8a\xa8  Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color),
        text("  "),
    }));
    rows.push_back(text(""));

    if (hs.loading) {
        rows.push_back(text("    \xe6\xad\xa3\xe5\x9c\xa8\xe7\xad\x89\xe5\xbe\x85 AI \xe5\x93\x8d\xe5\xba\x94...") | color(theme.accent_color));
        rows.push_back(text(""));
        rows.push_back(text("    \xe6\x8f\x90\xe7\xa4\xba: AI \xe5\x93\x8d\xe5\xba\x94\xe5\x8f\xaf\xe8\x83\xbd\xe9\x9c\x80\xe8\xa6\x81\xe5\x87\xa0\xe7\xa7\x92\xe9\x92\x9f\xef\xbc\x8c\xe8\xaf\xb7\xe8\x80\x90\xe5\xbf\x83\xe7\xad\x89\xe5\xbe\x85\xe3\x80\x82\xe6\x8c\x89 Esc \xe5\x8f\xaf\xe8\xbf\x94\xe5\x9b\x9e\xe3\x80\x82") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    if (!hs.error.empty()) {
        rows.push_back(text("    " + hs.error) | color(theme.error_color));
        rows.push_back(text(""));
        rows.push_back(text("    \xe6\x8f\x90\xe7\xa4\xba: \xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5 API Key \xe6\x98\xaf\xe5\x90\xa6\xe5\xb7\xb2\xe9\x85\x8d\xe7\xbd\xae, \xe4\xbd\xbf\xe7\x94\xa8 /config \xe8\xbf\x9b\xe8\xa1\x8c\xe8\xae\xbe\xe7\xbd\xae\xe3\x80\x82") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    if (hs.lines.empty()) {
        rows.push_back(text("    \xe6\x9a\x82\xe6\x97\xa0\xe6\x8f\x90\xe7\xa4\xba\xe5\x86\x85\xe5\xae\xb9\xe3\x80\x82") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    Elements body_lines;
    for (int i = 0; i < static_cast<int>(hs.lines.size()); i++) {
        auto line_el = text("    " + hs.lines[i]) | color(theme.text_color);
        if (i == hs.scroll_offset) {
            line_el = line_el | focus;
        }
        body_lines.push_back(line_el);
    }

    auto body = vbox(std::move(body_lines))
                | vscroll_indicator | frame | flex;
    rows.push_back(body);

    int total = static_cast<int>(hs.lines.size());
    std::string pos_info = "    " + std::to_string(std::min(hs.scroll_offset + 1, total))
                         + "/" + std::to_string(total) + " \xe8\xa1\x8c";
    rows.push_back(text(""));
    rows.push_back(text(pos_info) | color(theme.dim_color));

    return vbox(std::move(rows));
}

} // namespace tui
} // namespace shuati
