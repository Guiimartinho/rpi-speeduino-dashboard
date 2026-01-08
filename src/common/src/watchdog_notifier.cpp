/**
 * @file watchdog_notifier.cpp
 * @brief Implementation of systemd watchdog integration
 */

#include "common/watchdog_notifier.hpp"
#include "common/logger.hpp"

#ifdef __linux__
#include <systemd/sd-daemon.h>
#endif

#include <cstdlib>

namespace speeduino {

WatchdogNotifier::WatchdogNotifier() {
#ifdef __linux__
    uint64_t usec = 0;
    int ret = sd_watchdog_enabled(0, &usec);

    if (ret > 0 && usec > 0) {
        enabled_ = true;
        timeout_ = std::chrono::microseconds(usec);
        // Notify at half the timeout interval as recommended by systemd
        interval_ = std::chrono::microseconds(usec / 2);

        Logger::info("systemd watchdog enabled, timeout: " +
                    std::to_string(usec / 1000) + "ms, notify interval: " +
                    std::to_string(usec / 2000) + "ms");
    } else {
        Logger::debug("systemd watchdog not enabled");
    }
#endif
}

WatchdogNotifier::~WatchdogNotifier() {
    stop();
}

bool WatchdogNotifier::isEnabled() const noexcept {
    return enabled_;
}

std::chrono::microseconds WatchdogNotifier::getTimeout() const noexcept {
    return timeout_;
}

std::chrono::microseconds WatchdogNotifier::getInterval() const noexcept {
    return interval_;
}

void WatchdogNotifier::setHealthCheck(WatchdogHealthCheck callback) {
    healthCheck_ = std::move(callback);
}

void WatchdogNotifier::start() {
    if (!enabled_) {
        Logger::debug("Watchdog not enabled, skipping start");
        return;
    }

    if (running_.exchange(true)) {
        Logger::warn("Watchdog notifier already running");
        return;
    }

    // Notify systemd that we're ready
    notifyReady();

    // Start the notification thread
    thread_ = std::thread(&WatchdogNotifier::notifyLoop, this);

    Logger::info("Watchdog notifier started");
}

void WatchdogNotifier::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // Notify systemd that we're stopping
    notifyStopping();

    // Wait for thread to finish
    if (thread_.joinable()) {
        thread_.join();
    }

    Logger::info("Watchdog notifier stopped");
}

bool WatchdogNotifier::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}

void WatchdogNotifier::notify() {
#ifdef __linux__
    if (enabled_) {
        sd_notify(0, "WATCHDOG=1");
        notifyCount_.fetch_add(1, std::memory_order_relaxed);
    }
#endif
}

void WatchdogNotifier::setStatus(const std::string& status) {
#ifdef __linux__
    if (enabled_) {
        std::string msg = "STATUS=" + status;
        sd_notify(0, msg.c_str());
    }
#else
    (void)status;
#endif
}

void WatchdogNotifier::notifyReady() {
#ifdef __linux__
    sd_notify(0, "READY=1");
    Logger::debug("Notified systemd: READY=1");
#endif
}

void WatchdogNotifier::notifyReloading() {
#ifdef __linux__
    sd_notify(0, "RELOADING=1");
    Logger::debug("Notified systemd: RELOADING=1");
#endif
}

void WatchdogNotifier::notifyStopping() {
#ifdef __linux__
    sd_notify(0, "STOPPING=1");
    Logger::debug("Notified systemd: STOPPING=1");
#endif
}

void WatchdogNotifier::notifyError(int err) {
#ifdef __linux__
    std::string msg = "ERRNO=" + std::to_string(err);
    sd_notify(0, msg.c_str());
#else
    (void)err;
#endif
}

uint64_t WatchdogNotifier::getNotificationCount() const noexcept {
    return notifyCount_.load(std::memory_order_relaxed);
}

uint64_t WatchdogNotifier::getSkipCount() const noexcept {
    return skipCount_.load(std::memory_order_relaxed);
}

void WatchdogNotifier::notifyLoop() {
    while (running_.load(std::memory_order_acquire)) {
        // Check health before notifying
        bool healthy = true;
        if (healthCheck_) {
            try {
                healthy = healthCheck_();
            } catch (const std::exception& e) {
                Logger::error(std::string("Watchdog health check threw: ") + e.what());
                healthy = false;
            } catch (...) {
                Logger::error("Watchdog health check threw unknown exception");
                healthy = false;
            }
        }

        if (healthy) {
            notify();
        } else {
            skipCount_.fetch_add(1, std::memory_order_relaxed);
            Logger::warn("Watchdog notification skipped - health check failed");
        }

        // Sleep for the interval
        // Use shorter sleeps to be more responsive to stop() calls
        auto remaining = interval_;
        const auto sleepChunk = std::chrono::milliseconds(100);

        while (remaining > std::chrono::microseconds(0) &&
               running_.load(std::memory_order_acquire)) {
            auto sleepTime = std::min(
                remaining,
                std::chrono::duration_cast<std::chrono::microseconds>(sleepChunk));

            std::this_thread::sleep_for(sleepTime);
            remaining -= sleepTime;
        }
    }
}

} // namespace speeduino
