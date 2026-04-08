#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../store/app_state.hpp"
#include "../worker/task_runner.hpp"
#include "../../tui/command_specs.hpp"

// Forward declare TuiTheme (defined in tui_views.hpp header)
namespace shuati { namespace tui { struct TuiTheme; } }

namespace shuati {
namespace tui {
namespace views {

/// Shared context passed to every view's factory function.
struct ViewContext {
    AppState& state;
    const TuiTheme& theme;
    TaskRunner& runner;
    ftxui::ScreenInteractive& screen;

    /// Execute a slash-command.  If record_input is true, the command appears
    /// in the output buffer and command history.
    std::function<void(const std::string& cmd, bool record_input)> run_command;

    /// Specs for autocomplete.
    const std::vector<CommandSpec>& specs;
};

}  // namespace views
}  // namespace tui
}  // namespace shuati
