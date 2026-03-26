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
    (void)ctx;
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

        // Auto-detect best editor for this platform, then save config in one shot
        Config cfg;
        cfg.language = "cpp";
        cfg.editor = Config::detect_editor();
        cfg.autostart_repl = true;

        // Write config once (also serialises version via raw JSON merge)
        auto cfg_path = shuati_dir / "config.json";
        cfg.save(cfg_path);
        // Inject "version" field (not tracked in Config struct, kept as metadata)
        {
            nlohmann::json j;
            { std::ifstream in(cfg_path); if (in.good()) in >> j; }
            j["version"] = current_version().to_string();
            std::ofstream out(cfg_path);
            out << j.dump(4);
        }

        // Database initialisation
        Database db((shuati_dir / "shuati.db").string());
        db.init_schema();

        std::cout << "[+] 初始化成功!" << std::endl;
        std::cout << "    位置: " << shuati_dir.string() << std::endl;
        std::cout << "    编辑器: " << cfg.editor << std::endl;
        std::cout << "    自动启动 REPL: " << (cfg.autostart_repl ? "开启" : "关闭") << std::endl;
        std::cout << "    (使用 'shuati config --editor <编辑器>' 可更改编辑器)" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[!] 初始化失败: " << e.what() << std::endl;
    }
}

void cmd_info(CommandContext& ctx) {
    (void)ctx;
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
        auto root = find_root_or_die();
        
        if (ctx.cfg_show) {
            std::ifstream i(root / ".shuati" / "config.json");
            std::cout << i.rdbuf() << std::endl;
            return;
        }

        // Load existing config
        auto cfg_path = Config::config_path(root);
        auto cfg = Config::load(cfg_path);
        bool changed = false;

        if (!ctx.cfg_key.empty()) {
            cfg.api_key = ctx.cfg_key;
            changed = true;
            std::cout << "[+] API Key 已设置。" << std::endl;
        }
        if (!ctx.cfg_model.empty()) {
            cfg.model = ctx.cfg_model;
            changed = true;
            std::cout << "[+] 模型已设置为: " << ctx.cfg_model << std::endl;
        }
        if (!ctx.cfg_language.empty() && (ctx.cfg_language == "cpp" || ctx.cfg_language == "python")) {
            cfg.language = ctx.cfg_language;
            changed = true;
            std::cout << "[+] 语言已设置为: " << ctx.cfg_language << std::endl;
        }
        // Editor config with auto-detect support
        if (!ctx.cfg_editor.empty()) {
            if (ctx.cfg_editor == "auto") {
                cfg.editor = Config::detect_editor();
                std::cout << "[+] 编辑器已自动检测并设置为: " << cfg.editor << std::endl;
            } else {
                cfg.editor = ctx.cfg_editor;
                std::cout << "[+] 编辑器已设置为: " << cfg.editor << std::endl;
            }
            changed = true;
        }
        // Autostart REPL toggle
        if (!ctx.cfg_autostart_repl.empty()) {
            if (ctx.cfg_autostart_repl == "on" || ctx.cfg_autostart_repl == "true" || ctx.cfg_autostart_repl == "1") {
                cfg.autostart_repl = true;
                changed = true;
                std::cout << "[+] 自动启动 REPL: 已开启。" << std::endl;
                std::cout << "    (现在运行 'shuati' (无参数) 将自动进入交互模式)" << std::endl;
            } else if (ctx.cfg_autostart_repl == "off" || ctx.cfg_autostart_repl == "false" || ctx.cfg_autostart_repl == "0") {
                cfg.autostart_repl = false;
                changed = true;
                std::cout << "[+] 自动启动 REPL: 已关闭。" << std::endl;
                std::cout << "    (运行 'shuati repl' 可手动进入交互模式)" << std::endl;
            } else {
                std::cerr << "[!] --autostart-repl 的值必须是 'on' 或 'off'" << std::endl;
            }
        }
        if (!ctx.cfg_ui_mode.empty()) {
            if (ctx.cfg_ui_mode == "tui" || ctx.cfg_ui_mode == "legacy") {
                cfg.ui_mode = ctx.cfg_ui_mode;
                changed = true;
                std::cout << "[+] UI 模式已设置为: " << ctx.cfg_ui_mode << std::endl;
            } else {
                std::cerr << "[!] --ui-mode 的值必须是 'tui' 或 'legacy'" << std::endl;
            }
        }

        if (changed) {
            cfg.save(cfg_path);
            std::cout << "[+] 配置已保存。" << std::endl;
        } else {
            std::cout << "未指定任何更改。使用 --show 查看当前配置。" << std::endl;
            std::cout << "\n可用选项:\n";
            std::cout << "  --show                显示当前所有配置\n";
            std::cout << "  --api-key <key>       设置 AI API Key\n";
            std::cout << "  --model <model>       设置模型名称\n";
            std::cout << "  --language <cpp|python> 设置默认编程语言\n";
            std::cout << "  --editor <cmd|auto>   设置编辑器命令 (auto=自动检测)\n";
            std::cout << "  --autostart-repl <on|off> 是否在无参数时自动启动 REPL\n";
            std::cout << "  --ui-mode <tui|legacy> 启动 UI 模式\n";
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
            // TUI mode cannot use stdin - guide user to CLI
            if (ctx.is_tui) {
                std::cout << "[!] 登录配置需要粘贴 Cookie，TUI 模式下无法完成。" << std::endl;
                std::cout << "    请退出 TUI 后在命令行执行: shuati login lanqiao" << std::endl;
                std::cout << "    或使用 /config 在 lanqiao_cookie 字段中直接填入。" << std::endl;
                return;
            }
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
