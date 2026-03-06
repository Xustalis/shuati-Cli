#include "tui_menu_model.hpp"

#include <sstream>
#include <unordered_map>

namespace shuati {
namespace tui {

std::vector<std::string> parse_command_line(const std::string& line) {
    std::stringstream ss(line);
    std::string part;
    std::vector<std::string> args;
    args.push_back("shuati");
    while (ss >> part) {
        args.push_back(part);
    }
    return args;
}

std::string required_prompt_for(const std::string& base_cmd) {
    static const std::unordered_map<std::string, std::string> prompts = {
        {"pull", "请输入题目链接："},
        {"new", "请输入题目标题："},
        {"submit", "请输入题目 ID："},
        {"test", "请输入题目 ID："},
        {"hint", "请输入题目 ID："},
        {"view", "请输入题目 ID："},
        {"login", "请输入平台名称："},
        {"delete", "请输入题目 ID："}
    };
    auto it = prompts.find(base_cmd);
    if (it == prompts.end()) return "";
    return it->second;
}

void process_menu_input(MenuState& state, const std::string& line, const CommandRunner& runner) {
    if (line.empty()) return;

    if (state.mode == MenuMode::Submenu) {
        auto args = parse_command_line(state.pending_command + " " + line);
        state.output = runner(args, state.pending_command);
        state.mode = MenuMode::Main;
        state.pending_command.clear();
        state.prompt.clear();
        return;
    }

    std::string trimmed = line;
    if (!trimmed.empty() && trimmed.front() == '/') {
        trimmed.erase(trimmed.begin());
    }
    auto args = parse_command_line(trimmed);
    if (args.size() <= 1) return;
    const std::string& base_cmd = args[1];

    if (base_cmd == "exit" || base_cmd == "quit") {
        state.should_exit = true;
        return;
    }
    if (base_cmd == "clear" || base_cmd == "cls") {
        state.output.clear();
        return;
    }

    auto prompt = required_prompt_for(base_cmd);
    if (!prompt.empty() && args.size() == 2) {
        state.mode = MenuMode::Submenu;
        state.pending_command = base_cmd;
        state.prompt = prompt;
        return;
    }

    state.output = runner(args, base_cmd);
    state.mode = MenuMode::Main;
    state.pending_command.clear();
    state.prompt.clear();
}

} // namespace tui
} // namespace shuati
