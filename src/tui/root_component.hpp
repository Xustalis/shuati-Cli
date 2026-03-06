#pragma once
#include <ftxui/component/component.hpp>
#include "store/app_state.hpp"

namespace shuati {
namespace tui {

ftxui::Component RootComponent(AppState& state, std::function<void()> on_exit);

// Page Renderers
ftxui::Component WelcomePage(AppState& state);
ftxui::Component SolveWorkspacePage(AppState& state);
ftxui::Component ProblemListPage(AppState& state);
ftxui::Component ReportViewPage(AppState& state);

} // namespace tui
} // namespace shuati
