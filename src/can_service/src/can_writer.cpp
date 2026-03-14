#include "can_service/can_writer.hpp"

#include "common/logger.hpp"

#include <cstring>

namespace speeduino {

// TokenBucket implementation
TokenBucket::TokenBucket(uint32_t rate_hz)
    : m_rateHz(rate_hz), m_tokens(static_cast<double>(rate_hz)),
      m_maxTokens(static_cast<double>(rate_hz)), m_lastRefill(std::chrono::steady_clock::now()) {}

bool TokenBucket::tryConsume() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_lastRefill).count();

    // Refill tokens based on elapsed time
    m_tokens     = std::min(m_maxTokens, m_tokens + elapsed * m_rateHz);
    m_lastRefill = now;

    // Try to consume one token
    if (m_tokens >= 1.0) {
        m_tokens -= 1.0;
        return true;
    }

    return false;
}

void TokenBucket::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tokens     = m_maxTokens;
    m_lastRefill = std::chrono::steady_clock::now();
}

// CanWriter implementation
CanWriter::CanWriter(CanInterface& interface) : m_interface(interface) {}

void CanWriter::loadAllowedCommands(const std::vector<CanCommandDef>& commands) {
    m_whitelist.clear();
    m_rateLimiters.clear();

    for (const auto& cmd : commands) {
        m_whitelist[cmd.can_id] = cmd.rate_limit_hz;
        m_rateLimiters.emplace(std::piecewise_construct, std::forward_as_tuple(cmd.can_id),
                               std::forward_as_tuple(cmd.rate_limit_hz));
        LOG_DEBUG("Whitelisted CAN ID 0x" + std::to_string(cmd.can_id) + " with rate limit " +
                  std::to_string(cmd.rate_limit_hz) + " Hz");
    }

    LOG_INFO("Loaded " + std::to_string(commands.size()) + " allowed CAN commands");
}

bool CanWriter::isAllowed(uint32_t can_id) const {
    return m_whitelist.find(can_id) != m_whitelist.end();
}

uint32_t CanWriter::getRateLimit(uint32_t can_id) const {
    auto it = m_whitelist.find(can_id);
    if (it != m_whitelist.end()) {
        return it->second;
    }
    return 0;
}

CanWriter::SendResult CanWriter::send(uint32_t can_id, const uint8_t* data, uint8_t dlc) {
    // Validate parameters
    if (dlc > 8 || data == nullptr) {
        LOG_WARN("Invalid CAN command parameters");
        return SendResult::INVALID;
    }

    // Check whitelist
    if (!isAllowed(can_id)) {
        LOG_WARN("CAN ID 0x" + std::to_string(can_id) + " blocked (not in whitelist)");
        m_blockedCount++;
        return SendResult::BLOCKED;
    }

    // Check rate limit
    auto limiter_it = m_rateLimiters.find(can_id);
    if (limiter_it != m_rateLimiters.end()) {
        if (!limiter_it->second.tryConsume()) {
            LOG_WARN("CAN ID 0x" + std::to_string(can_id) + " rate limited");
            m_rateLimitedCount++;
            return SendResult::RATE_LIMITED;
        }
    }

    // Build and send frame
    CanFrame frame;
    frame.id  = can_id;
    frame.dlc = dlc;
    std::memcpy(frame.data.data(), data, dlc);

    if (!m_interface.send(frame)) {
        LOG_ERROR("Failed to send CAN frame");
        return SendResult::CAN_ERROR;
    }

    m_sentCount++;
    LOG_DEBUG("Sent CAN frame: ID=0x" + std::to_string(can_id) + ", DLC=" + std::to_string(dlc));

    return SendResult::OK;
}

}  // namespace speeduino
