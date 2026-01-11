#include "reverse_service/reverse_detector.hpp"
#include "common/logger.hpp"
#include <chrono>

#ifdef HAS_GPIOD
#include <gpiod.h>
#endif

namespace speeduino {

namespace {

uint32_t getMonotonicMs() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

} // anonymous namespace

ReverseDetector::ReverseDetector() = default;

ReverseDetector::~ReverseDetector() {
    shutdown();
}

bool ReverseDetector::init(const ReverseConfig& config) {
    m_config = config;

    LOG_INFO("Initializing reverse detector");
    LOG_INFO("  CAN enabled: " + std::string(config.can_enabled ? "yes" : "no"));
    if (config.can_enabled) {
        LOG_INFO("  CAN ID: 0x" + std::to_string(config.can_id));
        LOG_INFO("  Byte index: " + std::to_string(config.byte_index));
        LOG_INFO("  Bit mask: 0x" + std::to_string(config.bit_mask));
    }

    LOG_INFO("  GPIO enabled: " + std::string(config.gpio_enabled ? "yes" : "no"));
    if (config.gpio_enabled) {
        LOG_INFO("  GPIO chip: " + config.gpio_chip);
        LOG_INFO("  GPIO line: " + std::to_string(config.gpio_line));

        if (!initGpio()) {
            LOG_WARN("GPIO initialization failed, CAN-only mode");
        }
    }

    m_lastTransition = std::chrono::steady_clock::now();

    return true;
}

void ReverseDetector::shutdown() {
#ifdef HAS_GPIOD
    if (m_gpioRequest) {
        gpiod_line_request_release(m_gpioRequest);
        m_gpioRequest = nullptr;
    }
    if (m_gpioChip) {
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
    }
#endif
    m_gpioInitialized = false;
}

void ReverseDetector::setCallback(ReverseStateCallback callback) {
    m_callback = std::move(callback);
}

void ReverseDetector::procesCanFrame(uint32_t can_id, const uint8_t* data, uint8_t dlc) {
    if (!m_config.can_enabled) {
        return;
    }

    if (can_id != m_config.can_id) {
        return;
    }

    if (m_config.byte_index >= dlc) {
        return;
    }

    uint8_t value = data[m_config.byte_index] & m_config.bit_mask;
    bool reverseDetected = (value == m_config.expected_value);

    // Apply debounce
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastTransition).count();

    if (reverseDetected != m_pendingState) {
        m_pendingState = reverseDetected;
        m_lastTransition = now;
    } else if (elapsed >= static_cast<long>(m_config.debounce_ms) && m_pendingState != m_engaged) {
        setState(m_pendingState, Source::CAN);
    }
}

void ReverseDetector::checkGpio() {
    if (!m_config.gpio_enabled || !m_gpioInitialized) {
        return;
    }

    bool gpioState = readGpio();

    // Apply active_low configuration
    if (m_config.gpio_active_low) {
        gpioState = !gpioState;
    }

    // Apply debounce
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastTransition).count();

    if (gpioState != m_pendingState) {
        m_pendingState = gpioState;
        m_lastTransition = now;
    } else if (elapsed >= static_cast<long>(m_config.debounce_ms) && m_pendingState != m_engaged) {
        setState(m_pendingState, Source::GPIO);
    }
}

void ReverseDetector::setState(bool engaged, Source source) {
    bool changed = (engaged != m_engaged);

    m_engaged = engaged;
    m_source = source;
    m_lastChangeTimestamp = getMonotonicMs();

    if (changed) {
        const char* sourceStr = (source == Source::CAN) ? "CAN" : "GPIO";
        const char* stateStr = engaged ? "ENGAGED" : "DISENGAGED";

        LOG_INFO("Reverse gear " + std::string(stateStr) +
                 " (source: " + sourceStr + ")");

        if (m_callback) {
            m_callback(engaged, static_cast<uint8_t>(source));
        }
    }
}

bool ReverseDetector::initGpio() {
#ifdef HAS_GPIOD
    // gpiod v2 API - open chip by path
    std::string chipPath = "/dev/" + m_config.gpio_chip;
    m_gpioChip = gpiod_chip_open(chipPath.c_str());
    if (!m_gpioChip) {
        LOG_ERROR("Failed to open GPIO chip: " + chipPath);
        return false;
    }

    // Store the offset for later use
    m_gpioOffset = m_config.gpio_line;

    // Create line settings for input
    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings) {
        LOG_ERROR("Failed to create GPIO line settings");
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return false;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    // Create line config
    struct gpiod_line_config* lineConfig = gpiod_line_config_new();
    if (!lineConfig) {
        LOG_ERROR("Failed to create GPIO line config");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return false;
    }

    // Add line with settings
    unsigned int offsets[] = {m_gpioOffset};
    if (gpiod_line_config_add_line_settings(lineConfig, offsets, 1, settings) < 0) {
        LOG_ERROR("Failed to add GPIO line settings");
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return false;
    }

    // Create request config
    struct gpiod_request_config* reqConfig = gpiod_request_config_new();
    if (!reqConfig) {
        LOG_ERROR("Failed to create GPIO request config");
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return false;
    }

    gpiod_request_config_set_consumer(reqConfig, "reverse_service");

    // Request the line
    m_gpioRequest = gpiod_chip_request_lines(m_gpioChip, reqConfig, lineConfig);

    // Cleanup config objects
    gpiod_request_config_free(reqConfig);
    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (!m_gpioRequest) {
        LOG_ERROR("Failed to request GPIO line: " + std::to_string(m_config.gpio_line));
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return false;
    }

    m_gpioInitialized = true;
    LOG_INFO("GPIO initialized: " + m_config.gpio_chip + " line " +
             std::to_string(m_config.gpio_line));
    return true;
#else
    LOG_WARN("GPIO support not compiled in (libgpiod not found)");
    return false;
#endif
}

bool ReverseDetector::readGpio() {
#ifdef HAS_GPIOD
    if (!m_gpioRequest) {
        return false;
    }
    enum gpiod_line_value value = gpiod_line_request_get_value(m_gpioRequest, m_gpioOffset);
    return value == GPIOD_LINE_VALUE_ACTIVE;
#else
    return false;
#endif
}

} // namespace speeduino
