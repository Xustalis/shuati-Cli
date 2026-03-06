#pragma once

#include <functional>
#include <string>
#include <vector>

namespace shuati {
namespace tui {

enum class MenuMode {
    Main,
    Submenu
};

struct MenuState {
    MenuMode mode = MenuMode::Main;
    bool should_exit = false;
    std::string pending_command;
    std::string prompt;
    std::string output;
};

using CommandRunner = std::function<std::string(const std::vector<std::string>&, const std::string&)>;

std::vector<std::string> parse_command_line(const std::string& line);
std::string required_prompt_for(const std::string& base_cmd);
void process_menu_input(MenuState& state, const std::string& line, const CommandRunner& runner);

} // namespace tui
} // namespace shuati
