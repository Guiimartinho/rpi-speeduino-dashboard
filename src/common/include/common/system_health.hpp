/**
 * @file system_health.hpp
 * @brief System health monitoring and graceful degradation
 *
 * Implements a state machine for system health monitoring that enables
 * graceful degradation when subsystems fail. Based on automotive safety
 * patterns from IEEE and AUTOSAR guidelines.
 */

#ifndef COMMON_SYSTEM_HEALTH_HPP
#define COMMON_SYSTEM_HEALTH_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

namespace speeduino {

/**
 * @enum SystemMode
 * @brief Operating modes for graceful degradation
 *
 * States are ordered by severity - higher values indicate more degraded operation.
 */
enum class SystemMode : uint8_t {
    Normal = 0,       ///< All systems operational
    DegradedCAN,      ///< CAN bus offline, UI shows last values + warning
    DegradedCamera,   ///< Camera failed, using placeholder
    DegradedHMI,      ///< HMI issues, reduced functionality
    DegradedMultiple, ///< Multiple subsystems degraded
    SafeMode,         ///< Minimal operation mode
    SafeShutdown      ///< Controlled shutdown in progress
};

/**
 * @brief Convert SystemMode to string for logging
 */
inline const char* systemModeToString(SystemMode mode) {
    switch (mode) {
        case SystemMode::Normal:           return "Normal";
        case SystemMode::DegradedCAN:      return "DegradedCAN";
        case SystemMode::DegradedCamera:   return "DegradedCamera";
        case SystemMode::DegradedHMI:      return "DegradedHMI";
        case SystemMode::DegradedMultiple: return "DegradedMultiple";
        case SystemMode::SafeMode:         return "SafeMode";
        case SystemMode::SafeShutdown:     return "SafeShutdown";
        default:                           return "Unknown";
    }
}

/**
 * @enum SubsystemId
 * @brief Identifiers for monitored subsystems
 */
enum class SubsystemId : uint8_t {
    CAN = 0,
    Camera,
    OpenAuto,
    GPIO,
    ZMQ,
    Config,
    NumSubsystems
};

/**
 * @brief Convert SubsystemId to string
 */
inline const char* subsystemIdToString(SubsystemId id) {
    switch (id) {
        case SubsystemId::CAN:      return "CAN";
        case SubsystemId::Camera:   return "Camera";
        case SubsystemId::OpenAuto: return "OpenAuto";
        case SubsystemId::GPIO:     return "GPIO";
        case SubsystemId::ZMQ:      return "ZMQ";
        case SubsystemId::Config:   return "Config";
        default:                    return "Unknown";
    }
}

/**
 * @struct SubsystemStatus
 * @brief Status information for a single subsystem
 */
struct SubsystemStatus {
    SubsystemId id = SubsystemId::CAN;
    bool healthy = false;
    std::chrono::steady_clock::time_point lastHeartbeat;
    std::chrono::milliseconds timeout{500};
    uint32_t errorCount = 0;
    std::string lastError;
};

/**
 * @brief Callback type for mode change notifications
 */
using ModeChangeCallback = std::function<void(SystemMode oldMode, SystemMode newMode)>;

/**
 * @brief Callback type for subsystem health change notifications
 */
using HealthChangeCallback = std::function<void(SubsystemId id, bool healthy)>;

/**
 * @class SystemHealth
 * @brief Singleton class for system health monitoring
 *
 * Tracks the health of all subsystems and manages graceful degradation.
 * Thread-safe for use from multiple services.
 *
 * Example usage:
 * @code
 *   auto& health = SystemHealth::instance();
 *   health.setSubsystemTimeout(SubsystemId::CAN, std::chrono::milliseconds(500));
 *
 *   // In CAN service main loop:
 *   health.reportHeartbeat(SubsystemId::CAN);
 *
 *   // In watchdog thread:
 *   health.checkTimeouts();
 *
 *   // React to mode changes:
 *   health.setModeChangeCallback([](SystemMode old, SystemMode new) {
 *       LOG_WARN("System mode changed: " + std::string(systemModeToString(new)));
 *   });
 * @endcode
 */
class SystemHealth {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the SystemHealth instance
     */
    static SystemHealth& instance();

    // Delete copy/move operations for singleton
    SystemHealth(const SystemHealth&) = delete;
    SystemHealth& operator=(const SystemHealth&) = delete;
    SystemHealth(SystemHealth&&) = delete;
    SystemHealth& operator=(SystemHealth&&) = delete;

    /**
     * @brief Report a heartbeat from a subsystem
     * @param id The subsystem reporting health
     *
     * Call this periodically from each subsystem to indicate it's alive.
     * If heartbeats stop, the subsystem will be marked unhealthy after timeout.
     */
    void reportHeartbeat(SubsystemId id);

    /**
     * @brief Report a subsystem error
     * @param id The subsystem reporting the error
     * @param errorMessage Description of the error
     * @param isFatal If true, immediately marks subsystem unhealthy
     */
    void reportError(SubsystemId id, const std::string& errorMessage, bool isFatal = false);

    /**
     * @brief Report subsystem recovery
     * @param id The subsystem that has recovered
     *
     * Call this when a subsystem recovers from an error state.
     */
    void reportRecovery(SubsystemId id);

    /**
     * @brief Mark a subsystem as healthy or unhealthy
     * @param id The subsystem
     * @param healthy The health status
     */
    void setSubsystemHealth(SubsystemId id, bool healthy);

    /**
     * @brief Set the timeout for a subsystem
     * @param id The subsystem
     * @param timeout Timeout duration for heartbeat checks
     */
    void setSubsystemTimeout(SubsystemId id, std::chrono::milliseconds timeout);

    /**
     * @brief Check all subsystems for timeout
     *
     * Call this periodically (e.g., every 100ms) from a watchdog thread.
     * Updates subsystem health and recalculates system mode.
     */
    void checkTimeouts();

    /**
     * @brief Get the current system operating mode
     * @return Current SystemMode
     */
    [[nodiscard]] SystemMode currentMode() const;

    /**
     * @brief Check if a specific subsystem is healthy
     * @param id The subsystem to check
     * @return true if the subsystem is healthy
     */
    [[nodiscard]] bool isSubsystemHealthy(SubsystemId id) const;

    /**
     * @brief Get the status of all subsystems
     * @return Vector of SubsystemStatus
     */
    [[nodiscard]] std::vector<SubsystemStatus> getAllStatus() const;

    /**
     * @brief Get the status of a specific subsystem
     * @param id The subsystem
     * @return SubsystemStatus for the subsystem
     */
    [[nodiscard]] SubsystemStatus getStatus(SubsystemId id) const;

    /**
     * @brief Set callback for mode changes
     * @param callback Function to call when mode changes
     */
    void setModeChangeCallback(ModeChangeCallback callback);

    /**
     * @brief Set callback for subsystem health changes
     * @param callback Function to call when subsystem health changes
     */
    void setHealthChangeCallback(HealthChangeCallback callback);

    /**
     * @brief Check if all subsystems are healthy
     * @return true if all subsystems are healthy
     */
    [[nodiscard]] bool isAllHealthy() const;

    /**
     * @brief Get count of unhealthy subsystems
     * @return Number of unhealthy subsystems
     */
    [[nodiscard]] size_t unhealthyCount() const;

    /**
     * @brief Request a safe shutdown
     *
     * Sets the mode to SafeShutdown and notifies all callbacks.
     */
    void requestShutdown();

    /**
     * @brief Reset all subsystems to healthy state
     *
     * Use with caution - primarily for testing or system restart.
     */
    void reset();

private:
    SystemHealth();
    ~SystemHealth() = default;

    void updateSystemMode();
    void notifyModeChange(SystemMode oldMode, SystemMode newMode);
    void notifyHealthChange(SubsystemId id, bool healthy);

    static constexpr size_t kNumSubsystems =
        static_cast<size_t>(SubsystemId::NumSubsystems);

    mutable std::mutex mutex_;
    std::atomic<SystemMode> currentMode_{SystemMode::Normal};
    SubsystemStatus subsystems_[kNumSubsystems];
    ModeChangeCallback modeCallback_;
    HealthChangeCallback healthCallback_;
    bool shutdownRequested_ = false;
};

/**
 * @class SubsystemGuard
 * @brief RAII helper for automatic heartbeat reporting
 *
 * Reports heartbeat in constructor and can optionally report on destruction.
 *
 * Example:
 * @code
 *   void processCanFrame() {
 *       SubsystemGuard guard(SubsystemId::CAN);
 *       // ... process frame ...
 *   } // Heartbeat reported automatically
 * @endcode
 */
class SubsystemGuard {
public:
    explicit SubsystemGuard(SubsystemId id, bool reportOnDestroy = false)
        : id_(id)
        , reportOnDestroy_(reportOnDestroy) {
        SystemHealth::instance().reportHeartbeat(id_);
    }

    ~SubsystemGuard() {
        if (reportOnDestroy_) {
            SystemHealth::instance().reportHeartbeat(id_);
        }
    }

    SubsystemGuard(const SubsystemGuard&) = delete;
    SubsystemGuard& operator=(const SubsystemGuard&) = delete;

private:
    SubsystemId id_;
    bool reportOnDestroy_;
};

} // namespace speeduino

#endif // COMMON_SYSTEM_HEALTH_HPP
