#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

namespace shuati {

namespace fs = std::filesystem;

/**
 * Get the base directory for Shuati resources.
 *
 * Search order:
 * 1. SHUATI_RESOURCE_DIR environment variable (for testing/custom deployments)
 * 2. Executable-relative "resources/" directory (Windows / dev builds)
 * 3. FHS standard path /usr/share/shuati-cli/resources (Linux DEB install)
 * 4. Last resort fallback to "resources"
 */
inline fs::path get_resource_base_dir() {
    // 1. Environment variable override
    const char* env_dir = std::getenv("SHUATI_RESOURCE_DIR");
    if (env_dir && fs::exists(env_dir)) {
        return fs::path(env_dir);
    }

    // 2. Executable-relative "resources/" (works for Windows & dev builds)
    std::vector<fs::path> relative_paths = {
        "resources",
        "../resources",
        "../../resources",
    };

    for (const auto& p : relative_paths) {
        if (fs::exists(p)) {
            try {
                return fs::canonical(p);
            } catch (...) {
                return p;
            }
        }
    }

    // 3. FHS standard install paths (Linux DEB / system installs)
#ifndef _WIN32
    {
        std::vector<fs::path> system_paths = {
            "/usr/share/shuati-cli/resources",
            "/usr/local/share/shuati-cli/resources",
        };
        for (const auto& sp : system_paths) {
            if (fs::exists(sp)) {
                return sp;
            }
        }
    }
#endif

    // 4. Last resort: return "resources" and let the caller handle the error
    return fs::path("resources");
}

/**
 * Get the full path to a specific resource file.
 */
inline fs::path get_resource_path(const std::string& relative_path) {
    return get_resource_base_dir() / relative_path;
}

} // namespace shuati
