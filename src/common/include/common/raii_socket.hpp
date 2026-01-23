/**
 * @file raii_socket.hpp
 * @brief RAII wrapper for file descriptors (sockets, files, etc.)
 *
 * Provides automatic resource management for file descriptors following
 * the RAII pattern. Ensures proper cleanup even in case of exceptions
 * or early returns.
 */

#ifndef COMMON_RAII_SOCKET_HPP
#define COMMON_RAII_SOCKET_HPP

#include <unistd.h>
#include <utility>

namespace speeduino {

/**
 * @class RaiiSocket
 * @brief RAII wrapper for file descriptors
 *
 * This class manages the lifetime of a file descriptor. When the object
 * goes out of scope, the file descriptor is automatically closed.
 * The class is move-only to prevent double-close issues.
 */
class RaiiSocket {
public:
    /**
     * @brief Default constructor - creates an invalid socket
     */
    RaiiSocket() noexcept = default;

    /**
     * @brief Construct from raw file descriptor
     * @param fd File descriptor to take ownership of
     */
    explicit RaiiSocket(int fd) noexcept : fd_(fd) {}

    /**
     * @brief Destructor - closes the file descriptor if valid
     */
    ~RaiiSocket() noexcept { close(); }

    // Move constructor
    RaiiSocket(RaiiSocket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    // Move assignment
    RaiiSocket& operator=(RaiiSocket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    // Non-copyable
    RaiiSocket(const RaiiSocket&)            = delete;
    RaiiSocket& operator=(const RaiiSocket&) = delete;

    /**
     * @brief Get the raw file descriptor
     * @return The file descriptor, or -1 if invalid
     */
    [[nodiscard]] int get() const noexcept { return fd_; }

    /**
     * @brief Check if the file descriptor is valid
     * @return true if fd >= 0
     */
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    /**
     * @brief Boolean conversion operator
     * @return true if valid
     */
    explicit operator bool() const noexcept { return valid(); }

    /**
     * @brief Release ownership of the file descriptor
     * @return The file descriptor (caller takes ownership)
     *
     * After calling release(), this object no longer owns the fd
     * and will not close it on destruction.
     */
    [[nodiscard]] int release() noexcept { return std::exchange(fd_, -1); }

    /**
     * @brief Close the file descriptor immediately
     *
     * Safe to call multiple times - will only close once.
     */
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    /**
     * @brief Reset with a new file descriptor
     * @param fd New file descriptor to manage (default -1)
     *
     * Closes the current fd (if any) before taking ownership of the new one.
     */
    void reset(int fd = -1) noexcept {
        if (fd_ != fd) {
            close();
            fd_ = fd;
        }
    }

    /**
     * @brief Swap with another RaiiSocket
     * @param other The other RaiiSocket to swap with
     */
    void swap(RaiiSocket& other) noexcept { std::swap(fd_, other.fd_); }

private:
    int fd_ = -1;
};

/**
 * @brief Swap function for RaiiSocket (ADL-enabled)
 */
inline void swap(RaiiSocket& a, RaiiSocket& b) noexcept {
    a.swap(b);
}

}  // namespace speeduino

#endif  // COMMON_RAII_SOCKET_HPP
