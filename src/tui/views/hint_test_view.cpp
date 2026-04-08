#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "../tui_command_engine.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

// ══════════════════════════════════════════════════════════════════
// HintView — AI streaming with cancellation
// ══════════════════════════════════════════════════════════════════

Component CreateHintView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& hs = state.hint_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   AI \xe8\xa7\xa3\xe9\xa2\x98\xe6\x8f\x90\xe7\xa4\xba") | bold | color(theme.heading_color),
            filler(),
            text("\xe2\x86\x91\xe2\x86\x93/PgUp/PgDn \xe6\xbb\x9a\xe5\x8a\xa8  ") | color(theme.dim_color),
            (hs.loading || hs.streaming)
                ? (text("Esc \xe5\x8f\x96\xe6\xb6\x88") | bold | color(theme.warn_color))
                : (text("Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color)),
            text("  "),
        }));
        rows.push_back(text(""));

        if (hs.loading) {
            rows.push_back(text("    \xe2\x8f\xb3 \xe6\xad\xa3\xe5\x9c\xa8\xe7\xad\x89\xe5\xbe\x85 AI \xe5\x93\x8d\xe5\xba\x94...") | color(theme.accent_color));
            rows.push_back(text(""));
            rows.push_back(text("    \xe6\x8c\x89 Esc \xe5\x8f\xaf\xe4\xbb\xa5\xe5\x8f\x96\xe6\xb6\x88\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82") | bold | color(theme.warn_color));
            return vbox(std::move(rows));
        }
        if (!hs.error.empty()) {
            rows.push_back(text("    " + hs.error) | color(theme.error_color));
            rows.push_back(text(""));
            rows.push_back(text("    \xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5 API Key \xe6\x98\xaf\xe5\x90\xa6\xe5\xb7\xb2\xe9\x85\x8d\xe7\xbd\xae, \xe4\xbd\xbf\xe7\x94\xa8 /config \xe8\xbf\x9b\xe8\xa1\x8c\xe8\xae\xbe\xe7\xbd\xae\xe3\x80\x82") | color(theme.dim_color));
            return vbox(std::move(rows));
        }
        if (hs.lines.empty()) {
            rows.push_back(text("    \xe6\x9a\x82\xe6\x97\xa0\xe6\x8f\x90\xe7\xa4\xba\xe5\x86\x85\xe5\xae\xb9\xe3\x80\x82") | color(theme.dim_color));
            return vbox(std::move(rows));
        }

        int total_lines = static_cast<int>(hs.lines.size());
        auto dim = ftxui::Terminal::Size();
        int visible_lines = std::max(5, dim.dimy - 12);
        int start_idx = std::max(0, std::min(hs.scroll_offset, total_lines - 1));
        int end_idx = std::min(start_idx + visible_lines, total_lines);

        Elements body_lines;
        for (int i = start_idx; i < end_idx; i++)
            body_lines.push_back(text("    " + hs.lines[i]) | color(theme.text_color));
        rows.push_back(vbox(std::move(body_lines)) | vscroll_indicator | frame | flex);

        std::string pos_info = "    " + std::to_string(std::min(hs.scroll_offset + 1, total_lines))
                             + "/" + std::to_string(total_lines) + " \xe8\xa1\x8c";
        rows.push_back(text(""));
        rows.push_back(text(pos_info) | color(theme.dim_color));

        if (hs.streaming)
            rows.push_back(text("    \xe2\x8f\xb3 AI \xe6\xad\xa3\xe5\x9c\xa8\xe7\x94\x9f\xe6\x88\x90\xe5\x93\x8d\xe5\xba\x94...") | color(theme.accent_color));

        if (!hs.loading && !hs.streaming && !hs.lines.empty()) {
            rows.push_back(text(""));
            Elements action_list;
            action_list.push_back(text("   \xe5\xbf\xab\xe6\x8d\xb7\xe6\x93\x8d\xe4\xbd\x9c: ") | color(theme.dim_color));
            for (int i = 0; i < static_cast<int>(hs.actions.size()); i++) {
                auto item = text(" [" + hs.actions[i] + "] ");
                if (i == hs.selected_action)
                    item = item | bold | bgcolor(theme.selected_bg) | color(theme.accent_color);
                else
                    item = item | color(theme.text_color);
                action_list.push_back(item);
            }
            rows.push_back(hbox(std::move(action_list)));
        }
        return vbox(std::move(rows));
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::HintView) return false;
        auto& hs = state.hint_state;
        if (event == Event::Escape) {
            if ((hs.loading || hs.streaming) && ctx.runner.cancel_flag()) {
                ctx.runner.cancel();
                hs.lines.push_back("[\xe5\x8f\x96\xe6\xb6\x88\xe4\xb8\xad...] \xe6\xad\xa3\xe5\x9c\xa8\xe4\xb8\xad\xe6\x96\xad AI \xe8\xaf\xb7\xe6\xb1\x82...");
                return true;
            }
            state.pop_view();
            return true;
        }
        if (event == Event::ArrowLeft) { hs.selected_action = std::max(0, hs.selected_action - 1); return true; }
        if (event == Event::ArrowRight) { hs.selected_action = std::min((int)hs.actions.size() - 1, hs.selected_action + 1); return true; }
        if (event == Event::Return && !hs.lines.empty() && !hs.loading && !hs.actions.empty()
            && hs.selected_action >= 0 && hs.selected_action < (int)hs.actions.size()) {
            std::string label = hs.actions[hs.selected_action];
            if (label.find("Solve") != std::string::npos) { state.pop_view(); state.push_view(ViewMode::SolveView); }
            else if (label.find("List") != std::string::npos) { state.pop_view(); state.push_view(ViewMode::ListView); }
            return true;
        }
        if (event == Event::ArrowUp || event == Event::Character('k')) { hs.scroll_offset = std::max(0, hs.scroll_offset - 1); return true; }
        if (event == Event::ArrowDown || event == Event::Character('j')) { hs.scroll_offset = std::min(std::max(0, (int)hs.lines.size() - 1), hs.scroll_offset + 1); return true; }
        if (event == Event::PageUp) { hs.scroll_offset = std::max(0, hs.scroll_offset - 10); return true; }
        if (event == Event::PageDown) { hs.scroll_offset = std::min(std::max(0, (int)hs.lines.size() - 1), hs.scroll_offset + 10); return true; }
        return false;
    });
}

// ══════════════════════════════════════════════════════════════════
// TestView — test execution with streaming output
// ══════════════════════════════════════════════════════════════════

Component CreateTestView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& ts = state.test_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        {
            std::string title = ts.problem_title.empty()
                ? " \xe6\xb5\x8b\xe8\xaf\x95"
                : " \xe6\xb5\x8b\xe8\xaf\x95: " + ts.problem_title;
            rows.push_back(hbox({
                text(title) | bold | color(theme.heading_color), filler(),
                text("\xe2\x86\x91\xe2\x86\x93/PgUp/PgDn \xe6\xbb\x9a\xe5\x8a\xa8  ") | color(theme.dim_color),
                (ts.running || ts.streaming)
                    ? (text("Esc \xe5\x8f\x96\xe6\xb6\x88") | bold | color(theme.warn_color))
                    : (text("Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color)),
                text("  "),
            }));
        }
        rows.push_back(text(""));
        if (!ts.error.empty()) {
            rows.push_back(text("    [!] " + ts.error) | color(theme.error_color));
            return vbox(std::move(rows));
        }
        if (ts.running && !ts.streaming) {
            rows.push_back(hbox({text("    \xe2\x8f\xb3 "), text("\xe6\xad\xa3\xe5\x9c\xa8\xe7\xbc\x96\xe8\xaf\x91/\xe6\xb5\x8b\xe8\xaf\x95...") | color(theme.accent_color)}));
            rows.push_back(text(""));
        }
        if (!ts.verdict_label.empty()) {
            auto v_color = theme.success_color;
            if (ts.verdict_color == "error") v_color = theme.error_color;
            else if (ts.verdict_color == "warn") v_color = theme.warn_color;
            std::string icon = ts.all_ac ? "\xe2\x9c\x93" : "\xc3\x97";
            rows.push_back(hbox({
                text("    "), text(icon + " ") | bold | color(v_color),
                text("[\xe7\xbb\x93\xe6\x9d\x9f] ") | color(theme.dim_color),
                text(ts.verdict_label) | bold | color(v_color),
                text("  " + std::to_string(ts.pass_count) + "/" + std::to_string(ts.total_count) + " \xe9\x80\x9a\xe8\xbf\x87") | color(theme.dim_color),
            }));
            rows.push_back(text(""));
        }
        if (!ts.output_lines.empty()) {
            int start = std::max(0, (int)ts.output_lines.size() - 30);
            for (int i = start; i < (int)ts.output_lines.size(); ++i)
                rows.push_back(render_buffer_line(ts.output_lines[i], theme));
            rows.push_back(text(""));
        }
        if (!ts.ai_lines.empty()) {
            rows.push_back(hbox({text("    "), text("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 AI \xe9\x94\x99\xe8\xaf\xaf\xe5\x88\x86\xe6\x9e\x90 \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80") | bold | color(theme.warn_color)}));
            rows.push_back(text(""));
            int tl = (int)ts.ai_lines.size();
            int start_idx = std::max(0, std::min(ts.scroll_offset, tl - 1));
            int end_idx = std::min(start_idx + 15, tl);
            for (int i = start_idx; i < end_idx; i++)
                rows.push_back(text("    " + ts.ai_lines[i]) | color(theme.text_color));
            rows.push_back(text(""));
        }
        if (!ts.running && !ts.streaming && (ts.show_actions || !ts.verdict_label.empty())) {
            Elements action_list;
            action_list.push_back(text("   \xe5\xbf\xab\xe6\x8d\xb7\xe6\x93\x8d\xe4\xbd\x9c: ") | color(theme.dim_color));
            for (int i = 0; i < (int)ts.actions.size(); i++) {
                auto item = text(" [" + ts.actions[i] + "] ");
                if (i == ts.selected_action)
                    item = item | bold | bgcolor(theme.selected_bg) | color(theme.accent_color);
                else
                    item = item | color(theme.text_color);
                action_list.push_back(item);
            }
            rows.push_back(hbox(std::move(action_list)));
        }
        return vbox(std::move(rows));
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::TestView) return false;
        auto& ts = state.test_state;
        if (event == Event::Escape) {
            if ((ts.running || ts.streaming) && ctx.runner.cancel_flag()) {
                ctx.runner.cancel();
                ts.output_lines.push_back({LineType::SystemLog, "[\xe5\x8f\x96\xe6\xb6\x88\xe4\xb8\xad...]"});
                return true;
            }
            state.pop_view();
            return true;
        }
        if (event == Event::ArrowLeft) { ts.selected_action = std::max(0, ts.selected_action - 1); return true; }
        if (event == Event::ArrowRight) { ts.selected_action = std::min((int)ts.actions.size() - 1, ts.selected_action + 1); return true; }
        if (event == Event::Return && !ts.running && !ts.streaming && !ts.actions.empty()
            && ts.selected_action >= 0 && ts.selected_action < (int)ts.actions.size()) {
            std::string label = ts.actions[ts.selected_action];
            if (label.find("Again") != std::string::npos || label.find("\xe9\x87\x8d\xe6\x96\xb0") != std::string::npos) {
                state.pop_view();
                ctx.run_command("/test " + std::to_string(ts.problem_id), true);
            } else
                state.pop_view();
            return true;
        }
        if (event == Event::ArrowUp || event == Event::Character('k')) { ts.scroll_offset = std::max(0, ts.scroll_offset - 1); return true; }
        if (event == Event::ArrowDown || event == Event::Character('j')) { ts.scroll_offset = std::min(std::max(0, (int)ts.ai_lines.size() - 1), ts.scroll_offset + 1); return true; }
        if (event == Event::PageUp) { ts.scroll_offset = std::max(0, ts.scroll_offset - 10); return true; }
        if (event == Event::PageDown) { ts.scroll_offset = std::min(std::max(0, (int)ts.ai_lines.size() - 1), ts.scroll_offset + 10); return true; }
        return false;
    });
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
