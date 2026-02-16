#include "commands.hpp"
#include "shuati/version.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace shuati {
namespace cmd {

namespace fs = std::filesystem;

void cmd_init(CommandContext& ctx) {
    try {
        fs::path cwd = fs::current_path();
        fs::path shuati_dir = cwd / ".shuati";
        if (fs::exists(shuati_dir)) {
            std::cout << "[!] 项目已初始化 (.shuati 目录存在)。" << std::endl;
            return;
        }

        fs::create_directories(shuati_dir);
        fs::create_directories(shuati_dir / "problems");
        fs::create_directories(shuati_dir / "temp");
        fs::create_directories(shuati_dir / "logs");

        // Default Config
        nlohmann::json config;
        config["version"] = current_version().to_string();
        config["language"] = "cpp"; // default
        config["editor"] = "code";  // default vscode
        
        std::ofstream o(shuati_dir / "config.json");
        o << config.dump(4);

        // Database
        Database db((shuati_dir / "shuati.db").string());
        db.init_schema();

        std::cout << "[+] 初始化成功!" << std::endl;
        std::cout << "    位置: " << shuati_dir.string() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] 初始化失败: " << e.what() << std::endl;
    }
}

void cmd_info(CommandContext& ctx) {
    std::cout << "Shuati CLI " << current_version().to_string() << std::endl;
    std::cout << "Executable: " << executable_path_utf8() << std::endl;
    
    auto root = find_root_or_die();
    if (!root.empty()) {
        std::cout << "Project Root: " << root.string() << std::endl;
        try {
            auto svc = Services::load(root);
            std::cout << "Language: " << svc.cfg.language << std::endl;
            std::cout << "Editor: " << svc.cfg.editor << std::endl;
        } catch (...) {
            std::cout << "Config: (Error loading)" << std::endl;
        }
    } else {
        std::cout << "Project Root: (Not in a project)" << std::endl;
    }
}

void cmd_config(CommandContext& ctx) {
    try {
        if (ctx.cfg_show) {
            auto root = find_root_or_die();
            std::ifstream i(root / ".shuati" / "config.json");
            std::cout << i.rdbuf() << std::endl;
            return;
        }

        auto root = find_root_or_die();
        // Load existing
        nlohmann::json j;
        {
            std::ifstream i(root / ".shuati" / "config.json");
            if (i.good()) i >> j;
        }

        bool changed = false;
        if (!ctx.cfg_key.empty()) {
            j["api_key"] = ctx.cfg_key;
            changed = true;
            std::cout << "[+] API Key set." << std::endl;
        }
        if (!ctx.cfg_model.empty()) {
            j["model"] = ctx.cfg_model;
            changed = true;
            std::cout << "[+] Model set to: " << ctx.cfg_model << std::endl;
        }
        if (!ctx.new_diff.empty() && (ctx.new_diff == "cpp" || ctx.new_diff == "python")) {
            // Reusing new_diff field for language arg as per commands.cpp setup??
            // commands.cpp: cfg->add_option("--language", ctx.new_diff, "设置语言");
            // Yes, reusing variable.
            j["language"] = ctx.new_diff;
            changed = true;
            std::cout << "[+] Language set to: " << ctx.new_diff << std::endl;
        }

        if (changed) {
            std::ofstream o(root / ".shuati" / "config.json");
            o << j.dump(4);
        } else {
            std::cout << "No changes specified. Use --show to view config." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
