#include "tui_command_engine.hpp"

namespace shuati {
namespace tui {

std::vector<std::string> tui_command_candidates() {
    return {
        "/help", "/ls", "/dir", "/cd", "/pwd", "/clear",
        "/init", "/info", "/pull", "/new", "/solve", "/list", "/delete",
        "/record", "/test", "/hint", "/view", "/clean", "/login",
        "/config", "/repl", "/tui", "/exit"
    };
}

std::vector<std::string> tui_cli_command_candidates() {
    return {
        "init", "info", "pull", "new", "solve", "list", "delete",
        "record", "test", "hint", "view", "clean", "login",
        "config", "repl", "tui", "exit"
    };
}

bool tui_is_selection_command(const std::string& base_cmd, size_t argc) {
    return (base_cmd == "list" || base_cmd == "solve") && argc == 2;
}

} // namespace tui
} // namespace shuati
