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
    
    // Enable VT output processing for ANSI color rendering (Windows Terminal).
    // NOTE: Do NOT set ENABLE_VIRTUAL_TERMINAL_INPUT on stdin — replxx handles
    // raw keyboard input itself, and setting that flag causes backspace to emit
    // ^H instead of being processed correctly.
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif

    // Initialize Logger
    auto root = shuati::Config::find_root();
    if (!root.empty()) {
        shuati::Logger::instance().init((shuati::Config::data_dir(root) / "logs" / "shuati.log").string());
    }
    
    shuati::Logger::instance().info("Session started. Version: {}", shuati::current_version().to_string());

    if (!shuati::BootGuard::check(argc)) return 0;

    auto config = root.empty() ? shuati::Config() : shuati::Config::load(shuati::Config::config_path(root));
    shuati::router::AppRouter router(config);

    return router.route(argc, argv);
}
