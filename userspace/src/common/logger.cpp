#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace sensorhub {

namespace {

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::string now_string() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }

    /*
     * 初学者提示：
     * 多线程程序同时写 stdout 会交错，所以这里用 mutex 保护。
     * 真实项目里一般还会把日志写到文件或 journald。
     */
    std::cerr << now_string()
              << " [" << level_name(level) << "]"
              << " [tid=" << std::this_thread::get_id() << "] "
              << message << std::endl;
}

void log_debug(const std::string& message) {
    Logger::instance().log(LogLevel::Debug, message);
}

void log_info(const std::string& message) {
    Logger::instance().log(LogLevel::Info, message);
}

void log_warn(const std::string& message) {
    Logger::instance().log(LogLevel::Warn, message);
}

void log_error(const std::string& message) {
    Logger::instance().log(LogLevel::Error, message);
}

} // namespace sensorhub

