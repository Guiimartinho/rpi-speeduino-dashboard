#ifndef CAN_SERVICE_CAN_WRITER_HPP
#define CAN_SERVICE_CAN_WRITER_HPP

#include "can_interface.hpp"

#include "common/config_loader.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace speeduino {

// Token bucket for rate limiting
class TokenBucket {
public:
    TokenBucket(uint32_t rate_hz = 10);

    // Try to consume a token, returns true if allowed
    bool tryConsume();

    // Reset the bucket
    void reset();

private:
    uint32_t m_rateHz;
    double m_tokens;
    double m_maxTokens;
    std::chrono::steady_clock::time_point m_lastRefill;
    std::mutex m_mutex;
};

// CAN command writer with whitelist and rate limiting
class CanWriter {
public:
    CanWriter(CanInterface& interface);
    // MISRA C++:2008 Rule 15-5-1: Destructors shall not throw exceptions
    ~CanWriter() noexcept = default;

    // Load allowed commands from config
    void loadAllowedCommands(const std::vector<CanCommandDef>& commands);

    // Send a command (returns error code)
    enum class SendResult {
        OK           = 0,
        BLOCKED      = 1,  // Not in whitelist
        RATE_LIMITED = 2,  // Rate limit exceeded
        CAN_ERROR    = 3,  // CAN write failed
        INVALID      = 4   // Invalid parameters
    };

    SendResult send(uint32_t can_id, const uint8_t* data, uint8_t dlc);

    // Check if a CAN ID is allowed
    bool isAllowed(uint32_t can_id) const;

    // Get rate limit for a CAN ID
    uint32_t getRateLimit(uint32_t can_id) const;

    // Statistics
    uint32_t getSentCount() const { return m_sentCount; }
    uint32_t getBlockedCount() const { return m_blockedCount; }
    uint32_t getRateLimitedCount() const { return m_rateLimitedCount; }

private:
    CanInterface& m_interface;

    // Whitelist: CAN ID -> rate limit (Hz)
    std::unordered_map<uint32_t, uint32_t> m_whitelist;

    // Rate limiters per CAN ID
    std::unordered_map<uint32_t, TokenBucket> m_rateLimiters;

    // Statistics
    std::atomic<uint32_t> m_sentCount{0};
    std::atomic<uint32_t> m_blockedCount{0};
    std::atomic<uint32_t> m_rateLimitedCount{0};
};

}  // namespace speeduino

#endif  // CAN_SERVICE_CAN_WRITER_HPP
