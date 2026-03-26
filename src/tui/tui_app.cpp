#include "shuati/tui_app.hpp"
#include "shuati/tui_views.hpp"
#include "shuati/config.hpp"
#include "shuati/version.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "commands.hpp"

#include "tui_command_engine.hpp"
#include "command_specs.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <ctime>
#include <memory>
#include <unordered_set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace tui {

using namespace ftxui;

namespace {

#ifdef _WIN32
bool configure_windows_console_for_tui() {
    auto apply = [](DWORD handle_type, DWORD add, DWORD remove) -> bool {
        HANDLE h = GetStdHandle(handle_type);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        return !!SetConsoleMode(h, (mode | add) & ~remove);
    };
    bool ok = apply(STD_OUTPUT_HANDLE, ENABLE_VIRTUAL_TERMINAL_PROCESSING, 0);
    // TUI mode: Enable VT input for ftxui
    ok = apply(STD_INPUT_HANDLE,
               ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_EXTENDED_FLAGS,
               ENABLE_QUICK_EDIT_MODE) && ok;
    return ok;
}
#endif

std::string render_help_text() {
    auto specs = tui_command_specs();
    std::string out;
    out += "全部指令\n\n";
    for (const auto& spec : specs) {
        out += "  " + spec.usage + "  --  " + spec.summary + "\n";
    }
    out += "\n内置命令: ls, cd, pwd, clear\n";
    return out;
}

void update_autocomplete(AppState& state, const std::vector<CommandSpec>& specs) {
    if (state.input.empty() || state.input[0] != '/') {
        state.is_autocomplete_open = false;
        return;
    }
    std::string prefix = state.input;
    auto space_pos = prefix.find(' ');
    if (space_pos != std::string::npos) {
        state.is_autocomplete_open = false;
        return;
    }

    state.autocomplete_labels.clear();
    state.autocomplete_commands.clear();
    bool exact = false;
    for (const auto& spec : specs) {
        if (spec.slash.starts_with(prefix)) {
            if (spec.slash == prefix) exact = true;
            state.autocomplete_commands.push_back(spec.slash);
            state.autocomplete_labels.push_back(spec.slash + "  " + spec.summary);
        }
    }

    if (exact && state.autocomplete_labels.size() == 1) {
        state.is_autocomplete_open = false;
    } else if (!state.autocomplete_labels.empty()) {
        state.is_autocomplete_open = true;
        state.autocomplete_selected = std::clamp(state.autocomplete_selected,
            0, static_cast<int>(state.autocomplete_labels.size()) - 1);
    } else {
        state.is_autocomplete_open = false;
    }
}

struct CmdHint {
    const char* no_args;
    const char* with_args;
};

const std::unordered_map<std::string, CmdHint>& hint_table() {
    static const std::unordered_map<std::string, CmdHint> table = {
        {"/pull",   {"用法: /pull <url>  例: /pull https://www.luogu.com.cn/problem/P1000",
                     "✓ Enter 执行拉取"}},
        {"/solve",  {"用法: /solve <题号>  例: /solve 1",
                     "✓ Enter 开始做题"}},
        {"/test",   {"用法: /test <题号>  编译并测试你的代码",
                     "✓ Enter 运行测试"}},
        {"/hint",   {"用法: /hint <题号>  AI 分析题目并给出解题思路",
                     "✓ Enter 获取 AI 提示"}},
        {"/view",   {"用法: /view <题号>  查看测试用例和题目信息",
                     "✓ Enter 查看详情"}},
        {"/delete", {"用法: /delete <题号>  从本地题库删除题目",
                     "✓ Enter 删除题目"}},
        {"/record", {"用法: /record <题号>  复习推荐检查完成并记录",
                     "✓ Enter 记录完成度"}},
        {"/new",    {"用法: /new <标题>  创建一道本地题目",
                     "✓ Enter 创建题目"}},
        {"/login",  {"用法: /login <平台>  例: /login lanqiao",
                     "✓ Enter 配置登录"}},
        {"/list",   {"Enter 打开题库浏览器, 支持筛选和快捷操作", nullptr}},
        {"/config", {"Enter 打开配置编辑器", nullptr}},
        {"/help",   {"Enter 查看全部可用命令及用法", nullptr}},
        {"/?",      {"Enter 查看全部可用命令及用法", nullptr}},
        {"/init",   {"Enter 在当前目录初始化 .shuati 项目结构", nullptr}},
        {"/uninstall", {"Enter 查看并清除所有相关记录及 .shuati 文件夹", nullptr}},
        {"/info",   {"Enter 显示当前环境和版本信息", nullptr}},
        {"/clean",  {"Enter 清理临时文件和缓存", nullptr}},
        {"/exit",   {"Enter 退出程序", nullptr}},
        {"/repl",   {"Enter 进入经典命令行模式", nullptr}},
    };
    return table;
}

std::string get_input_hint(const std::string& input) {
    if (input.empty()) {
        return "输入 / 开头的命令, 例如 /help 查看帮助";
    }

    auto start = input.find_first_not_of(' ');
    if (start == std::string::npos || input[start] != '/') {
        return "命令需以 / 开头, 例如 /help";
    }

    auto space_pos = input.find(' ', start);
    std::string cmd = (space_pos != std::string::npos) ? input.substr(start, space_pos - start) : input.substr(start);
    bool has_args = (space_pos != std::string::npos);

    auto& table = hint_table();
    auto it = table.find(cmd);
    if (it == table.end()) return "";

    const auto& h = it->second;
    if (has_args && h.with_args) return h.with_args;
    return h.no_args;
}

void load_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "未找到 .shuati 项目，请先运行 /init";
        state.config_state.loaded = true;
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    state.config_state.language       = cfg.language;
    state.config_state.editor         = cfg.editor;
    state.config_state.api_key        = cfg.api_key;
    state.config_state.model          = cfg.model;
    state.config_state.ui_mode        = cfg.ui_mode;
    state.config_state.autostart_repl = cfg.autostart_repl;
    state.config_state.max_tokens     = cfg.max_tokens;
    state.config_state.ai_enabled     = cfg.ai_enabled;
    state.config_state.template_enabled = cfg.template_enabled;
    state.config_state.lanqiao_cookie = cfg.lanqiao_cookie;
    state.config_state.focused_field  = 0;
    state.config_state.status_msg.clear();
    state.config_state.loaded         = true;
}

void save_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "未找到 .shuati 项目，请先运行 /init";
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    cfg.language       = state.config_state.language;
    cfg.editor         = state.config_state.editor;
    cfg.api_key        = state.config_state.api_key;
    cfg.model          = state.config_state.model;
    cfg.ui_mode        = state.config_state.ui_mode;
    cfg.autostart_repl = state.config_state.autostart_repl;
    cfg.max_tokens     = state.config_state.max_tokens;
    cfg.ai_enabled     = state.config_state.ai_enabled;
    cfg.template_enabled = state.config_state.template_enabled;
    cfg.lanqiao_cookie = state.config_state.lanqiao_cookie;
    cfg.save(Config::config_path(root));
    state.config_state.status_msg = "配置已保存。";
}

void load_list_state(AppState& state, const std::string& status_filter = "all", const std::string& difficulty_filter = "all", const std::string& source_filter = "all") {
    auto root = Config::find_root();
    state.list_state.status_filter = status_filter;
    state.list_state.difficulty_filter = difficulty_filter;
    state.list_state.source_filter = source_filter;
    if (root.empty()) {
        state.list_state.error = "未找到 .shuati 项目，请先运行 /init";
        state.list_state.rows.clear();
        state.list_state.loaded = true;
        return;
    }
    try {
        auto svc = cmd::Services::load(root);
        auto problems = svc.pm->filter_problems(svc.pm->list_problems(), status_filter, difficulty_filter, source_filter);
        
        auto current_time = std::time(nullptr);
        auto reviews = svc.db->get_due_reviews(current_time);
        std::unordered_set<std::string> due_ids;
        for (const auto& r : reviews) due_ids.insert(r.problem_id);

        std::string prev_id;
        if (state.list_state.selected < static_cast<int>(state.list_state.rows.size())) {
            prev_id = state.list_state.rows[state.list_state.selected].id;
        }
        state.list_state.rows.clear();
        for (const auto& p : problems) {
            ListState::Row row;
            row.tid        = p.display_id;
            row.id         = p.id;
            row.title      = p.title;
            row.difficulty = p.difficulty;
            row.source     = cmd::canonical_source(p.source);
            row.status     = p.last_verdict.empty() ? "-" : p.last_verdict;
            row.passed     = (p.last_verdict == "AC");
            row.review_due = (due_ids.find(p.id) != due_ids.end());
            {
                char buf[16] = {};
                std::time_t t = static_cast<std::time_t>(p.created_at);
                struct std::tm tm_val{};
#ifdef _WIN32
                localtime_s(&tm_val, &t);
#else
                localtime_r(&t, &tm_val);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
                row.date = buf;
            }
            state.list_state.rows.push_back(std::move(row));
        }
        state.list_state.selected = 0;
        if (!prev_id.empty()) {
            for (int i = 0; i < static_cast<int>(state.list_state.rows.size()); ++i) {
                if (state.list_state.rows[i].id == prev_id) {
                    state.list_state.selected = i;
                    break;
                }
            }
        }
        state.list_state.error.clear();
    } catch (const std::exception& e) {
        state.list_state.rows.clear();
        state.list_state.error = std::string("数据库错误: ") + e.what();
    }
    state.list_state.loaded = true;
}

LineType classify_output_line(const std::string& text) {
    if (text.empty()) return LineType::Output;
    if (text.starts_with("[Error]") || text.starts_with("[!]") || text.starts_with("Error:")) return LineType::Error;
    if (text.starts_with("[+]") || text.starts_with("[*]") ||
        text.starts_with("[Hint]") || text.starts_with("[TUI]") ||
        text.starts_with("[Coach]")) {
        return LineType::SystemLog;
    }
    return LineType::Output;
}

} // namespace

int TuiApp::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    configure_windows_console_for_tui();
#endif

    auto screen = ScreenInteractive::Fullscreen();
    TuiTheme theme;
    AppState state;

    std::string version = current_version().to_string();
    auto specs = tui_command_specs();

    std::string project_path;
    {
        auto root = Config::find_root();
        if (!root.empty()) project_path = root.string();
    }

    auto alive = std::make_shared<std::atomic_bool>(true);
    std::vector<std::thread> workers;
    std::mutex workers_mutex;

    auto cleanup_workers = [&]() {
        std::lock_guard<std::mutex> lock(workers_mutex);
        for (auto it = workers.begin(); it != workers.end(); ) {
            if (it->joinable()) {
                // We can't easily check if a thread is done without a flag, 
                // but for simplicity in this refactor, we just keep them.
                // In a production app, we'd use a thread pool.
                it++;
            } else {
                it = workers.erase(it);
            }
        }
    };

    auto quit = [&] {
        alive->store(false);
        state.pending_commands.clear();
        state.is_running = false;
        state.active_command.clear();
        screen.ExitLoopClosure()();
    };

    state.post_task = [&screen](std::function<void()> task) {
        screen.Post(std::move(task));
    };

    auto run_command_ptr = std::make_shared<std::function<void(const std::string&, bool)>>();
    *run_command_ptr = [&](const std::string& line, bool record_input) {
        std::string trimmed = line;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) return;

        if (record_input) {
            state.push_history(trimmed);
            state.append(LineType::Input, trimmed);
        }

        std::string normalized = trimmed;
        if (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());

        auto args = parse_command_line(normalized);
        if (args.size() <= 1) return;

        std::string base_cmd = args[1];

        if (base_cmd == "solve" && args.size() > 2) {
            state.append(LineType::SystemLog, "[TUI] 准备开始做题: " + args[2]);
        }

        if (base_cmd == "?" || base_cmd == "help") {
            state.append(LineType::Heading, "帮助");
            state.append_output_lines(render_help_text());
            return;
        }
        if (base_cmd == "clear" || base_cmd == "cls") {
            state.buffer.clear();
            state.scroll_offset = 0;
            return;
        }
        if (base_cmd == "exit" || base_cmd == "quit") {
            quit();
            return;
        }

        if (base_cmd == "menu") {
            state.menu_state.init_from_specs();
            state.push_view(ViewMode::MenuView);
            return;
        }

        if (base_cmd == "hint") {
            if (args.size() < 3) {
                state.append(LineType::Error, "[!] 用法: /hint <题号>  例: /hint 1");
                return;
            }
            if (state.is_running) { state.pending_commands.push_back(trimmed); return; }
            
            // Check if API key is configured before starting
            auto root = Config::find_root();
            if (root.empty()) {
                state.append(LineType::Error, "[!] 未找到 .shuati 项目，请先运行 /init");
                return;
            }
            auto cfg = Config::load(Config::config_path(root));
            if (cfg.api_key.empty()) {
                state.append(LineType::Error, "[!] API Key 未配置，请使用 /config 设置 API Key");
                return;
            }
            
            state.hint_state = HintState{};
            state.hint_state.loading = true;
            state.push_view(ViewMode::HintView);
            state.is_running = true;
            state.active_command = base_cmd;

            std::lock_guard<std::mutex> lock(workers_mutex);
            workers.emplace_back([alive, run_command_ptr, &screen, &state, args = std::move(args), base_cmd]() mutable {
                auto buffer_mtx = std::make_shared<std::mutex>();
                auto buffer_str = std::make_shared<std::string>();
                auto last_post = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());

                auto cb = [alive, buffer_mtx, buffer_str, last_post, &screen, &state](const std::string& chunk) {
                    if (!alive->load()) return;
                    if (chunk.empty()) return;
                    
                    std::lock_guard<std::mutex> lk(*buffer_mtx);
                    *buffer_str += chunk;

                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *last_post).count() < 50) {
                        return; // Debounce to avoid Event Loop flooding
                    }
                    *last_post = now;

                    // UTF-8 boundary check
                    int valid_len = buffer_str->size();
                    while (valid_len > 0) {
                        unsigned char c = (*buffer_str)[valid_len - 1];
                        if ((c & 0xC0) != 0x80) { // Start of a UTF-8 char
                            int char_len = 1;
                            if ((c & 0xE0) == 0xC0) char_len = 2;
                            else if ((c & 0xF0) == 0xE0) char_len = 3;
                            else if ((c & 0xF8) == 0xF0) char_len = 4;
                            
                            if (valid_len - 1 + char_len > (int)buffer_str->size()) {
                                valid_len -= 1; // Incomplete, trim it from this chunk
                            }
                            break;
                        }
                        valid_len--;
                    }

                    if (valid_len <= 0) return;

                    std::string to_post = buffer_str->substr(0, valid_len);
                    *buffer_str = buffer_str->substr(valid_len);

                    screen.Post([&state, to_post]() {
                        std::string lines = to_post;
                        for (auto& c : lines) if (c == '\r') c = '\n';
                        
                        state.hint_state.loading = false;
                        std::string::size_type start = 0;
                        while (start < lines.size()) {
                            auto nl = lines.find('\n', start);
                            if (nl == std::string::npos) {
                                if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                                state.hint_state.lines.back() += lines.substr(start);
                                break;
                            }
                            if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                            state.hint_state.lines.back() += lines.substr(start, nl - start);
                            state.hint_state.lines.push_back("");
                            start = nl + 1;
                        }
                        // Auto-scroll to the bottom during streaming
                        int total_lines = static_cast<int>(state.hint_state.lines.size());
                        state.hint_state.scroll_offset = std::max(0, total_lines - 15);
                    });
                };
                
                tui_execute_command_stream(args, base_cmd, cb);
                
                if (!alive->load()) return;

                std::string remainder;
                {
                    std::lock_guard<std::mutex> lk(*buffer_mtx);
                    remainder = *buffer_str;
                    buffer_str->clear();
                }

                auto run_command_ptr_local = run_command_ptr;
                screen.Post([&state, run_command_ptr_local, remainder] {
                    if (!remainder.empty()) {
                        std::string lines = remainder;
                        for (auto& c : lines) if (c == '\r') c = '\n';
                        std::string::size_type start = 0;
                        while (start < lines.size()) {
                            auto nl = lines.find('\n', start);
                            if (nl == std::string::npos) {
                                if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                                state.hint_state.lines.back() += lines.substr(start);
                                break;
                            }
                            if (state.hint_state.lines.empty()) state.hint_state.lines.push_back("");
                            state.hint_state.lines.back() += lines.substr(start, nl - start);
                            state.hint_state.lines.push_back("");
                            start = nl + 1;
                        }
                        int total_lines = static_cast<int>(state.hint_state.lines.size());
                        state.hint_state.scroll_offset = std::max(0, total_lines - 15);
                    }

                    state.hint_state.loading = false;
                    state.is_running = false;
                    state.active_command.clear();
                    if (!state.pending_commands.empty()) {
                        auto next = state.pending_commands.front();
                        state.pending_commands.pop_front();
                        (*run_command_ptr_local)(next, false);
                    }
                });
            });
            return;
        }

        if (base_cmd == "pull" && args.size() == 2) {
            state.pull_state = PullState{};
            state.push_view(ViewMode::PullView);
            return;
        }

        if (base_cmd == "solve" && args.size() == 2) {
            state.solve_state = SolveState{};
            auto root = Config::find_root();
            if (!root.empty()) {
                auto svc = cmd::Services::load(root);
                auto problems = svc.pm->list_problems();
                for (const auto& p : problems) {
                    state.solve_state.filtered_rows.push_back({
                        p.display_id, p.id, p.title, p.difficulty, cmd::canonical_source(p.source)
                    });
                }
            }
            state.push_view(ViewMode::SolveView);
            return;
        }

        if (base_cmd == "status") {
            state.status_state = StatusState{};
            state.push_view(ViewMode::StatusView);
            std::lock_guard<std::mutex> lock(workers_mutex);
            workers.emplace_back([&screen, &state]() {
                try {
                    auto root = Config::find_root();
                    auto svc = cmd::Services::load(root);
                    auto problems = svc.pm->list_problems();
                    int ac = 0;
                    for (const auto& p : problems) if (p.last_verdict == "AC") ac++;
                    auto reviews = svc.db->get_due_reviews(std::time(nullptr));
                    screen.Post([&state, total = static_cast<int>(problems.size()), ac, pending = static_cast<int>(reviews.size())]() {
                        state.status_state.total_problems = total;
                        state.status_state.ac_problems = ac;
                        state.status_state.pending_reviews = pending;
                        state.status_state.last_activity = "刚刚";
                        state.status_state.loaded = true;
                    });
                } catch (...) {
                    screen.Post([&state]() { state.status_state.loaded = true; });
                }
            });
            return;
        }

        if (base_cmd == "config" && args.size() == 2) {
            load_config_state(state);
            state.push_view(ViewMode::ConfigView);
            return;
        }

        if (base_cmd == "list") {
            load_list_state(state);
            state.push_view(ViewMode::ListView);
            return;
        }

        if (state.is_running) { state.pending_commands.push_back(trimmed); return; }
        state.is_running = true;
        state.active_command = base_cmd;

        std::lock_guard<std::mutex> lock(workers_mutex);
        workers.emplace_back([alive, run_command_ptr, &screen, &state, args = std::move(args), base_cmd]() mutable {
            auto buffer_mtx = std::make_shared<std::mutex>();
            auto buffer_str = std::make_shared<std::string>();
            auto last_post = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());

            auto cb = [alive, buffer_mtx, buffer_str, last_post, &screen, &state](const std::string& chunk) {
                if (!alive->load()) return;
                if (chunk.empty()) return;
                
                std::lock_guard<std::mutex> lk(*buffer_mtx);
                *buffer_str += chunk;

                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *last_post).count() < 50) return;
                *last_post = now;

                int valid_len = buffer_str->size();
                while (valid_len > 0) {
                    unsigned char c = (*buffer_str)[valid_len - 1];
                    if ((c & 0xC0) != 0x80) {
                        int char_len = 1;
                        if ((c & 0xE0) == 0xC0) char_len = 2;
                        else if ((c & 0xF0) == 0xE0) char_len = 3;
                        else if ((c & 0xF8) == 0xF0) char_len = 4;
                        if (valid_len - 1 + char_len > (int)buffer_str->size()) valid_len -= 1;
                        break;
                    }
                    valid_len--;
                }
                if (valid_len <= 0) return;

                std::string to_post = buffer_str->substr(0, valid_len);
                *buffer_str = buffer_str->substr(valid_len);

                screen.Post([&state, to_post]() {
                    std::string lines = to_post;
                    for (auto& c : lines) if (c == '\r') c = '\n';
                    std::string::size_type start = 0;
                    while (start < lines.size()) {
                        auto nl = lines.find('\n', start);
                        std::string seg = (nl == std::string::npos) ? lines.substr(start) : lines.substr(start, nl - start);
                        if (!seg.empty() || nl != std::string::npos) state.buffer.push_back({classify_output_line(seg), seg});
                        if (nl == std::string::npos) break;
                        start = nl + 1;
                    }
                    state.scroll_offset = 0;
                });
            };
            tui_execute_command_stream(args, base_cmd, cb);
            if (!alive->load()) return;

            std::string remainder;
            {
                std::lock_guard<std::mutex> lk(*buffer_mtx);
                remainder = *buffer_str;
                buffer_str->clear();
            }

            auto run_command_ptr_local = run_command_ptr;
            screen.Post([&state, run_command_ptr_local, base_cmd, remainder]() {
                if (!remainder.empty()) {
                    std::string lines = remainder;
                    for (auto& c : lines) if (c == '\r') c = '\n';
                    std::string::size_type start = 0;
                    while (start < lines.size()) {
                        auto nl = lines.find('\n', start);
                        std::string seg = (nl == std::string::npos) ? lines.substr(start) : lines.substr(start, nl - start);
                        if (!seg.empty() || nl != std::string::npos) state.buffer.push_back({classify_output_line(seg), seg});
                        if (nl == std::string::npos) break;
                        start = nl + 1;
                    }
                    state.scroll_offset = 0;
                }

                state.is_running = false;
                state.active_command.clear();
                if (base_cmd == "record" || base_cmd == "delete") load_list_state(state);
                if (!state.pending_commands.empty()) {
                    auto next = state.pending_commands.front();
                    state.pending_commands.pop_front();
                    (*run_command_ptr_local)(next, false);
                }
            });
        });
    };

    // --- Main View Components ---
    int main_cursor_pos = 0;
    InputOption main_input_opt;
    main_input_opt.cursor_position = &main_cursor_pos;
    main_input_opt.on_change = [&] { update_autocomplete(state, specs); };
    main_input_opt.on_enter = [&] {
        if (state.input.empty()) return;
        std::string cmd = std::move(state.input);
        state.input.clear();
        main_cursor_pos = 0;
        state.is_autocomplete_open = false;
        state.history_index = -1;
        (*run_command_ptr)(cmd, true);
    };
    auto main_input_comp = Input(&state.input, "", main_input_opt);

    auto main_view = Renderer(main_input_comp, [&] {
        auto buffer_content = render_buffer(state, theme);
        auto buffer_area = hbox({ text("  "), buffer_content | focusPositionRelative(0, 1) | frame | flex, text("  ") }) | flex;

        Element ac_element = emptyElement();
        if (state.is_autocomplete_open && !state.autocomplete_labels.empty()) {
            Elements ac_items;
            for (int i = 0; i < static_cast<int>(state.autocomplete_labels.size()); i++) {
                auto item = text("  " + state.autocomplete_labels[i] + "  ");
                if (i == state.autocomplete_selected) item = item | bold | bgcolor(theme.selected_bg) | color(theme.accent_color);
                else item = item | color(theme.dim_color);
                ac_items.push_back(item);
            }
            ac_element = vbox(std::move(ac_items)) | size(HEIGHT, LESS_THAN, 8) | borderStyled(ROUNDED) | color(theme.border_color);
        }

        std::string hint = get_input_hint(state.input);
        Element hint_line = hint.empty() ? emptyElement() : text("  " + hint) | color(theme.dim_color);

        auto prompt_char = state.is_running ? "  " : "> ";
        auto input_line = hbox({ text(" ") | color(theme.dim_color), text(prompt_char) | bold | color(theme.prompt_color), main_input_comp->Render() | flex });

        return vbox({ buffer_area, ac_element, separatorLight() | color(theme.border_color), hint_line, input_line });
    });

    // --- Pull View Components ---
    InputOption pull_input_opt;
    pull_input_opt.on_enter = [&] {
        if (state.pull_state.url.empty() || state.pull_state.loading) return;
        std::string url = state.pull_state.url;
        if (url.find("luogu.com") == std::string::npos && url.find("leetcode") == std::string::npos &&
            url.find("codeforces.com") == std::string::npos && url.find("lanqiao.cn") == std::string::npos) {
            state.pull_state.error = "非法 URL，请输入正确的题目地址。";
            return;
        }
        state.pull_state.error.clear();
        state.pull_state.loading = true;
        state.pull_state.progress = 0.1f;
        state.pull_state.status_msg = "正在分析 URL...";

        std::lock_guard<std::mutex> lock(workers_mutex);
        workers.emplace_back([alive, &screen, &state, url]() mutable {
            try {
                screen.Post([&state]() { state.pull_state.progress = 0.3f; state.pull_state.status_msg = "正在连接题库..."; });
                tui_execute_command_stream({"shuati", "pull", url}, "pull", [&](const std::string& chunk) {
                    screen.Post([&state, chunk]() {
                        if (chunk.find("Downloaded") != std::string::npos) state.pull_state.progress = 0.7f;
                        if (chunk.find("Saved") != std::string::npos) state.pull_state.progress = 0.9f;
                    });
                });
                auto root = Config::find_root();
                auto svc = cmd::Services::load(root);
                auto problems = svc.pm->list_problems();
                if (!problems.empty()) {
                    const auto& p = problems.back();
                    screen.Post([&state, p]() {
                        state.pull_state.result.tid = p.display_id;
                        state.pull_state.result.title = p.title;
                        state.pull_state.result.path = p.content_path;
                        state.pull_state.finished = true;
                        state.pull_state.loading = false;
                    });
                }
            } catch (const std::exception& e) {
                screen.Post([&state, err = std::string(e.what())]() { state.pull_state.error = err; state.pull_state.finished = true; state.pull_state.loading = false; });
            }
            for (int i = 3; i > 0; i--) {
                if (!alive->load()) break;
                screen.Post([&state, i]() { state.pull_state.countdown = i; });
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            screen.Post([&state]() { if (state.view_mode == ViewMode::PullView) state.pop_view(); });
        });
    };
    auto pull_input_comp = Input(&state.pull_state.url, "请粘贴 URL 并回车", pull_input_opt);
    auto pull_view = Renderer(pull_input_comp, [&] { return render_pull_view(state, theme, pull_input_comp); });

    // --- Solve View Components ---
    auto refresh_solve_list = [&] {
        auto root = Config::find_root();
        if (root.empty()) return;
        auto svc = cmd::Services::load(root);
        auto problems = svc.pm->list_problems();
        state.solve_state.filtered_rows.clear();
        for (const auto& p : problems) {
            std::string src = cmd::canonical_source(p.source);
            bool src_match = std::find(state.solve_state.selected_sources.begin(), state.solve_state.selected_sources.end(), src) != state.solve_state.selected_sources.end();
            bool diff_match = std::find(state.solve_state.selected_difficulties.begin(), state.solve_state.selected_difficulties.end(), p.difficulty) != state.solve_state.selected_difficulties.end();
            bool search_match = state.solve_state.search_query.empty() || p.title.find(state.solve_state.search_query) != std::string::npos || std::to_string(p.display_id).find(state.solve_state.search_query) != std::string::npos;
            if (src_match && diff_match && search_match) state.solve_state.filtered_rows.push_back({ p.display_id, p.id, p.title, p.difficulty, src });
        }
        state.solve_state.selected_idx = 0;
    };
    InputOption solve_search_opt;
    solve_search_opt.on_change = refresh_solve_list;
    auto solve_search_comp = Input(&state.solve_state.search_query, "搜索题号或标题...", solve_search_opt);
    auto solve_view = Renderer(solve_search_comp, [&] { return render_solve_view(state, theme, solve_search_comp); });

    // --- Static Views ---
    auto config_view = Renderer([&] { return render_config_view(state, theme); });
    auto list_view = Renderer([&] { return render_list_view(state, theme); });
    auto hint_view = Renderer([&] { return render_hint_view(state, theme); });
    auto status_view = Renderer([&] { return render_status_view(state, theme); });
    auto menu_view = Renderer([&] { return render_menu_view(state, theme); });

    // --- Main Router ---
    auto main_router = Container::Tab({
        main_view, config_view, list_view, hint_view, pull_view, solve_view, status_view, menu_view
    }, (int*)&state.view_mode);

    auto global_handler = CatchEvent(main_router, [&](Event event) -> bool {
        if (event == Event::Character('\x03')) { quit(); return true; }
        
        if (event == Event::Tab) { 
            state.active_pane = 1 - state.active_pane; 
            return true; 
        }
        if (event == Event::ArrowLeft && state.active_pane == 1) {
            state.active_pane = 0;
            return true;
        }
        if (event == Event::ArrowRight && state.active_pane == 0) {
            state.active_pane = 1;
            return true;
        }

        if (state.active_pane == 0) {
            if (event == Event::ArrowUp || event == Event::Character('k')) {
                state.sidebar_state.selected = std::max(0, state.sidebar_state.selected - 1);
                return true;
            }
            if (event == Event::ArrowDown || event == Event::Character('j')) {
                state.sidebar_state.selected = std::min((int)state.sidebar_state.categories.size() - 1, state.sidebar_state.selected + 1);
                return true;
            }
            if (event == Event::Return) {
                int sel = state.sidebar_state.selected;
                if (sel == 0) state.view_mode = ViewMode::Main;
                else if (sel == 1) { load_list_state(state); state.view_mode = ViewMode::ListView; }
                else if (sel == 2) { (*run_command_ptr)("/solve", false); }
                else if (sel == 3) { (*run_command_ptr)("/status", false); }
                else if (sel == 4) { load_config_state(state); state.view_mode = ViewMode::ConfigView; }
                state.active_pane = 1;
                return true;
            }
            return false;
        }
        
        if (state.view_mode == ViewMode::Main) {
            if (event == Event::Character('\x0C')) { state.buffer.clear(); state.scroll_offset = 0; return true; }
            if (event == Event::Character('\x15')) { state.input.clear(); main_cursor_pos = 0; state.is_autocomplete_open = false; return true; }
            if (state.is_autocomplete_open) {
                if (event == Event::ArrowDown) { state.autocomplete_selected = std::min((int)state.autocomplete_labels.size() - 1, state.autocomplete_selected + 1); return true; }
                if (event == Event::ArrowUp) { state.autocomplete_selected = std::max(0, state.autocomplete_selected - 1); return true; }
                if (event == Event::Tab || event == Event::Return) {
                    if (state.autocomplete_selected >= 0 && state.autocomplete_selected < (int)state.autocomplete_commands.size()) {
                        std::string cmd = state.autocomplete_commands[state.autocomplete_selected];
                        
                        // Silent jump logic: if the command has a direct view mapping, jump to it
                        bool jumped = false;
                        for (const auto& spec : specs) {
                            if (spec.slash == cmd) {
                                if (cmd == "/config") { state.push_view(ViewMode::ConfigView); jumped = true; }
                                else if (cmd == "/list") { state.push_view(ViewMode::ListView); jumped = true; }
                                else if (cmd == "/status") { state.push_view(ViewMode::StatusView); jumped = true; }
                                else if (cmd == "/menu") { state.push_view(ViewMode::MenuView); jumped = true; }
                                break;
                            }
                        }

                        if (jumped) {
                            state.input.clear();
                            main_cursor_pos = 0;
                            state.is_autocomplete_open = false;
                        } else {
                            state.input = cmd + " ";
                            main_cursor_pos = (int)state.input.size();
                            update_autocomplete(state, specs);
                        }
                    }
                    return true;
                }
                if (event == Event::Escape) { state.is_autocomplete_open = false; return true; }
            } else if (!state.command_history.empty()) {
                if (event == Event::ArrowUp) {
                    if (state.history_index == -1) { state.history_stash = state.input; state.history_index = (int)state.command_history.size() - 1; }
                    else if (state.history_index > 0) state.history_index--;
                    state.input = state.command_history[state.history_index];
                    main_cursor_pos = (int)state.input.size();
                    return true;
                }
                if (event == Event::ArrowDown && state.history_index >= 0) {
                    state.history_index++;
                    if (state.history_index >= (int)state.command_history.size()) { state.history_index = -1; state.input = state.history_stash; state.history_stash.clear(); }
                    else state.input = state.command_history[state.history_index];
                    main_cursor_pos = (int)state.input.size();
                    return true;
                }
            }
            if (event == Event::PageUp || event == Event::Special("\x1B[5~")) { state.scroll_offset = std::min(state.scroll_offset + 5, std::max(0, (int)state.buffer.size() - 20)); return true; }
            if (event == Event::PageDown || event == Event::Special("\x1B[6~")) { state.scroll_offset = std::max(state.scroll_offset - 5, 0); return true; }
        } else {
            if (event == Event::Escape) {
                if (state.view_mode == ViewMode::SolveView && state.solve_state.show_preview) {
                    state.solve_state.show_preview = false;
                } else {
                    state.pop_view();
                }
                return true;
            }
            
            if (state.view_mode == ViewMode::ConfigView) {
                if (event == Event::ArrowUp) { state.config_state.focused_field = std::max(0, state.config_state.focused_field - 1); return true; }
                if (event == Event::ArrowDown) { state.config_state.focused_field = std::min(ConfigState::TOTAL_ITEMS - 1, state.config_state.focused_field + 1); return true; }
                if (event == Event::Return) {
                    auto& cs = state.config_state;
                    if (cs.focused_field == ConfigState::SAVE_INDEX) { save_config_state(state); return true; }
                    if (cs.focused_field == ConfigState::CANCEL_INDEX) { state.pop_view(); return true; }
                    if (cs.focused_field == 0) { cs.language = (cs.language == "cpp") ? "python" : "cpp"; return true; }
                    if (cs.focused_field == 4) { cs.ui_mode = (cs.ui_mode == "tui") ? "legacy" : "tui"; return true; }
                    if (cs.focused_field == 5) { cs.autostart_repl = !cs.autostart_repl; return true; }
                    if (cs.focused_field == 7) { cs.ai_enabled = !cs.ai_enabled; return true; }
                    if (cs.focused_field == 8) { cs.template_enabled = !cs.template_enabled; return true; }
                }
            } else if (state.view_mode == ViewMode::ListView) {
                auto& ls = state.list_state;
                if (event == Event::ArrowUp || event == Event::Character('k')) { ls.selected = std::max(0, ls.selected - 1); return true; }
                if (event == Event::ArrowDown || event == Event::Character('j')) { ls.selected = std::min((int)ls.rows.size() - 1, ls.selected + 1); return true; }
                if (event == Event::Return && !ls.rows.empty()) { std::string tid = std::to_string(ls.rows[ls.selected].tid); state.pop_view(); (*run_command_ptr)("/solve " + tid, true); return true; }
            } else if (state.view_mode == ViewMode::HintView) {
                auto& hs = state.hint_state;
                if (event == Event::ArrowLeft) {
                    hs.selected_action = std::max(0, hs.selected_action - 1);
                    return true;
                }
                if (event == Event::ArrowRight) {
                    hs.selected_action = std::min(static_cast<int>(hs.actions.size()) - 1, hs.selected_action + 1);
                    return true;
                }
                if (event == Event::Return && !hs.lines.empty()) {
                    // Silent jump based on action
                    std::string label = hs.actions[hs.selected_action];
                    if (label.find("Solve") != std::string::npos) {
                        state.pop_view(); state.push_view(ViewMode::SolveView);
                    } else if (label.find("Test") != std::string::npos) {
                        // For /test, we need a problem ID. Try to extract from current command context or state
                        // Actually, just go to SolveView which lists problems
                        state.pop_view(); state.push_view(ViewMode::SolveView);
                    } else if (label.find("List") != std::string::npos) {
                        state.pop_view(); state.push_view(ViewMode::ListView);
                    }
                    return true;
                }
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    hs.scroll_offset = std::max(0, hs.scroll_offset - 1);
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    int max_scroll = std::max(0, static_cast<int>(hs.lines.size()) - 1);
                    hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 1);
                    return true;
                }
                if (event == Event::PageUp) {
                    hs.scroll_offset = std::max(0, hs.scroll_offset - 10);
                    return true;
                }
                if (event == Event::PageDown) {
                    int max_scroll = std::max(0, static_cast<int>(hs.lines.size()) - 1);
                    hs.scroll_offset = std::min(max_scroll, hs.scroll_offset + 10);
                    return true;
                }
            } else if (state.view_mode == ViewMode::SolveView) {
                auto& ss = state.solve_state;
                if (ss.show_preview) {
                    if (event == Event::Return) {
                        std::string tid = std::to_string(ss.filtered_rows[ss.selected_idx].tid);
                        ss.show_preview = false; // Exit preview first
                        state.pop_view(); // Then pop to main
                        (*run_command_ptr)("/solve " + tid, true);
                        return true;
                    }
                    if (event == Event::Character('t') || event == Event::Character('T')) {
                        std::string tid = std::to_string(ss.filtered_rows[ss.selected_idx].tid);
                        ss.show_preview = false;
                        state.pop_view();
                        (*run_command_ptr)("/test " + tid, true);
                        return true;
                    }
                } else {
                    if (event == Event::Character('s')) {
                        if (ss.selected_sources.size() == ss.sources.size()) { ss.selected_sources = {"luogu"}; }
                        else if (ss.selected_sources.size() == 1 && ss.selected_sources[0] == "luogu") { ss.selected_sources = {"leetcode"}; }
                        else { ss.selected_sources = ss.sources; }
                        refresh_solve_list();
                        return true;
                    }
                    if (event == Event::Character('d')) {
                        if (ss.selected_difficulties.size() == 3) { ss.selected_difficulties = {"easy"}; }
                        else if (ss.selected_difficulties.size() == 1 && ss.selected_difficulties[0] == "easy") { ss.selected_difficulties = {"medium"}; }
                        else { ss.selected_difficulties = {"easy", "medium", "hard"}; }
                        refresh_solve_list();
                        return true;
                    }
                    if (event == Event::ArrowUp || event == Event::Character('k')) { ss.selected_idx = std::max(0, ss.selected_idx - 1); return true; }
                    if (event == Event::ArrowDown || event == Event::Character('j')) { ss.selected_idx = std::min((int)ss.filtered_rows.size() - 1, ss.selected_idx + 1); return true; }
                    if (event == Event::Return && !ss.filtered_rows.empty()) {
                        ss.show_preview = true; ss.preview_content = "正在加载题面...";
                        int tid = ss.filtered_rows[ss.selected_idx].tid;
                        std::lock_guard<std::mutex> lock(workers_mutex);
                        workers.emplace_back([&screen, &state, tid]() {
                            try {
                                auto svc = cmd::Services::load(Config::find_root());
                                auto p = svc.pm->get_problem(std::to_string(tid));
                                screen.Post([&state, content = p.description]() { state.solve_state.preview_content = content; });
                            } catch (...) { screen.Post([&state]() { state.solve_state.preview_content = "无法加载题面。"; }); }
                        });
                        return true;
                    }
                }
            } else if (state.view_mode == ViewMode::MenuView) {
                auto& ms = state.menu_state;
                if (ms.categories.empty()) {
                    state.pop_view();
                    return true;
                }
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    int current_cat_items = static_cast<int>(ms.categories[ms.selected_category].items.size());
                    if (current_cat_items == 0) {
                        if (ms.selected_category > 0) {
                            ms.selected_category--;
                            ms.selected_item = 0;
                        }
                    } else {
                        if (ms.selected_item > 0) {
                            ms.selected_item--;
                        } else if (ms.selected_category > 0) {
                            ms.selected_category--;
                            ms.selected_item = static_cast<int>(ms.categories[ms.selected_category].items.size()) - 1;
                        }
                    }
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    int current_cat_items = static_cast<int>(ms.categories[ms.selected_category].items.size());
                    if (current_cat_items == 0) {
                        if (ms.selected_category < static_cast<int>(ms.categories.size()) - 1) {
                            ms.selected_category++;
                            ms.selected_item = 0;
                        }
                    } else if (ms.selected_item < current_cat_items - 1) {
                        ms.selected_item++;
                    } else if (ms.selected_category < static_cast<int>(ms.categories.size()) - 1) {
                        ms.selected_category++;
                        ms.selected_item = 0;
                    }
                    return true;
                }
                if (event == Event::ArrowLeft) {
                    if (ms.selected_category > 0) {
                        ms.selected_category--;
                        ms.selected_item = std::min(ms.selected_item, static_cast<int>(ms.categories[ms.selected_category].items.size()) - 1);
                        if (ms.selected_item < 0) ms.selected_item = 0;
                    }
                    return true;
                }
                if (event == Event::ArrowRight) {
                    if (ms.selected_category < static_cast<int>(ms.categories.size()) - 1) {
                        ms.selected_category++;
                        ms.selected_item = std::min(ms.selected_item, static_cast<int>(ms.categories[ms.selected_category].items.size()) - 1);
                        if (ms.selected_item < 0) ms.selected_item = 0;
                    }
                    return true;
                }
                if (event == Event::Return) {
                    const auto& item = ms.categories[ms.selected_category].items[ms.selected_item];
                    state.pop_view();
                    
                    if (!item.route_id.empty() && item.route_id != "Main") {
                        // Silent jump to view
                        if (item.route_id == "ConfigView") state.push_view(ViewMode::ConfigView);
                        else if (item.route_id == "ListView") state.push_view(ViewMode::ListView);
                        else if (item.route_id == "HintView") state.push_view(ViewMode::HintView);
                        else if (item.route_id == "PullView") state.push_view(ViewMode::PullView);
                        else if (item.route_id == "SolveView") state.push_view(ViewMode::SolveView);
                        else if (item.route_id == "StatusView") state.push_view(ViewMode::StatusView);
                        else if (item.route_id == "MenuView") state.push_view(ViewMode::MenuView);
                    } else if (item.requires_args) {
                        state.input = item.command + " ";
                        main_cursor_pos = (int)state.input.size();
                    } else {
                        (*run_command_ptr)(item.command, true);
                    }
                    return true;
                }
            }
        }
        return false;
    });

    auto root_layout = Renderer(global_handler, [&] {
        cleanup_workers();
        auto top_bar = render_top_bar(theme, version, project_path);
        auto bottom_bar = render_bottom_bar(theme, state.is_running, state.active_command);
        Element separator_line = separatorLight() | color(theme.border_color);

        auto content = hbox({
            render_sidebar(state, theme),
            separatorLight() | color(theme.border_color),
            main_router->Render() | flex
        });

        return vbox({
            top_bar,
            separator_line,
            content | flex,
            separator_line,
            bottom_bar,
        }) | borderStyled(ROUNDED);
    });

    screen.Loop(root_layout);
    alive->store(false);
    std::lock_guard<std::mutex> lock(workers_mutex);
    for (auto& t : workers) if (t.joinable()) t.detach();
    return 0;
}

} // namespace tui
} // namespace shuati
