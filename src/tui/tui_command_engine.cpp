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
#include <string>
#include <mutex>
#include <vector>
#include <thread>

namespace shuati {
namespace tui {

namespace {

// ---------------------------------------------------------------------------
// CallbackStreamBuf
// Intercepts std::cout / std::cerr writes and forwards them to a callback.
// Checks thread ID to avoid intercepting FTXUI's main thread rendering.
// ---------------------------------------------------------------------------
class CallbackStreamBuf : public std::streambuf {
public:
    CallbackStreamBuf(std::function<void(const std::string&)> cb, std::streambuf* real_buf)
        : cb_(std::move(cb)), real_buf_(real_buf), cmd_thread_id_(std::this_thread::get_id()) {}

protected:
    int overflow(int c) override {
        if (std::this_thread::get_id() == cmd_thread_id_) {
            if (c != EOF) {
                char ch = static_cast<char>(c);
                // Treat carriage-return as line overwrite boundary.
                // We convert it into '\n' so UI streaming can flush promptly.
                if (ch == '\r') ch = '\n';
                buf_ += ch;
                if (ch == '\n' || buf_.size() >= 256) flush_buf();
            }
            return c;
        } else if (real_buf_) {
            return real_buf_->sputc(c);
        }
        return EOF;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        if (std::this_thread::get_id() == cmd_thread_id_) {
            for (std::streamsize i = 0; i < n; i++) {
                char ch = s[i];
                if (ch == '\r') ch = '\n';
                buf_.push_back(ch);
                if (ch == '\n' || buf_.size() >= 512) {
                    flush_buf();
                }
            }
            return n;
        } else if (real_buf_) {
            return real_buf_->sputn(s, n);
        }
        return 0;
    }

    int sync() override {
        if (std::this_thread::get_id() == cmd_thread_id_) {
            if (!buf_.empty()) flush_buf();
            return 0;
        } else if (real_buf_) {
            return real_buf_->pubsync();
        }
        return 0;
    }

private:
    void flush_buf() {
        if (!buf_.empty() && cb_) cb_(buf_);
        buf_.clear();
    }

    std::function<void(const std::string&)> cb_;
    std::streambuf* real_buf_;
    std::thread::id cmd_thread_id_;
    std::string buf_;
};

// ---------------------------------------------------------------------------
// RAII guard that installs CallbackStreamBuf on cout+cerr for scope duration.
// ---------------------------------------------------------------------------
class ScopedStreamRedirect {
public:
    explicit ScopedStreamRedirect(std::function<void(const std::string&)> cb)
        // Lock first so rdbuf capture in cb_buf_ is race-free.
        // Deferred construction via lambda ensures old_cout_ is captured
        // inside the locked section before cb_buf_ is fully constructed.
        : cb_buf_([&cb, this]() -> CallbackStreamBuf {
            std::lock_guard<std::mutex> lk(stream_redirect_mutex());
            old_cout_ = std::cout.rdbuf();
            old_cerr_ = std::cerr.rdbuf();
            return CallbackStreamBuf(std::move(cb), old_cout_);
        }()) {
        // cb_buf_ constructed with real old_cout_; restore under lock
        std::lock_guard<std::mutex> lk(stream_redirect_mutex());
        std::cout.rdbuf(cb_buf_.fallback_buf());
        std::cerr.rdbuf(cb_buf_.fallback_buf());
    }

    ~ScopedStreamRedirect() {
        std::cout.flush();
        std::cerr.flush();
        std::lock_guard<std::mutex> lk(stream_redirect_mutex());
        std::cout.rdbuf(old_cout_);
        std::cerr.rdbuf(old_cerr_);
    }

    ScopedStreamRedirect(const ScopedStreamRedirect&) = delete;
    ScopedStreamRedirect& operator=(const ScopedStreamRedirect&) = delete;

private:
    std::streambuf* old_cout_;
    std::streambuf* old_cerr_;
// ---------------------------------------------------------------------------
// RAII guard that installs CallbackStreamBuf on cout+cerr for scope duration.
// All rdbuf operations are serialized by the mutex to prevent concurrent swap races.
// Uses a recursive_mutex so that ~ScopedStreamRedirect can safely lock even if
// called on the same thread that constructed the object.
// ---------------------------------------------------------------------------
class ScopedStreamRedirect {
    static std::recursive_mutex& redirect_mutex() {
        static std::recursive_mutex m;
        return m;
    }

public:
    explicit ScopedStreamRedirect(std::function<void(const std::string&)> cb) {
        std::lock_guard<std::recursive_mutex> lk(redirect_mutex());
        // Capture original buffers under lock — no race with other instances
        old_cout_ = std::cout.rdbuf();
        old_cerr_ = std::cerr.rdbuf();
        // Build CallbackStreamBuf with real cout as its fallback buffer
        cb_buf_ = std::make_unique<CallbackStreamBuf>(std::move(cb), old_cout_);
        // Install it as the new rdbuf for both streams
        std::cout.rdbuf(cb_buf_.get());
        std::cerr.rdbuf(cb_buf_.get());
    }

    ~ScopedStreamRedirect() {
        std::cout.flush();
        std::cerr.flush();
        std::lock_guard<std::recursive_mutex> lk(redirect_mutex());
        // Restore original buffers (safe even if called on construction thread)
        std::cout.rdbuf(old_cout_);
        std::cerr.rdbuf(old_cerr_);
    }

    ScopedStreamRedirect(const ScopedStreamRedirect&) = delete;
    ScopedStreamRedirect& operator=(const ScopedStreamRedirect&) = delete;

private:
    std::streambuf* old_cout_ = nullptr;
    std::streambuf* old_cerr_ = nullptr;
    std::unique_ptr<CallbackStreamBuf> cb_buf_;
};

// ---------------------------------------------------------------------------
// ANSI escape and terminal control-char stripping
// ---------------------------------------------------------------------------
std::string strip_ansi_escapes(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\x1B') {
            i++;
            if (i >= input.size()) break;
            if (input[i] == '[') {
                i++;
                while (i < input.size() && (input[i] < 0x40 || input[i] > 0x7E)) i++;
                if (i < input.size()) i++;
            } else if (input[i] == ']') {
                i++;
                while (i < input.size()) {
                    if (input[i] == '\x07') { i++; break; }
                    if (input[i] == '\x1B' && i + 1 < input.size() && input[i + 1] == '\\') { i += 2; break; }
                    i++;
                }
            } else {
                i++;
            }
        } else {
            out.push_back(input[i]);
            i++;
        }
    }
    return out;
}

// Simulate terminal \r (carriage return without newline = overwrite current line),
// then strip ANSI, then strip markdown syntax, then trim trailing whitespace.
std::string normalize_text(const std::string& text) {
    std::string resolved;
    resolved.reserve(text.size());
    std::string line;
    for (char c : text) {
        if (c == '\r') {
            line.clear();
        } else if (c == '\n') {
            resolved += line;
            resolved += '\n';
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) resolved += line;

    resolved = strip_ansi_escapes(resolved);

    // Strip common markdown syntax that looks ugly in TUI
    // Keep newlines and indentation but remove formatting chars
    std::string stripped;
    stripped.reserve(resolved.size());
    size_t i = 0;
    while (i < resolved.size()) {
        char c = resolved[i];
        // Skip inline code markers (`) and bold/italic markers (*_)
        if (c == '`' || c == '*' || c == '_') {
            // Skip runs of formatting chars, but keep line-start emphasis
            // e.g. "**bold**" -> "bold", "## 标题" -> keep ## at line start
            size_t j = i;
            while (j < resolved.size() && resolved[j] == c) j++;
            int run = (int)(j - i);
            // "## " at line start is a heading, keep it
            bool at_line_start = (stripped.empty() || stripped.back() == '\n');
            // heading: run >= 2 of '#' at (possibly indented) line start, followed by non-# char
            // Use resolved index to correctly detect line start even when preceding spaces
            // were skipped by a previous formatting-char run
            bool looks_like_heading = false;
            if (at_line_start && run >= 2 && j < resolved.size() && resolved[j] != c) {
                // verify no non-whitespace between last newline and this position in resolved
                size_t last_nl = resolved.rfind('\n', i - 1);
                size_t line_start = (last_nl == std::string::npos) ? 0 : (last_nl + 1);
                bool only_ws_before = std::all_of(resolved.begin() + line_start, resolved.begin() + i,
                    [](char ch){ return ch == ' ' || ch == '\t'; });
                if (only_ws_before) looks_like_heading = true;
            }
            if (looks_like_heading) {
                // keep the heading markers
                for (int k = i; k < j; k++) stripped += resolved[k];
            }
            // Otherwise skip formatting chars
            i = j;
            continue;
        }
        stripped += c;
        i++;
    }

    // Collapse multiple blank lines into one
    std::string collapsed;
    int blank_count = 0;
    for (char c : stripped) {
        if (c == '\n') {
            blank_count++;
            if (blank_count <= 1) collapsed += c;
        } else {
            blank_count = 0;
            collapsed += c;
        }
    }

    while (!collapsed.empty() &&
           (collapsed.back() == '\n' || collapsed.back() == ' ' || collapsed.back() == '\t')) {
        collapsed.pop_back();
    }
    return collapsed;
}

// ---------------------------------------------------------------------------
// Built-in shell commands (ls/dir, cd, pwd)  — no stream capture needed.
// ---------------------------------------------------------------------------
std::string execute_shell_command(const std::vector<std::string>& args,
                                  const std::string& base_cmd) {
    std::ostringstream out;

    if (base_cmd == "ls" || base_cmd == "dir") {
        try {
            auto cwd = std::filesystem::current_path();
            out << "\xe5\xbd\x93\xe5\x89\x8d\xe7\x9b\xae\xe5\xbd\x95: " << cwd.string() << "\n\n";
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto& entry : std::filesystem::directory_iterator(cwd))
                entries.push_back(entry);
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) return a.is_directory();
                return a.path().filename().string() < b.path().filename().string();
            });
            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    out << "  [\xe7\x9b\xae\xe5\xbd\x95]  " << name << "/\n";
                } else {
                    auto size = entry.file_size();
                    std::string size_str;
                    if (size < 1024) size_str = std::to_string(size) + " B";
                    else if (size < 1024 * 1024) size_str = fmt::format("{:.1f} KB", size / 1024.0);
                    else size_str = fmt::format("{:.1f} MB", size / (1024.0 * 1024.0));
                    out << "  [\xe6\x96\x87\xe4\xbb\xb6] " << std::left << std::setw(40)
                        << name << " " << size_str << "\n";
                }
            }
        } catch (const std::exception& e) {
            out << "[Error] " << e.what() << "\n";
        }
        return out.str();
    }

    if (base_cmd == "cd") {
        if (args.size() < 3) {
            out << "\xe7\x94\xa8\xe6\xb3\x95: cd <\xe7\x9b\xae\xe5\xbd\x95>\n";
            return out.str();
        }
        try {
            std::filesystem::path target = args[2];
            if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
                std::filesystem::current_path(target);
                out << "\xe5\xb7\xb2\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0: "
                    << std::filesystem::current_path().string() << "\n";
            } else {
                out << "[Error] \xe7\x9b\xae\xe5\xbd\x95\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8: "
                    << target.string() << "\n";
            }
        } catch (const std::exception& e) {
            out << "[Error] " << e.what() << "\n";
        }
        return out.str();
    }

    if (base_cmd == "pwd") {
        out << std::filesystem::current_path().string() << "\n";
        return out.str();
    }

    return "";
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<std::string> parse_command_line(const std::string& line) {
    std::stringstream ss(line);
    std::string part;
    std::vector<std::string> args;
    args.push_back("shuati");
    while (ss >> part) args.push_back(part);
    return args;
}

// Capture: run command, collect all output into a string, normalize, return.
std::string tui_execute_command_capture(const std::vector<std::string>& args,
                                        const std::string& base_cmd) {
    if (base_cmd == "ls" || base_cmd == "dir" || base_cmd == "cd" ||
        base_cmd == "pwd" || base_cmd == "help") {
        return normalize_text(execute_shell_command(args, base_cmd));
    }

    std::string accumulated;
    {
        ScopedStreamRedirect redir([&accumulated](const std::string& chunk) {
            accumulated += chunk;
        });

        try {
            CLI::App app{"TUI"};
            app.allow_extras();
            shuati::cmd::CommandContext cmd_ctx;
            cmd_ctx.is_tui      = true;
            // Forward captured output through the same stream (already redirected)
            cmd_ctx.stream_cb   = [&accumulated](const std::string& s) { accumulated += s; };
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
            accumulated += std::string("[Error] ") + e.what() + "\n";
        }
        // ScopedStreamRedirect dtor restores streams here, flushing cb_buf_
    }

    return normalize_text(accumulated);
}

// Stream: run command, call stream_cb with each chunk of output as it arrives.
void tui_execute_command_stream(const std::vector<std::string>& args,
                                const std::string& base_cmd,
                                std::function<void(const std::string&)> stream_cb) {
    // Wrap stream_cb to normalize markdown/ANSI before UI display
    std::function<void(const std::string&)> wrapped_cb;
    if (stream_cb) {
        wrapped_cb = [stream_cb](const std::string& chunk) {
            std::string norm = normalize_text(chunk);
            if (!norm.empty()) stream_cb(norm);
        };
    } else {
        wrapped_cb = {};
    }

    if (base_cmd == "ls" || base_cmd == "dir" || base_cmd == "cd" ||
        base_cmd == "pwd" || base_cmd == "help") {
        auto result = execute_shell_command(args, base_cmd);
        std::string norm = normalize_text(result);
        if (stream_cb && !norm.empty()) stream_cb(norm);
        return;
    }

    {
        ScopedStreamRedirect redir(wrapped_cb);

        try {
            CLI::App app{"TUI"};
            app.allow_extras();
            shuati::cmd::CommandContext cmd_ctx;
            cmd_ctx.is_tui    = true;
            cmd_ctx.stream_cb = wrapped_cb;
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
            if (wrapped_cb) wrapped_cb(std::string("[Error] ") + e.what() + "\n");
        }
        // ScopedStreamRedirect dtor restores streams, flushing any remaining buffer
    }
}

} // namespace tui
} // namespace shuati
