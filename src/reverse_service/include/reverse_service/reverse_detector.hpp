#ifndef REVERSE_SERVICE_REVERSE_DETECTOR_HPP
#define REVERSE_SERVICE_REVERSE_DETECTOR_HPP

#include "common/config_loader.hpp"
#include "common/zmq_messages.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>

// Forward declarations for gpiod v2 types
#ifdef HAS_GPIOD
struct gpiod_chip;
struct gpiod_line_request;
#endif

namespace speeduino {

// Callback when reverse state changes
using ReverseStateCallback = std::function<void(bool engaged, uint8_t source)>;

class ReverseDetector {
public:
    ReverseDetector();
    // MISRA C++:2008 Rule 15-5-1: Destructors shall not throw exceptions
    ~ReverseDetector() noexcept;

    // Initialize with config
    bool init(const ReverseConfig& config);

    // Shutdown
    void shutdown();

    // Set callback for state changes
    void setCallback(ReverseStateCallback callback);

    // Process a CAN frame (call from CAN receive loop)
    void procesCanFrame(uint32_t can_id, const uint8_t* data, uint8_t dlc);

    // Check GPIO state (call periodically if GPIO fallback enabled)
    void checkGpio();

    // Get current state
    bool isReverseEngaged() const { return m_engaged; }

    // Get last state change timestamp
    uint32_t getLastChangeTimestamp() const { return m_lastChangeTimestamp; }

    // Source of last detection
    enum class Source : uint8_t {
        CAN  = 0,
        GPIO = 1
    };
    // FIX #5: Thread-safe getter using atomic load
    Source getSource() const { return m_source.load(std::memory_order_acquire); }

private:
    void setState(bool engaged, Source source);
    bool initGpio();
    bool readGpio();

    ReverseConfig m_config;
    ReverseStateCallback m_callback;

    std::atomic<bool> m_engaged{false};
    std::atomic<uint32_t> m_lastChangeTimestamp{0};
    // FIX #5: Made atomic to prevent data race between CAN/GPIO threads and readers
    std::atomic<Source> m_source{Source::CAN};

    // ═══════════════════════════════════════════════════════════════════════
    // ISO 26262 DATA RACE FIX: Debounce state protected by mutex
    // Both procesCanFrame() and checkGpio() may run on different threads
    // MISRA C++:2008 Rule 14-7-1: All shared data requires synchronization
    // ═══════════════════════════════════════════════════════════════════════
    mutable std::mutex m_debounceMutex;
    std::chrono::steady_clock::time_point m_lastTransition;
    bool m_pendingState{false};

    // GPIO handle (platform-specific) - gpiod v2 API
#ifdef HAS_GPIOD
    gpiod_chip* m_gpioChip{nullptr};
    gpiod_line_request* m_gpioRequest{nullptr};
    unsigned int m_gpioOffset{0};
#endif

    bool m_gpioInitialized{false};
};

}  // namespace speeduino

#endif  // REVERSE_SERVICE_REVERSE_DETECTOR_HPP
