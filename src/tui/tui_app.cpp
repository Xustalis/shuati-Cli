#include "shuati/tui_app.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "shuati/version.hpp"
#include "shuati/database.hpp"

#include "tui_command_engine.hpp"
#include "command_specs.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace tui {

using namespace ftxui;

namespace {

#ifdef _WIN32
bool configure_windows_console_for_tui() {
    auto set_out = [](DWORD flags) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        return !!SetConsoleMode(h, mode | flags);
    };
    auto set_in = [](DWORD flags) {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        return !!SetConsoleMode(h, mode | flags);
    };
    auto clear_in = [](DWORD flags) {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        return !!SetConsoleMode(h, mode & ~flags);
    };

    bool ok = set_out(ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    ok = set_in(ENABLE_VIRTUAL_TERMINAL_INPUT) && ok;
    ok = set_in(ENABLE_EXTENDED_FLAGS) && ok;
    ok = clear_in(ENABLE_QUICK_EDIT_MODE) && ok;
    return ok;
}
#endif

std::string render_help_text() {
    auto specs = tui_command_specs();
    std::string out;
    out += "Commands\n\n";
    for (const auto& spec : specs) {
        out += "  " + spec.usage + "  --  " + spec.summary + "\n";
    }
    out += "\nShell: ls, cd, pwd, clear\n";
    return out;
}

void update_autocomplete(AppState& state, const std::vector<CommandSpec>& specs) {
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
            state.autocomplete_labels.push_back(spec.usage + "  --  " + spec.summary);
        }
    }

    if (exact && state.autocomplete_labels.size() == 1) {
        state.is_autocomplete_open = false;
    } else if (!state.autocomplete_labels.empty()) {
        state.is_autocomplete_open = true;
        state.autocomplete_selected = std::min(state.autocomplete_selected,
            static_cast<int>(state.autocomplete_labels.size()) - 1);
    } else {
        state.is_autocomplete_open = false;
    }
}

void load_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "No .shuati project found";
        state.config_state.loaded = true;
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    state.config_state.language       = cfg.language;
    state.config_state.editor         = cfg.editor;
    state.config_state.api_key        = cfg.api_key;
    state.config_state.model          = cfg.model;
    state.config_state.ui_mode        = cfg.ui_mode;
    state.config_state.autostart_repl = cfg.autostart_repl;
    state.config_state.focused_field  = 0;
    state.config_state.status_msg.clear();
    state.config_state.loaded         = true;
}

void save_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "No .shuati project found";
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    cfg.language       = state.config_state.language;
    cfg.editor         = state.config_state.editor;
    cfg.api_key        = state.config_state.api_key;
    cfg.model          = state.config_state.model;
    cfg.ui_mode        = state.config_state.ui_mode;
    cfg.autostart_repl = state.config_state.autostart_repl;
    cfg.save(Config::config_path(root));
    state.config_state.status_msg = "Saved.";
}

void load_list_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.list_state.error = "No .shuati project found";
        state.list_state.loaded = true;
        return;
    }
    try {
        Database db(Config::db_path(root).string());
        auto problems = db.get_all_problems();
        state.list_state.rows.clear();
        int tid = 1;
        for (const auto& p : problems) {
            ListState::Row row;
            row.tid        = tid++;
            row.id         = p.id;
            row.title      = p.title;
            row.difficulty = p.difficulty;
            row.status     = p.last_verdict.empty() ? "-" : p.last_verdict;
            {
                char buf[16] = {};
                std::time_t t = static_cast<std::time_t>(p.created_at);
                struct std::tm tm_val{};
#ifdef _WIN32
                localtime_s(&tm_val, &t);
#else
                localtime_r(&t, &tm_val);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
                row.date = buf;
            }
            state.list_state.rows.push_back(std::move(row));
        }
        state.list_state.selected = 0;
        state.list_state.error.clear();
    } catch (const std::exception& e) {
        state.list_state.error = std::string("DB error: ") + e.what();
    }
    state.list_state.loaded = true;
}

} // namespace

int TuiApp::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    configure_windows_console_for_tui();
#endif

    auto screen = ScreenInteractive::Fullscreen();
    TuiTheme theme;
    AppState state;

    std::string version = current_version().to_string();
    auto specs = tui_command_specs();

    bool should_quit = false;
    auto quit = [&] {
        should_quit = true;
        screen.ExitLoopClosure()();
    };

    state.post_task = [&screen](std::function<void()> task) {
        screen.Post(std::move(task));
    };

    auto run_command = [&](const std::string& line) {
        std::string trimmed = line;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) return;

        state.append(LineType::Input, trimmed);

        std::string normalized = trimmed;
        if (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());

        auto args = parse_command_line(normalized);
        if (args.size() <= 1) return;

        std::string base_cmd = args[1];

        // Built-in immediate commands
        if (base_cmd == "?" || base_cmd == "help") {
            state.append(LineType::Heading, "Help");
            state.append_output_lines(render_help_text());
            return;
        }
        if (base_cmd == "clear" || base_cmd == "cls") {
            state.buffer.clear();
            state.scroll_offset = 0;
            return;
        }
        if (base_cmd == "exit" || base_cmd == "quit") {
            quit();
            return;
        }

        // Subview routing: bare /config -> full-screen config editor
        if (base_cmd == "config" && args.size() == 2) {
            load_config_state(state);
            state.view_mode = ViewMode::ConfigView;
            return;
        }

        // Subview routing: bare /list -> full-screen problem browser
        if (base_cmd == "list" && args.size() == 2) {
            load_list_state(state);
            state.view_mode = ViewMode::ListView;
            return;
        }

        // Subview routing: /hint <id> -> full-screen scrollable hint
        if (base_cmd == "hint") {
            state.hint_state = HintState{};
            state.hint_state.loading = true;
            state.view_mode = ViewMode::HintView;

            std::thread([&screen, &state, args = std::move(args), base_cmd]() mutable {
                std::string output = tui_execute_command_capture(args, base_cmd);
                screen.Post([&state, output = std::move(output)] {
                    state.hint_state.loading = false;
                    if (output.empty()) {
                        state.hint_state.error = "No hint returned.";
                        return;
                    }
                    std::string::size_type start = 0;
                    while (start < output.size()) {
                        auto nl = output.find('\n', start);
                        if (nl == std::string::npos) {
                            state.hint_state.lines.push_back(output.substr(start));
                            break;
                        }
                        state.hint_state.lines.push_back(output.substr(start, nl - start));
                        start = nl + 1;
                    }
                    state.hint_state.scroll_offset = 0;
                });
            }).detach();
            return;
        }

        // Generic command: run async with separator header/footer
        std::string sep_header = "--- " + base_cmd + " ---";
        state.append(LineType::Separator, sep_header);

        state.is_running = true;
        state.active_command = base_cmd;

        std::thread([&screen, &state, args = std::move(args), base_cmd]() mutable {
            std::string output = tui_execute_command_capture(args, base_cmd);
            screen.Post([&state, output = std::move(output), base_cmd] {
                if (!output.empty()) {
                    state.append_output_lines(output);
                }
                std::string footer(static_cast<size_t>(
                    std::max(8, static_cast<int>(base_cmd.size()) + 10)), '-');
                state.append(LineType::Separator, footer);
                state.is_running = false;
                state.active_command.clear();
            });
        }).detach();
    };

    // --- FTXUI Component Tree ---

    InputOption input_opt;
    input_opt.on_change = [&] { update_autocomplete(state, specs); };
    input_opt.on_enter = [&] {
        if (state.input.empty()) return;
        std::string cmd = std::move(state.input);
        state.input.clear();
        state.is_autocomplete_open = false;
        run_command(cmd);
    };

    auto input_comp = Input(&state.input, "Type a command...", input_opt);
    auto autocomplete_comp = Menu(&state.autocomplete_labels, &state.autocomplete_selected);

    auto main_container = Container::Vertical({
        autocomplete_comp,
        input_comp,
    });

    auto event_handler = CatchEvent(main_container, [&](Event event) -> bool {
        if (event == Event::Character('\x03')) {
            quit();
            return true;
        }

        // --- Subview event handling ---

        // ConfigView key handling
        if (state.view_mode == ViewMode::ConfigView) {
            auto& cs = state.config_state;
            if (event == Event::Escape) {
                state.view_mode = ViewMode::Main;
                return true;
            }
            if (event == Event::ArrowUp) {
                cs.focused_field = std::max(0, cs.focused_field - 1);
                return true;
            }
            if (event == Event::ArrowDown) {
                cs.focused_field = std::min(ConfigState::TOTAL_ITEMS - 1, cs.focused_field + 1);
                return true;
            }
            if (event == Event::Return) {
                if (cs.focused_field == ConfigState::SAVE_INDEX) {
                    save_config_state(state);
                    return true;
                }
                if (cs.focused_field == ConfigState::CANCEL_INDEX) {
                    state.view_mode = ViewMode::Main;
                    return true;
                }
                // Toggle fields
                if (cs.focused_field == 0) {
                    cs.language = (cs.language == "cpp") ? "python" : "cpp";
                    return true;
                }
                if (cs.focused_field == 4) {
                    cs.ui_mode = (cs.ui_mode == "tui") ? "legacy" : "tui";
                    return true;
                }
                if (cs.focused_field == 5) {
                    cs.autostart_repl = !cs.autostart_repl;
                    return true;
                }
                return true;
            }
            // Text input for editable fields (1=editor, 2=api_key, 3=model)
            if (cs.focused_field >= 1 && cs.focused_field <= 3) {
                auto target = [&]() -> std::string* {
                    switch (cs.focused_field) {
                    case 1: return &cs.editor;
                    case 2: return &cs.api_key;
                    case 3: return &cs.model;
                    default: return nullptr;
                    }
                }();
                if (target) {
                    if (event == Event::Backspace) {
                        if (!target->empty()) target->pop_back();
                        return true;
                    }
                    if (event.is_character()) {
                        *target += event.character();
                        return true;
                    }
                }
            }
            return true;
        }

        // ListView key handling
        if (state.view_mode == ViewMode::ListView) {
            auto& ls = state.list_state;
            if (event == Event::Escape) {
                state.view_mode = ViewMode::Main;
                return true;
            }
            if (event == Event::ArrowUp) {
                ls.selected = std::max(0, ls.selected - 1);
                return true;
            }
            if (event == Event::ArrowDown) {
                ls.selected = std::min(static_cast<int>(ls.rows.size()) - 1, ls.selected + 1);
                return true;
            }
            if (event == Event::Character('f')) {
                if (ls.filter == "all") ls.filter = "AC";
                else if (ls.filter == "AC") ls.filter = "WA";
                else ls.filter = "all";
                return true;
            }
            return true;
        }

        // HintView key handling
        if (state.view_mode == ViewMode::HintView) {
            auto& hs = state.hint_state;
            if (event == Event::Escape) {
                state.view_mode = ViewMode::Main;
                return true;
            }
            int max_scroll = std::max(0, static_cast<int>(hs.lines.size()) - 10);
            if (event == Event::ArrowUp) {
                hs.scroll_offset = std::max(0, hs.scroll_offset - 1);
                return true;
            }
            if (event == Event::ArrowDown) {
                hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 1);
                return true;
            }
            if (event == Event::PageUp ||
                event == Event::Special("\x1B[5~")) {
                hs.scroll_offset = std::max(0, hs.scroll_offset - 10);
                return true;
            }
            if (event == Event::PageDown ||
                event == Event::Special("\x1B[6~")) {
                hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 10);
                return true;
            }
            return true;
        }

        // --- Main view event handling ---

        if (state.is_autocomplete_open) {
            if (event == Event::Tab) {
                if (state.autocomplete_selected >= 0 &&
                    state.autocomplete_selected < static_cast<int>(state.autocomplete_commands.size())) {
                    state.input = state.autocomplete_commands[state.autocomplete_selected] + " ";
                    update_autocomplete(state, specs);
                }
                return true;
            }
        }

        // Buffer scroll
        if (event == Event::Special("\x1B[5~") || event == Event::PageUp) {
            int max_scroll = std::max(0, static_cast<int>(state.buffer.size()) - 10);
            state.scroll_offset = std::min(state.scroll_offset + 5, max_scroll);
            return true;
        }
        if (event == Event::Special("\x1B[6~") || event == Event::PageDown) {
            state.scroll_offset = std::max(state.scroll_offset - 5, 0);
            return true;
        }

        return false;
    });

    auto renderer = Renderer(event_handler, [&] {
        auto top_bar = render_top_bar(theme, version);
        auto bottom_bar = render_bottom_bar(theme, state.is_running);

        // Subview rendering
        if (state.view_mode == ViewMode::ConfigView) {
            return vbox({
                top_bar,
                separatorLight() | color(theme.dim_color),
                render_config_view(state, theme) | flex,
                separatorLight() | color(theme.dim_color),
                bottom_bar,
            });
        }
        if (state.view_mode == ViewMode::ListView) {
            return vbox({
                top_bar,
                separatorLight() | color(theme.dim_color),
                render_list_view(state, theme) | flex,
                separatorLight() | color(theme.dim_color),
                bottom_bar,
            });
        }
        if (state.view_mode == ViewMode::HintView) {
            return vbox({
                top_bar,
                separatorLight() | color(theme.dim_color),
                render_hint_view(state, theme) | flex,
                separatorLight() | color(theme.dim_color),
                bottom_bar,
            });
        }

        // Main view
        auto buffer_content = render_buffer(state, theme);
        auto buffer_area = buffer_content | focusPositionRelative(0, 1) | frame | flex;

        Element ac_element = emptyElement();
        if (state.is_autocomplete_open && !state.autocomplete_labels.empty()) {
            ac_element = autocomplete_comp->Render()
                | vscroll_indicator | frame
                | size(HEIGHT, LESS_THAN, 8)
                | borderStyled(ROUNDED)
                | color(theme.dim_color);
        }

        auto input_line = hbox({
            text(state.is_running ? "  " : "> ") | bold | color(theme.prompt_color),
            input_comp->Render() | flex,
        });

        return vbox({
            top_bar,
            separatorLight() | color(theme.dim_color),
            buffer_area,
            separatorLight() | color(theme.dim_color),
            ac_element,
            input_line,
            separatorLight() | color(theme.dim_color),
            bottom_bar,
        });
    });

    screen.Loop(renderer);
    return 0;
}

} // namespace tui
} // namespace shuati
