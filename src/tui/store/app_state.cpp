#include "app_state.hpp"
#include "../command_specs.hpp"

namespace shuati {
namespace tui {

void MenuState::init_from_specs() {
    auto specs = tui_command_specs();

    categories.clear();

    MenuCategory cat_problem;
    cat_problem.name = "\xe9\xa2\x98\xe7\x9b\xae (Problem)";
    MenuCategory cat_ai;
    cat_ai.name = "AI \xe5\x8a\x9f\xe8\x83\xbd (AI)";
    MenuCategory cat_project;
    cat_project.name = "\xe9\xa1\xb9\xe7\x9b\xae (Project)";
    MenuCategory cat_system;
    cat_system.name = "\xe7\xb3\xbb\xe7\xbb\x9f (System)";

    for (const auto& spec : specs) {
        MenuItem item;
        item.label = spec.slash;
        item.description = spec.summary;
        item.command = spec.slash;
        
        // Map slash commands to internal route IDs
        if (spec.slash == "/config") item.route_id = "ConfigView";
        else if (spec.slash == "/list") item.route_id = "ListView";
        else if (spec.slash == "/hint") item.route_id = "HintView";
        else if (spec.slash == "/pull") item.route_id = "PullView";
        else if (spec.slash == "/solve") item.route_id = "SolveView";
        else if (spec.slash == "/status") item.route_id = "StatusView";
        else if (spec.slash == "/menu") item.route_id = "MenuView";
        else item.route_id = "Main";

        if (spec.slash == "/pull" || spec.slash == "/solve" || spec.slash == "/test" ||
            spec.slash == "/view" || spec.slash == "/delete" || spec.slash == "/record") {
            item.requires_args = true;
            item.args_placeholder = spec.usage;
            cat_problem.items.push_back(item);
        } else if (spec.slash == "/hint") {
            item.requires_args = true;
            item.args_placeholder = "\xe9\xa2\x98\xe5\x8f\xb7";
            cat_ai.items.push_back(item);
        } else if (spec.slash == "/init" || spec.slash == "/clean" || spec.slash == "/login" || spec.slash == "/uninstall") {
            if (spec.slash == "/login" || spec.slash == "/uninstall") {
                item.requires_args = true;
                item.args_placeholder = "\xe5\xb9\xb3\xe5\x8f\xb0";
            }
            cat_project.items.push_back(item);
        } else {
            if (spec.slash == "/new") {
                item.requires_args = true;
                item.args_placeholder = "\xe6\xa0\x87\xe9\xa2\x98";
            }
            cat_system.items.push_back(item);
        }
    }

    if (!cat_problem.items.empty()) categories.push_back(std::move(cat_problem));
    if (!cat_ai.items.empty()) categories.push_back(std::move(cat_ai));
    if (!cat_project.items.empty()) categories.push_back(std::move(cat_project));
    if (!cat_system.items.empty()) categories.push_back(std::move(cat_system));

    selected_category = 0;
    selected_item = 0;
}

} // namespace tui
} // namespace shuati