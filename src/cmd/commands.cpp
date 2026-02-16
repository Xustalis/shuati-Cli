#include "commands.hpp"
#include "shuati/version.hpp"
#include <iostream>

namespace shuati {
namespace cmd {

// Forward declarations of view command which I added to view_command.cpp but not commands.hpp yet?
// Wait, I need to add `cmd_view` to `commands.hpp` first.
// I will update commands.hpp later or rely on linker? No, declaration needed.
// I'll add `void cmd_view(CommandContext& ctx);` to commands.hpp now via another tool or assume I'll do it.
// Actually, I can just add it here and hope for the best, but better to follow up.
// I'll update commands.hpp after this.

// Placeholder implementations for missing file split


// I will implement these in separate files in next steps to complete the task.
// For `setup_commands`, I need to wire them up.

void setup_commands(CLI::App& app, CommandContext& ctx) {
    app.require_subcommand(1);
    
    app.add_subcommand("init", "在当前目录初始化项目")->callback([&](){ cmd_init(ctx); });
    
    static std::string v_str = shuati::current_version().to_string();
    app.set_version_flag("-v,--version", v_str, "显示版本信息");
    app.add_subcommand("info", "显示环境信息")->callback([&](){ cmd_info(ctx); });

    auto pull = app.add_subcommand("pull", "从 URL 拉取题目");
    pull->add_option("url", ctx.pull_url, "题目链接")->required();
    pull->callback([&](){ cmd_pull(ctx); });

    auto newcmd = app.add_subcommand("new", "创建本地题目");
    newcmd->add_option("title", ctx.new_title, "标题")->required();
    newcmd->add_option("-t,--tags", ctx.new_tags, "标签");
    newcmd->add_option("-d,--difficulty", ctx.new_diff, "难度");
    newcmd->callback([&](){ cmd_new(ctx); });

    auto solvecmd = app.add_subcommand("solve", "开始解决一道题");
    solvecmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    solvecmd->callback([&](){ cmd_solve(ctx); });

    auto list_cmd = app.add_subcommand("list", "列出所有题目");
    list_cmd->add_option("-f,--filter", ctx.list_filter, "过滤状态: all, ac, failed, unaudited, review");
    list_cmd->callback([&](){ cmd_list(ctx); });

    auto del = app.add_subcommand("delete", "删除题目");
    del->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    del->callback([&](){ cmd_delete(ctx); });

    auto sub = app.add_subcommand("submit", "提交并记录心得");
    sub->add_option("id", ctx.submit_pid, "题目 ID")->required();
    sub->add_option("-q,--quality", ctx.submit_quality, "掌握程度 (0-5)");
    sub->callback([&](){ cmd_submit(ctx); });

    auto tst = app.add_subcommand("test", "运行测试用例");
    tst->add_option("id", ctx.solve_pid, "题目 ID")->required();
    tst->add_option("--max", ctx.test_max_cases, "最大用例数");
    tst->add_option("--oracle", ctx.test_oracle, "Oracle 模式");
    // tst->add_flag("--ui", ctx.test_ui, "交互模式 (暂不可用)"); 
    tst->callback([&](){ cmd_test(ctx); });

    auto hint = app.add_subcommand("hint", "获取 AI 提示");
    hint->add_option("id", ctx.hint_pid, "题目 ID")->required();
    hint->add_option("-f,--file", ctx.hint_file, "代码文件");
    hint->callback([&](){ cmd_hint(ctx); });
    
    // View command
    auto view = app.add_subcommand("view", "查看测试详情");
    view->add_option("id", ctx.solve_pid, "题目 ID")->required();
    view->add_option("--save-to", ctx.view_export_dir, "导出测试点数据到指定目录");

    view->callback([&](){ cmd_view(ctx); });

    app.add_subcommand("clean", "清理临时文件")->callback([&](){ cmd_clean(ctx); });

    auto cfg = app.add_subcommand("config", "配置工具");
    cfg->add_flag("--show", ctx.cfg_show, "显示配置");
    cfg->add_option("--api-key", ctx.cfg_key, "设置 API Key");
    cfg->add_option("--model", ctx.cfg_model, "设置模型");
    cfg->add_option("--language", ctx.new_diff, "设置语言");
    cfg->callback([&](){ cmd_config(ctx); });
    
    app.add_subcommand("exit", "退出")->callback([](){ exit(0); });
}

} // namespace cmd
} // namespace shuati
