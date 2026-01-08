/**
 * @file config_watcher.cpp
 * @brief Implementation of configuration file hot-reload
 */

#include "common/config_watcher.hpp"
#include "common/logger.hpp"

#ifdef __linux__
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#endif

#include <algorithm>
#include <filesystem>

namespace speeduino {

ConfigWatcher::ConfigWatcher() {
#ifdef __linux__
    inotifyFd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyFd_ < 0) {
        Logger::error("Failed to initialize inotify: " +
                     std::string(strerror(errno)));
    }
#endif
}

ConfigWatcher::~ConfigWatcher() {
    stop();

#ifdef __linux__
    // Remove all watches
    for (const auto& watch : watches_) {
        if (watch.watchDescriptor >= 0) {
            inotify_rm_watch(inotifyFd_, watch.watchDescriptor);
        }
    }

    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
    }
#endif
}

bool ConfigWatcher::addPath(const std::string& path) {
#ifdef __linux__
    if (inotifyFd_ < 0) {
        Logger::error("inotify not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already watching
    for (const auto& watch : watches_) {
        if (watch.path == path) {
            Logger::debug("Already watching: " + path);
            return true;
        }
    }

    // Determine if path is file or directory
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        Logger::error("Cannot stat path: " + path + " - " +
                     std::string(strerror(errno)));
        return false;
    }

    bool isDir = S_ISDIR(st.st_mode);

    // Set up inotify mask
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO |
                    IN_MOVED_FROM | IN_CLOSE_WRITE;

    // For directories, watch for changes to files within
    if (isDir) {
        mask |= IN_ONLYDIR;
    }

    int wd = inotify_add_watch(inotifyFd_, path.c_str(), mask);
    if (wd < 0) {
        Logger::error("Failed to add watch for " + path + ": " +
                     std::string(strerror(errno)));
        return false;
    }

    WatchEntry entry;
    entry.path = path;
    entry.watchDescriptor = wd;
    entry.isDirectory = isDir;

    wdToIndex_[wd] = watches_.size();
    watches_.push_back(entry);

    Logger::info("Watching for changes: " + path +
                (isDir ? " (directory)" : " (file)"));
    return true;
#else
    (void)path;
    return false;
#endif
}

bool ConfigWatcher::removePath(const std::string& path) {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(watches_.begin(), watches_.end(),
        [&path](const WatchEntry& entry) {
            return entry.path == path;
        });

    if (it == watches_.end()) {
        return false;
    }

    if (it->watchDescriptor >= 0) {
        inotify_rm_watch(inotifyFd_, it->watchDescriptor);
        wdToIndex_.erase(it->watchDescriptor);
    }

    watches_.erase(it);

    // Rebuild index map
    wdToIndex_.clear();
    for (size_t i = 0; i < watches_.size(); ++i) {
        if (watches_[i].watchDescriptor >= 0) {
            wdToIndex_[watches_[i].watchDescriptor] = i;
        }
    }

    Logger::info("Stopped watching: " + path);
    return true;
#else
    (void)path;
    return false;
#endif
}

void ConfigWatcher::clearPaths() {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& watch : watches_) {
        if (watch.watchDescriptor >= 0) {
            inotify_rm_watch(inotifyFd_, watch.watchDescriptor);
        }
    }

    watches_.clear();
    wdToIndex_.clear();

    Logger::info("Cleared all config watches");
#endif
}

std::vector<std::string> ConfigWatcher::getWatchedPaths() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(watches_.size());

    for (const auto& watch : watches_) {
        result.push_back(watch.path);
    }

    return result;
}

void ConfigWatcher::setCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void ConfigWatcher::setDebounceInterval(std::chrono::milliseconds interval) {
    debounceInterval_ = interval;
}

bool ConfigWatcher::start() {
#ifdef __linux__
    if (inotifyFd_ < 0) {
        Logger::error("Cannot start: inotify not initialized");
        return false;
    }

    if (running_.exchange(true)) {
        Logger::warn("Config watcher already running");
        return true;
    }

    thread_ = std::thread(&ConfigWatcher::watchLoop, this);
    Logger::info("Config watcher started");
    return true;
#else
    return false;
#endif
}

void ConfigWatcher::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    Logger::info("Config watcher stopped");
}

bool ConfigWatcher::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}

uint64_t ConfigWatcher::getChangeCount() const noexcept {
    return changeCount_.load(std::memory_order_relaxed);
}

bool ConfigWatcher::isSupported() {
#ifdef __linux__
    int fd = inotify_init();
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return false;
#else
    return false;
#endif
}

void ConfigWatcher::watchLoop() {
#ifdef __linux__
    constexpr size_t kEventSize = sizeof(struct inotify_event);
    constexpr size_t kBufferSize = 4096;
    char buffer[kBufferSize];

    while (running_.load(std::memory_order_acquire)) {
        // Poll with timeout to allow clean shutdown
        struct pollfd pfd;
        pfd.fd = inotifyFd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);  // 100ms timeout

        if (ret < 0) {
            if (errno == EINTR) continue;
            Logger::error("poll() error: " + std::string(strerror(errno)));
            break;
        }

        if (ret == 0) {
            continue;  // Timeout, check running flag
        }

        // Read events
        ssize_t length = read(inotifyFd_, buffer, kBufferSize);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            Logger::error("read() error: " + std::string(strerror(errno)));
            break;
        }

        // Process events
        ssize_t i = 0;
        while (i < length) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

            std::string filename;
            if (event->len > 0) {
                filename = event->name;
            }

            // Find the watch path
            std::string watchPath;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = wdToIndex_.find(event->wd);
                if (it != wdToIndex_.end() && it->second < watches_.size()) {
                    watchPath = watches_[it->second].path;
                }
            }

            if (!watchPath.empty()) {
                processEvent(event->mask, filename, watchPath);
            }

            i += kEventSize + event->len;
        }
    }
#endif
}

void ConfigWatcher::processEvent(uint32_t mask, const std::string& filename,
                                 const std::string& watchPath) {
#ifdef __linux__
    // Determine the full path
    std::string fullPath = watchPath;
    if (!filename.empty()) {
        if (fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += filename;
    }

    // Determine change type
    ConfigChangeType changeType;
    if (mask & IN_CREATE) {
        changeType = ConfigChangeType::Created;
    } else if (mask & IN_DELETE) {
        changeType = ConfigChangeType::Deleted;
    } else if (mask & (IN_MOVED_TO | IN_MOVED_FROM)) {
        changeType = ConfigChangeType::Moved;
    } else {
        changeType = ConfigChangeType::Modified;
    }

    // Debounce - ignore rapid repeated events for same file
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lastEventTime_.find(fullPath);
        if (it != lastEventTime_.end()) {
            auto elapsed = now - it->second;
            if (elapsed < debounceInterval_) {
                return;  // Debounce - skip this event
            }
        }
        lastEventTime_[fullPath] = now;
    }

    // Only care about actual file changes (IN_CLOSE_WRITE is most reliable)
    if (!(mask & (IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO))) {
        return;
    }

    changeCount_.fetch_add(1, std::memory_order_relaxed);

    Logger::debug("Config change detected: " + fullPath +
                 " (" + configChangeTypeToString(changeType) + ")");

    // Invoke callback
    ConfigChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = callback_;
    }

    if (callback) {
        ConfigChangeEvent event;
        event.path = fullPath;
        event.type = changeType;
        event.timestamp = now;

        try {
            callback(event);
        } catch (const std::exception& e) {
            Logger::error("Config change callback threw: " +
                         std::string(e.what()));
        }
    }
#else
    (void)mask;
    (void)filename;
    (void)watchPath;
#endif
}

// SingleFileWatcher implementation

SingleFileWatcher::SingleFileWatcher(const std::string& path,
                                     ConfigChangeCallback callback)
    : path_(path) {
    watcher_.setCallback(std::move(callback));
    watcher_.addPath(path);
}

bool SingleFileWatcher::start() {
    return watcher_.start();
}

void SingleFileWatcher::stop() {
    watcher_.stop();
}

bool SingleFileWatcher::isRunning() const noexcept {
    return watcher_.isRunning();
}

// AutoReloadConfig implementation

AutoReloadConfig::AutoReloadConfig(const std::string& path,
                                   ReloadFunction reloadFunc)
    : path_(path)
    , reloadFunc_(std::move(reloadFunc)) {

    watcher_.addPath(path);
    watcher_.setCallback([this](const ConfigChangeEvent& event) {
        if (event.type == ConfigChangeType::Modified ||
            event.type == ConfigChangeType::Created) {

            Logger::info("Reloading config: " + event.path);

            if (reloadFunc_) {
                try {
                    if (reloadFunc_(event.path)) {
                        reloadCount_.fetch_add(1, std::memory_order_relaxed);
                        Logger::info("Config reloaded successfully");
                    } else {
                        errorCount_.fetch_add(1, std::memory_order_relaxed);
                        Logger::error("Config reload failed");
                    }
                } catch (const std::exception& e) {
                    errorCount_.fetch_add(1, std::memory_order_relaxed);
                    Logger::error("Config reload threw: " +
                                 std::string(e.what()));
                }
            }
        }
    });
}

bool AutoReloadConfig::start() {
    return watcher_.start();
}

void AutoReloadConfig::stop() {
    watcher_.stop();
}

uint64_t AutoReloadConfig::getReloadCount() const noexcept {
    return reloadCount_.load(std::memory_order_relaxed);
}

uint64_t AutoReloadConfig::getErrorCount() const noexcept {
    return errorCount_.load(std::memory_order_relaxed);
}

} // namespace speeduino
