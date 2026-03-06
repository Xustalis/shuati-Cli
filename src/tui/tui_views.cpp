#include "shuati/tui_views.hpp"
#include <algorithm>

namespace shuati {
namespace tui {

using namespace ftxui;

Element render_logo(const TuiTheme& theme) {
    auto logo = std::vector<std::string>{
        "  ███████╗██╗  ██╗██╗   ██╗ █████╗ ████████╗██╗",
        "  ██╔════╝██║  ██║██║   ██║██╔══██╗╚══██╔══╝██║",
        "  ███████╗███████║██║   ██║███████║   ██║   ██║",
        "  ╚════██║██╔══██║██║   ██║██╔══██║   ██║   ██║",
        "  ███████║██║  ██║╚██████╔╝██║  ██║   ██║   ██║",
        "  ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝"
    };
    Elements lines;
    for (const auto& l : logo) {
        lines.push_back(text(l) | color(theme.accent_color) | bold);
    }
    return vbox(lines) | center;
}

Element render_chat_message(const tui::ChatMessage& msg, const TuiTheme& theme) {
    if (msg.role == MessageRole::User) {
        return vbox({
            hbox({
                filler(),
                vbox({
                    text("You") | color(theme.accent_color) | bold,
                    paragraph(msg.text) | color(theme.text_color)
                }) | borderStyled(ROUNDED) | color(theme.accent_color) | size(WIDTH, LESS_THAN, 82)
            }),
            separatorEmpty()
        });
    }
    if (msg.role == MessageRole::System) {
        return vbox({
            text(msg.text) | color(theme.dim_color) | center,
            separatorEmpty()
        });
    }
    if (msg.role == MessageRole::Error) {
        return vbox({
            vbox({
                text("Error") | color(theme.error_color) | bold,
                paragraph(msg.text) | color(theme.error_color)
            }) | borderStyled(ROUNDED) | color(theme.error_color) | size(WIDTH, LESS_THAN, 82),
            separatorEmpty()
        });
    }
    return vbox({
        vbox({
            text("shuatiCLI") | color(theme.accent_color) | bold,
            paragraph(msg.text) | color(theme.text_color)
        }) | borderStyled(ROUNDED) | color(theme.dim_color) | size(WIDTH, LESS_THAN, 82),
        separatorEmpty()
    });
}

Element render_chat_layout(
    const tui::AppState& state,
    const TuiTheme& theme,
    ftxui::Element input_element,
    ftxui::Element history_element,
    ftxui::Element autocomplete_element,
    ftxui::Element selection_element,
    int width,
    int height
) {
    auto command_help = vbox({
        text("主菜单") | bold | color(theme.accent_color),
        text("init info pull new solve list delete submit"),
        text("test hint view clean login config repl tui exit"),
        separator(),
        state.view_state == AppState::ViewState::Selection ? selection_element : emptyElement()
    }) | borderStyled(ROUNDED) | color(theme.accent_color);

    auto output_panel = vbox({
        text("输出") | bold | color(theme.accent_color),
        separator(),
        history_element | flex
    }) | borderStyled(ROUNDED) | color(theme.dim_color) | flex;

    auto input_panel = vbox({
        state.is_autocomplete_open ? (autocomplete_element | frame | size(HEIGHT, LESS_THAN, 6) | borderStyled(ROUNDED) | color(theme.accent_color)) : emptyElement(),
        hbox({
            text("> ") | bold | color(theme.accent_color),
            input_element | flex
        }),
        separator(),
        text(state.selection_title.empty() ? "请输入命令：" : state.selection_title) | dim
    }) | borderStyled(ROUNDED) | color(theme.dim_color);

    auto body = vbox({
        hbox({
            text("shuatiCLI " + state.environment_info) | color(theme.dim_color),
            filler(),
            text("UTF-8") | color(theme.accent_color)
        }),
        separator(),
        command_help,
        separator(),
        output_panel,
        separator(),
        input_panel
    }) | size(WIDTH, EQUAL, width) | size(HEIGHT, EQUAL, height) | bgcolor(theme.bg_color);

    return body | clear_under;
}

}
}
