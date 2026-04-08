#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/tui_views.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

// ══════════════════════════════════════════════════════════════════
// ConfigView
// ══════════════════════════════════════════════════════════════════

namespace {

Element render_config_view_impl(const AppState& state, const TuiTheme& theme) {
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

    auto pad = [](const std::string& s, size_t w = 18) {
        std::string r = s;
        while (r.size() < w) r += ' ';
        return r;
    };
    auto toggle = [&](int idx, const std::string& label,
                      const std::string& val, const std::string& opt1,
                      const std::string& opt2,
                      const std::string& hint_text = "") -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? "  > " : "    ";
        auto o1 = text(" " + opt1 + " ");
        auto o2 = text(" " + opt2 + " ");
        if (val == opt1) { o1 = o1 | bold | color(theme.accent_color); o2 = o2 | color(theme.dim_color); }
        else { o1 = o1 | color(theme.dim_color); o2 = o2 | bold | color(theme.accent_color); }
        Elements rp = {text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
                       text(pad(label)) | color(focused ? theme.text_color : theme.dim_color),
                       o1, text(" / ") | color(theme.dim_color), o2};
        if (focused && !hint_text.empty())
            rp.push_back(text("  " + hint_text) | color(theme.dim_color));
        return hbox(std::move(rp));
    };
    auto field = [&](int idx, const std::string& label,
                     const std::string& val, bool masked = false,
                     const std::string& hint_text = "") -> Element {
        bool focused = (cs.focused_field == idx);
        std::string ind = focused ? "  > " : "    ";
        std::string display = val;
        if (masked && display.size() > 4)
            display = display.substr(0, 4) + std::string(display.size() - 4, '*');
        if (focused) display += "_";
        Elements rp = {text(ind) | color(focused ? theme.prompt_color : theme.dim_color),
                       text(pad(label)) | color(focused ? theme.text_color : theme.dim_color),
                       text(display) | color(focused ? theme.input_color : theme.dim_color)};
        if (focused && !hint_text.empty())
            rp.push_back(text("  " + hint_text) | color(theme.dim_color));
        return hbox(std::move(rp));
    };

    rows.push_back(toggle(0, "\xe7\xbc\x96\xe7\xa8\x8b\xe8\xaf\xad\xe8\xa8\x80", cs.language, "cpp", "python", "Enter \xe5\x88\x87\xe6\x8d\xa2"));
    rows.push_back(text(""));
    rows.push_back(field(1, "\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8", cs.editor));
    rows.push_back(text(""));
    rows.push_back(field(2, "API \xe5\xaf\x86\xe9\x92\xa5", cs.api_key, true));
    rows.push_back(text(""));
    rows.push_back(field(3, "AI \xe6\xa8\xa1\xe5\x9e\x8b", cs.model));
    rows.push_back(text(""));
    rows.push_back(toggle(4, "\xe7\x95\x8c\xe9\x9d\xa2\xe6\xa8\xa1\xe5\xbc\x8f", cs.ui_mode, "tui", "legacy"));
    rows.push_back(text(""));
    rows.push_back(toggle(5, "\xe8\x87\xaa\xe5\x8a\xa8\xe5\x90\xaf\xe5\x8a\xa8 REPL", cs.autostart_repl ? "on" : "off", "on", "off"));
    rows.push_back(text(""));
    rows.push_back(field(6, "\xe6\x9c\x80\xe5\xa4\xa7 Token", std::to_string(cs.max_tokens)));
    rows.push_back(text(""));
    rows.push_back(toggle(7, "AI \xe5\x8a\x9f\xe8\x83\xbd", cs.ai_enabled ? "on" : "off", "on", "off"));
    rows.push_back(text(""));
    rows.push_back(toggle(8, "\xe6\xa8\xa1\xe6\x9d\xbf\xe7\x94\x9f\xe6\x88\x90", cs.template_enabled ? "on" : "off", "on", "off"));
    rows.push_back(text(""));
    rows.push_back(field(9, "\xe8\x93\x9d\xe6\xa1\xa5 Cookie", cs.lanqiao_cookie, true));
    rows.push_back(text(""));
    rows.push_back(text(""));

    {
        bool sf = (cs.focused_field == ConfigState::SAVE_INDEX);
        bool cf = (cs.focused_field == ConfigState::CANCEL_INDEX);
        auto save_btn = text("  \xe4\xbf\x9d\xe5\xad\x98  ");
        if (sf) save_btn = save_btn | bold | bgcolor(theme.accent_color) | color(Color::RGB(30, 20, 10));
        else save_btn = save_btn | color(theme.dim_color);
        auto cancel_btn = text("  \xe5\x8f\x96\xe6\xb6\x88  ");
        if (cf) cancel_btn = cancel_btn | bold | bgcolor(theme.error_color) | color(Color::RGB(255, 240, 240));
        else cancel_btn = cancel_btn | color(theme.dim_color);
        rows.push_back(hbox({text("    "), save_btn, text("    "), cancel_btn}));
    }
    if (!cs.status_msg.empty()) {
        rows.push_back(text(""));
        rows.push_back(text("    " + cs.status_msg) | color(theme.system_color));
    }
    return vbox(std::move(rows));
}

}  // namespace

Component CreateConfigView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        return render_config_view_impl(state, ctx.theme);
    });
    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::ConfigView) return false;
        if (event == Event::Escape) { state.pop_view(); return true; }
        auto& cs = state.config_state;
        if (event == Event::ArrowUp) { cs.focused_field = std::max(0, cs.focused_field - 1); return true; }
        if (event == Event::ArrowDown) { cs.focused_field = std::min(ConfigState::TOTAL_ITEMS - 1, cs.focused_field + 1); return true; }
        if (event == Event::Return) {
            if (cs.focused_field == ConfigState::SAVE_INDEX) { save_config_state(state); return true; }
            if (cs.focused_field == ConfigState::CANCEL_INDEX) { state.pop_view(); return true; }
            if (cs.focused_field == 0) { cs.language = (cs.language == "cpp") ? "python" : "cpp"; return true; }
            if (cs.focused_field == 4) { cs.ui_mode = (cs.ui_mode == "tui") ? "legacy" : "tui"; return true; }
            if (cs.focused_field == 5) { cs.autostart_repl = !cs.autostart_repl; return true; }
            if (cs.focused_field == 7) { cs.ai_enabled = !cs.ai_enabled; return true; }
            if (cs.focused_field == 8) { cs.template_enabled = !cs.template_enabled; return true; }
        }
        return false;
    });
}

// ══════════════════════════════════════════════════════════════════
// StatusView
// ══════════════════════════════════════════════════════════════════

Component CreateStatusView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& ss = state.status_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xe5\xbd\x93\xe5\x89\x8d\xe7\x8a\xb6\xe6\x80\x81") | bold | color(theme.heading_color),
            filler(),
            text("Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        if (!ss.loaded) {
            rows.push_back(text("    \xe6\xad\xa3\xe5\x9c\xa8\xe5\x8a\xa0\xe8\xbd\xbd...") | color(theme.dim_color));
            return vbox(std::move(rows));
        }
        rows.push_back(hbox({text("    \xe6\x80\xbb\xe9\xa2\x98\xe6\x95\xb0: "), text(std::to_string(ss.total_problems)) | bold | color(theme.accent_color)}));
        rows.push_back(hbox({text("    \xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87: "), text(std::to_string(ss.ac_problems)) | bold | color(theme.success_color)}));
        rows.push_back(hbox({text("    \xe5\xbe\x85\xe5\xa4\x8d\xe4\xb9\xa0: "), text(std::to_string(ss.pending_reviews)) | bold | color(theme.warn_color)}));
        rows.push_back(text(""));
        rows.push_back(hbox({text("    \xe6\x9c\x80\xe8\xbf\x91\xe6\xb4\xbb\xe5\x8a\xa8: "), text(ss.last_activity) | color(theme.dim_color)}));
        return vbox(std::move(rows)) | flex;
    });
    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::StatusView) return false;
        if (event == Event::Escape) { state.pop_view(); return true; }
        return false;
    });
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
