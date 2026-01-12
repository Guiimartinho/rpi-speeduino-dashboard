/**
 * @file can_error_handler.cpp
 * @brief Implementation of CAN bus error handling and recovery
 */

#include "can_service/can_error_handler.hpp"
#include "common/logger.hpp"

#ifdef __linux__
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <linux/can/netlink.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#endif

#include <sstream>
#include <iomanip>

namespace speeduino {

CanErrorHandler::CanErrorHandler() {
    stats_.lastError = std::chrono::steady_clock::time_point::min();
    stats_.lastBusOff = std::chrono::steady_clock::time_point::min();
    stats_.lastRecovery = std::chrono::steady_clock::time_point::min();
}

bool CanErrorHandler::enableErrorFrames(int socketFd) {
#ifdef __linux__
    // Enable all error classes
    can_err_mask_t errMask = CAN_ERR_MASK;

    int ret = setsockopt(socketFd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
                        &errMask, sizeof(errMask));
    if (ret < 0) {
        Logger::error("Failed to enable CAN error frames: " +
                     std::string(strerror(errno)));
        return false;
    }

    Logger::info("CAN error frame reception enabled");
    return true;
#else
    (void)socketFd;
    return false;
#endif
}

void CanErrorHandler::processErrorFrame(uint32_t canId, const uint8_t* data) {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    stats_.lastError = now;
    stats_.totalErrors++;

    CanErrorEvent event;
    event.timestamp = now;

    // Parse error class from CAN ID
    if (canId & CAN_ERR_TX_TIMEOUT) {
        event.type = CanErrorType::TxTimeout;
        stats_.txTimeouts++;
        Logger::warn("CAN TX timeout");
    }

    if (canId & CAN_ERR_LOSTARB) {
        event.type = CanErrorType::ArbitrationLost;
        event.arbitrationLostBit = data[0];
        stats_.arbitrationLost++;
        Logger::debug("CAN arbitration lost at bit " +
                     std::to_string(data[0]));
    }

    if (canId & CAN_ERR_CRTL) {
        event.type = CanErrorType::ControllerError;

        // Parse controller state from data[1]
        if (data[1] & CAN_ERR_CRTL_RX_WARNING) {
            updateState(CanBusState::ErrorWarning);
        }
        if (data[1] & CAN_ERR_CRTL_TX_WARNING) {
            updateState(CanBusState::ErrorWarning);
        }
        if (data[1] & CAN_ERR_CRTL_RX_PASSIVE) {
            updateState(CanBusState::ErrorPassive);
        }
        if (data[1] & CAN_ERR_CRTL_TX_PASSIVE) {
            updateState(CanBusState::ErrorPassive);
        }
        if (data[1] & CAN_ERR_CRTL_ACTIVE) {
            updateState(CanBusState::ErrorActive);
        }
    }

    if (canId & CAN_ERR_PROT) {
        event.type = CanErrorType::ProtocolViolation;
        event.protocolErrorLocation = data[3];
        event.description = parseProtocolError(data[2], data[3]);
        stats_.protocolErrors++;
        Logger::warn("CAN protocol error: " + event.description);
    }

    if (canId & CAN_ERR_TRX) {
        event.type = CanErrorType::TransceiverError;
        Logger::warn("CAN transceiver error");
    }

    if (canId & CAN_ERR_ACK) {
        event.type = CanErrorType::NoAck;
        stats_.noAckErrors++;
        Logger::warn("CAN no acknowledgment");
    }

    if (canId & CAN_ERR_BUSOFF) {
        event.type = CanErrorType::BusOff;
        stats_.busOffEvents++;
        stats_.lastBusOff = now;
        updateState(CanBusState::BusOff);
        Logger::error("CAN BUS-OFF - controller disconnected from bus");
    }

    if (canId & CAN_ERR_BUSERROR) {
        event.type = CanErrorType::BusError;
        Logger::warn("CAN bus error");
    }

    if (canId & CAN_ERR_RESTARTED) {
        event.type = CanErrorType::ControllerRestarted;
        stats_.controllerRestarts++;
        stats_.lastRecovery = now;
        updateState(CanBusState::ErrorActive);
        Logger::info("CAN controller restarted - recovered from bus-off");
    }

    // Extract error counters (if provided)
    counters_.txErrorCount = data[6];
    counters_.rxErrorCount = data[7];

    event.state = currentState_.load(std::memory_order_acquire);
    event.counters = counters_;

    // Invoke error callback
    if (errorCallback_) {
        errorCallback_(event);
    }
#else
    (void)canId;
    (void)data;
#endif
}

CanBusState CanErrorHandler::currentState() const {
    return currentState_.load(std::memory_order_acquire);
}

CanErrorCounters CanErrorHandler::getCounters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_;
}

CanErrorStats CanErrorHandler::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool CanErrorHandler::hasError() const {
    return currentState() != CanBusState::ErrorActive;
}

bool CanErrorHandler::isOperational() const {
    return currentState() != CanBusState::BusOff;
}

void CanErrorHandler::setBusStateCallback(BusStateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = std::move(callback);
}

void CanErrorHandler::setErrorEventCallback(ErrorEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = std::move(callback);
}

bool CanErrorHandler::triggerRecovery(const std::string& interfaceName) {
#ifdef __linux__
    Logger::info("Triggering CAN recovery for " + interfaceName);

    // ═══════════════════════════════════════════════════════════════════════
    // SECURITY FIX: Removed system() call to prevent command injection
    // ISO 26262 ASIL-B: Input validation required for safety-critical systems
    // ═══════════════════════════════════════════════════════════════════════

    // Validate interface name to prevent injection attacks
    // Valid CAN interface names: can0, can1, vcan0, slcan0, etc.
    // Pattern: [a-z]+[0-9]+ (letters followed by numbers, max 15 chars)
    if (interfaceName.empty() || interfaceName.size() > IFNAMSIZ - 1) {
        Logger::error("Invalid interface name length: " + interfaceName);
        return false;
    }

    // Strict validation: only alphanumeric characters allowed
    bool hasLetters = false;
    bool hasDigits = false;
    for (char c : interfaceName) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            hasLetters = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            hasDigits = true;
        } else {
            // Reject any non-alphanumeric characters (prevents injection)
            Logger::error("Invalid character in interface name: " + interfaceName);
            return false;
        }
    }

    if (!hasLetters) {
        Logger::error("Interface name must contain letters: " + interfaceName);
        return false;
    }

    // Use ioctl-based restart instead of system() command
    // This is the safe, non-injectable approach
    int sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sockfd < 0) {
        Logger::error("Failed to create socket for CAN recovery: " +
                     std::string(strerror(errno)));
        return false;
    }

    // Get interface index
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // Ensure null termination

    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        Logger::error("Failed to get interface index for recovery: " +
                     std::string(strerror(errno)));
        ::close(sockfd);
        return false;
    }

    // Bring interface down
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        Logger::error("Failed to get interface flags: " +
                     std::string(strerror(errno)));
        ::close(sockfd);
        return false;
    }

    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        Logger::error("Failed to bring interface down: " +
                     std::string(strerror(errno)));
        ::close(sockfd);
        return false;
    }

    // Brief delay to allow hardware to reset
    usleep(100000);  // 100ms

    // Bring interface back up
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        Logger::error("Failed to bring interface up: " +
                     std::string(strerror(errno)));
        ::close(sockfd);
        return false;
    }

    ::close(sockfd);
    Logger::info("CAN recovery completed successfully via ioctl");
    return true;
#else
    (void)interfaceName;
    return false;
#endif
}

void CanErrorHandler::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = CanErrorStats{};
    counters_ = CanErrorCounters{};
    currentState_.store(CanBusState::ErrorActive, std::memory_order_release);
}

std::chrono::milliseconds CanErrorHandler::timeSinceLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stats_.lastError == std::chrono::steady_clock::time_point::min()) {
        return std::chrono::milliseconds::max();
    }

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats_.lastError);
}

std::string CanErrorHandler::getStatusString() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::stringstream ss;
    ss << "CAN Bus Status: " << canBusStateToString(currentState_.load())
       << ", TEC=" << static_cast<int>(counters_.txErrorCount)
       << ", REC=" << static_cast<int>(counters_.rxErrorCount)
       << ", Total Errors=" << stats_.totalErrors
       << ", Bus-Off Events=" << stats_.busOffEvents;

    return ss.str();
}

void CanErrorHandler::updateState(CanBusState newState) {
    // ═══════════════════════════════════════════════════════════════════════
    // DEADLOCK FIX: Callback executed with exception safety
    // ISO 26262 ASIL-B: Prevents recursive mutex acquisition deadlocks
    // ═══════════════════════════════════════════════════════════════════════
    // Called with mutex_ held

    auto oldState = currentState_.exchange(newState, std::memory_order_acq_rel);

    if (oldState != newState) {
        Logger::info(std::string("CAN bus state: ") +
                    canBusStateToString(oldState) + " -> " +
                    canBusStateToString(newState));

        // Copy callback while holding lock
        BusStateCallback callbackCopy;
        if (stateCallback_) {
            callbackCopy = stateCallback_;
        }

        // Release lock BEFORE invoking callback (exception-safe)
        if (callbackCopy) {
            mutex_.unlock();
            try {
                callbackCopy(oldState, newState);
            } catch (const std::exception& e) {
                Logger::error(std::string("Bus state callback exception: ") + e.what());
            } catch (...) {
                Logger::error("Bus state callback threw unknown exception");
            }
            mutex_.lock();
        }
    }
}

void CanErrorHandler::recordError(CanErrorType type) {
    // Called with mutex_ held
    (void)type;  // Type already counted in processErrorFrame
    stats_.totalErrors++;
    stats_.lastError = std::chrono::steady_clock::now();
}

std::string CanErrorHandler::parseProtocolError(uint8_t location,
                                                uint8_t type) const {
    std::stringstream ss;

#ifdef __linux__
    // Error type
    switch (type) {
        case CAN_ERR_PROT_BIT:      ss << "single bit error"; break;
        case CAN_ERR_PROT_FORM:     ss << "frame format error"; break;
        case CAN_ERR_PROT_STUFF:    ss << "bit stuffing error"; break;
        case CAN_ERR_PROT_BIT0:     ss << "unable to send dominant bit"; break;
        case CAN_ERR_PROT_BIT1:     ss << "unable to send recessive bit"; break;
        case CAN_ERR_PROT_OVERLOAD: ss << "bus overload"; break;
        case CAN_ERR_PROT_ACTIVE:   ss << "active error announcement"; break;
        case CAN_ERR_PROT_TX:       ss << "error on transmission"; break;
        default:                    ss << "unknown (0x" << std::hex << (int)type << ")";
    }

    ss << " at ";

    // Error location
    switch (location) {
        case CAN_ERR_PROT_LOC_SOF:       ss << "start of frame"; break;
        case CAN_ERR_PROT_LOC_ID28_21:   ss << "ID bits 28-21"; break;
        case CAN_ERR_PROT_LOC_ID20_18:   ss << "ID bits 20-18"; break;
        case CAN_ERR_PROT_LOC_SRTR:      ss << "SRTR bit"; break;
        case CAN_ERR_PROT_LOC_IDE:       ss << "IDE bit"; break;
        case CAN_ERR_PROT_LOC_ID17_13:   ss << "ID bits 17-13"; break;
        case CAN_ERR_PROT_LOC_ID12_05:   ss << "ID bits 12-5"; break;
        case CAN_ERR_PROT_LOC_ID04_00:   ss << "ID bits 4-0"; break;
        case CAN_ERR_PROT_LOC_RTR:       ss << "RTR bit"; break;
        case CAN_ERR_PROT_LOC_RES1:      ss << "reserved bit 1"; break;
        case CAN_ERR_PROT_LOC_RES0:      ss << "reserved bit 0"; break;
        case CAN_ERR_PROT_LOC_DLC:       ss << "data length code"; break;
        case CAN_ERR_PROT_LOC_DATA:      ss << "data section"; break;
        case CAN_ERR_PROT_LOC_CRC_SEQ:   ss << "CRC sequence"; break;
        case CAN_ERR_PROT_LOC_CRC_DEL:   ss << "CRC delimiter"; break;
        case CAN_ERR_PROT_LOC_ACK:       ss << "ACK slot"; break;
        case CAN_ERR_PROT_LOC_ACK_DEL:   ss << "ACK delimiter"; break;
        case CAN_ERR_PROT_LOC_EOF:       ss << "end of frame"; break;
        case CAN_ERR_PROT_LOC_INTERM:    ss << "intermission"; break;
        default:                         ss << "unknown (0x" << std::hex << (int)location << ")";
    }
#else
    ss << "type=0x" << std::hex << (int)type
       << " loc=0x" << (int)location;
#endif

    return ss.str();
}

// CanBusMonitor implementation

void CanBusMonitor::recordFrame(bool isError) {
    frameCount_.fetch_add(1, std::memory_order_relaxed);
    if (isError) {
        errorCount_.fetch_add(1, std::memory_order_relaxed);
    }
}

CanBusMonitor::HealthMetrics CanBusMonitor::getMetrics(
    const CanErrorHandler& errorHandler) const {

    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - startTime_).count();

    HealthMetrics metrics;
    metrics.state = errorHandler.currentState();
    metrics.totalFrames = frameCount_.load(std::memory_order_relaxed);
    metrics.totalErrors = errorCount_.load(std::memory_order_relaxed);

    if (elapsed > 0) {
        metrics.frameRate = static_cast<float>(metrics.totalFrames) / elapsed;
        metrics.errorRate = static_cast<float>(metrics.totalErrors) / elapsed;
    }

    // Consider healthy if error rate < 1% and not in bus-off
    metrics.isHealthy = (metrics.state != CanBusState::BusOff) &&
                       (metrics.errorRate < 0.01f * metrics.frameRate);

    return metrics;
}

void CanBusMonitor::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    frameCount_.store(0, std::memory_order_relaxed);
    errorCount_.store(0, std::memory_order_relaxed);
    startTime_ = std::chrono::steady_clock::now();
}

} // namespace speeduino
