#include "../root_component.hpp"
#include "../tui_command_engine.hpp"
#include "../tui_menu_model.hpp"
#include "shuati/tui_views.hpp"

#include <ftxui/component/event.hpp>
#include <string>

using namespace ftxui;

namespace shuati {
namespace tui {

Component SolveWorkspacePage(AppState& state) {
    struct ViewContext {
        std::string input_text;
        MenuState menu_state;
        std::vector<std::string> all_commands = tui_command_candidates();
    };
    auto ctx = std::make_shared<ViewContext>();
    state.selection_title = "请输入命令：";
    state.view_state = AppState::ViewState::Chat;

    auto update_autocomplete = [&state, ctx]() {
        if (ctx->menu_state.mode != MenuMode::Main || ctx->input_text.empty() || ctx->input_text[0] != '/') {
            state.is_autocomplete_open = false;
            return;
        }
        std::string prefix = ctx->input_text;
        const size_t space_pos = ctx->input_text.find(' ');
        if (space_pos != std::string::npos) {
            prefix = ctx->input_text.substr(0, space_pos);
            state.is_autocomplete_open = false;
            return;
        }
        state.autocomplete_options.clear();
        bool exact_match = false;
        for (const auto& cmd : ctx->all_commands) {
            if (cmd.starts_with(prefix)) {
                if (cmd == prefix) exact_match = true;
                state.autocomplete_options.push_back(cmd);
            }
        }
        if (exact_match && state.autocomplete_options.size() == 1) {
            state.is_autocomplete_open = false;
        } else if (!state.autocomplete_options.empty()) {
            state.is_autocomplete_open = true;
            state.autocomplete_selected = 0;
        } else {
            state.is_autocomplete_open = false;
        }
    };

    auto execute_line = [&state, ctx, update_autocomplete](const std::string& raw_line) {
        process_menu_input(ctx->menu_state, raw_line, [](const std::vector<std::string>& args, const std::string& base_cmd) {
            return tui_execute_command_capture(args, base_cmd);
        });
        state.captured_output = ctx->menu_state.output;
        state.selection_title = ctx->menu_state.mode == MenuMode::Submenu ? ctx->menu_state.prompt : "请输入命令：";
        state.view_state = ctx->menu_state.mode == MenuMode::Submenu ? AppState::ViewState::Selection : AppState::ViewState::Chat;
        if (ctx->menu_state.should_exit) {
            ctx->menu_state.should_exit = false;
            state.captured_output.clear();
            ctx->input_text.clear();
        }
        update_autocomplete();
    };

    InputOption input_opt;
    input_opt.on_change = [&state, ctx, update_autocomplete] { update_autocomplete(); };
    input_opt.on_enter = [ctx, execute_line, update_autocomplete] {
        if (ctx->input_text.empty()) return;
        std::string cmd_line = std::move(ctx->input_text);
        ctx->input_text.clear();
        update_autocomplete();
        execute_line(cmd_line);
    };

    auto input_comp = Input(&ctx->input_text, "输入命令（例如 pull 或 /help）", input_opt);
    auto autocomplete_comp = Menu(&state.autocomplete_options, &state.autocomplete_selected);
    auto submenu_hint = Renderer([&state] {
        if (state.view_state != AppState::ViewState::Selection) return emptyElement();
        return vbox({
            text(state.selection_title) | color(Color::YellowLight) | bold,
            text("按 Enter 提交，Esc 返回主菜单") | dim
        });
    });

    auto main_container = Container::Vertical({
        submenu_hint,
        autocomplete_comp,
        input_comp
    });

    auto root_events = CatchEvent(main_container, [&state, ctx, update_autocomplete](Event event) -> bool {
        if (ctx->menu_state.mode == MenuMode::Submenu && event == Event::Escape) {
            ctx->menu_state.mode = MenuMode::Main;
            ctx->menu_state.pending_command.clear();
            ctx->menu_state.prompt.clear();
            state.selection_title = "请输入命令：";
            state.view_state = AppState::ViewState::Chat;
            ctx->input_text.clear();
            update_autocomplete();
            return true;
        }
        if (ctx->menu_state.mode == MenuMode::Main && state.is_autocomplete_open && (event == Event::Return || event == Event::Tab)) {
            if (!state.autocomplete_options.empty() &&
                state.autocomplete_selected >= 0 &&
                state.autocomplete_selected < (int)state.autocomplete_options.size()) {
                ctx->input_text = state.autocomplete_options[state.autocomplete_selected] + " ";
                update_autocomplete();
                return true;
            }
        }
        return false;
    });

    auto renderer = Renderer(root_events, [&state, input_comp, autocomplete_comp, submenu_hint] {
        auto output_element = state.captured_output.empty() ? text("主菜单已就绪，输入命令后按 Enter 执行。") : paragraph(state.captured_output);
        return render_chat_layout(
            state,
            TuiTheme{},
            input_comp->Render(),
            output_element,
            autocomplete_comp->Render(),
            submenu_hint->Render(),
            120,
            36
        ) | clear_under;
    });

    return renderer;
}

} // namespace tui
} // namespace shuati
