#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <fmt/core.h>

#ifndef _WIN32
#include <cstdlib>
#endif

namespace shuati {

namespace fs = std::filesystem;

struct Config {
    std::string api_key;
    std::string api_base = "https://api.deepseek.com/v1";
    std::string model = "deepseek-chat";
    std::string language = "cpp";     // default solution language
    int max_tokens = 4096;
    std::string editor;               // Editor command (e.g., "code", "vim", "nvim", "nano")
    bool ai_enabled = true;           // Enable AI features
    bool template_enabled = true;     // Enable template generation
    std::string lanqiao_cookie;       // Cookie for authenticated Lanqiao problem fetching
    bool autostart_repl = true;       // Auto-start REPL when running 'shuati' with no args

    // --- Linux / cross-platform editor detection ---
    // Returns a best-guess editor command, checking $VISUAL, $EDITOR, then common editors in PATH.
    // NOTE: uses filesystem PATH scan, NOT std::system(), to be safe in CI and restricted environments.
    static std::string detect_editor() {
#ifndef _WIN32
        // Prefer user-configured env vars (set by shell profile or multiplexer)
        for (const char* env_var : {"VISUAL", "EDITOR"}) {
            const char* val = std::getenv(env_var);
            if (val && val[0] != '\0') return std::string(val);
        }
        // Scan PATH directories for common terminal editors
        const char* path_env = std::getenv("PATH");
        if (path_env) {
            std::string path_str(path_env);
            std::vector<std::string> dirs;
            size_t start = 0, pos;
            while ((pos = path_str.find(':', start)) != std::string::npos) {
                dirs.push_back(path_str.substr(start, pos - start));
                start = pos + 1;
            }
            dirs.push_back(path_str.substr(start));

            for (const char* ed : {"nvim", "vim", "vi", "nano", "emacs", "micro", "code"}) {
                for (const auto& dir : dirs) {
                    if (dir.empty()) continue;
                    fs::path candidate = fs::path(dir) / ed;
                    std::error_code ec;
                    if (fs::is_regular_file(candidate, ec) || fs::is_symlink(candidate, ec)) {
                        return ed;
                    }
                }
            }
        }
        return "vi"; // POSIX fallback — always available
#else
        return "code"; // VSCode default on Windows
#endif
    }

    static constexpr const char* DIR_NAME = ".shuati";
    static constexpr const char* DB_NAME = "shuati.db";
    static constexpr const char* CONFIG_NAME = "config.json";

    // Find project root by walking up directories
    static fs::path find_root(const fs::path& start = fs::current_path()) {
        auto p = start;
        while (!p.empty()) {
            if (fs::exists(p / DIR_NAME)) return p;
            auto parent = p.parent_path();
            if (parent == p) break;
            p = parent;
        }
        return {};
    }

    static fs::path data_dir(const fs::path& root) {
        return root / DIR_NAME;
    }

    static fs::path db_path(const fs::path& root) {
        return data_dir(root) / DB_NAME;
    }

    static fs::path config_path(const fs::path& root) {
        return data_dir(root) / CONFIG_NAME;
    }

    void save(const fs::path& path) const {
        // Load existing JSON first so we don't destroy untracked fields (e.g. "version")
        nlohmann::json j;
        {
            std::ifstream in(path);
            if (in.good()) {
                try { in >> j; } catch (...) { j = nlohmann::json::object(); }
            } else {
                j = nlohmann::json::object();
            }
        }
        // Merge / update only the fields we own
        if (!api_key.empty())        j["api_key"]          = api_key;
        if (!api_base.empty())       j["api_base"]         = api_base;
        if (!model.empty())          j["model"]            = model;
        if (!language.empty())       j["language"]         = language;
        if (max_tokens != 4096)      j["max_tokens"]       = max_tokens;
        if (!editor.empty())         j["editor"]           = editor;
        j["ai_enabled"]              = ai_enabled;
        j["template_enabled"]        = template_enabled;
        if (!lanqiao_cookie.empty()) j["lanqiao_cookie"]   = lanqiao_cookie;
        j["autostart_repl"]          = autostart_repl;
        std::ofstream(path) << j.dump(2);
    }

    static Config load(const fs::path& path) {
        Config c;
        if (!fs::exists(path)) return c;
        try {
            auto j = nlohmann::json::parse(std::ifstream(path));
            if (j.contains("api_key"))    c.api_key    = j["api_key"];
            if (j.contains("api_base"))   c.api_base   = j["api_base"];
            if (j.contains("model"))      c.model      = j["model"];
            if (j.contains("language"))   c.language   = j["language"];
            if (j.contains("max_tokens")) c.max_tokens = j["max_tokens"];
            if (j.contains("editor"))     c.editor     = j["editor"];
            if (j.contains("ai_enabled")) c.ai_enabled = j["ai_enabled"];
            if (j.contains("template_enabled")) c.template_enabled = j["template_enabled"];
            if (j.contains("lanqiao_cookie")) c.lanqiao_cookie = j["lanqiao_cookie"];
            if (j.contains("autostart_repl")) c.autostart_repl = j["autostart_repl"];
        } catch (...) {}
        return c;
    }
};

} // namespace shuati
