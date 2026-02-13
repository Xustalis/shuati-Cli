#include "shuati/version.hpp"
#include <regex>
#include <tuple>

namespace shuati {

Version Version::parse(const std::string& str) {
    // Regex for parsing: v?(\d+)\.(\d+)\.(\d+)(-[\w\d]+)?
    static const std::regex ver_regex(R"(v?(\d+)\.(\d+)\.(\d+)(?:-([\w\d]+))?)");
    std::smatch match;
    
    Version v;
    if (std::regex_match(str, match, ver_regex)) {
        v.major = std::stoi(match[1]);
        v.minor = std::stoi(match[2]);
        v.patch = std::stoi(match[3]);
        if (match.size() > 4) {
             v.suffix = match[4].str();
        }
    }
    return v;
}

std::string Version::to_string() const {
    if (suffix.empty()) {
        return fmt::format("v{}.{}.{}", major, minor, patch);
    } else {
        return fmt::format("v{}.{}.{}-{}", major, minor, patch, suffix);
    }
}

bool Version::is_compatible_with(const Version& other) const {
    // Basic SemVer compatibility: Same major version means compatible API
    // Exception: 0.x.x is considered unstable and might break.
    if (major == 0 && other.major == 0) {
        return minor == other.minor; // Strict for 0.x
    }
    return major == other.major;
}

const Version& current_version() {
    static Version v = []() {
        // SHUATI_VERSION is defined in CMake or main.cpp, e.g., "1.3.1"
        // We need to ensure it's available here. 
        // For now, allow fallback or injection.
#ifdef SHUATI_VERSION
        return Version::parse(SHUATI_VERSION);
#else
        return Version(0, 0, 0, "unknown");
#endif
    }();
    return v;
}

} // namespace shuati
