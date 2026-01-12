#include "can_service/can_interface.hpp"
#include "common/logger.hpp"

#ifdef __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#endif

#include <chrono>

namespace speeduino {

namespace {

// ═══════════════════════════════════════════════════════════════════════════
// ISO 26262 ASIL-B: Named constants for CAN protocol limits
// MISRA C++:2008 Rule 2-13-5: Avoid magic numbers
// ═══════════════════════════════════════════════════════════════════════════
constexpr uint8_t CAN_CLASSIC_MAX_DLC = 8;    // CAN 2.0 maximum DLC
constexpr uint8_t CAN_FD_MAX_DLC = 64;        // CAN FD maximum DLC
constexpr uint32_t CAN_STD_ID_MAX = 0x7FF;    // 11-bit standard ID max
constexpr uint32_t CAN_EXT_ID_MAX = 0x1FFFFFFF; // 29-bit extended ID max

// Thread-safe error string helper
std::string getErrorString(int errnum) {
    // ═══════════════════════════════════════════════════════════════════════
    // THREAD SAFETY FIX: strerror() is not thread-safe
    // Using strerror_r() for POSIX compliance
    // ═══════════════════════════════════════════════════════════════════════
#ifdef __linux__
    char errbuf[256];
    // GNU-specific strerror_r returns char* (may or may not use buffer)
    const char* result = strerror_r(errnum, errbuf, sizeof(errbuf));
    return std::string(result);
#else
    return std::string(strerror(errnum));
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// TIMESTAMP OVERFLOW FIX: Handle 49-day rollover correctly
// Returns monotonic milliseconds with explicit wrap handling
// ═══════════════════════════════════════════════════════════════════════════
uint32_t getMonotonicMs() {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    // Explicit mask to handle overflow after ~49 days
    return static_cast<uint32_t>(ms & 0xFFFFFFFF);
}

// Calculate time delta with wrap-around handling
int32_t getTimeDeltaMs(uint32_t now, uint32_t past) {
    // Handle 32-bit wrap-around correctly
    // If now < past due to overflow, the subtraction still gives correct delta
    return static_cast<int32_t>(now - past);
}

} // anonymous namespace

CanInterface::CanInterface() = default;

CanInterface::~CanInterface() noexcept {
    close();
}

bool CanInterface::open(const std::string& interface_name) {
#ifdef __linux__
    m_interfaceName = interface_name;

    // Create socket
    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        LOG_ERROR("Failed to create CAN socket: " + getErrorString(errno));
        return false;
    }

    // Get interface index
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // Ensure null termination

    if (ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        LOG_ERROR("Failed to get interface index for " + interface_name +
                  ": " + getErrorString(errno));
        ::close(m_socket);
        m_socket = -1;
        return false;
    }

    // Bind socket
    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind CAN socket: " + getErrorString(errno));
        ::close(m_socket);
        m_socket = -1;
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // FIX: Check setsockopt return value (MISRA C++:2008 Rule 0-1-4)
    // ═══════════════════════════════════════════════════════════════════════
    int timestamp_on = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_TIMESTAMP, &timestamp_on, sizeof(timestamp_on)) < 0) {
        LOG_WARN("Failed to enable CAN timestamps: " + getErrorString(errno) +
                 " - timestamps will use system time");
        // Non-fatal: continue without hardware timestamps
    }

    LOG_INFO("CAN interface " + interface_name + " opened successfully");
    return true;
#else
    (void)interface_name;
    LOG_ERROR("CAN interface not supported on this platform");
    return false;
#endif
}

void CanInterface::close() {
    stopReceiveLoop();

#ifdef __linux__
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
        LOG_INFO("CAN interface closed");
    }
#endif
}

bool CanInterface::isConnected() const {
    return m_socket >= 0;
}

bool CanInterface::send(const CanFrame& frame) {
#ifdef __linux__
    if (m_socket < 0) {
        LOG_ERROR("CAN socket not open");
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BUFFER OVERFLOW FIX: Validate DLC before memcpy
    // ISO 26262 ASIL-B: Bounds checking required for memory safety
    // MISRA C++:2008 Rule 5-0-15: Array bounds must be checked
    // ═══════════════════════════════════════════════════════════════════════
    if (frame.dlc > CAN_CLASSIC_MAX_DLC) {
        LOG_ERROR("Invalid CAN DLC " + std::to_string(frame.dlc) +
                  " (max " + std::to_string(CAN_CLASSIC_MAX_DLC) + ")");
        m_errorCount++;
        return false;
    }

    // Validate CAN ID ranges
    if (frame.is_extended) {
        if (frame.id > CAN_EXT_ID_MAX) {
            LOG_ERROR("Invalid extended CAN ID 0x" + std::to_string(frame.id) +
                      " (max 0x1FFFFFFF)");
            m_errorCount++;
            return false;
        }
    } else {
        if (frame.id > CAN_STD_ID_MAX) {
            LOG_ERROR("Invalid standard CAN ID 0x" + std::to_string(frame.id) +
                      " (max 0x7FF)");
            m_errorCount++;
            return false;
        }
    }

    struct can_frame cf;
    std::memset(&cf, 0, sizeof(cf));

    cf.can_id = frame.id;
    if (frame.is_extended) {
        cf.can_id |= CAN_EFF_FLAG;
    }
    if (frame.is_rtr) {
        cf.can_id |= CAN_RTR_FLAG;
    }

    cf.can_dlc = frame.dlc;
    // Safe: DLC validated above to be <= 8 (size of cf.data)
    std::memcpy(cf.data, frame.data.data(), frame.dlc);

    ssize_t nbytes = write(m_socket, &cf, sizeof(cf));
    if (nbytes != sizeof(cf)) {
        LOG_ERROR("CAN write failed: " + getErrorString(errno));
        m_errorCount++;
        return false;
    }

    m_txCount++;
    return true;
#else
    (void)frame;
    return false;
#endif
}

std::optional<CanFrame> CanInterface::receive(int timeout_ms) {
#ifdef __linux__
    if (m_socket < 0) {
        return std::nullopt;
    }

    // Poll with timeout
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return std::nullopt;  // Timeout or error
    }

    struct can_frame cf;
    ssize_t nbytes = read(m_socket, &cf, sizeof(cf));
    if (nbytes != sizeof(cf)) {
        if (nbytes < 0 && errno != EAGAIN) {
            LOG_ERROR("CAN read error: " + getErrorString(errno));
            m_errorCount++;
        }
        return std::nullopt;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BUFFER OVERFLOW FIX: Validate received DLC before memcpy
    // ═══════════════════════════════════════════════════════════════════════
    uint8_t safeDlc = cf.can_dlc;
    if (safeDlc > CAN_CLASSIC_MAX_DLC) {
        LOG_WARN("Received CAN frame with invalid DLC " +
                 std::to_string(safeDlc) + ", clamping to 8");
        safeDlc = CAN_CLASSIC_MAX_DLC;
    }

    CanFrame frame;
    frame.id = cf.can_id & CAN_EFF_MASK;
    frame.is_extended = (cf.can_id & CAN_EFF_FLAG) != 0;
    frame.is_rtr = (cf.can_id & CAN_RTR_FLAG) != 0;
    frame.is_error = (cf.can_id & CAN_ERR_FLAG) != 0;
    frame.dlc = safeDlc;
    // Safe: safeDlc validated to be <= 8 (size of frame.data)
    std::memcpy(frame.data.data(), cf.data, safeDlc);
    frame.timestamp_us = getMonotonicMs() * 1000;

    m_rxCount++;
    m_lastRxTimestamp = getMonotonicMs();

    return frame;
#else
    (void)timeout_ms;
    return std::nullopt;
#endif
}

void CanInterface::setCallback(CanFrameCallback callback) {
    m_callback = std::move(callback);
}

void CanInterface::startReceiveLoop() {
    m_running = true;
}

void CanInterface::stopReceiveLoop() {
    m_running = false;
}

CanStatus CanInterface::getStatus() const {
    CanStatus status;
    status.connected = isConnected();
    status.rx_count = m_rxCount;
    status.tx_count = m_txCount;
    status.error_count = m_errorCount;
    status.last_rx_timestamp = m_lastRxTimestamp;
    return status;
}

} // namespace speeduino
