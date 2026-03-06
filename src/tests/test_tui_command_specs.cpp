#include <cassert>

#include "../tui/command_specs.hpp"

using namespace shuati::tui;

int main() {
    auto specs = tui_command_specs();
    assert(!specs.empty());
    bool has_pull = false;
    bool has_solve = false;
    bool has_exit = false;
    for (const auto& s : specs) {
        if (s.slash == "/pull") has_pull = true;
        if (s.slash == "/solve") has_solve = true;
        if (s.slash == "/exit") has_exit = true;
    }
    assert(has_pull && has_solve && has_exit);
    return 0;
}

