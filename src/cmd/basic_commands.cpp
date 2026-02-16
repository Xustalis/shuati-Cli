#include "commands.hpp"
#include "shuati/utils/encoding.hpp"
#include <string>
#include "shuati/version.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <iostream>

namespace shuati {
namespace cmd {

namespace fs = std::filesystem;
using shuati::utils::ensure_utf8;

void cmd_init(CommandContext& ctx) {
    auto current = fs::current_path();
    if (fs::exists(current / Config::DIR_NAME)) {
        fmt::print("[!] 项目已在当前目录初始化。\n");
        return;
    }
    
    auto dir = current / Config::DIR_NAME;
    fs::create_directories(dir / "templates");
    fs::create_directories(dir / "problems");
    
    Config cfg;
    cfg.save(Config::config_path(current));
    
    Database db(Config::db_path(current).string());
    db.init_schema();
    
    fmt::print(fg(fmt::color::green_yellow), "[+] 初始化成功: {}\n", dir.string().c_str());
    fmt::print("    请设置 API Key: shuati config --api-key <YOUR_KEY>\n");
}

void cmd_info(CommandContext& ctx) {
    try {
        std::string exe = executable_path_utf8();
        if (exe.empty()) exe = "shuati (path unknown)";
        
        fmt::print("Version: {}\n", current_version().to_string().c_str());
        fmt::print("Exe:     {}\n", ensure_utf8(exe).c_str());
        fmt::print("CWD:     {}\n", ensure_utf8(fs::current_path().string()).c_str());

        auto root = Config::find_root();
        if (root.empty()) {
            fmt::print("Root:    (not found)\n");
            return;
        }
        fmt::print("Root:    {}\n", ensure_utf8(root.string()).c_str());
        fmt::print("Config:  {}\n", ensure_utf8(Config::config_path(root).string()).c_str());
        fmt::print("DB:      {}\n", ensure_utf8(Config::db_path(root).string()).c_str());
    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
    }
}

void cmd_config(CommandContext& ctx) {
    try {
        auto root = find_root_or_die();
        auto cfg_path = Config::config_path(root);
        auto cfg = Config::load(cfg_path);

        if (ctx.cfg_show) {
            fmt::print(fg(fmt::color::cyan), "当前配置:\n");
            fmt::print("  API Key:      {}\n", cfg.api_key.empty() ? "(未设置)" : ("******" + cfg.api_key.substr(std::max(0, (int)cfg.api_key.length()-4))).c_str());
            fmt::print("  Model:        {}\n", cfg.model);
            fmt::print("  Language:     {}\n", cfg.language);
            fmt::print("  Editor:       {}\n", cfg.editor.empty() ? "(未设置)" : cfg.editor);
            fmt::print("  AI Enabled:   {}\n", cfg.ai_enabled ? "Yes" : "No");
            return;
        }

        bool changed = false;
        if (!ctx.cfg_key.empty()) {
            if (ctx.cfg_key.length() < 10) {
                 fmt::print(fg(fmt::color::red), "[!] API Key 格式似乎不正确。\n");
            } else {
                cfg.api_key = ctx.cfg_key;
                changed = true;
            }
        }
        if (!ctx.cfg_model.empty()) {
            cfg.model = ctx.cfg_model;
            changed = true;
        }
        
        // Handle language via new_diff reuse hack (cleaned up later ideally)
        // In main.cpp we saw it reused new_diff. In setup_commands we should map it properly.
        // Assuming ctx.new_diff provided here contains language if flag used.
        if (!ctx.new_diff.empty() && ctx.new_diff != "medium") {
            std::string lang = ctx.new_diff;
            if (lang == "cpp" || lang == "c++" || lang == "python" || lang == "py") {
                cfg.language = (lang == "py") ? "python" : (lang == "c++" ? "cpp" : lang);
                changed = true;
            } else {
                fmt::print(fg(fmt::color::yellow), "[!] 未知语言: {} (仅支持 cpp/python)\n", lang.c_str());
            }
        }
        
        if (changed) {
            cfg.save(cfg_path);
            fmt::print(fg(fmt::color::green), "[+] 配置已保存。\n");
        } else {
            if (!ctx.cfg_show) fmt::print("使用 --show 查看配置，或使用 --api-key/--model/--language 修改。\n");
        }
    } catch (...) { fmt::print(fg(fmt::color::red), "[!] 操作失败\n"); }
}

} // namespace cmd
} // namespace shuati
