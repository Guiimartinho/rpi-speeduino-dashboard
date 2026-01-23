#pragma once

#include "can_service/can_interface.hpp"

#include <optional>
#include <string>
#include <vector>

namespace speeduino::testing {

// ═══════════════════════════════════════════════════════════════════════════════
// Mock CAN Interface for Testing
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Mock implementation of CanInterface for unit testing.
 *
 * Provides test utilities for:
 * - Capturing sent frames
 * - Queueing responses to simulate received frames
 * - Filtering frames by ID
 */
class MockCanInterface : public CanInterface {
public:
    MockCanInterface() : CanInterface() {}

    bool open(const std::string& interface_name) override {
        (void)interface_name;
        return true;
    }

    void close() override {}

    bool isConnected() const override { return true; }

    std::optional<CanFrame> receive(int timeout_ms) override {
        (void)timeout_ms;
        if (!m_responseQueue.empty()) {
            auto frame = m_responseQueue.front();
            m_responseQueue.erase(m_responseQueue.begin());
            return frame;
        }
        return std::nullopt;
    }

    bool send(const CanFrame& frame) override {
        m_sentFrames.push_back(frame);
        return true;
    }

    // ───────────────────────────────────────────────────────────────────────────
    // Test Utilities
    // ───────────────────────────────────────────────────────────────────────────

    /// Queue a frame to be returned by receive()
    void queueResponse(const CanFrame& frame) { m_responseQueue.push_back(frame); }

    /// Get all frames that were sent via send()
    const std::vector<CanFrame>& getSentFrames() const { return m_sentFrames; }

    /// Clear the sent frames buffer
    void clearSentFrames() { m_sentFrames.clear(); }

    /// Get the last frame that was sent
    const CanFrame& getLastFrame() const { return m_sentFrames.back(); }

    /// Get all sent frames with a specific CAN ID
    std::vector<CanFrame> getFramesById(uint32_t id) const {
        std::vector<CanFrame> result;
        for (const auto& frame : m_sentFrames) {
            if (frame.id == id) {
                result.push_back(frame);
            }
        }
        return result;
    }

private:
    std::vector<CanFrame> m_sentFrames;
    std::vector<CanFrame> m_responseQueue;
};

}  // namespace speeduino::testing
