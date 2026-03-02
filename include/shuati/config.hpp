#pragma once

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <fmt/core.h>

namespace shuati {

namespace fs = std::filesystem;

struct Config {
    std::string api_key;
    std::string api_base = "https://api.deepseek.com/v1";
    std::string model = "deepseek-chat";
    std::string language = "cpp";     // default solution language
    int max_tokens = 4096;
    std::string editor;               // Editor command (e.g., "code", "vim")
    bool ai_enabled = true;           // Enable AI features
    bool template_enabled = true;     // Enable template generation
    std::string lanqiao_cookie;       // Cookie for authenticated Lanqiao problem fetching

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
        } catch (...) {}
        return c;
    }
};

} // namespace shuati
