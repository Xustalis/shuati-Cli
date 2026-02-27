#include "shuati/utils/encoding.hpp"
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace shuati::utils {

#ifdef _WIN32

bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F) {
            i++;
            continue;
        }

        size_t need = 0;
        if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else return false;

        if (i + need > s.size()) return false;
        for (size_t j = 1; j < need; j++) {
            unsigned char cc = static_cast<unsigned char>(s[i + j]);
            if ((cc & 0xC0) != 0x80) return false;
        }
        i += need;
    }
    return true;
}

std::string ascii_fallback(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 0x20 && c <= 0x7E) out.push_back(static_cast<char>(c));
        else if (c == '\n' || c == '\r' || c == '\t') out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

std::string acp_to_utf8(const std::string& s) {
    if (s.empty()) return s;
    int wlen = MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (wlen <= 0) return ascii_fallback(s);
    std::wstring w(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), w.data(), wlen);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return ascii_fallback(s);
    std::string out(static_cast<size_t>(u8len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, out.data(), u8len, nullptr, nullptr);
    return out;
}

std::string ensure_utf8(const std::string& s) {
    if (is_valid_utf8(s)) return s;
    auto converted = acp_to_utf8(s);
    if (is_valid_utf8(converted)) return converted;
    return ascii_fallback(s);
}

std::string ensure_utf8_lossy(const std::string& s) {
    return ensure_utf8(s); // Reuse logic
}

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int u8len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return {};
    std::string out((size_t)u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), u8len, nullptr, nullptr);
    return out;
}

#else

bool is_valid_utf8(const std::string& /*s*/) { return true; } // Assume Linux/macOS uses UTF-8 everywhere
std::string acp_to_utf8(const std::string& s) { return s; }
std::string ensure_utf8(const std::string& s) { return s; }
std::string ensure_utf8_lossy(const std::string& s) { return s; }
std::string wide_to_utf8(const std::wstring& /*w*/) { return ""; } // Not needed on POSIX usually

#endif


std::string shorten_utf8_lossy(const std::string& s, size_t max_bytes) {
    if (s.size() <= max_bytes) return ensure_utf8(s);
    size_t len = max_bytes;
    while (len > 0) {
        unsigned char c = static_cast<unsigned char>(s[len]);
        // If c is a continuation byte (10xxxxxx), backtrack.
        // If c is start byte (0xxxxxxx or 11xxxxxx), we can split BEFORE it? 
        if ((c & 0xC0) != 0x80) break; 
        len--;
    }
    if (len == 0) return "...";
    std::string cut = s.substr(0, len);
    return ensure_utf8(cut) + "...";
}


size_t utf8_display_width(const std::string& s) {
    size_t width = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            width += 1;
        } else if ((c & 0xC0) == 0x80) {
            // Continuation byte, ignore
            continue;
        } else {
            // Start of multi-byte sequence (assume width 2 for CJK etc)
            width += 2;
        }
    }
    return width;
}

std::string pad_string(const std::string& s, size_t width) {
    size_t w = utf8_display_width(s);
    if (w >= width) return s;
    return s + std::string(width - w, ' ');
}

} // namespace shuati::utils
