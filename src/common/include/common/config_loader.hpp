#ifndef COMMON_CONFIG_LOADER_HPP
#define COMMON_CONFIG_LOADER_HPP

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace speeduino {

// CAN signal definition
struct CanSignalDef {
    std::string name;
    uint32_t can_id     = 0;
    uint8_t start_byte  = 0;
    uint8_t start_bit   = 0;
    uint8_t length_bits = 0;
    bool is_big_endian  = false;
    bool is_signed      = false;
    double scale        = 1.0;
    double offset       = 0.0;
    std::string unit;
};

// Allowed CAN command definition
struct CanCommandDef {
    uint32_t can_id        = 0;
    uint32_t rate_limit_hz = 10;
    std::string description;
};

// Reverse detection config
struct ReverseConfig {
    // ═══════════════════════════════════════════════════════════════
    // DETECTION MODE
    // ═══════════════════════════════════════════════════════════════
    // "can"     - Modern cars with CAN bus (Haltech, BMW, VAG, etc.)
    // "gpio"    - Classic cars with 12V reverse light signal
    //             (VW Gol, Fusca, Chevette, Opala, etc.)
    // "both"    - Try CAN first, GPIO as fallback
    // "speeduino_can" - Speeduino ECU broadcasting gear via CAN
    std::string detection_mode = "both";

    // ═══════════════════════════════════════════════════════════════
    // CAN-BASED DETECTION (Modern ECUs / Speeduino with CAN)
    // ═══════════════════════════════════════════════════════════════
    bool can_enabled       = true;
    uint32_t can_id        = 0x370;  // CAN frame ID containing gear info
    uint8_t byte_index     = 3;      // Byte position in payload
    uint8_t bit_mask       = 0x80;   // Bit mask for reverse indicator
    uint8_t expected_value = 0x80;   // Value when reverse is engaged

    // ═══════════════════════════════════════════════════════════════
    // GPIO DETECTION (Classic cars / Direct 12V signal)
    // ═══════════════════════════════════════════════════════════════
    // For VW Gol Quadrado, Fusca, and other classic Brazilian cars:
    // - Connect reverse light wire (12V when reverse) to optocoupler
    // - Optocoupler output connects to Raspberry Pi GPIO
    // - See docs/WIRING_GOL_QUADRADO.md for circuit diagram
    bool gpio_enabled     = false;
    std::string gpio_chip = "gpiochip0";  // Raspberry Pi 4: gpiochip0
    uint32_t gpio_line    = 17;           // GPIO17 (Pin 11 on header)
    bool gpio_active_low  = false;        // true if signal is LOW when reverse engaged
                                          // (depends on optocoupler circuit)

    // ═══════════════════════════════════════════════════════════════
    // SIGNAL CONDITIONING
    // ═══════════════════════════════════════════════════════════════
    // Debounce time - filters electrical noise from gear lever
    // Classic cars may need higher values (100-200ms) due to older switches
    uint32_t debounce_ms = 50;

    // ═══════════════════════════════════════════════════════════════
    // HELPER METHODS
    // ═══════════════════════════════════════════════════════════════
    // Apply preset for common vehicle types
    void applyPreset(const std::string& preset) {
        if (preset == "gol_quadrado" || preset == "classic_vw") {
            // VW Gol Quadrado / Fusca / Kombi - GPIO only
            detection_mode  = "gpio";
            can_enabled     = false;
            gpio_enabled    = true;
            gpio_line       = 17;    // Recommended GPIO
            gpio_active_low = true;  // Optocoupler inverts signal
            debounce_ms     = 100;   // Older switches need more debounce
        } else if (preset == "speeduino_can") {
            // Speeduino with CAN broadcast enabled
            detection_mode = "can";
            can_enabled    = true;
            gpio_enabled   = false;
            can_id         = 0x370;  // Speeduino default
            byte_index     = 3;
            bit_mask       = 0x80;
        } else if (preset == "haltech") {
            // Haltech ECU
            detection_mode = "can";
            can_enabled    = true;
            gpio_enabled   = false;
            can_id         = 0x360;
            byte_index     = 4;
            bit_mask       = 0x02;
        }
    }
};

// Steering wheel button config
struct SteeringButtonDef {
    uint8_t button_id  = 0;
    uint32_t can_id    = 0;
    uint8_t byte_index = 0;
    uint8_t bit_mask   = 0;
    std::string action;  // "volume_up", "next_track", etc.
};

// Branding and splash screen configuration
struct BrandingConfig {
    // Splash screen settings
    bool show_splash          = true;
    uint32_t splash_duration_ms = 2500;  // Duration in milliseconds
    std::string splash_logo   = "";      // Path to logo image (PNG/JPG)
    double splash_logo_scale  = 1.0;     // Logo scale factor

    // Brand information
    std::string brand_name    = "Speeduino";
    std::string brand_color   = "#00ff00";  // Primary brand color (hex)

    // Vehicle manufacturer presets
    std::string manufacturer  = "";  // "volkswagen", "fiat", "chevrolet", "ford", etc.
};

// System configuration
struct SystemConfig {
    // CAN interface
    std::string can_interface = "can0";
    uint32_t can_bitrate      = 500000;

    // CAN protocol selection
    std::string can_protocol = "haltech";  // "haltech", "bmw", "vag"

    // ZMQ settings
    uint32_t zmq_publish_rate_hz = 50;

    // Camera settings
    std::string camera_device = "/dev/video0";
    uint32_t camera_width     = 640;
    uint32_t camera_height    = 480;
    uint32_t camera_fps       = 30;

    // OpenAuto path
    std::string openauto_path = "/usr/local/bin/openauto";

    // Timeouts
    uint32_t can_timeout_ms      = 500;
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
    static const BrandingConfig& getBrandingConfig();

    // Find signal by name
    static std::optional<CanSignalDef> findSignal(std::string_view name);

    // Check if CAN ID is allowed for commands
    static bool isCommandAllowed(uint32_t can_id);
    static uint32_t getCommandRateLimit(uint32_t can_id);

private:
    // ═══════════════════════════════════════════════════════════════════════
    // ISO 26262 ASIL-B: Thread-safe static members with mutex protection
    // Uses shared_mutex for read-heavy workloads (multiple readers, single writer)
    // ═══════════════════════════════════════════════════════════════════════
    static std::shared_mutex s_mutex;
    static SystemConfig s_systemConfig;
    static std::vector<CanSignalDef> s_signals;
    static std::vector<CanCommandDef> s_allowedCommands;
    static std::vector<SteeringButtonDef> s_steeringButtons;
    static ReverseConfig s_reverseConfig;
    static BrandingConfig s_brandingConfig;
    static bool s_loaded;
};

}  // namespace speeduino

#endif  // COMMON_CONFIG_LOADER_HPP
