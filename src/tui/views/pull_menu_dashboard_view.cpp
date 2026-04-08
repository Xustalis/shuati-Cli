#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "../tui_command_engine.hpp"
#include "../../cmd/commands.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

// ══════════════════════════════════════════════════════════════════
// PullView
// ══════════════════════════════════════════════════════════════════

Component CreatePullView(ViewContext& ctx) {
    auto& state = ctx.state;
    InputOption opt;
    opt.on_enter = [&] {
        if (state.pull_state.url.empty() || state.pull_state.loading) return;
        std::string url = state.pull_state.url;
        if (url.find("luogu.com") == std::string::npos &&
            url.find("leetcode") == std::string::npos &&
            url.find("codeforces.com") == std::string::npos &&
            url.find("lanqiao.cn") == std::string::npos) {
            state.pull_state.error = "\xe9\x9d\x9e\xe6\xb3\x95 URL";
            return;
        }
        state.pull_state.error.clear();
        state.pull_state.loading = true;
        state.pull_state.progress = 0.1f;
        state.pull_state.status_msg = "\xe6\xad\xa3\xe5\x9c\xa8\xe5\x88\x86\xe6\x9e\x90 URL...";

        ctx.runner.submit("pull",
            [url](TaskRunner::StreamCb emit, std::atomic_bool& /*cancel*/) {
                tui_execute_command_stream({"shuati", "pull", url}, "pull",
                    [&emit](const std::string& chunk) { emit(chunk); });
            },
            [&](const std::string& chunk) {
                if (chunk.find("Downloaded") != std::string::npos)
                    state.pull_state.progress = 0.7f;
                if (chunk.find("Saved") != std::string::npos)
                    state.pull_state.progress = 0.9f;
            },
            [&](bool cancelled) {
                if (cancelled) return;
                try {
                    auto root = Config::find_root();
                    auto svc = cmd::Services::load(root);
                    auto problems = svc.pm->list_problems();
                    if (!problems.empty()) {
                        const auto& p = problems.back();
                        state.pull_state.result.tid = p.display_id;
                        state.pull_state.result.title = p.title;
                        state.pull_state.result.path = p.content_path;
                    }
                } catch (...) {}
                state.pull_state.finished = true;
                state.pull_state.loading = false;
                state.list_state.loaded = false;
            },
            [&](std::string err) {
                state.pull_state.error = err;
                state.pull_state.finished = true;
                state.pull_state.loading = false;
            });
    };
    auto input_comp = Input(&state.pull_state.url, "\xe8\xaf\xb7\xe7\xb2\x98\xe8\xb4\xb4 URL \xe5\xb9\xb6\xe5\x9b\x9e\xe8\xbd\xa6", opt);

    auto renderer = Renderer(input_comp, [&, input_comp] {
        const auto& ps = state.pull_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xe6\x8b\x89\xe5\x8f\x96\xe9\xa2\x98\xe7\x9b\xae") | bold | color(theme.heading_color),
            filler(), text("Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        if (!ps.finished) {
            rows.push_back(text("    \xe8\xaf\xb7\xe7\xb2\x98\xe8\xb4\xb4\xe9\xa2\x98\xe7\x9b\xae URL\xef\xbc\x8c\xe5\x9b\x9e\xe8\xbd\xa6\xe7\xa1\xae\xe8\xae\xa4:") | color(theme.dim_color));
            rows.push_back(text(""));
            rows.push_back(hbox({
                text("    > ") | bold | color(theme.prompt_color),
                input_comp->Render() | color(theme.input_color),
            }));
            rows.push_back(text(""));
            if (!ps.error.empty())
                rows.push_back(text("    [!] " + ps.error) | color(theme.error_color));
            if (ps.loading) {
                rows.push_back(hbox({
                    text("    \xe6\xad\xa3\xe5\x9c\xa8\xe6\x8b\x89\xe5\x8f\x96: "),
                    gauge(ps.progress) | color(theme.accent_color) | size(WIDTH, EQUAL, 30),
                    text(" " + std::to_string((int)(ps.progress * 100)) + "%"),
                }));
            }
        } else {
            if (ps.error.empty()) {
                rows.push_back(text("    \xe2\x88\x9a \xe6\x8b\x89\xe5\x8f\x96\xe6\x88\x90\xe5\x8a\x9f!") | bold | color(theme.success_color));
                rows.push_back(text(""));
                rows.push_back(hbox({text("    \xe9\xa2\x98\xe5\x8f\xb7: "), text(std::to_string(ps.result.tid)) | bold | color(theme.accent_color)}));
                rows.push_back(hbox({text("    \xe6\xa0\x87\xe9\xa2\x98: "), text(ps.result.title) | color(theme.text_color)}));
            } else {
                rows.push_back(text("    \xc3\x97 \xe6\x8b\x89\xe5\x8f\x96\xe5\xa4\xb1\xe8\xb4\xa5: " + ps.error) | color(theme.error_color));
            }
            rows.push_back(text(""));
            rows.push_back(text("    \xe6\x8c\x89 Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color));
        }
        return vbox(std::move(rows)) | flex;
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::PullView) return false;
        if (event == Event::Escape) {
            state.pull_state = PullState{};
            state.pop_view(); return true;
        }
        return false;
    });
}

// ══════════════════════════════════════════════════════════════════
// MenuView
// ══════════════════════════════════════════════════════════════════

Component CreateMenuView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& ms = state.menu_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xe5\x91\xbd\xe4\xbb\xa4\xe8\x8f\x9c\xe5\x8d\x95") | bold | color(theme.heading_color),
            filler(), text("\xe2\x86\x91\xe2\x86\x93 Enter Esc") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        if (ms.categories.empty()) {
            rows.push_back(text("    \xe6\x9a\x82\xe6\x97\xa0\xe5\x8f\xaf\xe7\x94\xa8\xe5\x91\xbd\xe4\xbb\xa4") | color(theme.dim_color));
            return vbox(std::move(rows));
        }
        int cat_idx = 0;
        for (const auto& cat : ms.categories) {
            bool cf = (cat_idx == ms.selected_category);
            rows.push_back(hbox({text("  "), text(cat.name) | bold | color(cf ? theme.heading_color : theme.dim_color)}));
            rows.push_back(text(""));
            for (int ii = 0; ii < (int)cat.items.size(); ii++) {
                const auto& item = cat.items[ii];
                bool sel = cf && (ii == ms.selected_item);
                auto el = hbox({
                    text(sel ? "  > " : "    ") | color(sel ? theme.prompt_color : theme.dim_color),
                    text(item.label) | bold | color(sel ? theme.text_color : theme.dim_color),
                    text("  -  ") | color(theme.dim_color),
                    text(item.description) | color(sel ? theme.text_color : theme.dim_color),
                });
                if (sel) el = el | inverted;
                rows.push_back(el);
            }
            rows.push_back(text(""));
            cat_idx++;
        }
        return vbox(std::move(rows)) | flex;
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::MenuView) return false;
        auto& ms = state.menu_state;
        if (event == Event::Escape) { state.pop_view(); return true; }
        if (ms.categories.empty()) return false;
        if (event == Event::ArrowUp || event == Event::Character('k')) {
            int n = (int)ms.categories[ms.selected_category].items.size();
            if (n == 0) { if (ms.selected_category > 0) ms.selected_category--; }
            else if (ms.selected_item > 0) ms.selected_item--;
            else if (ms.selected_category > 0) {
                ms.selected_category--;
                ms.selected_item = (int)ms.categories[ms.selected_category].items.size() - 1;
            }
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character('j')) {
            int n = (int)ms.categories[ms.selected_category].items.size();
            if (n == 0) { if (ms.selected_category < (int)ms.categories.size() - 1) ms.selected_category++; }
            else if (ms.selected_item < n - 1) ms.selected_item++;
            else if (ms.selected_category < (int)ms.categories.size() - 1) {
                ms.selected_category++;
                ms.selected_item = 0;
            }
            return true;
        }
        if (event == Event::Return) {
            const auto& item = ms.categories[ms.selected_category].items[ms.selected_item];
            state.pop_view();
            if (!item.route_id.empty() && item.route_id != "Main") {
                if (item.route_id == "ConfigView") state.push_view(ViewMode::ConfigView);
                else if (item.route_id == "ListView") state.push_view(ViewMode::ListView);
                else if (item.route_id == "DashboardView") { load_dashboard_state(state); state.push_view(ViewMode::DashboardView); }
                else if (item.route_id == "SolveView") state.push_view(ViewMode::SolveView);
                else if (item.route_id == "PullView") state.push_view(ViewMode::PullView);
            } else if (item.requires_args) {
                state.input = item.command + " ";
            } else {
                ctx.run_command(item.command, true);
            }
            return true;
        }
        return false;
    });
}

// ══════════════════════════════════════════════════════════════════
// DashboardView
// ══════════════════════════════════════════════════════════════════

Component CreateDashboardView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& ds = state.dashboard_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xf0\x9f\x93\x8a \xe6\x95\xb0\xe6\x8d\xae\xe6\xa6\x82\xe8\xa7\x88") | bold | color(theme.heading_color),
            filler(), text("r \xe5\x88\xb7\xe6\x96\xb0  Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        if (!ds.error.empty()) {
            rows.push_back(text("    [!] " + ds.error) | color(theme.error_color));
            return vbox(std::move(rows)) | flex;
        }
        // Summary cards
        auto card = [&](const std::string& label, const std::string& value, const std::string& icon) {
            return vbox({
                text("  " + icon + " " + label) | color(theme.dim_color),
                text(""),
                text("  " + value) | bold | color(theme.accent_color),
            }) | size(WIDTH, EQUAL, 16) | size(HEIGHT, EQUAL, 5) | borderStyled(ROUNDED) | color(theme.border_color);
        };
        rows.push_back(hbox({
            card("\xe6\x80\xbb\xe9\xa2\x98\xe6\x95\xb0", std::to_string(ds.total_problems), "\xf0\x9f\x93\x9a"),
            text(" "),
            card("\xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87", std::to_string(ds.ac_count), "\xe2\x9c\x93"),
            text(" "),
            card("\xe5\xbe\x85\xe5\xa4\x8d\xe4\xb9\xa0", std::to_string(ds.pending_reviews), "\xf0\x9f\x93\x9d"),
            text(" "),
            card("\xe4\xbb\x8a\xe6\x97\xa5", std::to_string(ds.today_solved), "\xe2\x9a\xa1"),
        }));
        rows.push_back(text(""));
        rows.push_back(separatorLight() | color(theme.border_color));
        rows.push_back(text(""));
        // AC rate
        rows.push_back(hbox({
            text("   \xe9\x80\x9a\xe8\xbf\x87\xe7\x8e\x87: ") | color(theme.dim_color),
            text(std::to_string((int)ds.ac_rate) + "%") | bold | color(ds.ac_rate >= 50.0 ? theme.success_color : theme.warn_color),
        }));
        // Quick actions
        rows.push_back(text(""));
        rows.push_back(text("   \xe5\xbf\xab\xe6\x8d\xb7\xe6\x93\x8d\xe4\xbd\x9c:") | bold | color(theme.heading_color));
        std::vector<std::pair<std::string, std::string>> actions = {
            {"\xe6\x8b\x89\xe5\x8f\x96", "/pull"}, {"\xe5\x81\x9a\xe9\xa2\x98", "/solve"},
            {"\xe9\xa2\x98\xe5\xba\x93", "/list"}, {"\xe9\x85\x8d\xe7\xbd\xae", "/config"},
        };
        Elements action_row;
        for (int i = 0; i < (int)actions.size(); i++) {
            bool sel = (ds.selected_quick_idx == i);
            auto btn = hbox({
                text(" [") | color(theme.dim_color),
                text(actions[i].second) | bold | color(theme.accent_color),
                text("] ") | color(theme.dim_color),
                text(actions[i].first) | color(sel ? theme.text_color : theme.dim_color),
            });
            if (sel) btn = btn | inverted;
            action_row.push_back(btn);
            action_row.push_back(text("  "));
        }
        rows.push_back(hbox(std::move(action_row)));
        return vbox(std::move(rows)) | flex;
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::DashboardView) return false;
        if (event == Event::Escape) { state.pop_view(); return true; }
        auto& ds = state.dashboard_state;
        if (event == Event::Character('r') || event == Event::Character('R')) { load_dashboard_state(state); return true; }
        if (event == Event::ArrowLeft) { ds.selected_quick_idx = std::max(0, ds.selected_quick_idx - 1); return true; }
        if (event == Event::ArrowRight) { ds.selected_quick_idx = std::min(3, ds.selected_quick_idx + 1); return true; }
        if (event == Event::Return) {
            std::vector<std::string> cmds = {"/pull", "/solve", "/list", "/config"};
            if (ds.selected_quick_idx >= 0 && ds.selected_quick_idx < (int)cmds.size()) {
                auto cmd = cmds[ds.selected_quick_idx];
                if (cmd == "/pull") state.push_view(ViewMode::PullView);
                else if (cmd == "/solve") state.push_view(ViewMode::SolveView);
                else if (cmd == "/list") { load_list_state(state); state.push_view(ViewMode::ListView); }
                else if (cmd == "/config") { load_config_state(state); state.push_view(ViewMode::ConfigView); }
            }
            return true;
        }
        return false;
    });
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
