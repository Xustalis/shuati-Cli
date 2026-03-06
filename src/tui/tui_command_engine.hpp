#pragma once

#include "store/app_state.hpp"
#include <string>
#include <vector>

namespace shuati {
namespace tui {

std::vector<std::string> tui_command_candidates();
std::vector<std::string> tui_cli_command_candidates();
bool tui_is_selection_command(const std::string& base_cmd, size_t argc);
bool tui_prepare_selection(AppState& state, const std::string& base_cmd);
std::string tui_execute_command_capture(const std::vector<std::string>& args, const std::string& base_cmd);

} // namespace tui
} // namespace shuati
