/**
 * @file realtime_utils.cpp
 * @brief Implementation of real-time utilities
 */

#include "common/realtime_utils.hpp"
#include "common/logger.hpp"

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#endif

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace speeduino {

namespace {

#ifdef __linux__
int toLinuxPolicy(SchedulingPolicy policy) {
    switch (policy) {
        case SchedulingPolicy::Normal:     return SCHED_OTHER;
        case SchedulingPolicy::Fifo:       return SCHED_FIFO;
        case SchedulingPolicy::RoundRobin: return SCHED_RR;
        case SchedulingPolicy::Batch:      return SCHED_BATCH;
        case SchedulingPolicy::Idle:       return SCHED_IDLE;
#ifdef SCHED_DEADLINE
        case SchedulingPolicy::Deadline:   return SCHED_DEADLINE;
#endif
        default:                           return SCHED_OTHER;
    }
}

SchedulingPolicy fromLinuxPolicy(int policy) {
    switch (policy) {
        case SCHED_OTHER: return SchedulingPolicy::Normal;
        case SCHED_FIFO:  return SchedulingPolicy::Fifo;
        case SCHED_RR:    return SchedulingPolicy::RoundRobin;
        case SCHED_BATCH: return SchedulingPolicy::Batch;
        case SCHED_IDLE:  return SchedulingPolicy::Idle;
#ifdef SCHED_DEADLINE
        case SCHED_DEADLINE: return SchedulingPolicy::Deadline;
#endif
        default:          return SchedulingPolicy::Normal;
    }
}
#endif

} // anonymous namespace

bool RealtimeUtils::applyConfig(const RealtimeConfig& config) {
    bool success = true;

    // Apply CPU affinity
    if (!config.cpuAffinity.empty()) {
        if (!setThreadAffinity(config.cpuAffinity)) {
            Logger::warn("Failed to set CPU affinity");
            success = false;
        }
    }

    // Apply scheduling policy
    if (config.policy != SchedulingPolicy::Normal || config.priority > 0) {
        if (!setThreadScheduling(config.policy, config.priority)) {
            Logger::warn("Failed to set scheduling policy");
            success = false;
        }
    }

    // Apply nice value
    if (config.nice != 0) {
        if (!setThreadNice(config.nice)) {
            Logger::warn("Failed to set nice value");
            success = false;
        }
    }

    // Lock memory
    if (config.lockMemory) {
        if (!lockMemory()) {
            Logger::warn("Failed to lock memory");
            success = false;
        }
    }

    // Prefault stack
    if (config.stackSize > 0) {
        prefaultStack(config.stackSize);
    }

    if (success) {
        Logger::info("Real-time configuration applied successfully");
        logThreadConfig();
    }

    return success;
}

bool RealtimeUtils::setThreadAffinity(const std::vector<int>& cpus) {
#ifdef __linux__
    if (cpus.empty()) {
        return true;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    for (int cpu : cpus) {
        if (cpu >= 0 && cpu < CPU_SETSIZE) {
            CPU_SET(cpu, &cpuset);
        }
    }

    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (ret != 0) {
        Logger::error("pthread_setaffinity_np failed: " +
                     std::string(strerror(ret)));
        return false;
    }

    std::string cpuStr;
    for (size_t i = 0; i < cpus.size(); ++i) {
        if (i > 0) cpuStr += ",";
        cpuStr += std::to_string(cpus[i]);
    }
    Logger::debug("Set CPU affinity to: " + cpuStr);
    return true;
#else
    (void)cpus;
    return false;
#endif
}

bool RealtimeUtils::setThreadScheduling(SchedulingPolicy policy, int priority) {
#ifdef __linux__
    int linuxPolicy = toLinuxPolicy(policy);

    struct sched_param param;
    std::memset(&param, 0, sizeof(param));
    param.sched_priority = priority;

    // Validate priority range
    int minPrio = sched_get_priority_min(linuxPolicy);
    int maxPrio = sched_get_priority_max(linuxPolicy);

    if (priority < minPrio || priority > maxPrio) {
        if (policy == SchedulingPolicy::Fifo ||
            policy == SchedulingPolicy::RoundRobin) {
            param.sched_priority = std::clamp(priority, minPrio, maxPrio);
            Logger::warn("Priority " + std::to_string(priority) +
                        " clamped to " + std::to_string(param.sched_priority));
        }
    }

    int ret = pthread_setschedparam(pthread_self(), linuxPolicy, &param);
    if (ret != 0) {
        Logger::error("pthread_setschedparam failed: " +
                     std::string(strerror(ret)));
        return false;
    }

    Logger::debug("Set scheduling: " + std::string(schedulingPolicyToString(policy)) +
                 " priority " + std::to_string(param.sched_priority));
    return true;
#else
    (void)policy;
    (void)priority;
    return false;
#endif
}

bool RealtimeUtils::setThreadNice(int nice) {
#ifdef __linux__
    nice = std::clamp(nice, -20, 19);

    if (setpriority(PRIO_PROCESS, 0, nice) != 0) {
        Logger::error("setpriority failed: " +
                     std::string(strerror(errno)));
        return false;
    }

    Logger::debug("Set nice value to: " + std::to_string(nice));
    return true;
#else
    (void)nice;
    return false;
#endif
}

bool RealtimeUtils::lockMemory() {
#ifdef __linux__
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        Logger::error("mlockall failed: " +
                     std::string(strerror(errno)));
        return false;
    }

    Logger::debug("Memory locked (mlockall)");
    return true;
#else
    return false;
#endif
}

bool RealtimeUtils::unlockMemory() {
#ifdef __linux__
    if (munlockall() != 0) {
        Logger::error("munlockall failed: " +
                     std::string(strerror(errno)));
        return false;
    }

    Logger::debug("Memory unlocked (munlockall)");
    return true;
#else
    return false;
#endif
}

void RealtimeUtils::prefaultStack(size_t size) {
    // Allocate array on stack and touch each page
    volatile char* stack = static_cast<volatile char*>(alloca(size));
    size_t pageSize = 4096;  // Typical page size

    for (size_t i = 0; i < size; i += pageSize) {
        stack[i] = 0;
    }

    Logger::debug("Prefaulted " + std::to_string(size) + " bytes of stack");
}

int RealtimeUtils::getNumCpus() {
#ifdef __linux__
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return 1;
#endif
}

std::vector<int> RealtimeUtils::getThreadAffinity() {
    std::vector<int> result;

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    if (pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0) {
        for (int i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &cpuset)) {
                result.push_back(i);
            }
        }
    }
#endif

    return result;
}

SchedulingPolicy RealtimeUtils::getThreadSchedulingPolicy() {
#ifdef __linux__
    int policy = sched_getscheduler(0);
    return fromLinuxPolicy(policy);
#else
    return SchedulingPolicy::Normal;
#endif
}

int RealtimeUtils::getThreadPriority() {
#ifdef __linux__
    struct sched_param param;
    if (sched_getparam(0, &param) == 0) {
        return param.sched_priority;
    }
#endif
    return 0;
}

bool RealtimeUtils::hasRealtimeCapability() {
#ifdef __linux__
    // Try to get current parameters - if we can set FIFO, we have capability
    struct sched_param param;
    param.sched_priority = 1;

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0) {
        // Restore normal scheduling
        param.sched_priority = 0;
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
        return true;
    }
    return false;
#else
    return false;
#endif
}

int RealtimeUtils::getMinPriority(SchedulingPolicy policy) {
#ifdef __linux__
    return sched_get_priority_min(toLinuxPolicy(policy));
#else
    (void)policy;
    return 0;
#endif
}

int RealtimeUtils::getMaxPriority(SchedulingPolicy policy) {
#ifdef __linux__
    return sched_get_priority_max(toLinuxPolicy(policy));
#else
    (void)policy;
    return 0;
#endif
}

std::optional<float> RealtimeUtils::getCpuTemperature() {
#ifdef __linux__
    // Raspberry Pi thermal zone
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (file.is_open()) {
        int temp;
        file >> temp;
        if (file.good()) {
            return static_cast<float>(temp) / 1000.0f;
        }
    }
#endif
    return std::nullopt;
}

bool RealtimeUtils::isRealtimeKernel() {
#ifdef __linux__
    struct utsname buf;
    if (uname(&buf) == 0) {
        std::string version = buf.version;
        return version.find("PREEMPT_RT") != std::string::npos ||
               version.find("PREEMPT RT") != std::string::npos;
    }
#endif
    return false;
}

std::string RealtimeUtils::getKernelVersion() {
#ifdef __linux__
    struct utsname buf;
    if (uname(&buf) == 0) {
        return std::string(buf.release) + " " + std::string(buf.version);
    }
#endif
    return "Unknown";
}

void RealtimeUtils::logThreadConfig() {
    auto affinity = getThreadAffinity();
    auto policy = getThreadSchedulingPolicy();
    int priority = getThreadPriority();

    std::stringstream ss;
    ss << "Thread config: ";

    // CPU affinity
    ss << "CPUs=[";
    for (size_t i = 0; i < affinity.size(); ++i) {
        if (i > 0) ss << ",";
        ss << affinity[i];
    }
    ss << "] ";

    // Scheduling
    ss << "Policy=" << schedulingPolicyToString(policy);
    ss << " Priority=" << priority;

    // RT kernel
    if (isRealtimeKernel()) {
        ss << " [PREEMPT_RT]";
    }

    Logger::info(ss.str());
}

// ScopedRealtimeConfig implementation

ScopedRealtimeConfig::ScopedRealtimeConfig(const RealtimeConfig& config) {
    // Save current config
    originalAffinity_ = RealtimeUtils::getThreadAffinity();
    originalPolicy_ = RealtimeUtils::getThreadSchedulingPolicy();
    originalPriority_ = RealtimeUtils::getThreadPriority();

    // Apply new config
    applied_ = RealtimeUtils::applyConfig(config);
}

ScopedRealtimeConfig::~ScopedRealtimeConfig() {
    // Restore original config
    if (!originalAffinity_.empty()) {
        RealtimeUtils::setThreadAffinity(originalAffinity_);
    }
    RealtimeUtils::setThreadScheduling(originalPolicy_, originalPriority_);
}

// LatencyTracker implementation

void LatencyTracker::record(uint64_t microseconds) {
    min_ = std::min(min_, microseconds);
    max_ = std::max(max_, microseconds);
    sum_ += microseconds;
    ++count_;
}

double LatencyTracker::average() const noexcept {
    if (count_ == 0) return 0.0;
    return static_cast<double>(sum_) / static_cast<double>(count_);
}

void LatencyTracker::reset() {
    min_ = UINT64_MAX;
    max_ = 0;
    sum_ = 0;
    count_ = 0;
}

std::string LatencyTracker::summary() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (count_ == 0) {
        ss << "No samples";
    } else {
        ss << "Latency (us): min=" << min_
           << " max=" << max_
           << " avg=" << average()
           << " samples=" << count_;
    }

    return ss.str();
}

} // namespace speeduino
