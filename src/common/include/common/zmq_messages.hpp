#ifndef COMMON_ZMQ_MESSAGES_HPP
#define COMMON_ZMQ_MESSAGES_HPP

#include <msgpack.hpp>

#include <array>
#include <cstdint>
#include <string>

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
    std::string engine_data     = "ipc:///tmp/speeduino_data.ipc";
    std::string can_command     = "ipc:///tmp/speeduino_cmd.ipc";
    std::string reverse_trigger = "ipc:///tmp/reverse_trigger.ipc";
    std::string steering_events = "ipc:///tmp/steering_events.ipc";

    // Validation helper
    bool isValid() const {
        return !engine_data.empty() && !can_command.empty() && !reverse_trigger.empty() &&
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
    static void configure(const ZmqEndpointsConfig& config) { instance().config_ = config; }

    // Reset to default configuration
    static void reset() { instance().config_ = ZmqEndpointsConfig{}; }

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
constexpr const char* ENGINE_DATA     = "ipc:///tmp/speeduino_data.ipc";
constexpr const char* CAN_COMMAND     = "ipc:///tmp/speeduino_cmd.ipc";
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
}  // namespace endpoints

// Topic prefixes for PUB/SUB
namespace topics {
constexpr const char* ENGINE   = "ENGINE";
constexpr const char* REVERSE  = "REVERSE";
constexpr const char* STEERING = "STEERING";
constexpr const char* STATUS   = "STATUS";
}  // namespace topics

// ═══════════════════════════════════════════════════════════════════════════════
// Engine data structure - published at 50Hz
// ISO 26262 ASIL-B: Complete engine telemetry for dashboard display
//
// Supports data from:
//   - Haltech (IC-7/IC-10) - Most complete
//   - BMW E46 PT-CAN
//   - VAG (VW/Audi/Gol)
//   - OBD-II fallback
// ═══════════════════════════════════════════════════════════════════════════════
struct EngineData {
    // ═══════════════════════════════════════════════════════════════════════
    // CORE DATA (all protocols)
    // ═══════════════════════════════════════════════════════════════════════
    uint32_t timestamp_ms    = 0;  // Monotonic timestamp
    uint16_t rpm             = 0;  // Engine RPM
    int8_t coolant_temp      = 0;  // Coolant temperature (Celsius)
    int8_t intake_temp       = 0;  // Intake air temperature (Celsius)
    uint8_t tps              = 0;  // Throttle position (0-100%)
    uint16_t map_kpa         = 0;  // Manifold pressure (kPa x 10)
    uint16_t lambda          = 0;  // Lambda (x 1000, 1000 = stoich)
    int16_t ignition_advance = 0;  // Ignition advance (degrees x 10)
    uint8_t injector_duty    = 0;  // Injector duty cycle (0-100%)
    uint8_t gear             = 0;  // Current gear (0=N, 1-6, 7=R)
    uint16_t vehicle_speed   = 0;  // Speed (km/h x 10)

    // ═══════════════════════════════════════════════════════════════════════
    // PRESSURES (Haltech, some BMW)
    // ═══════════════════════════════════════════════════════════════════════
    uint16_t fuel_pressure = 0;  // Fuel pressure (kPa)
    uint16_t oil_pressure  = 0;  // Oil pressure (kPa)
    uint16_t boost_target  = 0;  // Boost target (kPa x 10) - Haltech
    uint16_t baro          = 0;  // Barometric pressure (kPa x 10)

    // ═══════════════════════════════════════════════════════════════════════
    // TEMPERATURES
    // ═══════════════════════════════════════════════════════════════════════
    int8_t oil_temp  = 0;  // Oil temperature (Celsius)
    int8_t fuel_temp = 0;  // Fuel temperature (Celsius) - Haltech

    // ═══════════════════════════════════════════════════════════════════════
    // ELECTRICAL
    // ═══════════════════════════════════════════════════════════════════════
    uint16_t battery_voltage = 0;  // Battery (mV)

    // ═══════════════════════════════════════════════════════════════════════
    // FUEL SYSTEM (Haltech, BMW)
    // ═══════════════════════════════════════════════════════════════════════
    uint16_t fuel_consumption = 0;  // Fuel consumption (L/h x 100) - BMW
    uint8_t fuel_load         = 0;  // Fuel load percentage - Haltech

    // ═══════════════════════════════════════════════════════════════════════
    // INJECTION (Haltech - per cylinder data)
    // ═══════════════════════════════════════════════════════════════════════
    uint16_t pw1 = 0;  // Pulse width cyl 1 (microseconds)
    uint16_t pw2 = 0;  // Pulse width cyl 2 (microseconds)
    uint16_t pw3 = 0;  // Pulse width cyl 3 (microseconds)
    uint16_t pw4 = 0;  // Pulse width cyl 4 (microseconds)

    // ═══════════════════════════════════════════════════════════════════════
    // VVT (Variable Valve Timing - Haltech)
    // ═══════════════════════════════════════════════════════════════════════
    int16_t vvt_intake  = 0;  // VVT intake angle (degrees x 10)
    int16_t vvt_exhaust = 0;  // VVT exhaust angle (degrees x 10)

    // ═══════════════════════════════════════════════════════════════════════
    // TORQUE (BMW only)
    // ═══════════════════════════════════════════════════════════════════════
    uint8_t torque_indexed   = 0;  // Indexed torque (%) - BMW
    uint8_t torque_indicated = 0;  // Indicated torque (%) - BMW

    // ═══════════════════════════════════════════════════════════════════════
    // PER-CYLINDER TRIMS (Speeduino 0x3E3)
    // ═══════════════════════════════════════════════════════════════════════
    int8_t fuel_trim_cyl1 = 0;  // Fuel trim cylinder 1 (% correction, -100 to +127)
    int8_t fuel_trim_cyl2 = 0;  // Fuel trim cylinder 2
    int8_t fuel_trim_cyl3 = 0;  // Fuel trim cylinder 3
    int8_t fuel_trim_cyl4 = 0;  // Fuel trim cylinder 4
    int8_t ign_trim_cyl1  = 0;  // Ignition trim cylinder 1 (degrees × 2)
    int8_t ign_trim_cyl2  = 0;  // Ignition trim cylinder 2
    int8_t ign_trim_cyl3  = 0;  // Ignition trim cylinder 3
    int8_t ign_trim_cyl4  = 0;  // Ignition trim cylinder 4

    // ═══════════════════════════════════════════════════════════════════════
    // ADDITIONAL ENGINE DATA (Speeduino 0x3E1, 0x3E2)
    // ═══════════════════════════════════════════════════════════════════════
    uint8_t ve              = 0;    // Volumetric efficiency (%)
    uint8_t afr_target      = 0;    // Target AFR × 10
    uint8_t idle_target_rpm = 0;    // Idle target RPM / 10
    uint8_t idle_valve_duty = 0;    // Idle valve duty cycle (%)
    uint8_t error_count     = 0;    // Number of active DTCs
    uint8_t sync_status     = 0;    // 0=lost, 1=synced

    // ═══════════════════════════════════════════════════════════════════════
    // STATUS FLAGS
    // ═══════════════════════════════════════════════════════════════════════
    uint16_t flags = 0;  // Status flags (expanded bitfield)

    // Flag bits (lower byte - common flags)
    static constexpr uint16_t FLAG_CEL_ON     = 0x0001;  // Check engine light
    static constexpr uint16_t FLAG_OVERHEAT   = 0x0002;  // Overheat warning
    static constexpr uint16_t FLAG_CAN_OK     = 0x0004;  // CAN bus healthy
    static constexpr uint16_t FLAG_ENGINE_RUN = 0x0008;  // Engine running
    static constexpr uint16_t FLAG_CLUTCH_IN  = 0x0010;  // Clutch pedal pressed
    static constexpr uint16_t FLAG_BRAKE_ON   = 0x0020;  // Brake pedal pressed
    static constexpr uint16_t FLAG_CRUISE_ON  = 0x0040;  // Cruise control active

    // Flag bits (upper byte - extended flags)
    static constexpr uint16_t FLAG_LAUNCH_SOFT = 0x0100;  // Soft launch active
    static constexpr uint16_t FLAG_LAUNCH_HARD = 0x0200;  // Hard launch active
    static constexpr uint16_t FLAG_FLAT_SHIFT  = 0x0400;  // Flat shift active
    static constexpr uint16_t FLAG_REV_LIMIT   = 0x0800;  // Rev limiter active
    static constexpr uint16_t FLAG_LOW_OIL_P   = 0x1000;  // Low oil pressure
    static constexpr uint16_t FLAG_LOW_FUEL_P  = 0x2000;  // Low fuel pressure
    static constexpr uint16_t FLAG_DFCO        = 0x4000;  // Decel Fuel Cut Off active
    static constexpr uint16_t FLAG_FAN_ON      = 0x8000;  // Cooling fan active

    // ═══════════════════════════════════════════════════════════════════════
    // HELPER METHODS
    // ═══════════════════════════════════════════════════════════════════════
    bool isCelOn() const { return flags & FLAG_CEL_ON; }
    bool isOverheat() const { return flags & FLAG_OVERHEAT; }
    bool isCanOk() const { return flags & FLAG_CAN_OK; }
    bool isEngineRunning() const { return flags & FLAG_ENGINE_RUN; }
    bool isClutchIn() const { return flags & FLAG_CLUTCH_IN; }
    bool isBrakeOn() const { return flags & FLAG_BRAKE_ON; }
    bool isCruiseOn() const { return flags & FLAG_CRUISE_ON; }
    bool isLaunching() const { return flags & (FLAG_LAUNCH_SOFT | FLAG_LAUNCH_HARD); }
    bool isFlatShifting() const { return flags & FLAG_FLAT_SHIFT; }
    bool isRevLimiterActive() const { return flags & FLAG_REV_LIMIT; }
    bool isLowOilPressure() const { return flags & FLAG_LOW_OIL_P; }
    bool isLowFuelPressure() const { return flags & FLAG_LOW_FUEL_P; }
    bool isDfcoActive() const { return flags & FLAG_DFCO; }
    bool isFanOn() const { return flags & FLAG_FAN_ON; }

    // Get AFR from lambda (for display)
    float getAFR() const { return lambda * 0.0147f; }  // lambda × 14.7

    // Get boost in PSI (for display)
    float getBoostPSI() const {
        int16_t boost_kpa = static_cast<int16_t>(map_kpa / 10) - 101;
        return boost_kpa * 0.145038f;
    }

    MSGPACK_DEFINE(timestamp_ms, rpm, coolant_temp, intake_temp, tps, map_kpa, lambda,
                   ignition_advance, injector_duty, gear, vehicle_speed, fuel_pressure,
                   oil_pressure, boost_target, baro, oil_temp, fuel_temp, battery_voltage,
                   fuel_consumption, fuel_load, pw1, pw2, pw3, pw4, vvt_intake, vvt_exhaust,
                   torque_indexed, torque_indicated, fuel_trim_cyl1, fuel_trim_cyl2,
                   fuel_trim_cyl3, fuel_trim_cyl4, ign_trim_cyl1, ign_trim_cyl2, ign_trim_cyl3,
                   ign_trim_cyl4, ve, afr_target, idle_target_rpm, idle_valve_duty, error_count,
                   sync_status, flags)
};

// Reverse gear event
struct ReverseEvent {
    uint32_t timestamp_ms = 0;
    bool engaged          = false;
    uint8_t source        = 0;  // 0 = CAN, 1 = GPIO

    MSGPACK_DEFINE(timestamp_ms, engaged, source)
};

// Steering wheel button event
struct SteeringEvent {
    uint32_t timestamp_ms = 0;
    uint8_t button_id     = 0;
    bool pressed          = false;

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
    uint32_t can_id             = 0;
    std::array<uint8_t, 8> data = {0};
    uint8_t dlc                 = 0;

    MSGPACK_DEFINE(can_id, data, dlc)
};

// CAN command response (REP)
struct CanCommandResponse {
    bool success       = false;
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
    uint32_t timestamp_ms   = 0;
    bool can_connected      = false;
    bool camera_ready       = false;
    bool openauto_running   = false;
    uint32_t uptime_seconds = 0;
    float cpu_temp          = 0.0f;

    MSGPACK_DEFINE(timestamp_ms, can_connected, camera_ready, openauto_running, uptime_seconds,
                   cpu_temp)
};

}  // namespace speeduino

#endif  // COMMON_ZMQ_MESSAGES_HPP
