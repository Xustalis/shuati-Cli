#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include "../store/app_state.hpp"

namespace shuati {
namespace tui {

// Render a modal dialog component that handles button clicks
ftxui::Component DialogComponent(AppState& state);

// Pure view renderer for fading toasts
ftxui::Element RenderToast(const AppState& state);

} // namespace tui
} // namespace shuati
