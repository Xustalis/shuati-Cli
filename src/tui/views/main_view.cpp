#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/tui_views.hpp"
#include "../command_specs.hpp"
#include "../tui_command_engine.hpp"
#include <algorithm>

namespace shuati {
namespace tui {
namespace views {

using namespace ftxui;

namespace {

std::string render_help_text() {
    auto specs = tui_command_specs();
    std::string out;
    out += "\xe5\x85\xa8\xe9\x83\xa8\xe6\x8c\x87\xe4\xbb\xa4\n\n";
    for (const auto& spec : specs)
        out += "  " + spec.usage + "  --  " + spec.summary + "\n";
    out += "\n\xe5\x86\x85\xe7\xbd\xae\xe5\x91\xbd\xe4\xbb\xa4: ls, cd, pwd, clear\n";
    return out;
}

void update_autocomplete(AppState& state,
                         const std::vector<CommandSpec>& specs) {
    if (state.input.empty() || state.input[0] != '/') {
        state.is_autocomplete_open = false;
        return;
    }
    std::string prefix = state.input;
    auto space_pos = prefix.find(' ');
    if (space_pos != std::string::npos) {
        state.is_autocomplete_open = false;
        return;
    }
    state.autocomplete_labels.clear();
    state.autocomplete_commands.clear();
    bool exact = false;
    for (const auto& spec : specs) {
        if (spec.slash.starts_with(prefix)) {
            if (spec.slash == prefix) exact = true;
            state.autocomplete_commands.push_back(spec.slash);
            state.autocomplete_labels.push_back(
                spec.slash + "  " + spec.summary);
        }
    }
    if (exact && state.autocomplete_labels.size() == 1)
        state.is_autocomplete_open = false;
    else if (!state.autocomplete_labels.empty()) {
        state.is_autocomplete_open = true;
        state.autocomplete_selected = std::clamp(
            state.autocomplete_selected, 0,
            static_cast<int>(state.autocomplete_labels.size()) - 1);
    } else
        state.is_autocomplete_open = false;
}

std::string get_input_hint(const std::string& input) {
    if (input.empty())
        return "\xe8\xbe\x93\xe5\x85\xa5 / \xe5\xbc\x80\xe5\xa4\xb4\xe7\x9a\x84\xe5\x91\xbd\xe4\xbb\xa4, \xe4\xbe\x8b\xe5\xa6\x82 /help \xe6\x9f\xa5\xe7\x9c\x8b\xe5\xb8\xae\xe5\x8a\xa9";
    auto start = input.find_first_not_of(' ');
    if (start == std::string::npos || input[start] != '/')
        return "\xe5\x91\xbd\xe4\xbb\xa4\xe9\x9c\x80\xe4\xbb\xa5 / \xe5\xbc\x80\xe5\xa4\xb4";
    return "";
}

}  // namespace

Component CreateMainView(ViewContext& ctx, int& cursor_pos) {
    auto& state = ctx.state;
    auto& theme = ctx.theme;
    auto& specs = ctx.specs;

    InputOption opt;
    opt.cursor_position = &cursor_pos;
    opt.on_change = [&] { update_autocomplete(state, specs); };
    opt.on_enter = [&] {
        if (state.input.empty()) return;
        std::string cmd = std::move(state.input);
        state.input.clear();
        cursor_pos = 0;
        state.is_autocomplete_open = false;
        state.history_index = -1;
        ctx.run_command(cmd, true);
    };
    auto input_comp = Input(&state.input, "", opt);

    auto renderer = Renderer(input_comp, [&, input_comp] {
        auto buffer_content = render_buffer(state, theme);
        auto buffer_area = hbox({
            text("  "),
            buffer_content | focusPositionRelative(0, 1) | frame | flex,
            text("  "),
        }) | flex;

        Element ac_element = emptyElement();
        if (state.is_autocomplete_open &&
            !state.autocomplete_labels.empty()) {
            Elements ac_items;
            for (int i = 0;
                 i < static_cast<int>(state.autocomplete_labels.size()); i++) {
                auto item = text("  " + state.autocomplete_labels[i] + "  ");
                if (i == state.autocomplete_selected)
                    item = item | bold | bgcolor(theme.selected_bg) |
                           color(theme.accent_color);
                else
                    item = item | color(theme.dim_color);
                ac_items.push_back(item);
            }
            ac_element = vbox(std::move(ac_items)) |
                         size(HEIGHT, LESS_THAN, 8) |
                         borderStyled(ROUNDED) | color(theme.border_color);
        }

        std::string hint = get_input_hint(state.input);
        Element hint_line = hint.empty()
                                ? emptyElement()
                                : text("  " + hint) | color(theme.dim_color);

        Element input_line;
        if (state.is_running) {
            std::string busy_msg =
                "  [" + state.active_command + " \xe8\xbf\x90\xe8\xa1\x8c\xe4\xb8\xad... \xe6\x8c\x89 Esc \xe5\x8f\x96\xe6\xb6\x88]";
            input_line = hbox({
                text(" ") | color(theme.dim_color),
                text("\xe2\x8f\xb3") | color(theme.accent_color),
                text(busy_msg) | color(theme.dim_color),
            });
        } else {
            input_line = hbox({
                text(" ") | color(theme.dim_color),
                text("> ") | bold | color(theme.prompt_color),
                input_comp->Render() | flex,
            });
        }

        return vbox({buffer_area, ac_element,
                     separatorLight() | color(theme.border_color), hint_line,
                     input_line});
    });

    return CatchEvent(renderer, [&](Event event) -> bool {
        if (state.view_mode != ViewMode::Main) return false;

        // Escape to cancel running command
        if (event == Event::Escape && state.is_running) {
            ctx.runner.cancel();
            state.is_running = false;
            state.active_command.clear();
            state.pending_commands.clear();
            state.append(LineType::SystemLog,
                         "[!] \xe5\x91\xbd\xe4\xbb\xa4\xe5\xb7\xb2\xe5\xbc\xba\xe5\x88\xb6\xe4\xb8\xad\xe6\x96\xad (Esc)");
            return true;
        }
        // Ctrl+L clear
        if (event == Event::Character('\x0C')) {
            state.buffer.clear();
            state.scroll_offset = 0;
            return true;
        }
        // Ctrl+U clear input
        if (event == Event::Character('\x15')) {
            state.input.clear();
            cursor_pos = 0;
            state.is_autocomplete_open = false;
            return true;
        }
        // Autocomplete navigation
        if (state.is_autocomplete_open) {
            if (event == Event::ArrowDown) {
                state.autocomplete_selected = std::min(
                    (int)state.autocomplete_labels.size() - 1,
                    state.autocomplete_selected + 1);
                return true;
            }
            if (event == Event::ArrowUp) {
                state.autocomplete_selected =
                    std::max(0, state.autocomplete_selected - 1);
                return true;
            }
            if (event == Event::Tab || event == Event::Return) {
                if (state.autocomplete_selected >= 0 &&
                    state.autocomplete_selected <
                        (int)state.autocomplete_commands.size()) {
                    std::string cmd =
                        state.autocomplete_commands[state.autocomplete_selected];
                    // Direct-jump logic for view commands
                    bool jumped = false;
                    if (cmd == "/config") {
                        load_config_state(state);
                        state.push_view(ViewMode::ConfigView);
                        jumped = true;
                    } else if (cmd == "/list") {
                        load_list_state(state);
                        state.push_view(ViewMode::ListView);
                        jumped = true;
                    } else if (cmd == "/dashboard" || cmd == "/dash") {
                        load_dashboard_state(state);
                        state.push_view(ViewMode::DashboardView);
                        jumped = true;
                    } else if (cmd == "/menu") {
                        state.menu_state.init_from_specs();
                        state.push_view(ViewMode::MenuView);
                        jumped = true;
                    }
                    if (jumped) {
                        state.input.clear();
                        cursor_pos = 0;
                        state.is_autocomplete_open = false;
                    } else {
                        state.input = cmd + " ";
                        cursor_pos = (int)state.input.size();
                        update_autocomplete(state, specs);
                    }
                }
                return true;
            }
            if (event == Event::Escape) {
                state.is_autocomplete_open = false;
                return true;
            }
        } else if (!state.command_history.empty()) {
            if (event == Event::ArrowUp) {
                if (state.history_index == -1) {
                    state.history_stash = state.input;
                    state.history_index =
                        (int)state.command_history.size() - 1;
                } else if (state.history_index > 0)
                    state.history_index--;
                state.input = state.command_history[state.history_index];
                cursor_pos = (int)state.input.size();
                return true;
            }
            if (event == Event::ArrowDown && state.history_index >= 0) {
                state.history_index++;
                if (state.history_index >=
                    (int)state.command_history.size()) {
                    state.history_index = -1;
                    state.input = state.history_stash;
                    state.history_stash.clear();
                } else
                    state.input =
                        state.command_history[state.history_index];
                cursor_pos = (int)state.input.size();
                return true;
            }
        }
        // Page scroll
        if (event == Event::PageUp) {
            state.scroll_offset = std::min(
                state.scroll_offset + 5,
                std::max(0, (int)state.buffer.size() - 20));
            return true;
        }
        if (event == Event::PageDown) {
            state.scroll_offset =
                std::max(state.scroll_offset - 5, 0);
            return true;
        }
        return false;
    });
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
