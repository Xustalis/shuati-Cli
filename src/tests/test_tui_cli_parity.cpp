#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "../tui/tui_command_engine.hpp"

int main() {
    using namespace shuati::tui;

    const std::set<std::string> expected_cli = {
        "init", "info", "pull", "new", "solve", "list", "delete",
        "submit", "test", "hint", "view", "clean", "login",
        "config", "repl", "tui", "exit"
    };

    auto cli_candidates = tui_cli_command_candidates();
    const std::set<std::string> actual_cli(cli_candidates.begin(), cli_candidates.end());

    assert(actual_cli == expected_cli);

    auto all_candidates = tui_command_candidates();
    for (const auto& cmd : expected_cli) {
        const auto slash = "/" + cmd;
        assert(std::find(all_candidates.begin(), all_candidates.end(), slash) != all_candidates.end());
    }

    assert(std::find(all_candidates.begin(), all_candidates.end(), "/help") != all_candidates.end());
    assert(std::find(all_candidates.begin(), all_candidates.end(), "/ls") != all_candidates.end());
    assert(std::find(all_candidates.begin(), all_candidates.end(), "/pwd") != all_candidates.end());
    assert(tui_is_selection_command("list", 2));
    assert(tui_is_selection_command("solve", 2));
    assert(!tui_is_selection_command("solve", 3));
    assert(!tui_is_selection_command("view", 2));

    std::cout << "TUI/CLI parity checks passed." << std::endl;
    return 0;
}
