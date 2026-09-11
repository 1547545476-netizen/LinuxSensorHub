#ifndef SENSORHUB_LOGGER_HPP
#define SENSORHUB_LOGGER_HPP

#include <mutex>
#include <string>

namespace sensorhub {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;
    std::mutex mutex_;
    LogLevel level_{LogLevel::Info};
};

void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warn(const std::string& message);
void log_error(const std::string& message);

} // namespace sensorhub

#endif

