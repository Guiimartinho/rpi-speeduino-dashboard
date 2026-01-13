#ifndef CAN_SERVICE_CAN_INTERFACE_HPP
#define CAN_SERVICE_CAN_INTERFACE_HPP

#include <cstdint>
#include <array>
#include <string>
#include <functional>
#include <atomic>
#include <optional>

namespace speeduino {

// Raw CAN frame structure
struct CanFrame {
    uint32_t id = 0;
    uint8_t  dlc = 0;
    std::array<uint8_t, 8> data = {0};
    uint32_t timestamp_us = 0;  // Relative timestamp

    // Extended frame flag
    bool is_extended = false;
    bool is_rtr = false;
    bool is_error = false;
};

// CAN interface status
struct CanStatus {
    bool     connected = false;
    uint32_t rx_count = 0;
    uint32_t tx_count = 0;
    uint32_t error_count = 0;
    uint32_t last_rx_timestamp = 0;
};

// Callback for received frames
using CanFrameCallback = std::function<void(const CanFrame&)>;

class CanInterface {
public:
    CanInterface();
    // MISRA C++:2008 Rule 15-5-1: Destructors shall not throw exceptions
    virtual ~CanInterface() noexcept;

    // Non-copyable
    CanInterface(const CanInterface&) = delete;
    CanInterface& operator=(const CanInterface&) = delete;

    // Initialize interface
    virtual bool open(const std::string& interface_name);

    // Close interface
    virtual void close();

    // Check if connected
    virtual bool isConnected() const;

    // Send a frame
    virtual bool send(const CanFrame& frame);

    // Receive a frame (blocking with timeout)
    virtual std::optional<CanFrame> receive(int timeout_ms = 100);

    // Set receive callback (called from read thread)
    virtual void setCallback(CanFrameCallback callback);

    // Start/stop async receive loop
    virtual void startReceiveLoop();
    virtual void stopReceiveLoop();

    // Get status
    virtual CanStatus getStatus() const;

    // Get file descriptor (for poll/select)
    int getFd() const { return m_socket; }

private:
    int m_socket = -1;
    std::string m_interfaceName;
    CanFrameCallback m_callback;

    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_rxCount{0};
    std::atomic<uint32_t> m_txCount{0};
    std::atomic<uint32_t> m_errorCount{0};
    std::atomic<uint32_t> m_lastRxTimestamp{0};
};

} // namespace speeduino

#endif // CAN_SERVICE_CAN_INTERFACE_HPP
