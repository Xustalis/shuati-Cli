#pragma once

#include <string>
#include <filesystem>

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

// Convert UTF-8 to wide string (Windows)
std::wstring utf8_to_wide(const std::string& u8str);

// Convert UTF-8 string to a filesystem path reliably
std::filesystem::path utf8_path(const std::string& u8str);

// Execute a system command containing UTF-8 characters safely
int utf8_system(const std::string& u8cmd);

// Shorten a UTF-8 string to a maximum number of bytes, ensuring valid UTF-8 and appending "..." if truncated
std::string shorten_utf8_lossy(const std::string& s, size_t max_bytes);

// Calculate the display width of a UTF-8 string (ASCII=1, others=2)
size_t utf8_display_width(const std::string& s);

// Pad a string on the right with spaces to reach a specific display width
std::string pad_string(const std::string& s, size_t width);

} // namespace shuati::utils
