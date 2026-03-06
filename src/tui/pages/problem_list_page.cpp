#include "../root_component.hpp"
#include "shuati/tui_views.hpp"
#include "../../cmd/commands.hpp"
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

namespace shuati {
namespace tui {

Component ProblemListPage(AppState& state) {
    struct Context {
        std::vector<std::string> problem_titles;
        std::vector<std::string> problem_pids;
        int selected = 0;
        bool loaded = false;
        std::string error;
    };
    auto ctx = std::make_shared<Context>();

    auto load_problems = [ctx]() {
        try {
            auto root = shuati::cmd::find_root_or_die();
            auto svc = shuati::cmd::Services::load(root);
            auto problems = svc.pm->list_problems();
            ctx->problem_titles.clear();
            ctx->problem_pids.clear();
            for (const auto& p : problems) {
                ctx->problem_titles.push_back(p.title + " [" + p.id + "]");
                ctx->problem_pids.push_back(p.id);
            }
            ctx->loaded = true;
        } catch (const std::exception& e) {
            ctx->error = e.what();
        }
    };

    if (!ctx->loaded) {
        load_problems();
    }

    auto menu = Menu(&ctx->problem_titles, &ctx->selected);

    auto renderer = Renderer(menu, [state, ctx, menu] {
        if (!ctx->error.empty()) {
            return vbox({
                text("错误: " + ctx->error) | color(Color::Red),
                text("按 Esc 返回") | dim
            }) | center;
        }

        if (ctx->problem_titles.empty()) {
            return vbox({
                text("尚无题目数据。") | center,
                text("请在 Solve Workspace 中运行 /pull。") | center,
                text("按 Esc 返回") | dim | center
            }) | center;
        }

        return vbox({
            text(" --- 题库列表 --- ") | bold | center | color(Color::Orange1),
            separator(),
            menu->Render() | vscroll_indicator | frame | flex,
            separator(),
            text(" ↑/↓ 选择 | Enter 解决该题 | Esc 返回 ") | dim | center
        }) | flex | border;
    });

    return CatchEvent(renderer, [&state, ctx](Event e) -> bool {
        if (e == Event::Escape) {
            state.pop_page();
            return true;
        }
        if (e == Event::Return) {
            if (ctx->selected >= 0 && ctx->selected < (int)ctx->problem_pids.size()) {
                std::string pid = ctx->problem_pids[ctx->selected];
                // Navigate to SolveWorkspace and pre-fill or execute /solve
                state.push_page(PageID::SolveWorkspace);
                // We'll let the user type /solve manually or we could add a "pending command" to AppState
                state.history.push_back({MessageRole::System, "已进入解决模式，题目 ID: " + pid});
                return true;
            }
        }
        return false;
    });
}

} // namespace tui
} // namespace shuati
