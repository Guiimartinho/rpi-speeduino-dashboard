/**
 * @file can_error_handler.hpp
 * @brief CAN bus error handling and bus-off recovery
 *
 * Implements comprehensive CAN error handling including:
 * - Error frame parsing and classification
 * - Bus state monitoring (Error Active, Warning, Passive, Bus-Off)
 * - Automatic bus-off recovery
 * - Error statistics and diagnostics
 *
 * Based on SocketCAN error handling best practices and
 * automotive CAN bus reliability requirements.
 */

#ifndef CAN_SERVICE_CAN_ERROR_HANDLER_HPP
#define CAN_SERVICE_CAN_ERROR_HANDLER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#ifdef __linux__
    #include <linux/can/error.h>
#endif

namespace speeduino {

/**
 * @enum CanBusState
 * @brief CAN controller error states per ISO 11898
 */
enum class CanBusState : uint8_t {
    ErrorActive,   ///< TEC/REC < 96, normal operation
    ErrorWarning,  ///< TEC/REC >= 96, warning threshold
    ErrorPassive,  ///< TEC/REC >= 128, limited transmission
    BusOff         ///< TEC >= 256, controller disconnected from bus
};

/**
 * @brief Convert CanBusState to string
 */
inline const char* canBusStateToString(CanBusState state) {
    switch (state) {
        case CanBusState::ErrorActive:
            return "ErrorActive";
        case CanBusState::ErrorWarning:
            return "ErrorWarning";
        case CanBusState::ErrorPassive:
            return "ErrorPassive";
        case CanBusState::BusOff:
            return "BusOff";
        default:
            return "Unknown";
    }
}

/**
 * @enum CanErrorType
 * @brief Types of CAN errors
 */
enum class CanErrorType : uint8_t {
    None = 0,
    TxTimeout,           ///< Transmission timeout
    ArbitrationLost,     ///< Lost arbitration during transmission
    ControllerError,     ///< Controller internal error
    ProtocolViolation,   ///< Protocol error (bit, stuff, form, CRC)
    TransceiverError,    ///< Transceiver error
    NoAck,               ///< No acknowledgment received
    BusOff,              ///< Bus-off state entered
    BusError,            ///< General bus error
    ControllerRestarted  ///< Controller was restarted
};

/**
 * @brief Convert CanErrorType to string
 */
inline const char* canErrorTypeToString(CanErrorType type) {
    switch (type) {
        case CanErrorType::None:
            return "None";
        case CanErrorType::TxTimeout:
            return "TxTimeout";
        case CanErrorType::ArbitrationLost:
            return "ArbitrationLost";
        case CanErrorType::ControllerError:
            return "ControllerError";
        case CanErrorType::ProtocolViolation:
            return "ProtocolViolation";
        case CanErrorType::TransceiverError:
            return "TransceiverError";
        case CanErrorType::NoAck:
            return "NoAck";
        case CanErrorType::BusOff:
            return "BusOff";
        case CanErrorType::BusError:
            return "BusError";
        case CanErrorType::ControllerRestarted:
            return "ControllerRestarted";
        default:
            return "Unknown";
    }
}

/**
 * @struct CanErrorCounters
 * @brief CAN controller error counters
 */
struct CanErrorCounters {
    uint8_t txErrorCount = 0;  ///< Transmit error counter (TEC)
    uint8_t rxErrorCount = 0;  ///< Receive error counter (REC)
};

/**
 * @struct CanErrorStats
 * @brief Error statistics since service start
 */
struct CanErrorStats {
    uint64_t totalErrors        = 0;
    uint64_t txTimeouts         = 0;
    uint64_t arbitrationLost    = 0;
    uint64_t protocolErrors     = 0;
    uint64_t noAckErrors        = 0;
    uint64_t busOffEvents       = 0;
    uint64_t controllerRestarts = 0;
    std::chrono::steady_clock::time_point lastError;
    std::chrono::steady_clock::time_point lastBusOff;
    std::chrono::steady_clock::time_point lastRecovery;
};

/**
 * @struct CanErrorEvent
 * @brief Detailed error event information
 */
struct CanErrorEvent {
    CanErrorType type = CanErrorType::None;
    CanBusState state = CanBusState::ErrorActive;
    CanErrorCounters counters;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t arbitrationLostBit   = 0;  ///< Bit position where arb was lost
    uint8_t protocolErrorLocation = 0;  ///< Where protocol error occurred
    std::string description;
};

/**
 * @brief Callback for bus state changes
 */
using BusStateCallback = std::function<void(CanBusState oldState, CanBusState newState)>;

/**
 * @brief Callback for error events
 */
using ErrorEventCallback = std::function<void(const CanErrorEvent&)>;

/**
 * @class CanErrorHandler
 * @brief Handles CAN bus errors and recovery
 *
 * Example usage:
 * @code
 *   CanErrorHandler errorHandler;
 *
 *   errorHandler.setBusStateCallback([](CanBusState old, CanBusState new) {
 *       if (new == CanBusState::BusOff) {
 *           LOG_ERROR("CAN bus-off! Recovery will be automatic.");
 *       }
 *   });
 *
 *   // Enable error frames on socket
 *   errorHandler.enableErrorFrames(canSocket);
 *
 *   // In receive loop:
 *   if (frame.is_error) {
 *       errorHandler.processErrorFrame(frame);
 *   }
 * @endcode
 */
class CanErrorHandler {
public:
    /**
     * @brief Constructor
     */
    CanErrorHandler();

    /**
     * @brief Destructor
     */
    ~CanErrorHandler() = default;

    // Non-copyable
    CanErrorHandler(const CanErrorHandler&)            = delete;
    CanErrorHandler& operator=(const CanErrorHandler&) = delete;

    /**
     * @brief Enable reception of error frames on a socket
     * @param socketFd The CAN socket file descriptor
     * @return true if error frames were enabled
     *
     * Sets the CAN_RAW_ERR_FILTER socket option to receive
     * all error classes.
     */
    bool enableErrorFrames(int socketFd);

    /**
     * @brief Process a received CAN error frame
     * @param canId The CAN ID (with CAN_ERR_FLAG set)
     * @param data The 8 bytes of error data
     *
     * Parses the error frame, updates internal state, and
     * triggers callbacks as appropriate.
     */
    void processErrorFrame(uint32_t canId, const uint8_t* data);

    /**
     * @brief Get the current bus state
     * @return Current CanBusState
     */
    [[nodiscard]] CanBusState currentState() const;

    /**
     * @brief Get the current error counters
     * @return CanErrorCounters with TEC and REC
     */
    [[nodiscard]] CanErrorCounters getCounters() const;

    /**
     * @brief Get error statistics
     * @return CanErrorStats with all counters
     */
    [[nodiscard]] CanErrorStats getStats() const;

    /**
     * @brief Check if the bus is in error state (not ErrorActive)
     * @return true if in warning, passive, or bus-off state
     */
    [[nodiscard]] bool hasError() const;

    /**
     * @brief Check if the bus is operational (not bus-off)
     * @return true if bus can communicate
     */
    [[nodiscard]] bool isOperational() const;

    /**
     * @brief Set callback for bus state changes
     * @param callback Function to call when state changes
     */
    void setBusStateCallback(BusStateCallback callback);

    /**
     * @brief Set callback for error events
     * @param callback Function to call on each error
     */
    void setErrorEventCallback(ErrorEventCallback callback);

    /**
     * @brief Manually trigger bus-off recovery
     * @param interfaceName The interface name (e.g., "can0")
     * @return true if recovery was initiated
     *
     * Uses netlink to restart the interface. Note that automatic
     * recovery via restart-ms is preferred.
     */
    bool triggerRecovery(const std::string& interfaceName);

    /**
     * @brief Reset error statistics
     */
    void resetStats();

    /**
     * @brief Get time since last error
     * @return Duration since last error, or max duration if no errors
     */
    [[nodiscard]] std::chrono::milliseconds timeSinceLastError() const;

    /**
     * @brief Get a human-readable status string
     * @return Status description
     */
    [[nodiscard]] std::string getStatusString() const;

private:
    void updateState(CanBusState newState);
    void recordError(CanErrorType type);
    std::string parseProtocolError(uint8_t location, uint8_t type) const;

    std::atomic<CanBusState> currentState_{CanBusState::ErrorActive};
    CanErrorCounters counters_;
    CanErrorStats stats_;

    BusStateCallback stateCallback_;
    ErrorEventCallback errorCallback_;

    mutable std::mutex mutex_;
};

/**
 * @class CanBusMonitor
 * @brief Continuous CAN bus health monitoring
 *
 * Periodically checks bus state and provides health metrics.
 */
class CanBusMonitor {
public:
    struct HealthMetrics {
        CanBusState state    = CanBusState::ErrorActive;
        float errorRate      = 0.0f;  ///< Errors per second
        float frameRate      = 0.0f;  ///< Frames per second
        uint64_t totalFrames = 0;
        uint64_t totalErrors = 0;
        bool isHealthy       = true;
    };

    /**
     * @brief Update metrics with new frame
     * @param isError true if this was an error frame
     */
    void recordFrame(bool isError);

    /**
     * @brief Calculate and return health metrics
     * @param errorHandler Reference to error handler for state
     * @return Current health metrics
     */
    [[nodiscard]] HealthMetrics getMetrics(const CanErrorHandler& errorHandler) const;

    /**
     * @brief Reset all metrics
     */
    void reset();

private:
    std::atomic<uint64_t> frameCount_{0};
    std::atomic<uint64_t> errorCount_{0};
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    mutable std::mutex mutex_;
};

}  // namespace speeduino

#endif  // CAN_SERVICE_CAN_ERROR_HANDLER_HPP
