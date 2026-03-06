#pragma once

#include "shuati/config.hpp"

namespace shuati {
namespace router {

class AppRouter {
public:
    explicit AppRouter(const Config& config);

    // Main entry point for routing
    int route(int argc, char** argv);

private:
    int execute_once_cli(int argc, char** argv);
    int launch_tui_app();
    int launch_legacy_repl();

    Config config_;
};

} // namespace router
} // namespace shuati
