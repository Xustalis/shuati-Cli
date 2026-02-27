#include "shuati/logger.hpp"
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <iostream>

namespace shuati {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::init(const std::string& log_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;

    try {
        std::filesystem::path p(log_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        log_file_.open(log_path, std::ios::out | std::ios::app);
        initialized_ = true;
    } catch (...) {
        // Fallback to stderr if file logging fails? Or just ignore.
        std::cerr << "[Logger] Failed to open log file: " << log_path << std::endl;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_ || !log_file_.is_open()) return;

    std::time_t now = std::time(nullptr);
    std::tm* tm_now = std::localtime(&now);

    char time_buf[20];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_now);

    const char* level_str = "";
    switch (level) {
        case LogLevel::DEBUG:   level_str = "DEBUG"; break;
        case LogLevel::INFO:    level_str = "INFO "; break;
        case LogLevel::WARNING: level_str = "WARN "; break;
        case LogLevel::ERR:     level_str = "ERROR"; break;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << "[" << time_buf << "] [" << level_str << "] " << message << std::endl;
}

} // namespace shuati
