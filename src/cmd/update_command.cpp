#include "commands.hpp"
#include "shuati/version.hpp"
#include <iostream>
#include <fmt/core.h>
#include <fmt/color.h>

namespace shuati {
namespace cmd {

void cmd_update(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        
        fmt::print("\n=== 检查 CLI 更新 ===\n");
        // Placeholder for real version check from GitHub API
        fmt::print(fg(fmt::color::yellow), "[!] 此功能（GitHub 自动更新）将在后续版本实现\n");
        fmt::print("当前版本: {}\n", current_version().to_string());
        
        fmt::print("\n=== 可用模型列表 ===\n");
        auto models = svc.ai->list_models();
        
        if (models.empty()) {
            fmt::print(fg(fmt::color::red), "[!] 无法获取模型清单 (远程/本地均失败)\n");
            if (svc.cfg.api_key.empty()) {
                fmt::print(fg(fmt::color::gray), "    提示: 未配置 API Key，且本地服务未启动。\n");
            }
        } else {
            for (const auto& m : models) {
                fmt::print("  [*] {}\n", m);
            }
        }

        if (svc.cfg.local_model_auto_start && !svc.cfg.local_model_path.empty()) {
            fmt::print("\n=== 本地模型服务 ===\n");
            std::string url;
            if (svc.ai->start_local_server(url)) {
                fmt::print(fg(fmt::color::green), "[+] 本地服务已启动: {}\n", url);
            } else {
                fmt::print(fg(fmt::color::red), "[!] 本地服务启动失败，请检查路径或端口。\n");
                fmt::print(fg(fmt::color::gray), "    路径: {}\n", svc.cfg.local_model_path);
            }
        }

        fmt::print("\n");
        
    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
    }
}

} // namespace cmd
} // namespace shuati
