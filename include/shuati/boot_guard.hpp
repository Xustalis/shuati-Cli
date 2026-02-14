#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace shuati {

class BootGuard {
public:
    struct ProjectHistory {
        std::string path;
        std::string last_accessed; // ISO 8601 or timestamp
        int access_count = 0;
    };

    // Main entry point: checks if CWD is valid, if not shows TUI
    // Returns true if we are in a valid project (possibly after navigation), false if user chose to exit
    static bool check();

    // Records a successful project load
    static void record_history(const std::string& path);

private:
    static std::vector<ProjectHistory> load_history();
    static void save_history(const std::vector<ProjectHistory>& history);
    static std::filesystem::path get_history_file();
};

} // namespace shuati
