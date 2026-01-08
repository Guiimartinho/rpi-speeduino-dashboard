#ifndef COMMON_LOGGER_HPP
#define COMMON_LOGGER_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <source_location>

namespace speeduino {

// Log levels compatible with journald/syslog
enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5
};

class Logger {
public:
    // Initialize logger with service name
    static void init(std::string_view service_name);

    // Shutdown logger
    static void shutdown();

    // Set minimum log level
    static void setLevel(LogLevel level);

    // Log methods
    static void trace(std::string_view message,
                      const std::source_location& loc = std::source_location::current());
    static void debug(std::string_view message,
                      const std::source_location& loc = std::source_location::current());
    static void info(std::string_view message,
                     const std::source_location& loc = std::source_location::current());
    static void warn(std::string_view message,
                     const std::source_location& loc = std::source_location::current());
    static void error(std::string_view message,
                      const std::source_location& loc = std::source_location::current());
    static void fatal(std::string_view message,
                      const std::source_location& loc = std::source_location::current());

    // Formatted log (printf-style)
    template<typename... Args>
    static void log(LogLevel level, const char* fmt, Args&&... args);

    // Check if level is enabled
    static bool isEnabled(LogLevel level);

private:
    static void logImpl(LogLevel level, std::string_view message,
                        const std::source_location& loc);

    static LogLevel s_minLevel;
    static std::string s_serviceName;
    static bool s_initialized;
};

// Convenience macros with source location
#define LOG_TRACE(msg) speeduino::Logger::trace(msg)
#define LOG_DEBUG(msg) speeduino::Logger::debug(msg)
#define LOG_INFO(msg)  speeduino::Logger::info(msg)
#define LOG_WARN(msg)  speeduino::Logger::warn(msg)
#define LOG_ERROR(msg) speeduino::Logger::error(msg)
#define LOG_FATAL(msg) speeduino::Logger::fatal(msg)

} // namespace speeduino

#endif // COMMON_LOGGER_HPP
