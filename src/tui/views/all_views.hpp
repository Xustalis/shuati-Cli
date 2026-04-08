#pragma once
/// All view factory functions for the TUI.
///
/// Each function returns an ftxui::Component that handles its own rendering
/// and event processing.  The ViewContext provides shared dependencies.

#include "view_base.hpp"
#include <ftxui/component/component.hpp>

namespace shuati {
namespace tui {
namespace views {

// ── View factory functions ────────────────────────────────────────
// Each returns a Component.  The Component internally handles its render()
// and event processing.  State is accessed via ctx references.

ftxui::Component CreateMainView(ViewContext& ctx, int& cursor_pos);
ftxui::Component CreateConfigView(ViewContext& ctx);
ftxui::Component CreateListView(ViewContext& ctx);
ftxui::Component CreateHintView(ViewContext& ctx);
ftxui::Component CreatePullView(ViewContext& ctx);
ftxui::Component CreateSolveView(ViewContext& ctx);
ftxui::Component CreateStatusView(ViewContext& ctx);
ftxui::Component CreateMenuView(ViewContext& ctx);
ftxui::Component CreateDashboardView(ViewContext& ctx);
ftxui::Component CreateTestView(ViewContext& ctx);

// ── Data loading helpers ──────────────────────────────────────────

void load_config_state(AppState& state);
void save_config_state(AppState& state);
void load_list_state(AppState& state,
                     const std::string& status_filter = "all",
                     const std::string& difficulty_filter = "all",
                     const std::string& source_filter = "all");
void load_dashboard_state(AppState& state);

}  // namespace views
}  // namespace tui
}  // namespace shuati
