#pragma once

#include <string>
#include <vector>

namespace shuati {
namespace tui {

enum class CommandCategory {
    Project,
    Problem,
    AI,
    System
};

struct CommandSpec {
    std::string slash;   // e.g. "/pull"
    std::string usage;   // e.g. "/pull <url>"
    std::string summary; // short description
    CommandCategory category = CommandCategory::System;
};

std::vector<CommandSpec> tui_command_specs();

} // namespace tui
} // namespace shuati

