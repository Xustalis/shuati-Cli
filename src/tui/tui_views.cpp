#include "shuati/tui_views.hpp"
#include <string>

namespace shuati {
namespace tui {

using namespace ftxui;

Element render_buffer_line(const BufferLine& line, const TuiTheme& theme) {
    switch (line.type) {
    case LineType::Input:
        return hbox({
            text("> ") | bold | color(theme.prompt_color),
            text(line.text) | color(theme.input_color)
        });
    case LineType::Output:
        return text("  " + line.text) | color(theme.text_color);
    case LineType::System:
        return text("  " + line.text) | color(theme.system_color);
    case LineType::Error:
        return text("  " + line.text) | color(theme.error_color);
    case LineType::Heading:
        return text("  " + line.text) | bold | color(theme.heading_color);
    case LineType::Separator:
        return text("  " + line.text) | color(theme.dim_color);
    }
    return text(line.text);
}

Element render_buffer(const AppState& state, const TuiTheme& theme) {
    Elements lines;
    lines.reserve(state.buffer.size());
    for (const auto& bl : state.buffer) {
        lines.push_back(render_buffer_line(bl, theme));
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
    lines.push_back(text(R"(       __            __  _ )") | color(theme.accent_color) | bold);
    lines.push_back(text(R"(  ___ / /  __ _____ _/ /_(_))") | color(theme.accent_color) | bold);
    lines.push_back(text(R"( (_-</ _ \/ // / _ `/ __/ / )") | color(theme.accent_color) | bold);
    lines.push_back(text(R"(/___/_//_/\_,_/\_,_/\__/_/  )") | color(theme.accent_color) | bold);
    lines.push_back(text(""));
    lines.push_back(text("  Practice makes perfect.") | color(theme.dim_color));
    lines.push_back(text(""));
    lines.push_back(separatorLight() | color(theme.dim_color));
    lines.push_back(text(""));
    lines.push_back(hbox({
        text("  /help  ") | bold | color(theme.prompt_color),
        text("  Show all commands") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("  /pull  ") | bold | color(theme.prompt_color),
        text("  Pull a problem from URL") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("  /list  ") | bold | color(theme.prompt_color),
        text("  Browse your problem library") | color(theme.dim_color),
    }));
    lines.push_back(hbox({
        text("  /config") | bold | color(theme.prompt_color),
        text("  Edit configuration") | color(theme.dim_color),
    }));
    lines.push_back(text(""));

    return vbox(std::move(lines));
}

Element render_top_bar(const TuiTheme& theme, const std::string& version) {
    return hbox({
        text(" shuati") | bold | color(theme.accent_color),
        text(" " + version) | color(theme.dim_color),
        filler(),
        text("UTF-8 ") | color(theme.dim_color)
    });
}

Element render_bottom_bar(const TuiTheme& theme, bool is_running) {
    return hbox({
        text(" Ctrl+C") | bold | color(theme.dim_color),
        text(" exit") | color(theme.dim_color),
        text("  /help") | bold | color(theme.dim_color),
        text(" commands") | color(theme.dim_color),
        filler(),
        is_running
            ? text("running ") | bold | color(theme.accent_color)
            : text("idle ") | color(theme.dim_color)
    });
}

Element render_config_view(const AppState& state, const TuiTheme& theme) {
    const auto& cs = state.config_state;
    Elements rows;

    rows.push_back(hbox({
        text("  Config") | bold | color(theme.accent_color),
        filler(),
        text("Esc") | bold | color(theme.dim_color),
        text(" back  ") | color(theme.dim_color),
    }));
    rows.push_back(separatorLight() | color(theme.dim_color));
    rows.push_back(text(""));

    auto pad_label = [](const std::string& s, size_t w = 18) {
        std::string r = s;
        while (r.size() < w) r += ' ';
        return r;
    };

    auto render_toggle = [&](int idx, const std::string& label,
                              const std::string& val,
                              const std::string& opt1,
                              const std::string& opt2) -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? " > " : "   ";

        auto o1 = text(" " + opt1 + " ");
        auto o2 = text(" " + opt2 + " ");
        if (val == opt1) {
            o1 = o1 | bold | color(theme.accent_color);
            o2 = o2 | color(theme.dim_color);
        } else {
            o1 = o1 | color(theme.dim_color);
            o2 = o2 | bold | color(theme.accent_color);
        }

        return hbox({
            text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
            text(pad_label(label)) | color(focused ? theme.text_color : theme.dim_color),
            o1, text("/") | color(theme.dim_color), o2,
        });
    };

    auto render_text_field = [&](int idx, const std::string& label,
                                  const std::string& val,
                                  bool masked = false) -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? " > " : "   ";

        std::string display = val;
        if (masked && display.size() > 4) {
            display = display.substr(0, 4) + std::string(display.size() - 4, '*');
        }
        if (focused) display += "_";

        return hbox({
            text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
            text(pad_label(label)) | color(focused ? theme.text_color : theme.dim_color),
            text("[") | color(theme.dim_color),
            text(display) | color(focused ? theme.input_color : theme.dim_color),
            text("]") | color(theme.dim_color),
        });
    };

    rows.push_back(render_toggle(0, "Language", cs.language, "cpp", "python"));
    rows.push_back(render_text_field(1, "Editor", cs.editor));
    rows.push_back(render_text_field(2, "API Key", cs.api_key, true));
    rows.push_back(render_text_field(3, "Model", cs.model));
    rows.push_back(render_toggle(4, "UI Mode", cs.ui_mode, "tui", "legacy"));
    rows.push_back(render_toggle(5, "Autostart REPL",
                                  cs.autostart_repl ? "on" : "off", "on", "off"));

    rows.push_back(text(""));
    rows.push_back(separatorLight() | color(theme.dim_color));

    {
        bool sf = (cs.focused_field == ConfigState::SAVE_INDEX);
        bool cf = (cs.focused_field == ConfigState::CANCEL_INDEX);

        auto save_btn = text("  Save  ");
        if (sf) save_btn = save_btn | bold | bgcolor(theme.accent_color)
                                            | color(Color::RGB(240, 240, 255));
        else    save_btn = save_btn | color(theme.dim_color);

        auto cancel_btn = text("  Cancel  ");
        if (cf) cancel_btn = cancel_btn | bold | bgcolor(theme.error_color)
                                                | color(Color::RGB(255, 240, 240));
        else    cancel_btn = cancel_btn | color(theme.dim_color);

        rows.push_back(hbox({
            filler(),
            save_btn,
            text("    "),
            cancel_btn,
            text("  "),
        }));
    }

    if (!cs.status_msg.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("  " + cs.status_msg) | color(theme.system_color));
    }

    return vbox(std::move(rows));
}

Element render_list_view(const AppState& state, const TuiTheme& theme) {
    const auto& ls = state.list_state;
    Elements rows;

    rows.push_back(hbox({
        text("  Problem List") | bold | color(theme.accent_color),
        filler(),
        text("Filter: ") | color(theme.dim_color),
        text(ls.filter) | bold | color(theme.accent_color),
        text("   "),
        text("Esc") | bold | color(theme.dim_color),
        text(" back  ") | color(theme.dim_color),
    }));
    rows.push_back(separatorLight() | color(theme.dim_color));

    if (!ls.error.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("  " + ls.error) | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    if (ls.rows.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("  No problems found.") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    rows.push_back(hbox({
        text("   ") | size(WIDTH, EQUAL, 3),
        text("TID") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 6),
        text("Title") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 28),
        text("Difficulty") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 12),
        text("Status") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 12),
        text("Date") | bold | color(theme.table_header_color),
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
            text(sel ? " > " : "   ") | color(sel ? theme.prompt_color : theme.dim_color)
                                       | size(WIDTH, EQUAL, 3),
            text(std::to_string(r.tid)) | color(rc) | size(WIDTH, EQUAL, 6),
            text(r.title)               | color(rc) | size(WIDTH, EQUAL, 28),
            text(r.difficulty)          | color(diff_c) | size(WIDTH, EQUAL, 12),
            text(r.status)              | color(stat_c) | size(WIDTH, EQUAL, 12),
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

    rows.push_back(separatorLight() | color(theme.dim_color));

    rows.push_back(hbox({
        text("  Enter") | bold | color(theme.dim_color),
        text(" solve  ") | color(theme.dim_color),
        text("t") | bold | color(theme.dim_color),
        text(" test  ") | color(theme.dim_color),
        text("v") | bold | color(theme.dim_color),
        text(" view  ") | color(theme.dim_color),
        text("d") | bold | color(theme.dim_color),
        text(" delete  ") | color(theme.dim_color),
        text("f") | bold | color(theme.dim_color),
        text(" filter") | color(theme.dim_color),
    }));

    return vbox(std::move(rows));
}

Element render_hint_view(const AppState& state, const TuiTheme& theme) {
    const auto& hs = state.hint_state;
    Elements rows;

    rows.push_back(hbox({
        text("  AI Hint") | bold | color(theme.accent_color),
        filler(),
        text("PgUp/PgDn") | bold | color(theme.dim_color),
        text(" scroll   ") | color(theme.dim_color),
        text("Esc") | bold | color(theme.dim_color),
        text(" back  ") | color(theme.dim_color),
    }));
    rows.push_back(separatorLight() | color(theme.dim_color));

    if (hs.loading) {
        rows.push_back(text(""));
        rows.push_back(text("  [loading] Waiting for AI response...") | color(theme.accent_color));
        rows.push_back(text(""));
        return vbox(std::move(rows));
    }

    if (!hs.error.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("  " + hs.error) | color(theme.error_color));
        return vbox(std::move(rows));
    }

    if (hs.lines.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("  No hint content.") | color(theme.dim_color));
        return vbox(std::move(rows));
    }

    Elements body_lines;
    for (const auto& ln : hs.lines) {
        body_lines.push_back(text("  " + ln) | color(theme.text_color));
    }

    auto body = vbox(std::move(body_lines))
                | focusPositionRelative(0, 0) | vscroll_indicator | frame | flex;
    rows.push_back(body);

    return vbox(std::move(rows));
}

} // namespace tui
} // namespace shuati
