#pragma once

#include <string>
#include <vector>
#include <compare>
#include <fmt/core.h>

namespace shuati {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string suffix; // e.g., "beta", "rc1", "dev"

    // Default constructor
    Version() = default;

    // Construct from components
    Version(int maj, int min, int pat, std::string suf = "")
        : major(maj), minor(min), patch(pat), suffix(std::move(suf)) {}

    // Parse from string "v1.2.3" or "1.2.3-beta"
    static Version parse(const std::string& str);

    // Convert to string "v1.2.3"
    std::string to_string() const;

    // Three-way comparison operator (spaceship)
    auto operator<=>(const Version& other) const {
        if (auto cmp = major <=> other.major; cmp != 0) return cmp;
        if (auto cmp = minor <=> other.minor; cmp != 0) return cmp;
        return patch <=> other.patch;
        // Suffix handling is tricky for ordering, usually ignores it or treats non-empty as smaller (pre-release)
        // For simplicity, let's ignore suffix in basic version comparison or handle it manually if needed.
        // Actually, semver says pre-release versions have lower precedence than the associated normal version.
        // Let's keep it simple for now and rely on major.minor.patch.
    }

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch && suffix == other.suffix;
    }
    
    // Check compatibility (e.g. same major version)
    bool is_compatible_with(const Version& other) const;
};

// Global version instance, populated from CMake or build definition
const Version& current_version();

} // namespace shuati
