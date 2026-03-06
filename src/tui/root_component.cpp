#include "root_component.hpp"
#include "components/dialogs.hpp"
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

namespace shuati {
namespace tui {

Component RootComponent(AppState& state, std::function<void()> on_exit) {
    // We defer to lazy-evaluated page containers
    auto active_page = Container::Tab({
        WelcomePage(state),
        SolveWorkspacePage(state),
        ProblemListPage(state),
        ReportViewPage(state)
    }, &state.current_page_index);

    auto dialog = DialogComponent(state);

    auto render_root = Renderer(active_page, [&state, active_page, dialog] {
        Element base = active_page->Render();
        
        if (state.current_dialog.is_open) {
            base = dbox({
                base | dim,
                dialog->Render() | center   
            });
        }

        if (!state.toast_message.empty()) {
            base = dbox({
                base,
                RenderToast(state) | clear_under | center
            });
        }
        
        return base;
    });

    // Global Key Event Interceptor
    return CatchEvent(render_root, [&state, on_exit](Event e) -> bool {
        if (state.current_dialog.is_open) {
            return false; // Let dialog handle events
        }
        
        // Global quit on Ctrl+C
        if (e == Event::Character((char)3)) { // Ctrl+C
            on_exit();
            return true;
        }

        // Welcome Page Shortcuts
        if (state.page_stack.back() == PageID::Welcome) {
            if (e == Event::Character('q') || e == Event::Character('Q')) {
                on_exit();
                return true;
            }
            if (e == Event::Character('s') || e == Event::Character('S')) {
                state.push_page(PageID::SolveWorkspace);
                return true;
            }
            if (e == Event::Character('l') || e == Event::Character('L')) {
                state.push_page(PageID::ProblemList);
                return true;
            }
        }

        if (e == Event::Escape) {
            if (state.page_stack.size() > 1 && state.page_stack.back() != PageID::SolveWorkspace) {
                state.pop_page(); // Go back
                return true;
            }
        }
        
        return false;
    });
}

} // namespace tui
} // namespace shuati
