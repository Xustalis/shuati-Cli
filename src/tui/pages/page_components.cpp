#include "../root_component.hpp"
#include "shuati/tui_views.hpp"
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

namespace shuati {
namespace tui {

Component WelcomePage(AppState& state) {
    return Renderer([&state] {
        TuiTheme theme;
        return vbox({
            render_logo(theme),
            separatorEmpty(),
            text("欢迎使用 Shuati CLI v0.1.0") | bold | center,
            separatorEmpty(),
            hbox({
                filler(),
                vbox({
                    text(" [S] 进入工作区 (Solve Workspace) ") | color(theme.accent_color),
                    text(" [L] 查看题库列表 (Problem List) "),
                    text(" [Q] 退出程序 (Quit) "),
                }),
                filler()
            }),
            filler(),
            text(" 使用方向键或快捷键进行操作 ") | dim | center
        }) | center | border;
    });
}

// ProblemListPage is defined in problem_list_page.cpp

Component ReportViewPage(AppState& state) {
    auto renderer = Renderer([&state] {
        TuiTheme theme;
        return vbox({
            text(" --- 执行报告 --- ") | bold | center | color(theme.accent_color),
            separator(),
            paragraph(state.report_text) | vscroll_indicator | frame | flex,
            separator(),
            text(" 按任意键或 Esc 返回聊天界面 ") | dim | center
        }) | flex | border;
    });

    return CatchEvent(renderer, [&state](Event e) -> bool {
        if (e == Event::Escape || e == Event::Return) {
            state.pop_page();
            return true;
        }
        return false;
    });
}

} // namespace tui
} // namespace shuati
