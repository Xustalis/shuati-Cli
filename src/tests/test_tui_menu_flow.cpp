#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../tui/store/app_state.hpp"
#include "../tui/tui_menu_model.hpp"

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

    AppState app_state;
    assert(app_state.current_page_index == static_cast<int>(PageID::SolveWorkspace));
    assert(app_state.toast_message.empty());

    MenuState menu_state;
    process_menu_input(menu_state, "pull", [](const std::vector<std::string>&, const std::string&) { return std::string("x"); });
    assert(menu_state.mode == MenuMode::Submenu);
    assert(menu_state.prompt == "请输入题目链接：");

    process_menu_input(menu_state, "https://leetcode.com/problems/two-sum", [](const std::vector<std::string>& args, const std::string& cmd) {
        assert(cmd == "pull");
        assert(args.size() >= 3);
        return std::string("pull finished");
    });
    assert(menu_state.mode == MenuMode::Main);
    assert(menu_state.output == "pull finished");

    process_menu_input(menu_state, "help", [](const std::vector<std::string>&, const std::string&) {
        return std::string("菜单输出UTF-8正常");
    });
    assert(menu_state.mode == MenuMode::Main);
    assert(menu_state.output.find("UTF-8") != std::string::npos);
    assert(menu_state.output.find("\x1b") == std::string::npos);
    const std::string invalid_utf8_mark = std::string("\xEF\xBF\xBD");
    assert(menu_state.output.find(invalid_utf8_mark) == std::string::npos);

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
    return 0;
}
