/**
 * @file watchdog_notifier.hpp
 * @brief systemd watchdog integration for service monitoring
 *
 * Provides automatic notification to systemd's watchdog mechanism,
 * ensuring services are automatically restarted if they become unresponsive.
 * Based on systemd service watchdog best practices for embedded Linux.
 */

#ifndef COMMON_WATCHDOG_NOTIFIER_HPP
#define COMMON_WATCHDOG_NOTIFIER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <string>

namespace speeduino {

/**
 * @brief Callback type for health check before watchdog notification
 *
 * If the callback returns false, the watchdog will NOT be notified,
 * which will eventually trigger a service restart by systemd.
 */
using WatchdogHealthCheck = std::function<bool()>;

/**
 * @class WatchdogNotifier
 * @brief Manages systemd watchdog notifications
 *
 * This class handles periodic notifications to systemd to indicate
 * the service is healthy. If notifications stop, systemd will restart
 * the service after WatchdogSec timeout.
 *
 * Usage:
 * @code
 *   // In service main()
 *   WatchdogNotifier watchdog;
 *   watchdog.setHealthCheck([]() {
 *       return canInterface.isConnected() && zmqPublisher.isHealthy();
 *   });
 *   watchdog.start();
 *
 *   // ... service main loop ...
 *
 *   watchdog.stop();
 * @endcode
 *
 * Systemd service configuration:
 * @code
 *   [Service]
 *   Type=notify
 *   WatchdogSec=10
 * @endcode
 */
class WatchdogNotifier {
public:
    /**
     * @brief Constructor
     *
     * Reads WATCHDOG_USEC environment variable to determine if watchdog
     * is enabled and what the timeout interval should be.
     */
    WatchdogNotifier();

    /**
     * @brief Destructor - stops the notification thread
     */
    ~WatchdogNotifier();

    // Non-copyable, non-movable
    WatchdogNotifier(const WatchdogNotifier&) = delete;
    WatchdogNotifier& operator=(const WatchdogNotifier&) = delete;
    WatchdogNotifier(WatchdogNotifier&&) = delete;
    WatchdogNotifier& operator=(WatchdogNotifier&&) = delete;

    /**
     * @brief Check if systemd watchdog is enabled
     * @return true if WATCHDOG_USEC environment variable is set
     */
    [[nodiscard]] bool isEnabled() const noexcept;

    /**
     * @brief Get the configured watchdog timeout
     * @return Watchdog timeout from systemd (0 if not enabled)
     */
    [[nodiscard]] std::chrono::microseconds getTimeout() const noexcept;

    /**
     * @brief Get the notification interval (half of timeout)
     * @return How often notifications are sent
     */
    [[nodiscard]] std::chrono::microseconds getInterval() const noexcept;

    /**
     * @brief Set a health check callback
     * @param callback Function that returns false to prevent watchdog notification
     *
     * The callback is called before each watchdog notification. If it returns
     * false, the notification is skipped, which will eventually cause systemd
     * to restart the service.
     */
    void setHealthCheck(WatchdogHealthCheck callback);

    /**
     * @brief Start the watchdog notification thread
     *
     * Begins sending periodic sd_notify("WATCHDOG=1") messages to systemd.
     * Also sends sd_notify("READY=1") to indicate service startup is complete.
     */
    void start();

    /**
     * @brief Stop the watchdog notification thread
     *
     * Sends sd_notify("STOPPING=1") before stopping.
     */
    void stop();

    /**
     * @brief Check if the notifier is running
     * @return true if the notification thread is active
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Manually trigger a watchdog notification
     *
     * Call this from critical code paths to refresh the watchdog.
     * The automatic thread also sends notifications periodically.
     */
    void notify();

    /**
     * @brief Send a status message to systemd
     * @param status Status message (shown in systemctl status)
     */
    void setStatus(const std::string& status);

    /**
     * @brief Notify systemd that the service is ready
     *
     * Call this when service initialization is complete.
     * Required when using Type=notify in systemd service.
     */
    void notifyReady();

    /**
     * @brief Notify systemd that the service is reloading configuration
     */
    void notifyReloading();

    /**
     * @brief Notify systemd that the service is stopping
     */
    void notifyStopping();

    /**
     * @brief Notify systemd with errno status
     * @param err The error number to report
     */
    void notifyError(int err);

    /**
     * @brief Get number of successful notifications sent
     * @return Notification count
     */
    [[nodiscard]] uint64_t getNotificationCount() const noexcept;

    /**
     * @brief Get number of skipped notifications (due to health check failure)
     * @return Skip count
     */
    [[nodiscard]] uint64_t getSkipCount() const noexcept;

private:
    void notifyLoop();

    bool enabled_ = false;
    std::chrono::microseconds timeout_{0};
    std::chrono::microseconds interval_{0};

    std::atomic<bool> running_{false};
    std::thread thread_;

    WatchdogHealthCheck healthCheck_;
    std::atomic<uint64_t> notifyCount_{0};
    std::atomic<uint64_t> skipCount_{0};
};

/**
 * @class ScopedWatchdogPause
 * @brief RAII helper to temporarily pause watchdog notifications
 *
 * Useful when performing long-running operations that might
 * exceed the watchdog timeout (e.g., firmware updates).
 *
 * @note This does NOT actually pause systemd's timer - it only
 * pauses the notification thread. Use with caution.
 */
class ScopedWatchdogPause {
public:
    explicit ScopedWatchdogPause(WatchdogNotifier& notifier)
        : notifier_(notifier)
        , wasRunning_(notifier.isRunning()) {
        if (wasRunning_) {
            notifier_.stop();
        }
    }

    ~ScopedWatchdogPause() {
        if (wasRunning_) {
            notifier_.start();
        }
    }

    ScopedWatchdogPause(const ScopedWatchdogPause&) = delete;
    ScopedWatchdogPause& operator=(const ScopedWatchdogPause&) = delete;

private:
    WatchdogNotifier& notifier_;
    bool wasRunning_;
};

} // namespace speeduino

#endif // COMMON_WATCHDOG_NOTIFIER_HPP
