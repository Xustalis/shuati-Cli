#include "app_router.hpp"
#include "legacy_repl.hpp"
#include "commands.hpp"
#include "shuati/tui_app.hpp"
#include "shuati/utils/encoding.hpp"
#include "shuati/version.hpp"
#include <CLI/CLI.hpp>
#include <fmt/core.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace router {

AppRouter::AppRouter(const Config& config) : config_(config) {}

int AppRouter::route(int argc, char** argv) {
    // If not autostart and no args, print help instead of entering TUI/CUI
    if (argc == 1 && !config_.autostart_repl && config_.ui_mode != "tui") {
        fmt::print("Shuati CLI {}\n", current_version().to_string());
        fmt::print("用法: shuati <命令> [选项]\n");
        fmt::print("  运行 'shuati --help' 查看所有命令\n");
        fmt::print("  运行 'shuati repl' 进入交互模式\n");
        fmt::print("  (提示: 执行 'shuati config --autostart-repl on' 恢复自动启动)\n");
        return 0;
    }

    if (argc > 1) {
        // Allow 'shuati config' to always run so users can rescue their config
        if (std::string(argv[1]) != "config" && config_.ui_mode != "legacy") {
            fmt::print(fg(fmt::color::yellow), "[!] CLI commands are disabled in TUI mode.\n");
            fmt::print("    Please run without arguments to enter TUI, or run 'shuati config' to change ui_mode to 'legacy'.\n");
            return 1;
        }
        // Execute one-shot CLI command
        return execute_once_cli(argc, argv);
    }

    // No arguments interactive mode
    if (config_.ui_mode == "legacy") {
        return launch_legacy_repl();
    } else {
        return launch_tui_app();
    }
}

int AppRouter::execute_once_cli(int argc, char** argv) {
    CLI::App app{"shuati CLI"};
    cmd::CommandContext ctx;
    cmd::setup_commands(app, ctx);

#ifdef _WIN32
    int wargc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
        std::vector<std::string> args;
        for (int i = 0; i < wargc; i++) {
            args.push_back(utils::wide_to_utf8(wargv[i]));
        }
        LocalFree(wargv);
        
        std::vector<const char*> c_args;
        for (const auto& arg : args) {
            c_args.push_back(arg.c_str());
        }

        try {
            app.parse(c_args.size(), const_cast<char**>(c_args.data()));
        } catch (const CLI::ParseError& e) {
            return app.exit(e);
        }
    } else {
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            return app.exit(e);
        }
    }
#else
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
#endif
    return 0;
}

int AppRouter::launch_tui_app() {
    tui::TuiApp app;
    return app.run();
}

int AppRouter::launch_legacy_repl() {
    cli::run_legacy_repl();
    return 0;
}

} // namespace router
} // namespace shuati
