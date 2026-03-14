#include "command_specs.hpp"

namespace shuati {
namespace tui {

std::vector<CommandSpec> tui_command_specs() {
    return {
        {"/help", "/help | /?", "查看全部指令与示例", CommandCategory::System},
        {"/init", "/init", "在当前目录初始化项目", CommandCategory::Project},
        {"/info", "/info", "显示环境信息", CommandCategory::System},
        {"/pull", "/pull <url>", "从 URL 拉取题目", CommandCategory::Problem},
        {"/new", "/new <title>", "创建本地题目", CommandCategory::Problem},
        {"/solve", "/solve [id]", "进入做题工作流", CommandCategory::Problem},
        {"/list", "/list [--filter all|ac|failed|unaudited|review]", "列出题库题目", CommandCategory::Problem},
        {"/view", "/view <id>", "查看测试详情", CommandCategory::Problem},
        {"/test", "/test <id>", "运行测试用例", CommandCategory::Problem},
        {"/hint", "/hint <id>", "获取 AI 提示", CommandCategory::AI},
        {"/submit", "/submit <id>", "提交并记录掌握度", CommandCategory::Problem},
        {"/delete", "/delete <id>", "删除题目", CommandCategory::Problem},
        {"/clean", "/clean", "清理临时文件", CommandCategory::Project},
        {"/login", "/login <platform>", "配置平台登录 Cookie", CommandCategory::Project},
        {"/config", "/config [--show]", "配置工具", CommandCategory::System},
        {"/repl", "/repl", "进入 legacy 交互模式", CommandCategory::System},
        {"/tui", "/tui", "重新进入 TUI", CommandCategory::System},
        {"/exit", "/exit", "退出程序", CommandCategory::System},
    };
}

} // namespace tui
} // namespace shuati
