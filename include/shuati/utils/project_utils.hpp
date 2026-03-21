#pragma once

#include <string>
#include <filesystem>
#include "shuati/config.hpp"

namespace shuati {
namespace utils {

/**
 * @brief Find project root or throw runtime_error with user-friendly message
 * @return Path to the project root (.shuati directory parent)
 */
std::filesystem::path find_root_or_die();

/**
 * @brief Canonicalize website source name for directory usage
 * @param source Original source string (e.g., "LeetCode")
 * @return Lowercase short name (e.g., "leetcode")
 */
std::string canonical_source(const std::string& source);

} // namespace utils
} // namespace shuati
