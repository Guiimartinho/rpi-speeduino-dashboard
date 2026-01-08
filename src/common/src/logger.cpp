#include "common/logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

#ifdef __linux__
#include <syslog.h>
#endif

namespace speeduino {

LogLevel Logger::s_minLevel = LogLevel::INFO;
std::string Logger::s_serviceName = "speeduino";
bool Logger::s_initialized = false;

namespace {

const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "?????";
    }
}

#ifdef __linux__
int levelToSyslog(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return LOG_DEBUG;
        case LogLevel::DEBUG: return LOG_DEBUG;
        case LogLevel::INFO:  return LOG_INFO;
        case LogLevel::WARN:  return LOG_WARNING;
        case LogLevel::ERROR: return LOG_ERR;
        case LogLevel::FATAL: return LOG_CRIT;
        default:              return LOG_INFO;
    }
}
#endif

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_now;
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string extractFilename(const char* path) {
    std::string_view sv(path);
    auto pos = sv.find_last_of("/\\");
    if (pos != std::string_view::npos) {
        return std::string(sv.substr(pos + 1));
    }
    return std::string(sv);
}

} // anonymous namespace

void Logger::init(std::string_view service_name) {
    s_serviceName = std::string(service_name);
    s_initialized = true;

#ifdef __linux__
    openlog(s_serviceName.c_str(), LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif

    info("Logger initialized for service: " + s_serviceName);
}

void Logger::shutdown() {
    if (!s_initialized) return;

    info("Logger shutting down");

#ifdef __linux__
    closelog();
#endif

    s_initialized = false;
}

void Logger::setLevel(LogLevel level) {
    s_minLevel = level;
}

bool Logger::isEnabled(LogLevel level) {
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(s_minLevel);
}

void Logger::logImpl(LogLevel level, std::string_view message,
                     const std::source_location& loc) {
    if (!isEnabled(level)) return;

    std::string filename = extractFilename(loc.file_name());

    // Format: [TIMESTAMP] [LEVEL] [file:line] message
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << levelToString(level) << "] "
        << "[" << filename << ":" << loc.line() << "] "
        << message;

    std::string formatted = oss.str();

    // Output to stderr (captured by journald when running as service)
    std::cerr << formatted << std::endl;

#ifdef __linux__
    // Also log to syslog/journald
    if (s_initialized) {
        syslog(levelToSyslog(level), "%s:%d %.*s",
               filename.c_str(), static_cast<int>(loc.line()),
               static_cast<int>(message.size()), message.data());
    }
#endif
}

void Logger::trace(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::TRACE, message, loc);
}

void Logger::debug(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::DEBUG, message, loc);
}

void Logger::info(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::INFO, message, loc);
}

void Logger::warn(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::WARN, message, loc);
}

void Logger::error(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::ERROR, message, loc);
}

void Logger::fatal(std::string_view message, const std::source_location& loc) {
    logImpl(LogLevel::FATAL, message, loc);
}

} // namespace speeduino
