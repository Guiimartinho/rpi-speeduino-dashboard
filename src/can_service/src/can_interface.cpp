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

uint32_t getMonotonicMs() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

} // anonymous namespace

CanInterface::CanInterface() = default;

CanInterface::~CanInterface() {
    close();
}

bool CanInterface::open(const std::string& interface_name) {
#ifdef __linux__
    m_interfaceName = interface_name;

    // Create socket
    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        LOG_ERROR("Failed to create CAN socket: " + std::string(strerror(errno)));
        return false;
    }

    // Get interface index
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        LOG_ERROR("Failed to get interface index for " + interface_name +
                  ": " + std::string(strerror(errno)));
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
        LOG_ERROR("Failed to bind CAN socket: " + std::string(strerror(errno)));
        ::close(m_socket);
        m_socket = -1;
        return false;
    }

    // Enable timestamps
    int timestamp_on = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_TIMESTAMP, &timestamp_on, sizeof(timestamp_on));

    LOG_INFO("CAN interface " + interface_name + " opened successfully");
    return true;
#else
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
    std::memcpy(cf.data, frame.data.data(), frame.dlc);

    ssize_t nbytes = write(m_socket, &cf, sizeof(cf));
    if (nbytes != sizeof(cf)) {
        LOG_ERROR("CAN write failed: " + std::string(strerror(errno)));
        m_errorCount++;
        return false;
    }

    m_txCount++;
    return true;
#else
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
            LOG_ERROR("CAN read error: " + std::string(strerror(errno)));
            m_errorCount++;
        }
        return std::nullopt;
    }

    CanFrame frame;
    frame.id = cf.can_id & CAN_EFF_MASK;
    frame.is_extended = (cf.can_id & CAN_EFF_FLAG) != 0;
    frame.is_rtr = (cf.can_id & CAN_RTR_FLAG) != 0;
    frame.is_error = (cf.can_id & CAN_ERR_FLAG) != 0;
    frame.dlc = cf.can_dlc;
    std::memcpy(frame.data.data(), cf.data, cf.can_dlc);
    frame.timestamp_us = getMonotonicMs() * 1000;

    m_rxCount++;
    m_lastRxTimestamp = getMonotonicMs();

    return frame;
#else
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
