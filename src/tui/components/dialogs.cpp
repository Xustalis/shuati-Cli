#include "dialogs.hpp"
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

namespace shuati {
namespace tui {

Component DialogComponent(AppState& state) {
    auto renderer = Renderer([&state] {
        if (!state.current_dialog.is_open) {
            return text("") | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);
        }

        Elements btn_elements;
        // In a more complex implementation, we would map actual Button components here.
        // But since this is a dynamic count, we'll render it as a simple view here,
        // and handle input via a CatchEvent wrapper around the Dialog component.
        for (size_t i = 0; i < state.current_dialog.buttons.size(); ++i) {
            btn_elements.push_back(
                text(state.current_dialog.buttons[i]) | border | center
            );
        }

        auto dlg = window(text(state.current_dialog.title) | bold | center,
            vbox({
                text(state.current_dialog.message) | center,
                separatorEmpty(),
                hbox(std::move(btn_elements)) | center
            })
        );
        return dlg | clear_under | center;
    });

    // Handle keys 1, 2, 3... or Y/N for buttons in MVP
    auto event_handler = CatchEvent(renderer, [&state](Event e) -> bool {
        if (!state.current_dialog.is_open) return false;
        
        if (e == Event::Escape || e == Event::Character('n') || e == Event::Character('N')) {
            if (state.current_dialog.on_resolve) state.current_dialog.on_resolve(1);
            else state.current_dialog.is_open = false;
            return true;
        }
        if (e == Event::Return || e == Event::Character('y') || e == Event::Character('Y')) {
            if (state.current_dialog.on_resolve) state.current_dialog.on_resolve(0);
            else state.current_dialog.is_open = false;
            return true;
        }
        return true; // Block all other inputs while dialog is open
    });
    
    return event_handler;
}

Element RenderToast(const AppState& state) {
    if (state.toast_message.empty()) {
        return text("") | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);
    }
    return text(" " + state.toast_message + " ") 
           | bgcolor(Color::Blue) 
           | color(Color::White) 
           | border;
}

} // namespace tui
} // namespace shuati
