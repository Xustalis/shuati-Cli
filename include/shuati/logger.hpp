#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <fmt/core.h>
#include <utility>

namespace shuati {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR
};

class Logger {
public:
    static Logger& instance();

    // Initialize logger (open file)
    void init(const std::string& log_path);

    // Write log message
    void log(LogLevel level, const std::string& message);

    template<typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::DEBUG, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::INFO, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::WARNING, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void log_error(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::ERR, fmt::format(fmt, std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    ~Logger();
    
    std::ofstream log_file_;
    std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace shuati
