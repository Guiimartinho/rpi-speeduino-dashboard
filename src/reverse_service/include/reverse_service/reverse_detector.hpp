#ifndef REVERSE_SERVICE_REVERSE_DETECTOR_HPP
#define REVERSE_SERVICE_REVERSE_DETECTOR_HPP

#include "common/config_loader.hpp"
#include "common/zmq_messages.hpp"
#include <cstdint>
#include <functional>
#include <atomic>
#include <chrono>

namespace speeduino {

// Callback when reverse state changes
using ReverseStateCallback = std::function<void(bool engaged, uint8_t source)>;

class ReverseDetector {
public:
    ReverseDetector();
    ~ReverseDetector();

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
        CAN = 0,
        GPIO = 1
    };
    Source getSource() const { return m_source; }

private:
    void setState(bool engaged, Source source);
    bool initGpio();
    bool readGpio();

    ReverseConfig m_config;
    ReverseStateCallback m_callback;

    std::atomic<bool> m_engaged{false};
    std::atomic<uint32_t> m_lastChangeTimestamp{0};
    Source m_source{Source::CAN};

    // Debounce
    std::chrono::steady_clock::time_point m_lastTransition;
    bool m_pendingState{false};

    // GPIO handle (platform-specific)
#ifdef HAS_GPIOD
    struct gpiod_chip* m_gpioChip{nullptr};
    struct gpiod_line* m_gpioLine{nullptr};
#endif

    bool m_gpioInitialized{false};
};

} // namespace speeduino

#endif // REVERSE_SERVICE_REVERSE_DETECTOR_HPP
