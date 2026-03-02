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

void cmd_login(CommandContext& ctx) {
    try {
        std::string platform = ctx.login_platform;
        if (platform.empty()) {
            std::cout << "可用平台: lanqiao\n";
            std::cout << "用法: login <平台名>\n";
            return;
        }

        // Normalize
        for (auto& c : platform) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (platform == "lanqiao") {
            std::cout << "\n";
            std::cout << "══════════════════════════════════════════════════\n";
            std::cout << "  蓝桥云课 (lanqiao.cn) 登录配置向导\n";
            std::cout << "══════════════════════════════════════════════════\n";
            std::cout << "\n";
            std::cout << "【步骤 1】用浏览器打开并登录 https://www.lanqiao.cn\n";
            std::cout << "\n";
            std::cout << "【步骤 2】按 F12 打开开发者工具，选择【网络(Network)】标签\n";
            std::cout << "\n";
            std::cout << "【步骤 3】刷新页面，在左侧请求列表中点击任意一个\n";
            std::cout << "          lanqiao.cn 开头的请求\n";
            std::cout << "\n";
            std::cout << "【步骤 4】在右侧找到【请求头(Request Headers)】中的\n";
            std::cout << "          \"Cookie\" 一行，右键复制其对应的完整值\n";
            std::cout << "\n";
            std::cout << "──────────────────────────────────────────────────\n";
            std::cout << "  请在此处粘贴您的 Cookie 并按回车:\n";
            std::cout << "  > ";
            std::cout.flush();

            std::string cookie;
            std::getline(std::cin, cookie);

            // Trim whitespace
            auto trim_str = [](std::string s) {
                size_t start = s.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) return std::string{};
                size_t end = s.find_last_not_of(" \t\r\n");
                return s.substr(start, end - start + 1);
            };
            cookie = trim_str(cookie);

            if (cookie.empty()) {
                std::cout << "\n[!] 未输入任何内容，操作取消。\n";
                return;
            }

            // Simple sanity check: valid cookies contain '=' sign
            if (cookie.find('=') == std::string::npos) {
                std::cout << "\n[!] 看起来不像是有效的 Cookie (应包含'='字符)。\n";
                std::cout << "    您粘贴的是否是正确的 Cookie 值?\n";
                return;
            }

            auto root = Config::find_root();
            if (root.empty()) {
                std::cerr << "[!] 未找到项目目录。请先在项目目录中运行 'shuati init'。\n";
                return;
            }

            auto cfg_path = Config::config_path(root);
            auto cfg = Config::load(cfg_path);
            cfg.lanqiao_cookie = cookie;
            cfg.save(cfg_path);

            std::cout << "\n[+] Cookie 已保存！\n";
            std::cout << "    现在可以使用 'shuati pull <蓝桥题目URL>' 抓取完整题目了。\n\n";

        } else {
            std::cout << "[!] 未知平台: " << platform << "\n";
            std::cout << "    目前支持: lanqiao\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << "\n";
    }
}

} // namespace cmd
} // namespace shuati
