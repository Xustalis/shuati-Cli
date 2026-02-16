#pragma once

#include <string>

namespace shuati::utils {

// Check if a string is valid UTF-8
bool is_valid_utf8(const std::string& s);

// Convert Active Code Page (Windows) to UTF-8
std::string acp_to_utf8(const std::string& s);

// Ensure a string is UTF-8 (converting from ACP if necessary on Windows)
std::string ensure_utf8(const std::string& s);

// Ensure a string is UTF-8, replacing invalid characters with '?' if necessary
std::string ensure_utf8_lossy(const std::string& s);

// Convert wide string to UTF-8 (Windows)
std::string wide_to_utf8(const std::wstring& w);

// Shorten a UTF-8 string to a maximum number of bytes, ensuring valid UTF-8 and appending "..." if truncated
// Shorten a UTF-8 string to a maximum number of bytes, ensuring valid UTF-8 and appending "..." if truncated
std::string shorten_utf8_lossy(const std::string& s, size_t max_bytes);

// Calculate the display width of a UTF-8 string (ASCII=1, others=2)
size_t utf8_display_width(const std::string& s);

// Pad a string on the right with spaces to reach a specific display width
std::string pad_string(const std::string& s, size_t width);

} // namespace shuati::utils
