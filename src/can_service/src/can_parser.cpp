#include "can_service/can_parser.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <cstring>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// Engine Safety Thresholds
// ISO 26262 ASIL-B: Named constants for safety-critical thresholds
// ═══════════════════════════════════════════════════════════════════════════════
namespace {
    /// Coolant temperature threshold for overheat warning (Celsius)
    constexpr int8_t COOLANT_OVERHEAT_THRESHOLD_C = 105;
} // anonymous namespace

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
    // FIX #8: ISO 26262 CAN DLC validation
    // CAN 2.0 max DLC is 8, CAN FD max is 64
    // Validate DLC before any array access to prevent buffer overread
    constexpr uint8_t CAN_MAX_DLC = 64;  // CAN FD max
    if (frame.dlc > CAN_MAX_DLC) {
        LOG_WARN("Invalid CAN DLC " + std::to_string(frame.dlc) +
                 " for frame 0x" + std::to_string(frame.id) + ", ignoring");
        return;
    }

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
    // FIX #8: Comprehensive bounds validation (ISO 26262)
    // Check start byte is within DLC
    if (signal.start_byte >= frame.dlc) {
        return 0;
    }

    // Validate signal length (max 64 bits for uint64_t result)
    constexpr uint8_t MAX_SIGNAL_BITS = 64;
    if (signal.length_bits == 0 || signal.length_bits > MAX_SIGNAL_BITS) {
        return 0;
    }

    // Validate start_bit is within byte bounds
    if (signal.start_bit >= 8) {
        return 0;
    }

    uint64_t result = 0;
    uint8_t bytesNeeded = (signal.length_bits + 7) / 8;

    // Prevent overflow in index calculation
    // Check that start_byte + bytesNeeded won't overflow uint8_t
    if (bytesNeeded > 8 || signal.start_byte > (255 - bytesNeeded)) {
        return 0;
    }

    if (signal.is_big_endian) {
        // Big-endian: MSB first
        for (uint8_t i = 0; i < bytesNeeded && (signal.start_byte + i) < frame.dlc; i++) {
            result = (result << 8) | frame.data[signal.start_byte + i];
        }
    } else {
        // ═══════════════════════════════════════════════════════════════════════
        // MISRA C++:2008 Rule 5-0-8: Use unsigned types consistently
        // FIX: Replaced signed int8_t loop counter with unsigned uint8_t
        // Little-endian: LSB first - process bytes from highest index to lowest
        // ═══════════════════════════════════════════════════════════════════════
        for (uint8_t j = 0; j < bytesNeeded; ++j) {
            // Calculate byte index in reverse order (bytesNeeded-1 down to 0)
            uint8_t byteIndex = static_cast<uint8_t>(bytesNeeded - 1U - j);
            uint8_t frameIndex = static_cast<uint8_t>(signal.start_byte + byteIndex);

            // Bounds check against frame DLC
            if (frameIndex < frame.dlc) {
                result = (result << 8) | frame.data[frameIndex];
            }
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

    // ═══════════════════════════════════════════════════════════════════════
    // ISO 26262 ASIL-B: Safe integer conversion with clamping
    // MISRA C++:2008 Rule 5-0-6: Narrowing conversions shall be bounds-checked
    // Prevents undefined behavior from integer overflow
    // ═══════════════════════════════════════════════════════════════════════

    // Helper lambdas for safe clamping conversions
    auto clampU16 = [](double val) -> uint16_t {
        if (val < 0.0) return 0;
        if (val > 65535.0) return 65535;
        return static_cast<uint16_t>(val);
    };

    auto clampI16 = [](double val) -> int16_t {
        if (val < -32768.0) return -32768;
        if (val > 32767.0) return 32767;
        return static_cast<int16_t>(val);
    };

    auto clampU8 = [](double val) -> uint8_t {
        if (val < 0.0) return 0;
        if (val > 255.0) return 255;
        return static_cast<uint8_t>(val);
    };

    auto clampI8 = [](double val) -> int8_t {
        if (val < -128.0) return -128;
        if (val > 127.0) return 127;
        return static_cast<int8_t>(val);
    };

    m_engineData.timestamp_ms = m_lastUpdateTimestamp;
    m_engineData.rpm = clampU16(getValue("rpm"));
    m_engineData.coolant_temp = clampI8(getValue("coolant_temp"));
    m_engineData.intake_temp = clampI8(getValue("intake_temp"));
    m_engineData.tps = clampU8(getValue("tps"));
    m_engineData.map_kpa = clampU16(getValue("map") * 10.0);
    m_engineData.lambda = clampU16(getValue("lambda1") * 1000.0);
    m_engineData.ignition_advance = clampI16(getValue("ignition_advance") * 10.0);
    m_engineData.injector_duty = clampU8(getValue("injector_duty"));
    m_engineData.gear = clampU8(getValue("gear"));
    m_engineData.vehicle_speed = clampU16(getValue("vehicle_speed") * 10.0);
    m_engineData.fuel_pressure = clampU16(getValue("fuel_pressure"));
    m_engineData.oil_pressure = clampU16(getValue("oil_pressure"));
    m_engineData.oil_temp = clampI8(getValue("oil_temp"));

    // Set flags
    m_engineData.flags = EngineData::FLAG_CAN_OK;
    if (m_engineData.rpm > 0) {
        m_engineData.flags |= EngineData::FLAG_ENGINE_RUN;
    }
    if (m_engineData.coolant_temp > COOLANT_OVERHEAT_THRESHOLD_C) {
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
