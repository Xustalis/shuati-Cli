#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "../../cmd/commands.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

// ══════════════════════════════════════════════════════════════════
// ListView
// ══════════════════════════════════════════════════════════════════

namespace {
std::string filter_label(const std::string& f) {
    if (f == "ac") return "AC"; if (f == "failed") return "Failed";
    if (f == "unaudited") return "Unaudited"; if (f == "review") return "Review";
    return "All Status";
}
std::string diff_label(const std::string& d) {
    if (d == "easy") return "Easy"; if (d == "medium") return "Medium";
    if (d == "hard") return "Hard"; return "All Diffs";
}
}

Component CreateListView(ViewContext& ctx) {
    auto& state = ctx.state;
    auto renderer = Renderer([&] {
        const auto& ls = state.list_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xe9\xa2\x98\xe5\xba\x93") | bold | color(theme.heading_color), filler(),
            text(" f ") | color(theme.prompt_color) | bold,
            text("Status: [" + filter_label(ls.status_filter) + "] ") | color(theme.system_color),
            text(" F ") | color(theme.prompt_color) | bold,
            text("Diff: [" + diff_label(ls.difficulty_filter) + "] ") | color(theme.system_color),
            text("  Esc \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        if (!ls.error.empty()) {
            rows.push_back(text("    " + ls.error) | color(theme.dim_color));
            return vbox(std::move(rows));
        }
        if (ls.rows.empty()) {
            rows.push_back(text("    \xe6\x9a\x82\xe6\x97\xa0\xe9\xa2\x98\xe7\x9b\xae\xe3\x80\x82\xe4\xbd\xbf\xe7\x94\xa8 /pull <url> \xe6\x8b\x89\xe5\x8f\x96\xe3\x80\x82") | color(theme.dim_color));
            return vbox(std::move(rows));
        }
        // Header
        rows.push_back(hbox({
            text("     ") | size(WIDTH, EQUAL, 5),
            text("#") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 5),
            text("\xe9\xa2\x98\xe7\x9b\xae\xe5\x90\x8d\xe7\xa7\xb0") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 25),
            text("\xe9\x9a\xbe\xe5\xba\xa6") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 8),
            text("\xe6\x9d\xa5\xe6\xba\x90") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 10),
            text("\xe7\x8a\xb6\xe6\x80\x81") | bold | color(theme.table_header_color) | size(WIDTH, EQUAL, 10),
            text("\xe6\x97\xa5\xe6\x9c\x9f") | bold | color(theme.table_header_color),
        }));
        Elements table_rows;
        for (int i = 0; i < (int)ls.rows.size(); i++) {
            const auto& r = ls.rows[i];
            bool sel = (i == ls.selected);
            auto rc = sel ? theme.text_color : theme.dim_color;
            auto diff_c = theme.dim_color;
            if (r.difficulty == "easy") diff_c = theme.success_color;
            else if (r.difficulty == "medium") diff_c = theme.warn_color;
            else if (r.difficulty == "hard") diff_c = theme.error_color;
            auto stat_c = theme.dim_color;
            if (r.status == "AC") stat_c = theme.success_color;
            else if (r.status != "-") stat_c = theme.error_color;
            auto row_el = hbox({
                text(sel ? "  >  " : "     ") | color(sel ? theme.prompt_color : theme.dim_color) | size(WIDTH, EQUAL, 5),
                text(std::to_string(r.tid)) | color(rc) | size(WIDTH, EQUAL, 5),
                text(r.title) | color(rc) | size(WIDTH, EQUAL, 25),
                text(r.difficulty) | color(diff_c) | size(WIDTH, EQUAL, 8),
                text(r.source) | color(theme.dim_color) | size(WIDTH, EQUAL, 10),
                text(r.status) | color(stat_c) | size(WIDTH, EQUAL, 10),
                text(r.date) | color(rc),
            });
            if (sel) row_el = row_el | bold | inverted | focus;
            table_rows.push_back(row_el);
        }
        rows.push_back(vbox(std::move(table_rows)) | vscroll_indicator | frame | flex);
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   Enter") | bold | color(theme.dim_color), text(" \xe5\x81\x9a\xe9\xa2\x98") | color(theme.dim_color),
            text("  t") | bold | color(theme.dim_color), text(" \xe6\xb5\x8b\xe8\xaf\x95") | color(theme.dim_color),
            text("  h") | bold | color(theme.dim_color), text(" \xe6\x8f\x90\xe7\xa4\xba") | color(theme.dim_color),
            text("  d") | bold | color(theme.dim_color), text(" \xe5\x88\xa0\xe9\x99\xa4") | color(theme.dim_color),
            text("  f/F") | bold | color(theme.dim_color), text(" \xe7\xad\x9b\xe9\x80\x89") | color(theme.dim_color),
            filler(),
            text(std::to_string(ls.selected + 1) + "/" + std::to_string(ls.rows.size()) + " ") | color(theme.dim_color),
        }));
        return vbox(std::move(rows));
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::ListView) return false;
        if (event == Event::Escape) { state.pop_view(); return true; }
        auto& ls = state.list_state;
        if (event == Event::ArrowUp || event == Event::Character('k')) { ls.selected = std::max(0, ls.selected - 1); return true; }
        if (event == Event::ArrowDown || event == Event::Character('j')) { ls.selected = std::min((int)ls.rows.size() - 1, ls.selected + 1); return true; }
        if (event == Event::PageUp) { ls.selected = std::max(0, ls.selected - 10); return true; }
        if (event == Event::PageDown) { ls.selected = std::min((int)ls.rows.size() - 1, ls.selected + 10); return true; }
        if (event == Event::Return && !ls.rows.empty()) {
            std::string tid = std::to_string(ls.rows[ls.selected].tid);
            state.pop_view();
            ctx.run_command("/solve " + tid, true);
            return true;
        }
        if (event == Event::Character('t') && !ls.rows.empty()) {
            std::string tid = std::to_string(ls.rows[ls.selected].tid);
            state.pop_view(); ctx.run_command("/test " + tid, true); return true;
        }
        if (event == Event::Character('h') && !ls.rows.empty()) {
            std::string tid = std::to_string(ls.rows[ls.selected].tid);
            state.pop_view(); ctx.run_command("/hint " + tid, true); return true;
        }
        if (event == Event::Character('v') && !ls.rows.empty()) {
            std::string tid = std::to_string(ls.rows[ls.selected].tid);
            state.pop_view(); ctx.run_command("/view " + tid, true); return true;
        }
        if (event == Event::Character('r') && !ls.rows.empty()) {
            std::string tid = std::to_string(ls.rows[ls.selected].tid);
            state.pop_view(); ctx.run_command("/record " + tid, true); return true;
        }
        if (event == Event::Character('d') && !ls.rows.empty()) {
            int tid = ls.rows[ls.selected].tid;
            if (tid <= 0) return false;
            state.pop_view(); ctx.run_command("/delete " + std::to_string(tid) + " --confirm", true); return true;
        }
        if (event == Event::Character('f')) {
            static const std::vector<std::string> cycle = {"all", "ac", "failed", "unaudited", "review"};
            auto it = std::find(cycle.begin(), cycle.end(), ls.status_filter);
            int idx = (it != cycle.end()) ? (int)(it - cycle.begin()) : 0;
            load_list_state(state, cycle[(idx + 1) % (int)cycle.size()], ls.difficulty_filter, ls.source_filter);
            return true;
        }
        if (event == Event::Character('F')) {
            static const std::vector<std::string> cycle = {"all", "easy", "medium", "hard"};
            auto it = std::find(cycle.begin(), cycle.end(), ls.difficulty_filter);
            int idx = (it != cycle.end()) ? (int)(it - cycle.begin()) : 0;
            load_list_state(state, ls.status_filter, cycle[(idx + 1) % (int)cycle.size()], ls.source_filter);
            return true;
        }
        return false;
    });
}

// ══════════════════════════════════════════════════════════════════
// SolveView
// ══════════════════════════════════════════════════════════════════

Component CreateSolveView(ViewContext& ctx) {
    auto& state = ctx.state;

    auto refresh_solve_list = [&] {
        if (!state.solve_state.loaded || state.solve_state.all_rows.empty()) {
            auto root = Config::find_root();
            if (root.empty()) return;
            auto svc = cmd::Services::load(root, true);
            auto problems = svc.pm->list_problems();
            state.solve_state.all_rows.clear();
            for (const auto& p : problems)
                state.solve_state.all_rows.push_back({p.display_id, p.id, p.title, p.difficulty, cmd::canonical_source(p.source)});
            state.solve_state.loaded = true;
        }
        state.solve_state.filtered_rows.clear();
        for (const auto& row : state.solve_state.all_rows) {
            bool src = std::find(state.solve_state.selected_sources.begin(), state.solve_state.selected_sources.end(), row.source) != state.solve_state.selected_sources.end();
            bool diff = std::find(state.solve_state.selected_difficulties.begin(), state.solve_state.selected_difficulties.end(), row.difficulty) != state.solve_state.selected_difficulties.end();
            bool search = state.solve_state.search_query.empty() || row.title.find(state.solve_state.search_query) != std::string::npos || std::to_string(row.tid).find(state.solve_state.search_query) != std::string::npos;
            if (src && diff && search) state.solve_state.filtered_rows.push_back(row);
        }
        if (state.solve_state.filtered_rows.empty()) state.solve_state.selected_idx = 0;
        else if (state.solve_state.selected_idx >= (int)state.solve_state.filtered_rows.size())
            state.solve_state.selected_idx = (int)state.solve_state.filtered_rows.size() - 1;
        state.solve_state.show_preview = false;
        state.solve_state.preview_content.clear();
    };

    InputOption search_opt;
    search_opt.on_change = refresh_solve_list;
    auto search_comp = Input(&state.solve_state.search_query, "\xe6\x90\x9c\xe7\xb4\xa2...", search_opt);

    auto renderer = Renderer(search_comp, [&, search_comp] {
        const auto& ss = state.solve_state;
        const auto& theme = ctx.theme;
        Elements rows;
        rows.push_back(text(""));
        rows.push_back(hbox({
            text("   \xe5\x81\x9a\xe9\xa2\x98\xe6\xa8\xa1\xe5\xbc\x8f") | bold | color(theme.heading_color), filler(),
            text("Esc \xe8\xbf\x94\xe5\x9b\x9e  Enter \xe7\xa1\xae\xe8\xae\xa4") | color(theme.dim_color), text("  "),
        }));
        rows.push_back(text(""));
        rows.push_back(hbox({text("    \xe6\x90\x9c\xe7\xb4\xa2: ") | color(theme.dim_color), search_comp->Render() | color(theme.input_color)}));
        rows.push_back(text(""));
        if (ss.filtered_rows.empty()) {
            rows.push_back(text("    (\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0\xe5\x8c\xb9\xe9\x85\x8d\xe9\xa2\x98\xe7\x9b\xae)") | color(theme.dim_color));
        } else {
            Elements list_items;
            for (int i = 0; i < (int)ss.filtered_rows.size(); i++) {
                const auto& r = ss.filtered_rows[i];
                bool sel = (i == ss.selected_idx);
                auto rc = sel ? theme.text_color : theme.dim_color;
                auto row_el = hbox({
                    text(sel ? "  > " : "    ") | color(sel ? theme.prompt_color : theme.dim_color),
                    text(std::to_string(r.tid)) | size(WIDTH, EQUAL, 6) | color(rc),
                    text(r.title) | size(WIDTH, EQUAL, 30) | color(rc),
                    text(r.difficulty) | size(WIDTH, EQUAL, 10) | color(rc),
                    text(r.source) | color(theme.dim_color),
                });
                if (sel) row_el = row_el | bold | inverted | focus;
                list_items.push_back(row_el);
            }
            rows.push_back(vbox(std::move(list_items)) | frame | size(HEIGHT, LESS_THAN, 12));
        }
        if (ss.show_preview) {
            rows.push_back(separatorLight() | color(theme.border_color));
            rows.push_back(text("    \xe9\xa2\x98\xe9\x9d\xa2\xe9\xa2\x84\xe8\xa7\x88:") | bold | color(theme.heading_color));
            Elements para;
            std::string lines = ss.preview_content;
            for (auto& c : lines) if (c == '\r') c = '\n';
            std::string::size_type start = 0;
            while (start < lines.size()) {
                auto nl = lines.find('\n', start);
                std::string seg = (nl == std::string::npos) ? lines.substr(start) : lines.substr(start, nl - start);
                para.push_back(text("    " + seg) | color(theme.text_color));
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            rows.push_back(vbox(std::move(para)) | frame | size(HEIGHT, LESS_THAN, 15));
            rows.push_back(text(""));
            rows.push_back(text("    [Enter] \xe6\x89\x93\xe5\xbc\x80  [T] \xe6\xb5\x8b\xe8\xaf\x95  [Esc] \xe8\xbf\x94\xe5\x9b\x9e") | color(theme.accent_color));
        }
        return vbox(std::move(rows)) | flex;
    });

    return CatchEvent(renderer, [&, refresh_solve_list](Event event) -> bool {
        if (state.view_mode != ViewMode::SolveView) return false;
        auto& ss = state.solve_state;
        if (event == Event::Escape) {
            if (ss.show_preview) { ss.show_preview = false; return true; }
            state.pop_view(); return true;
        }
        if (ss.show_preview) {
            if (event == Event::Return && !ss.filtered_rows.empty()) {
                std::string tid = std::to_string(ss.filtered_rows[ss.selected_idx].tid);
                ss.show_preview = false; state.pop_view();
                ctx.run_command("/solve " + tid, true); return true;
            }
            if (event == Event::Character('t') && !ss.filtered_rows.empty()) {
                std::string tid = std::to_string(ss.filtered_rows[ss.selected_idx].tid);
                ss.show_preview = false; state.pop_view();
                ctx.run_command("/test " + tid, true); return true;
            }
        } else {
            if (event == Event::ArrowUp || event == Event::Character('k')) { ss.selected_idx = std::max(0, ss.selected_idx - 1); return true; }
            if (event == Event::ArrowDown || event == Event::Character('j')) { ss.selected_idx = std::min((int)ss.filtered_rows.size() - 1, ss.selected_idx + 1); return true; }
            if (event == Event::Return && !ss.filtered_rows.empty()) {
                ss.show_preview = true;
                ss.preview_content = "\xe6\xad\xa3\xe5\x9c\xa8\xe5\x8a\xa0\xe8\xbd\xbd...";
                int tid = ss.filtered_rows[ss.selected_idx].tid;
                ctx.runner.submit("preview",
                    [tid](TaskRunner::StreamCb emit, std::atomic_bool& cancel) {
                        try {
                            auto svc = cmd::Services::load(Config::find_root());
                            auto p = svc.pm->get_problem(std::to_string(tid));
                            if (!cancel.load()) {
                                emit(p.description);
                            }
                        } catch (...) {
                            if (!cancel.load()) {
                                emit("\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe8\xbd\xbd\xe3\x80\x82");
                            }
                        }
                    },
                    [&](const std::string& chunk) {
                        if (state.solve_state.preview_content == "\xe6\xad\xa3\xe5\x9c\xa8\xe5\x8a\xa0\xe8\xbd\xbd...") {
                            state.solve_state.preview_content = chunk;
                        } else {
                            state.solve_state.preview_content += chunk;
                        }
                    },
                    [&](bool cancelled) {
                        if (cancelled) state.solve_state.preview_content = "\xe5\xb7\xb2\xe5\x8f\x96\xe6\xb6\x88";
                    },
                    [&](std::string err) { 
                        state.solve_state.preview_content = "\xe5\x8a\xa0\xe8\xbd\xbd\xe5\xa4\xb1\xe8\xb4\xa5: " + err; 
                    }
                );
                return true;
            }
            if (event == Event::Character('s')) {
                if (ss.selected_sources.size() == ss.sources.size()) ss.selected_sources = {"luogu"};
                else if (ss.selected_sources.size() == 1 && ss.selected_sources[0] == "luogu") ss.selected_sources = {"leetcode"};
                else ss.selected_sources = ss.sources;
                refresh_solve_list(); return true;
            }
            if (event == Event::Character('d')) {
                if (ss.selected_difficulties.size() == 3) ss.selected_difficulties = {"easy"};
                else if (ss.selected_difficulties.size() == 1 && ss.selected_difficulties[0] == "easy") ss.selected_difficulties = {"medium"};
                else ss.selected_difficulties = {"easy", "medium", "hard"};
                refresh_solve_list(); return true;
            }
        }
        return false;
    });
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
