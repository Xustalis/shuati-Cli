#include "tui_command_engine.hpp"
#include "../cmd/commands.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <filesystem>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <streambuf>

namespace shuati {
namespace tui {

namespace {

class ScopedStreamCapture {
public:
    ScopedStreamCapture() : old_out_(std::cout.rdbuf()), old_err_(std::cerr.rdbuf()) {
        std::cout.rdbuf(buffer_.rdbuf());
        std::cerr.rdbuf(buffer_.rdbuf());
    }

    ~ScopedStreamCapture() {
        std::cout.rdbuf(old_out_);
        std::cerr.rdbuf(old_err_);
    }

    std::string str() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf* old_out_;
    std::streambuf* old_err_;
};

std::string normalize_text(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    return text;
}

std::string execute_shell_command(const std::vector<std::string>& args, const std::string& base_cmd) {
    std::ostringstream out;

    if (base_cmd == "ls" || base_cmd == "dir") {
        try {
            auto cwd = std::filesystem::current_path();
            out << "目录: " << cwd.string() << "\n\n";
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto& entry : std::filesystem::directory_iterator(cwd)) {
                entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) return a.is_directory();
                return a.path().filename().string() < b.path().filename().string();
            });
            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    out << "  [DIR]  " << name << "/\n";
                } else {
                    auto size = entry.file_size();
                    std::string size_str;
                    if (size < 1024) size_str = std::to_string(size) + " B";
                    else if (size < 1024 * 1024) size_str = fmt::format("{:.1f} KB", size / 1024.0);
                    else size_str = fmt::format("{:.1f} MB", size / (1024.0 * 1024.0));
                    out << "  [FILE] " << std::left << std::setw(40) << name << " " << size_str << "\n";
                }
            }
        } catch (const std::exception& e) {
            out << "[!] 错误: " << e.what() << "\n";
        }
        return out.str();
    }

    if (base_cmd == "cd") {
        if (args.size() < 3) {
            out << "用法: cd <目录>\n";
            return out.str();
        }
        try {
            std::filesystem::path target = args[2];
            if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
                std::filesystem::current_path(target);
                out << "[+] 已切换到: " << std::filesystem::current_path().string() << "\n";
            } else {
                out << "[!] 目录不存在: " << target.string() << "\n";
            }
        } catch (const std::exception& e) {
            out << "[!] 错误: " << e.what() << "\n";
        }
        return out.str();
    }

    if (base_cmd == "pwd") {
        out << std::filesystem::current_path().string() << "\n";
        return out.str();
    }

    if (base_cmd == "help") {
        out << "Shuati CLI TUI 模式工作区\n\n";
        out << "Shell 命令:\n";
        out << "  ls/dir, cd, pwd, clear, exit\n\n";
        out << "Shuati 命令:\n";
        for (const auto& cmd : tui_cli_command_candidates()) {
            out << "  " << cmd << "\n";
        }
        return out.str();
    }

    return "";
}

} // namespace

bool tui_prepare_selection(AppState& state, const std::string& base_cmd) {
    try {
        auto root = shuati::cmd::find_root_or_die();
        auto svc = shuati::cmd::Services::load(root, true);
        auto problems = svc.pm->list_problems();
        if (problems.empty()) {
            state.history.push_back({MessageRole::Error, "题库目前为空，请先使用 pull 拉取题目。"});
            return false;
        }
        state.view_state = AppState::ViewState::Selection;
        state.selection_title = (base_cmd == "solve") ? "选择要解决的题目" : "请选择题目";
        state.selection_options.clear();
        state.selection_options.reserve(problems.size());
        for (const auto& p : problems) {
            state.selection_options.push_back(std::to_string(p.display_id) + " | " + p.title);
        }
        state.selection_selected = 0;
        state.selection_pending_command = "/" + base_cmd;
        return true;
    } catch (const std::exception& e) {
        state.history.push_back({MessageRole::Error, e.what()});
        return false;
    }
}

std::string tui_execute_command_capture(const std::vector<std::string>& args, const std::string& base_cmd) {
    if (base_cmd == "ls" || base_cmd == "dir" || base_cmd == "cd" || base_cmd == "pwd" || base_cmd == "help") {
        return normalize_text(execute_shell_command(args, base_cmd));
    }

    ScopedStreamCapture capture;
    try {
        CLI::App app{"TUI"};
        app.allow_extras();
        shuati::cmd::CommandContext cmd_ctx;
        shuati::cmd::setup_commands(app, cmd_ctx);
        std::vector<const char*> argv;
        argv.reserve(args.size());
        for (const auto& a : args) argv.push_back(a.c_str());
        try {
            app.parse(static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
        } catch (const CLI::ParseError& e) {
            app.exit(e);
        }
    } catch (const std::exception& e) {
        std::cout << "[!] 运行时错误: " << e.what() << "\n";
    }

    return normalize_text(capture.str());
}

} // namespace tui
} // namespace shuati
