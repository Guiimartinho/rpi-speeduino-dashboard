#include "can_service/can_parser.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <cstring>

namespace speeduino {

namespace {

uint32_t getMonotonicMs() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

// Swap bytes for big-endian values
uint16_t swapBytes16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

uint32_t swapBytes32(uint32_t val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

} // anonymous namespace

CanParser::CanParser() = default;

void CanParser::loadSignals(const std::vector<CanSignalDef>& signals) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_signalsByCanId.clear();
    m_values.clear();

    for (const auto& signal : signals) {
        m_signalsByCanId.emplace(signal.can_id, signal);
        m_values[signal.name] = ParsedSignal{signal.name, 0.0, 0, false};
    }

    LOG_INFO("Loaded " + std::to_string(signals.size()) + " CAN signals");
}

void CanParser::parseFrame(const CanFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find all signals for this CAN ID
    auto range = m_signalsByCanId.equal_range(frame.id);

    for (auto it = range.first; it != range.second; ++it) {
        const CanSignalDef& signal = it->second;

        // Extract raw value
        uint64_t rawValue = extractRawValue(frame, signal);

        // Apply scaling
        double scaledValue = applyScaling(rawValue, signal);

        // Store parsed value
        m_values[signal.name] = ParsedSignal{
            signal.name,
            scaledValue,
            frame.timestamp_us,
            true
        };
    }

    // Update timestamp
    m_lastUpdateTimestamp = getMonotonicMs();

    // Update aggregated engine data
    updateEngineData();
}

uint64_t CanParser::extractRawValue(const CanFrame& frame, const CanSignalDef& signal) const {
    if (signal.start_byte >= frame.dlc) {
        return 0;
    }

    uint64_t result = 0;
    uint8_t bytesNeeded = (signal.length_bits + 7) / 8;

    if (signal.is_big_endian) {
        // Big-endian: MSB first
        for (uint8_t i = 0; i < bytesNeeded && (signal.start_byte + i) < frame.dlc; i++) {
            result = (result << 8) | frame.data[signal.start_byte + i];
        }
    } else {
        // Little-endian: LSB first
        for (int8_t i = bytesNeeded - 1; i >= 0 && (signal.start_byte + i) < frame.dlc; i--) {
            result = (result << 8) | frame.data[signal.start_byte + i];
        }
    }

    // Apply bit offset within byte if specified
    if (signal.start_bit > 0) {
        result >>= signal.start_bit;
    }

    // Mask to signal length
    uint64_t mask = (1ULL << signal.length_bits) - 1;
    result &= mask;

    return result;
}

double CanParser::applyScaling(uint64_t raw, const CanSignalDef& signal) const {
    double value;

    if (signal.is_signed) {
        // Sign extend
        uint64_t signBit = 1ULL << (signal.length_bits - 1);
        if (raw & signBit) {
            // Negative value - extend sign
            raw |= ~((1ULL << signal.length_bits) - 1);
        }
        value = static_cast<double>(static_cast<int64_t>(raw));
    } else {
        value = static_cast<double>(raw);
    }

    return (value * signal.scale) + signal.offset;
}

void CanParser::updateEngineData() {
    // Map parsed signal names to EngineData fields
    auto getValue = [this](const std::string& name) -> double {
        auto it = m_values.find(name);
        if (it != m_values.end() && it->second.valid) {
            return it->second.value;
        }
        return 0.0;
    };

    m_engineData.timestamp_ms = m_lastUpdateTimestamp;
    m_engineData.rpm = static_cast<uint16_t>(getValue("rpm"));
    m_engineData.coolant_temp = static_cast<int8_t>(getValue("coolant_temp"));
    m_engineData.intake_temp = static_cast<int8_t>(getValue("intake_temp"));
    m_engineData.tps = static_cast<uint8_t>(getValue("tps"));
    m_engineData.map_kpa = static_cast<uint16_t>(getValue("map") * 10);
    m_engineData.lambda = static_cast<uint16_t>(getValue("lambda1") * 1000);
    m_engineData.ignition_advance = static_cast<int16_t>(getValue("ignition_advance") * 10);
    m_engineData.injector_duty = static_cast<uint8_t>(getValue("injector_duty"));
    m_engineData.gear = static_cast<uint8_t>(getValue("gear"));
    m_engineData.vehicle_speed = static_cast<uint16_t>(getValue("vehicle_speed") * 10);
    m_engineData.fuel_pressure = static_cast<uint16_t>(getValue("fuel_pressure"));
    m_engineData.oil_pressure = static_cast<uint16_t>(getValue("oil_pressure"));
    m_engineData.oil_temp = static_cast<int8_t>(getValue("oil_temp"));

    // Set flags
    m_engineData.flags = EngineData::FLAG_CAN_OK;
    if (m_engineData.rpm > 0) {
        m_engineData.flags |= EngineData::FLAG_ENGINE_RUN;
    }
    if (m_engineData.coolant_temp > 105) {
        m_engineData.flags |= EngineData::FLAG_OVERHEAT;
    }
}

EngineData CanParser::getEngineData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_engineData;
}

std::optional<double> CanParser::getSignalValue(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_values.find(name);
    if (it != m_values.end() && it->second.valid) {
        return it->second.value;
    }
    return std::nullopt;
}

bool CanParser::isDataFresh(uint32_t timeout_ms) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t now = getMonotonicMs();
    return (now - m_lastUpdateTimestamp) < timeout_ms;
}

void CanParser::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [name, value] : m_values) {
        value.value = 0.0;
        value.valid = false;
    }

    m_engineData = EngineData{};
    m_lastUpdateTimestamp = 0;
}

} // namespace speeduino
