/**
 * @file config_watcher.hpp
 * @brief Configuration file hot-reload using inotify
 *
 * Provides automatic detection of configuration file changes and
 * triggers callbacks for runtime configuration updates without
 * requiring service restarts.
 */

#ifndef COMMON_CONFIG_WATCHER_HPP
#define COMMON_CONFIG_WATCHER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace speeduino {

/**
 * @enum ConfigChangeType
 * @brief Type of configuration file change
 */
enum class ConfigChangeType {
    Modified,  ///< File content was modified
    Created,   ///< File was created
    Deleted,   ///< File was deleted
    Moved      ///< File was moved/renamed
};

/**
 * @brief Convert ConfigChangeType to string
 */
inline const char* configChangeTypeToString(ConfigChangeType type) {
    switch (type) {
        case ConfigChangeType::Modified:
            return "Modified";
        case ConfigChangeType::Created:
            return "Created";
        case ConfigChangeType::Deleted:
            return "Deleted";
        case ConfigChangeType::Moved:
            return "Moved";
        default:
            return "Unknown";
    }
}

/**
 * @struct ConfigChangeEvent
 * @brief Information about a configuration change
 */
struct ConfigChangeEvent {
    std::string path;                                 ///< Full path to the changed file
    ConfigChangeType type;                            ///< Type of change
    std::chrono::steady_clock::time_point timestamp;  ///< When the change occurred
};

/**
 * @brief Callback type for configuration change notifications
 */
using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;

/**
 * @class ConfigWatcher
 * @brief Watches configuration files for changes using inotify
 *
 * Monitors one or more files or directories for changes and invokes
 * callbacks when modifications are detected. Uses Linux inotify for
 * efficient file system monitoring.
 *
 * Example usage:
 * @code
 *   ConfigWatcher watcher;
 *
 *   watcher.setCallback([](const ConfigChangeEvent& event) {
 *       LOG_INFO("Config changed: " + event.path);
 *       if (event.path.find("system.yaml") != std::string::npos) {
 *           ConfigLoader::loadSystemConfig(event.path);
 *       }
 *   });
 *
 *   watcher.addPath("/etc/speeduino/system.yaml");
 *   watcher.addPath("/etc/speeduino/can_signals.yaml");
 *   watcher.start();
 *
 *   // ... service runs ...
 *
 *   watcher.stop();
 * @endcode
 */
class ConfigWatcher {
public:
    /**
     * @brief Constructor
     */
    ConfigWatcher();

    /**
     * @brief Destructor - stops watching
     */
    ~ConfigWatcher();

    // Non-copyable, non-movable
    ConfigWatcher(const ConfigWatcher&)            = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;
    ConfigWatcher(ConfigWatcher&&)                 = delete;
    ConfigWatcher& operator=(ConfigWatcher&&)      = delete;

    /**
     * @brief Add a file or directory to watch
     * @param path Path to file or directory
     * @return true if the watch was added successfully
     *
     * For files, watches for modifications.
     * For directories, watches for file changes within.
     */
    bool addPath(const std::string& path);

    /**
     * @brief Remove a watch
     * @param path Path to stop watching
     * @return true if the watch was removed
     */
    bool removePath(const std::string& path);

    /**
     * @brief Remove all watches
     */
    void clearPaths();

    /**
     * @brief Get list of watched paths
     * @return Vector of watched paths
     */
    [[nodiscard]] std::vector<std::string> getWatchedPaths() const;

    /**
     * @brief Set the callback for change notifications
     * @param callback Function to call when changes are detected
     */
    void setCallback(ConfigChangeCallback callback);

    /**
     * @brief Set debounce interval for rapid changes
     * @param interval Minimum time between callbacks for same file
     *
     * When a file changes multiple times rapidly (e.g., during save),
     * only one callback will be triggered per interval.
     * Default: 100ms
     */
    void setDebounceInterval(std::chrono::milliseconds interval);

    /**
     * @brief Start watching for changes
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop watching for changes
     */
    void stop();

    /**
     * @brief Check if the watcher is running
     * @return true if actively watching
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Get count of detected changes since start
     * @return Number of change events
     */
    [[nodiscard]] uint64_t getChangeCount() const noexcept;

    /**
     * @brief Check if inotify is available on this system
     * @return true if inotify is supported
     */
    static bool isSupported();

private:
    void watchLoop();
    void processEvent(uint32_t mask, const std::string& filename, const std::string& watchPath);

    struct WatchEntry {
        std::string path;
        int watchDescriptor = -1;
        bool isDirectory    = false;
    };

    int inotifyFd_ = -1;
    std::vector<WatchEntry> watches_;
    std::unordered_map<int, size_t> wdToIndex_;

    ConfigChangeCallback callback_;
    std::chrono::milliseconds debounceInterval_{100};

    // Debounce tracking
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastEventTime_;

    std::atomic<bool> running_{false};
    std::thread thread_;
    mutable std::mutex mutex_;

    std::atomic<uint64_t> changeCount_{0};
};

/**
 * @class SingleFileWatcher
 * @brief Simplified watcher for a single configuration file
 *
 * Convenient wrapper when you only need to watch one file.
 */
class SingleFileWatcher {
public:
    /**
     * @brief Constructor
     * @param path Path to the file to watch
     * @param callback Function to call when file changes
     */
    SingleFileWatcher(const std::string& path, ConfigChangeCallback callback);

    /**
     * @brief Start watching
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop watching
     */
    void stop();

    /**
     * @brief Check if running
     * @return true if actively watching
     */
    [[nodiscard]] bool isRunning() const noexcept;

private:
    ConfigWatcher watcher_;
    std::string path_;
};

/**
 * @class AutoReloadConfig
 * @brief RAII wrapper that automatically reloads config on changes
 *
 * Example:
 * @code
 *   AutoReloadConfig reloader("/etc/speeduino/system.yaml",
 *       [](const std::string& path) {
 *           return ConfigLoader::loadSystemConfig(path);
 *       });
 *
 *   reloader.start();
 *   // Config will automatically reload when file changes
 * @endcode
 */
class AutoReloadConfig {
public:
    using ReloadFunction = std::function<bool(const std::string& path)>;

    /**
     * @brief Constructor
     * @param path Path to config file
     * @param reloadFunc Function to reload the config (returns success)
     */
    AutoReloadConfig(const std::string& path, ReloadFunction reloadFunc);

    /**
     * @brief Start auto-reload
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop auto-reload
     */
    void stop();

    /**
     * @brief Get count of successful reloads
     * @return Reload count
     */
    [[nodiscard]] uint64_t getReloadCount() const noexcept;

    /**
     * @brief Get count of failed reloads
     * @return Error count
     */
    [[nodiscard]] uint64_t getErrorCount() const noexcept;

private:
    ConfigWatcher watcher_;
    std::string path_;
    ReloadFunction reloadFunc_;
    std::atomic<uint64_t> reloadCount_{0};
    std::atomic<uint64_t> errorCount_{0};
};

}  // namespace speeduino

#endif  // COMMON_CONFIG_WATCHER_HPP
