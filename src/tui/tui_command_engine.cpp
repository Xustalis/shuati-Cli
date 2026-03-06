#include "tui_command_engine.hpp"
#include "../cmd/commands.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace shuati {
namespace tui {

namespace {

class ScopedStreamCapture {
public:
    ScopedStreamCapture() : old_cout_(std::cout.rdbuf()), old_cerr_(std::cerr.rdbuf()) {
        fflush(stdout);
        fflush(stderr);

#ifdef _WIN32
        saved_fd1_ = _dup(1);
        saved_fd2_ = _dup(2);
#else
        saved_fd1_ = dup(1);
        saved_fd2_ = dup(2);
#endif
        tmp_path_ = (std::filesystem::temp_directory_path() / "shuati_capture.tmp").string();

#ifdef _WIN32
        int fd = -1;
        _sopen_s(&fd, tmp_path_.c_str(),
                  _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                  _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            _dup2(fd, 1);
            _dup2(fd, 2);
            _close(fd);
        }
#else
        int fd = open(tmp_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
#endif
        std::cout.rdbuf(cpp_buf_.rdbuf());
        std::cerr.rdbuf(cpp_buf_.rdbuf());
    }

    ~ScopedStreamCapture() {
        std::cout.rdbuf(old_cout_);
        std::cerr.rdbuf(old_cerr_);

        fflush(stdout);
        fflush(stderr);

#ifdef _WIN32
        _dup2(saved_fd1_, 1);
        _dup2(saved_fd2_, 2);
        _close(saved_fd1_);
        _close(saved_fd2_);
#else
        dup2(saved_fd1_, 1);
        dup2(saved_fd2_, 2);
        close(saved_fd1_);
        close(saved_fd2_);
#endif
    }

    ScopedStreamCapture(const ScopedStreamCapture&) = delete;
    ScopedStreamCapture& operator=(const ScopedStreamCapture&) = delete;

    std::string str() {
        fflush(stdout);
        fflush(stderr);

        std::string result = cpp_buf_.str();

        std::ifstream f(tmp_path_, std::ios::binary);
        if (f) {
            std::string c_output((std::istreambuf_iterator<char>(f)), {});
            if (!c_output.empty()) {
                if (!result.empty() && result.back() != '\n') result += '\n';
                result += c_output;
            }
        }

        try { std::filesystem::remove(tmp_path_); } catch (...) {}
        return result;
    }

private:
    std::ostringstream cpp_buf_;
    std::streambuf* old_cout_;
    std::streambuf* old_cerr_;
    int saved_fd1_ = -1;
    int saved_fd2_ = -1;
    std::string tmp_path_;
};

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

std::string normalize_text(const std::string& text) {
    // Simulate terminal \r: discard current line content and restart from column 0
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
    while (!resolved.empty() &&
           (resolved.back() == '\n' || resolved.back() == ' ' || resolved.back() == '\t')) {
        resolved.pop_back();
    }
    return resolved;
}

std::string execute_shell_command(const std::vector<std::string>& args, const std::string& base_cmd) {
    std::ostringstream out;

    if (base_cmd == "ls" || base_cmd == "dir") {
        try {
            auto cwd = std::filesystem::current_path();
            out << "\xe5\xbd\x93\xe5\x89\x8d\xe7\x9b\xae\xe5\xbd\x95: " << cwd.string() << "\n\n";
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
                    out << "  [\xe7\x9b\xae\xe5\xbd\x95]  " << name << "/\n";
                } else {
                    auto size = entry.file_size();
                    std::string size_str;
                    if (size < 1024) size_str = std::to_string(size) + " B";
                    else if (size < 1024 * 1024) size_str = fmt::format("{:.1f} KB", size / 1024.0);
                    else size_str = fmt::format("{:.1f} MB", size / (1024.0 * 1024.0));
                    out << "  [\xe6\x96\x87\xe4\xbb\xb6] " << std::left << std::setw(40) << name << " " << size_str << "\n";
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
                out << "\xe5\xb7\xb2\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0: " << std::filesystem::current_path().string() << "\n";
            } else {
                out << "[Error] \xe7\x9b\xae\xe5\xbd\x95\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8: " << target.string() << "\n";
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

std::vector<std::string> parse_command_line(const std::string& line) {
    std::stringstream ss(line);
    std::string part;
    std::vector<std::string> args;
    args.push_back("shuati");
    while (ss >> part) {
        args.push_back(part);
    }
    return args;
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
        std::cout << "[Error] " << e.what() << "\n";
    }

    return normalize_text(capture.str());
}

void tui_execute_command_stream(const std::vector<std::string>& args, const std::string& base_cmd, std::function<void(const std::string&)> stream_cb) {
    try {
        CLI::App app{"TUI"};
        app.allow_extras();
        shuati::cmd::CommandContext cmd_ctx;
        cmd_ctx.stream_cb = stream_cb;
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
        if (stream_cb) stream_cb(std::string("[Error] ") + e.what() + "\n");
    }
}

} // namespace tui
} // namespace shuati
