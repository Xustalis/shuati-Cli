#include "shuati/version.hpp"
#include "shuati/logger.hpp"
#include "shuati/boot_guard.hpp"
#include "shuati/config.hpp"
#include "app_router.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Enable VT processing globally to fix Replxx rendering and backspace ghosting 
    // on Windows Terminal and modern console hosts.
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            SetConsoleMode(hIn, dwMode | ENABLE_VIRTUAL_TERMINAL_INPUT);
        }
    }
#endif

    // Initialize Logger
    auto root = shuati::Config::find_root();
    if (!root.empty()) {
        shuati::Logger::instance().init((shuati::Config::data_dir(root) / "logs" / "shuati.log").string());
    }
    
    shuati::Logger::instance().info("Session started. Version: {}", shuati::current_version().to_string());

    if (!shuati::BootGuard::check()) return 0;

    auto config = root.empty() ? shuati::Config() : shuati::Config::load(shuati::Config::config_path(root));
    shuati::router::AppRouter router(config);

    return router.route(argc, argv);
}
