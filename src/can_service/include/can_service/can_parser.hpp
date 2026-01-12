#ifndef CAN_SERVICE_CAN_PARSER_HPP
#define CAN_SERVICE_CAN_PARSER_HPP

#include "can_interface.hpp"
#include "common/config_loader.hpp"
#include "common/zmq_messages.hpp"
#include <cstdint>
#include <unordered_map>
#include <mutex>

namespace speeduino {

// Parsed signal value
struct ParsedSignal {
    std::string name;
    double value = 0.0;
    uint32_t timestamp_us = 0;
    bool valid = false;
};

class CanParser {
public:
    CanParser();
    // MISRA C++:2008 Rule 15-5-1: Destructors shall not throw exceptions
    ~CanParser() noexcept = default;

    // Initialize with signal definitions
    void loadSignals(const std::vector<CanSignalDef>& signals);

    // Parse a CAN frame and update internal state
    void parseFrame(const CanFrame& frame);

    // Get current engine data (thread-safe)
    EngineData getEngineData() const;

    // Get specific signal value
    std::optional<double> getSignalValue(const std::string& name) const;

    // Check if CAN data is fresh (no timeout)
    bool isDataFresh(uint32_t timeout_ms = 500) const;

    // Reset all values
    void reset();

private:
    // Extract raw value from frame bytes
    uint64_t extractRawValue(const CanFrame& frame, const CanSignalDef& signal) const;

    // Apply scale and offset
    double applyScaling(uint64_t raw, const CanSignalDef& signal) const;

    // Update aggregated engine data from parsed signals
    void updateEngineData();

    // Signal definitions indexed by CAN ID
    std::unordered_multimap<uint32_t, CanSignalDef> m_signalsByCanId;

    // Current parsed values
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ParsedSignal> m_values;

    // Aggregated engine data
    EngineData m_engineData;
    uint32_t m_lastUpdateTimestamp = 0;
};

} // namespace speeduino

#endif // CAN_SERVICE_CAN_PARSER_HPP
