/**
 * @file realtime_utils.hpp
 * @brief Real-time utilities for CPU affinity and scheduling
 *
 * Provides utilities to optimize real-time performance on Raspberry Pi 5
 * and other embedded Linux systems. Based on PREEMPT_RT best practices
 * and automotive real-time requirements.
 */

#ifndef COMMON_REALTIME_UTILS_HPP
#define COMMON_REALTIME_UTILS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace speeduino {

/**
 * @enum SchedulingPolicy
 * @brief Linux scheduling policies
 */
enum class SchedulingPolicy {
    Normal,      ///< SCHED_OTHER - default time-sharing
    Fifo,        ///< SCHED_FIFO - first-in-first-out real-time
    RoundRobin,  ///< SCHED_RR - round-robin real-time
    Batch,       ///< SCHED_BATCH - batch processing
    Idle,        ///< SCHED_IDLE - very low priority
    Deadline     ///< SCHED_DEADLINE - deadline scheduling
};

/**
 * @brief Convert SchedulingPolicy to string
 */
inline const char* schedulingPolicyToString(SchedulingPolicy policy) {
    switch (policy) {
        case SchedulingPolicy::Normal:
            return "SCHED_OTHER";
        case SchedulingPolicy::Fifo:
            return "SCHED_FIFO";
        case SchedulingPolicy::RoundRobin:
            return "SCHED_RR";
        case SchedulingPolicy::Batch:
            return "SCHED_BATCH";
        case SchedulingPolicy::Idle:
            return "SCHED_IDLE";
        case SchedulingPolicy::Deadline:
            return "SCHED_DEADLINE";
        default:
            return "Unknown";
    }
}

/**
 * @struct RealtimeConfig
 * @brief Configuration for real-time thread setup
 */
struct RealtimeConfig {
    /// CPU cores to pin the thread to (empty = no affinity)
    std::vector<int> cpuAffinity;

    /// Scheduling policy
    SchedulingPolicy policy = SchedulingPolicy::Normal;

    /// Priority for real-time policies (1-99, higher = more priority)
    int priority = 0;

    /// Nice value for normal scheduling (-20 to 19)
    int nice = 0;

    /// Lock memory to prevent page faults
    bool lockMemory = false;

    /// Preallocate stack memory
    size_t stackSize = 0;
};

/**
 * @brief Recommended real-time config for CAN service
 *
 * Pins to CPU 2, uses SCHED_FIFO with priority 80,
 * and locks memory.
 */
inline RealtimeConfig canServiceConfig() {
    RealtimeConfig config;
    config.cpuAffinity = {2};
    config.policy      = SchedulingPolicy::Fifo;
    config.priority    = 80;
    config.lockMemory  = true;
    return config;
}

/**
 * @brief Recommended config for reverse service
 *
 * Lower priority than CAN, shares CPU 2.
 */
inline RealtimeConfig reverseServiceConfig() {
    RealtimeConfig config;
    config.cpuAffinity = {2};
    config.policy      = SchedulingPolicy::Fifo;
    config.priority    = 70;
    config.lockMemory  = true;
    return config;
}

/**
 * @brief Recommended config for HMI (non-real-time)
 *
 * Normal scheduling on CPUs 0-1, leaves 2-3 for real-time.
 */
inline RealtimeConfig hmiServiceConfig() {
    RealtimeConfig config;
    config.cpuAffinity = {0, 1};
    config.policy      = SchedulingPolicy::Normal;
    config.nice        = 0;
    return config;
}

/**
 * @class RealtimeUtils
 * @brief Utilities for real-time thread configuration
 *
 * Example usage:
 * @code
 *   // In can_service main():
 *   auto config = canServiceConfig();
 *   if (!RealtimeUtils::applyConfig(config)) {
 *       LOG_WARN("Failed to apply real-time config, continuing with defaults");
 *   }
 *
 *   // Or configure current thread manually:
 *   RealtimeUtils::setThreadAffinity({2, 3});
 *   RealtimeUtils::setThreadScheduling(SchedulingPolicy::Fifo, 80);
 * @endcode
 */
class RealtimeUtils {
public:
    /**
     * @brief Apply a complete real-time configuration to the current thread
     * @param config The configuration to apply
     * @return true if all settings were applied successfully
     */
    static bool applyConfig(const RealtimeConfig& config);

    /**
     * @brief Set CPU affinity for the current thread
     * @param cpus List of CPU IDs to pin to
     * @return true if successful
     *
     * On Raspberry Pi 5, CPUs 2 and 3 are recommended for real-time
     * tasks when isolcpus kernel parameter is used.
     */
    static bool setThreadAffinity(const std::vector<int>& cpus);

    /**
     * @brief Set scheduling policy and priority for the current thread
     * @param policy The scheduling policy
     * @param priority Priority (1-99 for RT policies, ignored for others)
     * @return true if successful
     */
    static bool setThreadScheduling(SchedulingPolicy policy, int priority = 0);

    /**
     * @brief Set nice value for the current thread
     * @param nice Nice value (-20 to 19)
     * @return true if successful
     */
    static bool setThreadNice(int nice);

    /**
     * @brief Lock all current and future memory to prevent page faults
     * @return true if successful
     *
     * Requires CAP_IPC_LOCK capability or root privileges.
     */
    static bool lockMemory();

    /**
     * @brief Unlock memory (allow paging)
     * @return true if successful
     */
    static bool unlockMemory();

    /**
     * @brief Preallocate and fault in stack memory
     * @param size Size in bytes to preallocate
     *
     * Useful to prevent page faults in real-time code paths.
     */
    static void prefaultStack(size_t size);

    /**
     * @brief Get the number of available CPUs
     * @return Number of CPUs
     */
    static int getNumCpus();

    /**
     * @brief Get the current thread's CPU affinity
     * @return List of CPUs the thread can run on
     */
    static std::vector<int> getThreadAffinity();

    /**
     * @brief Get the current thread's scheduling policy
     * @return The scheduling policy
     */
    static SchedulingPolicy getThreadSchedulingPolicy();

    /**
     * @brief Get the current thread's scheduling priority
     * @return The priority (0 if not real-time)
     */
    static int getThreadPriority();

    /**
     * @brief Check if the current process has real-time capabilities
     * @return true if real-time scheduling can be used
     */
    static bool hasRealtimeCapability();

    /**
     * @brief Get the minimum valid priority for a scheduling policy
     * @param policy The scheduling policy
     * @return Minimum priority value
     */
    static int getMinPriority(SchedulingPolicy policy);

    /**
     * @brief Get the maximum valid priority for a scheduling policy
     * @param policy The scheduling policy
     * @return Maximum priority value
     */
    static int getMaxPriority(SchedulingPolicy policy);

    /**
     * @brief Read CPU temperature (Raspberry Pi specific)
     * @return Temperature in Celsius, or nullopt on error
     */
    static std::optional<float> getCpuTemperature();

    /**
     * @brief Check if running on a real-time kernel (PREEMPT_RT)
     * @return true if PREEMPT_RT kernel is detected
     */
    static bool isRealtimeKernel();

    /**
     * @brief Get kernel version string
     * @return Kernel version
     */
    static std::string getKernelVersion();

    /**
     * @brief Log current thread's real-time configuration
     *
     * Logs CPU affinity, scheduling policy, and priority.
     */
    static void logThreadConfig();

private:
    RealtimeUtils() = delete;
};

/**
 * @class ScopedRealtimeConfig
 * @brief RAII helper to temporarily apply real-time configuration
 *
 * Saves current config, applies new config, and restores on destruction.
 */
class ScopedRealtimeConfig {
public:
    explicit ScopedRealtimeConfig(const RealtimeConfig& config);
    ~ScopedRealtimeConfig();

    ScopedRealtimeConfig(const ScopedRealtimeConfig&)            = delete;
    ScopedRealtimeConfig& operator=(const ScopedRealtimeConfig&) = delete;

    [[nodiscard]] bool wasApplied() const noexcept { return applied_; }

private:
    std::vector<int> originalAffinity_;
    SchedulingPolicy originalPolicy_;
    int originalPriority_;
    bool applied_;
};

/**
 * @class LatencyTracker
 * @brief Utility to track and report latency statistics
 *
 * Useful for profiling real-time performance.
 */
class LatencyTracker {
public:
    /**
     * @brief Record a latency measurement
     * @param microseconds Latency in microseconds
     */
    void record(uint64_t microseconds);

    /**
     * @brief Get the minimum recorded latency
     * @return Minimum latency in microseconds
     */
    [[nodiscard]] uint64_t min() const noexcept { return min_; }

    /**
     * @brief Get the maximum recorded latency
     * @return Maximum latency in microseconds
     */
    [[nodiscard]] uint64_t max() const noexcept { return max_; }

    /**
     * @brief Get the average latency
     * @return Average latency in microseconds
     */
    [[nodiscard]] double average() const noexcept;

    /**
     * @brief Get the number of samples recorded
     * @return Sample count
     */
    [[nodiscard]] uint64_t count() const noexcept { return count_; }

    /**
     * @brief Reset all statistics
     */
    void reset();

    /**
     * @brief Get a formatted summary string
     * @return Summary of min/max/avg latency
     */
    [[nodiscard]] std::string summary() const;

private:
    uint64_t min_   = UINT64_MAX;
    uint64_t max_   = 0;
    uint64_t sum_   = 0;
    uint64_t count_ = 0;
};

}  // namespace speeduino

#endif  // COMMON_REALTIME_UTILS_HPP
