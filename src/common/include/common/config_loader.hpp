#ifndef COMMON_CONFIG_LOADER_HPP
#define COMMON_CONFIG_LOADER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include <unordered_map>

namespace speeduino {

// CAN signal definition
struct CanSignalDef {
    std::string name;
    uint32_t can_id = 0;
    uint8_t start_byte = 0;
    uint8_t start_bit = 0;
    uint8_t length_bits = 0;
    bool is_big_endian = false;
    bool is_signed = false;
    double scale = 1.0;
    double offset = 0.0;
    std::string unit;
};

// Allowed CAN command definition
struct CanCommandDef {
    uint32_t can_id = 0;
    uint32_t rate_limit_hz = 10;
    std::string description;
};

// Reverse detection config
struct ReverseConfig {
    // CAN-based detection
    bool can_enabled = true;
    uint32_t can_id = 0x370;
    uint8_t byte_index = 3;
    uint8_t bit_mask = 0x80;  // Gear = reverse indicator
    uint8_t expected_value = 0x80;

    // GPIO fallback
    bool gpio_enabled = false;
    std::string gpio_chip = "gpiochip0";
    uint32_t gpio_line = 17;
    bool gpio_active_low = false;
};

// Steering wheel button config
struct SteeringButtonDef {
    uint8_t button_id = 0;
    uint32_t can_id = 0;
    uint8_t byte_index = 0;
    uint8_t bit_mask = 0;
    std::string action;  // "volume_up", "next_track", etc.
};

// System configuration
struct SystemConfig {
    // CAN interface
    std::string can_interface = "can0";
    uint32_t can_bitrate = 500000;

    // CAN protocol selection
    std::string can_protocol = "haltech";  // "haltech", "bmw", "vag"

    // ZMQ settings
    uint32_t zmq_publish_rate_hz = 50;

    // Camera settings
    std::string camera_device = "/dev/video0";
    uint32_t camera_width = 640;
    uint32_t camera_height = 480;
    uint32_t camera_fps = 30;

    // OpenAuto path
    std::string openauto_path = "/usr/local/bin/openauto";

    // Timeouts
    uint32_t can_timeout_ms = 500;
    uint32_t reverse_debounce_ms = 100;
};

// Main configuration class
class ConfigLoader {
public:
    // Load all configs from directory
    static bool loadFromDirectory(std::string_view config_dir);

    // Load specific config files
    static bool loadSystemConfig(std::string_view path);
    static bool loadSignalsConfig(std::string_view path);
    static bool loadSteeringConfig(std::string_view path);

    // Getters
    static const SystemConfig& getSystemConfig();
    static const std::vector<CanSignalDef>& getSignals();
    static const std::vector<CanCommandDef>& getAllowedCommands();
    static const std::vector<SteeringButtonDef>& getSteeringButtons();
    static const ReverseConfig& getReverseConfig();

    // Find signal by name
    static std::optional<CanSignalDef> findSignal(std::string_view name);

    // Check if CAN ID is allowed for commands
    static bool isCommandAllowed(uint32_t can_id);
    static uint32_t getCommandRateLimit(uint32_t can_id);

private:
    static SystemConfig s_systemConfig;
    static std::vector<CanSignalDef> s_signals;
    static std::vector<CanCommandDef> s_allowedCommands;
    static std::vector<SteeringButtonDef> s_steeringButtons;
    static ReverseConfig s_reverseConfig;
    static bool s_loaded;
};

} // namespace speeduino

#endif // COMMON_CONFIG_LOADER_HPP
