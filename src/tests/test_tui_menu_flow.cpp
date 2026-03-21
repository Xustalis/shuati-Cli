#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../tui/store/app_state.hpp"

static std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return s;
}

static std::filesystem::path find_repo_root() {
    auto p = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(p / "CMakeLists.txt")) return p;
        p = p.parent_path();
    }
    return std::filesystem::current_path();
}

int main() {
    using namespace shuati::tui;

    // AppState buffer operations
    AppState state;
    assert(state.buffer.empty());
    assert(!state.is_running);
    assert(state.scroll_offset == 0);

    state.append(LineType::System, "hello");
    assert(state.buffer.size() == 1);
    assert(state.buffer[0].type == LineType::System);
    assert(state.buffer[0].text == "hello");

    state.append(LineType::Input, "/pull https://example.com");
    assert(state.buffer.size() == 2);
    assert(state.buffer[1].type == LineType::Input);

    state.append_output_lines("line1\nline2\nline3");
    assert(state.buffer.size() == 5);
    assert(state.buffer[2].text == "line1");
    assert(state.buffer[3].text == "line2");
    assert(state.buffer[4].text == "line3");

    state.append(LineType::Error, "error msg");
    assert(state.buffer.back().type == LineType::Error);

    state.append(LineType::Heading, "heading");
    assert(state.buffer.back().type == LineType::Heading);

    // Empty string should not add lines
    if (auto n = state.buffer.size(); true) {
        state.append_output_lines("");
        if (state.buffer.size() != n) { std::cerr << "Empty append_output_lines added lines!\n"; return 1; }
    }

    // Verify no forbidden strings in source
    auto root = find_repo_root();
    const std::vector<std::filesystem::path> scan_dirs = {
        root / "src",
        root / "include",
        root / "scripts"
    };
    for (const auto& dir : scan_dirs) {
        if (!std::filesystem::exists(dir)) continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".hpp" && ext != ".h" && ext != ".ps1" && ext != ".md") continue;
            const auto content = read_all(entry.path());
            const std::string forbidden = std::string("Cl") + "aude";
            assert(content.find(forbidden) == std::string::npos);
        }
    }

    const auto tui_app_content = read_all(root / "src" / "tui" / "tui_app.cpp");
    const std::string welcome = std::string("Welcome to ") + "Shuati TUI!";
    assert(tui_app_content.find(welcome) == std::string::npos);

    // Command history
    {
        AppState hs;
        assert(hs.command_history.empty());
        assert(hs.history_index == -1);

        hs.push_history("/pull https://example.com");
        assert(hs.command_history.size() == 1);
        assert(hs.history_index == -1);

        hs.push_history("/list");
        assert(hs.command_history.size() == 2);

        // Duplicate should not add
        hs.push_history("/list");
        assert(hs.command_history.size() == 2);
    }

    {
        AppState ls;
        assert(ls.list_state.status_filter == "all");
    }

    return 0;
}
