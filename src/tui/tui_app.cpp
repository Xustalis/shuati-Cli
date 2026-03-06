#include "shuati/tui_app.hpp"
#include "shuati/config.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/utils/encoding.hpp"
#include "commands.hpp"
#include "legacy_repl.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include "root_component.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <fmt/core.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace tui {

namespace fs = std::filesystem;
using namespace ftxui;

namespace {

struct SharedLog {
    std::mutex mu;
    std::string text;
    bool running = false;
};

class LogStreambuf final : public std::streambuf {
public:
    explicit LogStreambuf(SharedLog& log, std::function<void()> on_update) 
        : log_(log), on_update_(std::move(on_update)) {}

protected:
    int overflow(int ch) override {
        if (ch == EOF) return EOF;
        {
            std::lock_guard<std::mutex> lk(log_.mu);
            log_.text.push_back(static_cast<char>(ch));
        }
        if (on_update_) on_update_();
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        if (n <= 0) return 0;
        {
            std::lock_guard<std::mutex> lk(log_.mu);
            log_.text.append(s, static_cast<size_t>(n));
        }
        if (on_update_) on_update_();
        return n;
    }

private:
    SharedLog& log_;
    std::function<void()> on_update_;
};

struct CaptureScope {
    explicit CaptureScope(SharedLog& log, std::function<void()> on_update)
        : log_(log), 
          out_buf_(log, on_update), 
          err_buf_(log, on_update), 
          old_out_(std::cout.rdbuf()), 
          old_err_(std::cerr.rdbuf()) {
        std::cout.rdbuf(&out_buf_);
        std::cerr.rdbuf(&err_buf_);
    }
    ~CaptureScope() {
        std::cout.rdbuf(old_out_);
        std::cerr.rdbuf(old_err_);
    }

private:
    SharedLog& log_;
    LogStreambuf out_buf_;
    LogStreambuf err_buf_;
    std::streambuf* old_out_;
    std::streambuf* old_err_;
};

std::string read_log_copy(SharedLog& log) {
    std::lock_guard<std::mutex> lk(log.mu);
    return log.text;
}

void clear_log(SharedLog& log) {
    std::lock_guard<std::mutex> lk(log.mu);
    log.text.clear();
}

std::string list_dir_text(const fs::path& p) {
    std::ostringstream oss;
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        oss << "[!] Directory not found: " << p.string() << "\n";
        return oss.str();
    }
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        if (ec) break;
        const auto path = entry.path();
        oss << (entry.is_directory() ? "[D] " : "    ");
        oss << path.filename().string() << "\n";
    }
    return oss.str();
}

} // namespace



int TuiApp::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    
    // Core state of the application
    AppState state;
    
    // Setup quit closure
    bool should_quit = false;
    auto on_exit = [&]() {
        should_quit = true;
        screen.ExitLoopClosure()();
    };

    // Setup thread-safe dispatcher
    state.post_task = [&screen](std::function<void()> task) {
        screen.Post(task);
    };

    // Build the application UI from RootComponent
    auto root = RootComponent(state, on_exit);

    // Run the application blocking loop
    screen.Loop(root);

    return 0;
}

} // namespace tui
} // namespace shuati
