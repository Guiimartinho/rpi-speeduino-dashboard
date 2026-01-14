/**
 * @file system_health.cpp
 * @brief Implementation of system health monitoring and graceful degradation
 */

#include "common/system_health.hpp"
#include "common/logger.hpp"

namespace speeduino {

SystemHealth& SystemHealth::instance() {
    static SystemHealth instance;
    return instance;
}

SystemHealth::SystemHealth() {
    // Initialize all subsystems
    for (size_t i = 0; i < kNumSubsystems; ++i) {
        subsystems_[i].id = static_cast<SubsystemId>(i);
        subsystems_[i].healthy = true;  // Assume healthy until proven otherwise
        subsystems_[i].lastHeartbeat = std::chrono::steady_clock::now();
        subsystems_[i].timeout = std::chrono::milliseconds(500);
        subsystems_[i].errorCount = 0;
    }
}

void SystemHealth::reportHeartbeat(SubsystemId id) {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    bool wasHealthy = subsystems_[idx].healthy;
    subsystems_[idx].lastHeartbeat = std::chrono::steady_clock::now();
    subsystems_[idx].healthy = true;

    if (!wasHealthy) {
        // Subsystem recovered
        Logger::info(std::string("Subsystem recovered: ") +
                    subsystemIdToString(id));
        notifyHealthChange(id, true);
        updateSystemMode();
    }
}

void SystemHealth::reportError(SubsystemId id, const std::string& errorMessage,
                               bool isFatal) {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    subsystems_[idx].errorCount++;
    subsystems_[idx].lastError = errorMessage;

    Logger::warn(std::string("Subsystem error [") + subsystemIdToString(id) +
                "]: " + errorMessage);

    if (isFatal) {
        bool wasHealthy = subsystems_[idx].healthy;
        subsystems_[idx].healthy = false;

        if (wasHealthy) {
            notifyHealthChange(id, false);
            updateSystemMode();
        }
    }
}

void SystemHealth::reportRecovery(SubsystemId id) {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    bool wasHealthy = subsystems_[idx].healthy;
    subsystems_[idx].healthy = true;
    subsystems_[idx].lastHeartbeat = std::chrono::steady_clock::now();

    if (!wasHealthy) {
        Logger::info(std::string("Subsystem recovered: ") +
                    subsystemIdToString(id));
        notifyHealthChange(id, true);
        updateSystemMode();
    }
}

void SystemHealth::setSubsystemHealth(SubsystemId id, bool healthy) {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    bool wasHealthy = subsystems_[idx].healthy;
    subsystems_[idx].healthy = healthy;

    if (healthy) {
        subsystems_[idx].lastHeartbeat = std::chrono::steady_clock::now();
    }

    if (wasHealthy != healthy) {
        notifyHealthChange(id, healthy);
        updateSystemMode();
    }
}

void SystemHealth::setSubsystemTimeout(SubsystemId id,
                                       std::chrono::milliseconds timeout) {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    subsystems_[idx].timeout = timeout;
}

void SystemHealth::checkTimeouts() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    bool anyChanged = false;

    for (size_t i = 0; i < kNumSubsystems; ++i) {
        auto& sub = subsystems_[i];

        if (sub.healthy) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - sub.lastHeartbeat);

            if (elapsed > sub.timeout) {
                sub.healthy = false;
                Logger::warn(std::string("Subsystem timeout: ") +
                            subsystemIdToString(sub.id) +
                            " (no heartbeat for " +
                            std::to_string(elapsed.count()) + "ms)");
                notifyHealthChange(sub.id, false);
                anyChanged = true;
            }
        }
    }

    if (anyChanged) {
        updateSystemMode();
    }
}

SystemMode SystemHealth::currentMode() const {
    return currentMode_.load(std::memory_order_acquire);
}

bool SystemHealth::isSubsystemHealthy(SubsystemId id) const {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_[idx].healthy;
}

std::vector<SubsystemStatus> SystemHealth::getAllStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SubsystemStatus> result;
    result.reserve(kNumSubsystems);

    for (size_t i = 0; i < kNumSubsystems; ++i) {
        result.push_back(subsystems_[i]);
    }

    return result;
}

SubsystemStatus SystemHealth::getStatus(SubsystemId id) const {
    auto idx = static_cast<size_t>(id);
    if (idx >= kNumSubsystems) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_[idx];
}

void SystemHealth::setModeChangeCallback(ModeChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    modeCallback_ = std::move(callback);
}

void SystemHealth::setHealthChangeCallback(HealthChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    healthCallback_ = std::move(callback);
}

bool SystemHealth::isAllHealthy() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0; i < kNumSubsystems; ++i) {
        if (!subsystems_[i].healthy) {
            return false;
        }
    }
    return true;
}

size_t SystemHealth::unhealthyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (size_t i = 0; i < kNumSubsystems; ++i) {
        if (!subsystems_[i].healthy) {
            ++count;
        }
    }
    return count;
}

void SystemHealth::requestShutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!shutdownRequested_) {
        shutdownRequested_ = true;
        auto oldMode = currentMode_.exchange(SystemMode::SafeShutdown);
        notifyModeChange(oldMode, SystemMode::SafeShutdown);
        Logger::info("Safe shutdown requested");
    }
}

void SystemHealth::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < kNumSubsystems; ++i) {
        subsystems_[i].healthy = true;
        subsystems_[i].lastHeartbeat = now;
        subsystems_[i].errorCount = 0;
        subsystems_[i].lastError.clear();
    }

    shutdownRequested_ = false;
    auto oldMode = currentMode_.exchange(SystemMode::Normal);

    if (oldMode != SystemMode::Normal) {
        notifyModeChange(oldMode, SystemMode::Normal);
    }

    Logger::info("System health reset to Normal");
}

void SystemHealth::updateSystemMode() {
    // Called with mutex_ already held

    if (shutdownRequested_) {
        return;  // Don't change mode during shutdown
    }

    SystemMode newMode = SystemMode::Normal;

    // Check individual critical subsystems
    bool canHealthy = subsystems_[static_cast<size_t>(SubsystemId::CAN)].healthy;
    bool cameraHealthy = subsystems_[static_cast<size_t>(SubsystemId::Camera)].healthy;

    // Count total unhealthy subsystems
    size_t unhealthyCount = 0;
    for (size_t i = 0; i < kNumSubsystems; ++i) {
        if (!subsystems_[i].healthy) {
            ++unhealthyCount;
        }
    }

    // Determine mode based on failures
    if (unhealthyCount == 0) {
        newMode = SystemMode::Normal;
    } else if (unhealthyCount >= 3) {
        newMode = SystemMode::SafeMode;
    } else if (!canHealthy && !cameraHealthy) {
        newMode = SystemMode::DegradedMultiple;
    } else if (!canHealthy) {
        newMode = SystemMode::DegradedCAN;
    } else if (!cameraHealthy) {
        newMode = SystemMode::DegradedCamera;
    } else {
        newMode = SystemMode::DegradedHMI;
    }

    auto oldMode = currentMode_.exchange(newMode);
    if (oldMode != newMode) {
        notifyModeChange(oldMode, newMode);
    }
}

void SystemHealth::notifyModeChange(SystemMode oldMode, SystemMode newMode) {
    // ═══════════════════════════════════════════════════════════════════════
    // DEADLOCK FIX: Callbacks are now queued and executed outside mutex
    // ISO 26262 ASIL-B: Prevents recursive mutex acquisition deadlocks
    // ═══════════════════════════════════════════════════════════════════════
    // IMPORTANT: This function is called with mutex_ already held
    // We copy the callback and release the mutex BEFORE calling it
    // to prevent deadlock if callback tries to access SystemHealth

    Logger::info(std::string("System mode changed: ") +
                systemModeToString(oldMode) + " -> " +
                systemModeToString(newMode));

    // Copy callback while holding lock
    ModeChangeCallback callbackCopy;
    if (modeCallback_) {
        callbackCopy = modeCallback_;
    }

    // Release lock BEFORE invoking callback (exception-safe)
    // The caller must handle re-acquisition if needed
    if (callbackCopy) {
        // Store current state to detect if re-entry occurred
        mutex_.unlock();
        try {
            callbackCopy(oldMode, newMode);
        } catch (const std::exception& e) {
            Logger::error(std::string("Mode change callback exception: ") + e.what());
        } catch (...) {
            Logger::error("Mode change callback threw unknown exception");
        }
        mutex_.lock();
    }
}

void SystemHealth::notifyHealthChange(SubsystemId id, bool healthy) {
    // ═══════════════════════════════════════════════════════════════════════
    // DEADLOCK FIX: Same pattern as notifyModeChange
    // ═══════════════════════════════════════════════════════════════════════
    // IMPORTANT: This function is called with mutex_ already held

    // Copy callback while holding lock
    HealthChangeCallback callbackCopy;
    if (healthCallback_) {
        callbackCopy = healthCallback_;
    }

    // Release lock BEFORE invoking callback (exception-safe)
    if (callbackCopy) {
        mutex_.unlock();
        try {
            callbackCopy(id, healthy);
        } catch (const std::exception& e) {
            Logger::error(std::string("Health change callback exception: ") + e.what());
        } catch (...) {
            Logger::error("Health change callback threw unknown exception");
        }
        mutex_.lock();
    }
}

} // namespace speeduino
