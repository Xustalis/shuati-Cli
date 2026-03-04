#include "shuati/boot_guard.hpp"
#include "shuati/config.hpp"
#include "shuati/database.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>

#include <fmt/core.h>
#include <fmt/color.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace shuati {

namespace fs = std::filesystem;

// --- Helper: Get AppData/shuati directory ---
fs::path BootGuard::get_history_file() {
    fs::path data_dir;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        data_dir = fs::path(path) / "shuati";
    } else {
        const char* appdata = std::getenv("APPDATA");
        data_dir = appdata ? fs::path(appdata) / "shuati" 
                           : fs::temp_directory_path() / "shuati";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        data_dir = fs::path(home) / ".config" / "shuati";
    } else {
        data_dir = fs::temp_directory_path() / "shuati"; 
    }
#endif
    
    if (!fs::exists(data_dir)) {
        fs::create_directories(data_dir);
    }
    return data_dir / "history.json";
}

std::vector<BootGuard::ProjectHistory> BootGuard::load_history() {
    std::vector<ProjectHistory> history;
    auto path = get_history_file();
    if (!fs::exists(path)) return history;

    try {
        std::ifstream f(path);
        nlohmann::json j;
        f >> j;

        for (const auto& item : j) {
            ProjectHistory h;
            h.path = item.value("path", "");
            h.last_accessed = item.value("last_accessed", "");
            h.access_count = item.value("access_count", 0);
            if (!h.path.empty() && fs::exists(h.path)) {
                history.push_back(h);
            }
        }
        
        // Sort by last accessed (descending)
        std::sort(history.begin(), history.end(), [](const ProjectHistory& a, const ProjectHistory& b) {
            return a.last_accessed > b.last_accessed;
        });

    } catch (...) {
        // Ignore errors, return empty or partial
    }
    return history;
}

void BootGuard::save_history(const std::vector<ProjectHistory>& history) {
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& h : history) {
            j.push_back({
                {"path", h.path},
                {"last_accessed", h.last_accessed},
                {"access_count", h.access_count}
            });
        }
        std::ofstream f(get_history_file());
        f << j.dump(4);
    } catch (...) {}
}

void BootGuard::record_history(const std::string& path) {
    auto abs_path = fs::absolute(path).string();
    auto history = load_history();
    
    // Check if exists
    auto it = std::find_if(history.begin(), history.end(), [&](const ProjectHistory& h) {
        return h.path == abs_path;
    });

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif
    std::stringstream ss;
    ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    
    if (it != history.end()) {
        it->last_accessed = ss.str();
        it->access_count++;
    } else {
        ProjectHistory h;
        h.path = abs_path;
        h.last_accessed = ss.str();
        h.access_count = 1;
        history.push_back(h);
    }

    // trim to 10
    if (history.size() > 10) {
        std::sort(history.begin(), history.end(), [](const ProjectHistory& a, const ProjectHistory& b) {
            return a.last_accessed > b.last_accessed;
        });
        history.resize(10);
    }

    save_history(history);
}

bool BootGuard::check() {
    // 1. Check if valid project
    auto root = Config::find_root();
    if (!root.empty()) {
        record_history(root.string());
        return true; // Already good
    }

    // 2. Not valid, show TUI
    using namespace ftxui;
    auto history = load_history();
    
    std::vector<std::string> options;
    options.push_back("🆕 初始化当前目录");
    
    for (const auto& h : history) {
        options.push_back(fmt::format("📂 {}", h.path));
    }
    options.push_back("❌ 退出");

    int selected = 0;
    auto menu = Menu(&options, &selected);
    auto screen = ScreenInteractive::TerminalOutput();

    auto component = CatchEvent(menu, [&](Event event) {
        if (event == Event::Return) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Escape || event == Event::Character('q')) {
            selected = options.size() - 1; // Exit
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    auto renderer = Renderer(component, [&] {
        auto cwd_info = text(fmt::format("当前目录: {}", fs::current_path().string()));
        auto warning = text("⚠️  未检测到 .shuati 项目文件夹") | color(Color::Yellow) | bold;
        auto hint = paragraph("请选择创建新项目，或跳转至最近使用的项目：");

        return vbox({
            cwd_info | dim,
            separator(),
            warning,
            hint,
            separator(),
            menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 10),
            separator(),
            text("使用 ↑/↓ 选择，Enter 确认") | dim
        }) | border | size(WIDTH, GREATER_THAN, 60);
    });

    screen.Loop(renderer);

    if (selected == 0) {
        // Init
        if (fs::exists(Config::DIR_NAME)) {
             // Race condition or manually created? check again
             return true; 
        }
        try {
            auto dir = fs::current_path() / Config::DIR_NAME;
            fs::create_directories(dir / "templates");
            fs::create_directories(dir / "problems");
            Config cfg;
            cfg.save(Config::config_path(fs::current_path()));
            Database db(Config::db_path(fs::current_path()).string());
            db.init_schema();
            
            fmt::print(fg(fmt::color::green), "[+] 初始化成功: {}\n", dir.string());
            record_history(fs::current_path().string());
            return true;
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 初始化失败: {}\n", e.what());
            return false;
        }
    } else if (selected == options.size() - 1) {
        return false; // Exit
    } else {
        // History jump
        // index 1 maps to history[0]
        int hist_idx = selected - 1;
        if (hist_idx >= 0 && hist_idx < history.size()) {
            try {
                fs::current_path(history[hist_idx].path);
                fmt::print(fg(fmt::color::green), "[+] 已跳转至: {}\n", history[hist_idx].path);
                record_history(history[hist_idx].path); // update timestamp
                return true;
            } catch (const std::exception& e) {
                 fmt::print(fg(fmt::color::red), "[!] 跳转失败: {}\n", e.what());
                 return false;
            }
        }
    }

    return false;
}

} // namespace shuati
