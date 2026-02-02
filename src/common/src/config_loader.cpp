#include "common/config_loader.hpp"

#include "common/logger.hpp"

#include <yaml-cpp/yaml.h>

#include <climits>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// ISO 26262 ASIL-B: Thread-safe static members with mutex protection
// Static mutex must be defined before other static members
// ═══════════════════════════════════════════════════════════════════════════════
std::shared_mutex ConfigLoader::s_mutex;
SystemConfig ConfigLoader::s_systemConfig;
std::vector<CanSignalDef> ConfigLoader::s_signals;
std::vector<CanCommandDef> ConfigLoader::s_allowedCommands;
std::vector<SteeringButtonDef> ConfigLoader::s_steeringButtons;
ReverseConfig ConfigLoader::s_reverseConfig;
BrandingConfig ConfigLoader::s_brandingConfig;
bool ConfigLoader::s_loaded = false;

namespace {

// ═══════════════════════════════════════════════════════════════════════════
// ISO 26262 ASIL-B: Safe hex/decimal parsing with bounds checking
// MISRA C++:2008 Rule 5-0-3: Check input validity before processing
// ═══════════════════════════════════════════════════════════════════════════
uint32_t parseHexOrDec(const YAML::Node& node) {
    if (!node.IsDefined() || node.IsNull()) {
        return 0;
    }

    try {
        std::string value = node.as<std::string>();

        // Bounds check: prevent substr from throwing
        if (value.empty()) {
            return 0;
        }

        // Check for hex prefix safely
        bool isHex = false;
        if (value.size() >= 2) {
            if ((value[0] == '0') && (value[1] == 'x' || value[1] == 'X')) {
                isHex = true;
            }
        }

        if (isHex) {
            // Validate hex string has actual digits after prefix
            if (value.size() <= 2) {
                LOG_WARN("Empty hex value in config, using 0");
                return 0;
            }

            // stoul can throw std::out_of_range or std::invalid_argument
            unsigned long parsed = std::stoul(value, nullptr, 16);

            // Check for overflow (unsigned long may be larger than uint32_t)
            if (parsed > UINT32_MAX) {
                LOG_WARN("Hex value " + value + " exceeds uint32_t max, clamping");
                return UINT32_MAX;
            }

            return static_cast<uint32_t>(parsed);
        }

        // Decimal parsing
        return node.as<uint32_t>();

    } catch (const std::out_of_range& e) {
        LOG_ERROR("Config value out of range: " + std::string(e.what()));
        return 0;
    } catch (const std::invalid_argument& e) {
        LOG_ERROR("Invalid config value format: " + std::string(e.what()));
        return 0;
    } catch (const YAML::Exception& e) {
        LOG_ERROR("YAML parse error: " + std::string(e.what()));
        return 0;
    }
}

void loadHaltechSignals(std::vector<CanSignalDef>& signals) {
    // DATA1 (0x360) - RPM, MAP, TPS @ 50Hz
    signals.push_back({"rpm", 0x360, 0, 0, 16, true, false, 1.0, 0, "rpm"});
    signals.push_back({"map", 0x360, 2, 0, 16, true, false, 0.1, 0, "kPa"});
    signals.push_back({"tps", 0x360, 4, 0, 16, true, false, 0.1, 0, "%"});

    // DATA2 (0x361) - Fuel/Oil Pressure @ 50Hz
    signals.push_back({"fuel_pressure", 0x361, 0, 0, 16, true, false, 0.1, 0, "kPa"});
    signals.push_back({"oil_pressure", 0x361, 2, 0, 16, true, false, 0.1, 0, "kPa"});

    // DATA3 (0x362) - Injector Duty, Ignition @ 50Hz
    signals.push_back({"injector_duty", 0x362, 0, 0, 16, true, false, 0.1, 0, "%"});
    signals.push_back({"ignition_advance", 0x362, 4, 0, 16, true, true, 0.1, 0, "deg"});

    // LAMBDA (0x368) - Lambda @ 15Hz
    signals.push_back({"lambda1", 0x368, 0, 0, 16, true, false, 0.001, 0, ""});
    signals.push_back({"lambda2", 0x368, 2, 0, 16, true, false, 0.001, 0, ""});

    // VSS (0x370) - Speed, Gear @ 15Hz
    signals.push_back({"vehicle_speed", 0x370, 0, 0, 16, true, false, 0.1, 0, "km/h"});
    signals.push_back({"gear", 0x370, 3, 0, 8, true, false, 1.0, 0, ""});

    // DATA5 (0x3E0) - Temps @ 10Hz
    signals.push_back({"coolant_temp", 0x3E0, 0, 0, 16, true, false, 0.1, -273.0, "C"});
    signals.push_back({"intake_temp", 0x3E0, 2, 0, 16, true, false, 0.1, -273.0, "C"});
    signals.push_back({"fuel_temp", 0x3E0, 4, 0, 16, true, false, 0.1, -273.0, "C"});
}

void loadBMWSignals(std::vector<CanSignalDef>& signals) {
    // DME1 (0x316) - RPM @ 30Hz
    // RPM is bytes 2-3, little endian, divided by 6.4
    signals.push_back({"rpm", 0x316, 2, 0, 16, false, false, 0.15625, 0, "rpm"});
    signals.push_back({"torque", 0x316, 1, 0, 8, true, false, 1.0, 0, "%"});

    // DME2 (0x329) - CLT, TPS @ 30Hz
    signals.push_back({"coolant_temp", 0x329, 1, 0, 8, true, false, 0.75, -48.373, "C"});
    signals.push_back({"baro", 0x329, 2, 0, 8, true, false, 1.0, 0, "kPa"});
    signals.push_back({"tps", 0x329, 5, 0, 8, true, false, 0.392157, 0, "%"});

    // DME4 (0x545) - CEL, Fuel, Oil @ 10Hz
    signals.push_back({"fuel_consumption", 0x545, 1, 0, 16, false, false, 0.01, 0, "L/h"});
    signals.push_back({"oil_temp", 0x545, 4, 0, 8, true, false, 0.75, -48, "C"});
}

}  // anonymous namespace

bool ConfigLoader::loadFromDirectory(std::string_view config_dir) {
    // ISO 26262: Exclusive lock for writing - prevents data races
    std::unique_lock<std::shared_mutex> lock(s_mutex);

    std::filesystem::path dir(config_dir);

    bool success = true;

    auto system_path = dir / "system.yaml";
    if (std::filesystem::exists(system_path)) {
        success &= loadSystemConfig(system_path.string());
    } else {
        LOG_WARN("system.yaml not found, using defaults");
    }

    auto signals_path = dir / "can_signals.yaml";
    if (std::filesystem::exists(signals_path)) {
        success &= loadSignalsConfig(signals_path.string());
    } else {
        LOG_WARN("can_signals.yaml not found, loading default Haltech signals");
        loadHaltechSignals(s_signals);
    }

    auto steering_path = dir / "steering_wheel.yaml";
    if (std::filesystem::exists(steering_path)) {
        success &= loadSteeringConfig(steering_path.string());
    }

    s_loaded = success;
    return success;
}

bool ConfigLoader::loadSystemConfig(std::string_view path) {
    try {
        YAML::Node config = YAML::LoadFile(std::string(path));

        if (config["can"]) {
            auto can = config["can"];
            if (can["interface"])
                s_systemConfig.can_interface = can["interface"].as<std::string>();
            if (can["bitrate"])
                s_systemConfig.can_bitrate = can["bitrate"].as<uint32_t>();
            if (can["protocol"])
                s_systemConfig.can_protocol = can["protocol"].as<std::string>();
            if (can["timeout_ms"])
                s_systemConfig.can_timeout_ms = can["timeout_ms"].as<uint32_t>();
        }

        if (config["zmq"]) {
            auto zmq = config["zmq"];
            if (zmq["publish_rate_hz"])
                s_systemConfig.zmq_publish_rate_hz = zmq["publish_rate_hz"].as<uint32_t>();
        }

        if (config["camera"]) {
            auto cam = config["camera"];
            if (cam["enabled"])
                s_systemConfig.camera_enabled = cam["enabled"].as<bool>();
            if (cam["device"])
                s_systemConfig.camera_device = cam["device"].as<std::string>();
            if (cam["standard"])
                s_systemConfig.camera_standard = cam["standard"].as<std::string>();
            if (cam["width"])
                s_systemConfig.camera_width = cam["width"].as<uint32_t>();
            if (cam["height"])
                s_systemConfig.camera_height = cam["height"].as<uint32_t>();
            if (cam["fps"])
                s_systemConfig.camera_fps = cam["fps"].as<uint32_t>();
            if (cam["input"])
                s_systemConfig.camera_input = cam["input"].as<uint32_t>();
            if (cam["show_guides"])
                s_systemConfig.camera_show_guides = cam["show_guides"].as<bool>();
            if (cam["test_mode"])
                s_systemConfig.camera_test_mode = cam["test_mode"].as<bool>();
            if (cam["test_pattern"])
                s_systemConfig.camera_test_pattern = cam["test_pattern"].as<std::string>();
        }

        if (config["openauto"]) {
            auto oa = config["openauto"];
            if (oa["path"])
                s_systemConfig.openauto_path = oa["path"].as<std::string>();
        }

        if (config["reverse"]) {
            auto rev = config["reverse"];

            // Check for preset first - this sets defaults for specific vehicles
            if (rev["preset"]) {
                std::string preset = rev["preset"].as<std::string>();
                s_reverseConfig.applyPreset(preset);
                LOG_INFO("Applied reverse detection preset: " + preset);
            }

            // Detection mode (can override preset)
            if (rev["detection_mode"]) {
                s_reverseConfig.detection_mode = rev["detection_mode"].as<std::string>();
            }

            // CAN settings (can override preset)
            if (rev["can_enabled"])
                s_reverseConfig.can_enabled = rev["can_enabled"].as<bool>();
            if (rev["can_id"])
                s_reverseConfig.can_id = parseHexOrDec(rev["can_id"]);
            if (rev["byte_index"])
                s_reverseConfig.byte_index = rev["byte_index"].as<uint8_t>();
            if (rev["bit_mask"])
                s_reverseConfig.bit_mask = parseHexOrDec(rev["bit_mask"]);
            if (rev["expected_value"])
                s_reverseConfig.expected_value = parseHexOrDec(rev["expected_value"]);

            // GPIO settings (can override preset)
            if (rev["gpio_enabled"])
                s_reverseConfig.gpio_enabled = rev["gpio_enabled"].as<bool>();
            if (rev["gpio_chip"])
                s_reverseConfig.gpio_chip = rev["gpio_chip"].as<std::string>();
            if (rev["gpio_line"])
                s_reverseConfig.gpio_line = rev["gpio_line"].as<uint32_t>();
            if (rev["gpio_active_low"])
                s_reverseConfig.gpio_active_low = rev["gpio_active_low"].as<bool>();

            // Debounce setting
            if (rev["debounce_ms"])
                s_reverseConfig.debounce_ms = rev["debounce_ms"].as<uint32_t>();

            LOG_INFO("Reverse detection mode: " + s_reverseConfig.detection_mode);
        }

        if (config["allowed_commands"]) {
            for (const auto& cmd : config["allowed_commands"]) {
                CanCommandDef def;
                def.can_id = parseHexOrDec(cmd["id"]);
                if (cmd["rate_limit"])
                    def.rate_limit_hz = cmd["rate_limit"].as<uint32_t>();
                if (cmd["description"])
                    def.description = cmd["description"].as<std::string>();
                s_allowedCommands.push_back(def);
            }
        }

        // Branding / Splash screen configuration
        if (config["branding"]) {
            auto brand = config["branding"];
            if (brand["show_splash"])
                s_brandingConfig.show_splash = brand["show_splash"].as<bool>();
            if (brand["splash_duration_ms"])
                s_brandingConfig.splash_duration_ms = brand["splash_duration_ms"].as<uint32_t>();
            if (brand["splash_logo"])
                s_brandingConfig.splash_logo = brand["splash_logo"].as<std::string>();
            if (brand["splash_logo_scale"])
                s_brandingConfig.splash_logo_scale = brand["splash_logo_scale"].as<double>();
            if (brand["brand_name"])
                s_brandingConfig.brand_name = brand["brand_name"].as<std::string>();
            if (brand["brand_color"])
                s_brandingConfig.brand_color = brand["brand_color"].as<std::string>();
            if (brand["manufacturer"])
                s_brandingConfig.manufacturer = brand["manufacturer"].as<std::string>();

            LOG_INFO("Loaded branding config: " + s_brandingConfig.brand_name);
        }

        LOG_INFO("Loaded system config from " + std::string(path));
        return true;
    } catch (const YAML::Exception& e) {
        LOG_ERROR("Failed to load system config: " + std::string(e.what()));
        return false;
    }
}

bool ConfigLoader::loadSignalsConfig(std::string_view path) {
    try {
        YAML::Node config = YAML::LoadFile(std::string(path));

        // Check if we should use built-in signals
        if (config["use_builtin"]) {
            std::string builtin = config["use_builtin"].as<std::string>();
            if (builtin == "haltech") {
                loadHaltechSignals(s_signals);
            } else if (builtin == "bmw") {
                loadBMWSignals(s_signals);
            }
        }

        // Load custom signals (can override or add to builtins)
        if (config["signals"]) {
            for (const auto& sig : config["signals"]) {
                CanSignalDef def;
                def.name       = sig["name"].as<std::string>();
                def.can_id     = parseHexOrDec(sig["can_id"]);
                def.start_byte = sig["start_byte"].as<uint8_t>();
                if (sig["start_bit"])
                    def.start_bit = sig["start_bit"].as<uint8_t>();
                def.length_bits = sig["length_bits"].as<uint8_t>();
                if (sig["big_endian"])
                    def.is_big_endian = sig["big_endian"].as<bool>();
                if (sig["signed"])
                    def.is_signed = sig["signed"].as<bool>();
                if (sig["scale"])
                    def.scale = sig["scale"].as<double>();
                if (sig["offset"])
                    def.offset = sig["offset"].as<double>();
                if (sig["unit"])
                    def.unit = sig["unit"].as<std::string>();
                s_signals.push_back(def);
            }
        }

        LOG_INFO("Loaded " + std::to_string(s_signals.size()) + " CAN signals");
        return true;
    } catch (const YAML::Exception& e) {
        LOG_ERROR("Failed to load signals config: " + std::string(e.what()));
        return false;
    }
}

bool ConfigLoader::loadSteeringConfig(std::string_view path) {
    try {
        YAML::Node config = YAML::LoadFile(std::string(path));

        if (config["buttons"]) {
            for (const auto& btn : config["buttons"]) {
                SteeringButtonDef def;
                def.button_id  = btn["id"].as<uint8_t>();
                def.can_id     = parseHexOrDec(btn["can_id"]);
                def.byte_index = btn["byte_index"].as<uint8_t>();
                def.bit_mask   = parseHexOrDec(btn["bit_mask"]);
                def.action     = btn["action"].as<std::string>();
                s_steeringButtons.push_back(def);
            }
        }

        LOG_INFO("Loaded " + std::to_string(s_steeringButtons.size()) + " steering buttons");
        return true;
    } catch (const YAML::Exception& e) {
        LOG_ERROR("Failed to load steering config: " + std::string(e.what()));
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISO 26262 ASIL-B: Thread-safe getters with shared_lock (multiple readers OK)
// ═══════════════════════════════════════════════════════════════════════════════

const SystemConfig& ConfigLoader::getSystemConfig() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_systemConfig;
}

const std::vector<CanSignalDef>& ConfigLoader::getSignals() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_signals;
}

const std::vector<CanCommandDef>& ConfigLoader::getAllowedCommands() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_allowedCommands;
}

const std::vector<SteeringButtonDef>& ConfigLoader::getSteeringButtons() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_steeringButtons;
}

const ReverseConfig& ConfigLoader::getReverseConfig() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_reverseConfig;
}

const BrandingConfig& ConfigLoader::getBrandingConfig() {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    return s_brandingConfig;
}

std::optional<CanSignalDef> ConfigLoader::findSignal(std::string_view name) {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    for (const auto& sig : s_signals) {
        if (sig.name == name)
            return sig;
    }
    return std::nullopt;
}

bool ConfigLoader::isCommandAllowed(uint32_t can_id) {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    for (const auto& cmd : s_allowedCommands) {
        if (cmd.can_id == can_id)
            return true;
    }
    return false;
}

uint32_t ConfigLoader::getCommandRateLimit(uint32_t can_id) {
    std::shared_lock<std::shared_mutex> lock(s_mutex);
    for (const auto& cmd : s_allowedCommands) {
        if (cmd.can_id == can_id)
            return cmd.rate_limit_hz;
    }
    return 0;
}

}  // namespace speeduino
