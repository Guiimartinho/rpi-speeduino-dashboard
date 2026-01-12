#ifndef COMMON_ZMQ_MESSAGES_HPP
#define COMMON_ZMQ_MESSAGES_HPP

#include <cstdint>
#include <array>
#include <string>
#include <msgpack.hpp>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// ZMQ IPC Endpoints Configuration
// ISO 26262: Configurable endpoints for testing and deployment flexibility
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @struct ZmqEndpointsConfig
 * @brief Configurable ZMQ IPC endpoints
 *
 * Default values are for production use.
 * Can be overridden via configuration file or for testing.
 */
struct ZmqEndpointsConfig {
    std::string engine_data = "ipc:///tmp/speeduino_data.ipc";
    std::string can_command = "ipc:///tmp/speeduino_cmd.ipc";
    std::string reverse_trigger = "ipc:///tmp/reverse_trigger.ipc";
    std::string steering_events = "ipc:///tmp/steering_events.ipc";

    // Validation helper
    bool isValid() const {
        return !engine_data.empty() &&
               !can_command.empty() &&
               !reverse_trigger.empty() &&
               !steering_events.empty();
    }
};

/**
 * @class ZmqEndpoints
 * @brief Singleton for managing ZMQ endpoint configuration
 *
 * Usage:
 *   // Get default endpoints
 *   auto& ep = ZmqEndpoints::instance();
 *   publisher.bind(ep.engineData());
 *
 *   // Override for testing
 *   ZmqEndpointsConfig testConfig;
 *   testConfig.engine_data = "ipc:///tmp/test_engine.ipc";
 *   ZmqEndpoints::configure(testConfig);
 */
class ZmqEndpoints {
public:
    static ZmqEndpoints& instance() {
        static ZmqEndpoints instance;
        return instance;
    }

    // Configure endpoints (call before using any endpoint)
    static void configure(const ZmqEndpointsConfig& config) {
        instance().config_ = config;
    }

    // Reset to default configuration
    static void reset() {
        instance().config_ = ZmqEndpointsConfig{};
    }

    // Endpoint getters
    const std::string& engineData() const { return config_.engine_data; }
    const std::string& canCommand() const { return config_.can_command; }
    const std::string& reverseTrigger() const { return config_.reverse_trigger; }
    const std::string& steeringEvents() const { return config_.steering_events; }

    // Get full configuration
    const ZmqEndpointsConfig& config() const { return config_; }

private:
    ZmqEndpoints() = default;
    ZmqEndpointsConfig config_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Legacy namespace for backward compatibility
// DEPRECATED: Use ZmqEndpoints::instance() instead
// ═══════════════════════════════════════════════════════════════════════════════
namespace endpoints {
    // Default endpoints (for backward compatibility)
    constexpr const char* ENGINE_DATA = "ipc:///tmp/speeduino_data.ipc";
    constexpr const char* CAN_COMMAND = "ipc:///tmp/speeduino_cmd.ipc";
    constexpr const char* REVERSE_TRIGGER = "ipc:///tmp/reverse_trigger.ipc";
    constexpr const char* STEERING_EVENTS = "ipc:///tmp/steering_events.ipc";

    // Helper to get configured endpoint (preferred)
    inline const std::string& getEngineData() {
        return ZmqEndpoints::instance().engineData();
    }
    inline const std::string& getCanCommand() {
        return ZmqEndpoints::instance().canCommand();
    }
    inline const std::string& getReverseTrigger() {
        return ZmqEndpoints::instance().reverseTrigger();
    }
    inline const std::string& getSteeringEvents() {
        return ZmqEndpoints::instance().steeringEvents();
    }
}

// Topic prefixes for PUB/SUB
namespace topics {
    constexpr const char* ENGINE = "ENGINE";
    constexpr const char* REVERSE = "REVERSE";
    constexpr const char* STEERING = "STEERING";
    constexpr const char* STATUS = "STATUS";
}

// Engine data structure - published at 50Hz
struct EngineData {
    uint32_t timestamp_ms = 0;      // Monotonic timestamp
    uint16_t rpm = 0;               // Engine RPM
    int8_t   coolant_temp = 0;      // Coolant temperature (Celsius)
    int8_t   intake_temp = 0;       // Intake air temperature (Celsius)
    uint8_t  tps = 0;               // Throttle position (0-100%)
    uint16_t map_kpa = 0;           // Manifold pressure (kPa x 10)
    uint16_t lambda = 0;            // Lambda (x 1000, 1000 = stoich)
    int16_t  ignition_advance = 0;  // Ignition advance (degrees x 10)
    uint8_t  injector_duty = 0;     // Injector duty cycle (0-100%)
    uint8_t  gear = 0;              // Current gear (0=N, 1-6)
    uint16_t vehicle_speed = 0;     // Speed (km/h x 10)
    uint16_t fuel_pressure = 0;     // Fuel pressure (kPa)
    uint16_t oil_pressure = 0;      // Oil pressure (kPa)
    int8_t   oil_temp = 0;          // Oil temperature (Celsius)
    uint16_t battery_voltage = 0;   // Battery (mV)
    uint8_t  flags = 0;             // Status flags (bitfield)

    // Flag bits
    static constexpr uint8_t FLAG_CEL_ON     = 0x01;
    static constexpr uint8_t FLAG_OVERHEAT   = 0x02;
    static constexpr uint8_t FLAG_CAN_OK     = 0x04;
    static constexpr uint8_t FLAG_ENGINE_RUN = 0x08;
    static constexpr uint8_t FLAG_CLUTCH_IN  = 0x10;
    static constexpr uint8_t FLAG_BRAKE_ON   = 0x20;

    // Helper methods
    bool isCelOn() const { return flags & FLAG_CEL_ON; }
    bool isOverheat() const { return flags & FLAG_OVERHEAT; }
    bool isCanOk() const { return flags & FLAG_CAN_OK; }
    bool isEngineRunning() const { return flags & FLAG_ENGINE_RUN; }

    MSGPACK_DEFINE(timestamp_ms, rpm, coolant_temp, intake_temp, tps,
                   map_kpa, lambda, ignition_advance, injector_duty, gear,
                   vehicle_speed, fuel_pressure, oil_pressure, oil_temp,
                   battery_voltage, flags)
};

// Reverse gear event
struct ReverseEvent {
    uint32_t timestamp_ms = 0;
    bool     engaged = false;
    uint8_t  source = 0;  // 0 = CAN, 1 = GPIO

    MSGPACK_DEFINE(timestamp_ms, engaged, source)
};

// Steering wheel button event
struct SteeringEvent {
    uint32_t timestamp_ms = 0;
    uint8_t  button_id = 0;
    bool     pressed = false;

    // Button IDs
    static constexpr uint8_t BTN_VOLUME_UP   = 0x01;
    static constexpr uint8_t BTN_VOLUME_DOWN = 0x02;
    static constexpr uint8_t BTN_NEXT_TRACK  = 0x03;
    static constexpr uint8_t BTN_PREV_TRACK  = 0x04;
    static constexpr uint8_t BTN_MUTE        = 0x05;
    static constexpr uint8_t BTN_MODE        = 0x06;
    static constexpr uint8_t BTN_PHONE       = 0x07;
    static constexpr uint8_t BTN_VOICE       = 0x08;

    MSGPACK_DEFINE(timestamp_ms, button_id, pressed)
};

// CAN command request (REQ)
struct CanCommand {
    uint32_t can_id = 0;
    std::array<uint8_t, 8> data = {0};
    uint8_t  dlc = 0;

    MSGPACK_DEFINE(can_id, data, dlc)
};

// CAN command response (REP)
struct CanCommandResponse {
    bool    success = false;
    uint8_t error_code = 0;
    std::string error_message;

    // Error codes
    static constexpr uint8_t ERR_OK         = 0x00;
    static constexpr uint8_t ERR_BLOCKED    = 0x01;  // Not in whitelist
    static constexpr uint8_t ERR_RATE_LIMIT = 0x02;  // Rate limit exceeded
    static constexpr uint8_t ERR_CAN_WRITE  = 0x03;  // CAN write failed
    static constexpr uint8_t ERR_INVALID    = 0x04;  // Invalid command

    MSGPACK_DEFINE(success, error_code, error_message)
};

// System status message
struct SystemStatus {
    uint32_t timestamp_ms = 0;
    bool     can_connected = false;
    bool     camera_ready = false;
    bool     openauto_running = false;
    uint32_t uptime_seconds = 0;
    float    cpu_temp = 0.0f;

    MSGPACK_DEFINE(timestamp_ms, can_connected, camera_ready,
                   openauto_running, uptime_seconds, cpu_temp)
};

} // namespace speeduino

#endif // COMMON_ZMQ_MESSAGES_HPP
