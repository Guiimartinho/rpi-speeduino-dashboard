#ifndef CAN_SERVICE_WIDEBAND_FORWARDER_HPP
#define CAN_SERVICE_WIDEBAND_FORWARDER_HPP

#include "can_service/can_interface.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// Wideband Lambda Forwarder
// ISO 26262 ASIL-B: Forward wideband O2 sensor data to ECU via CAN
//
// Supports multiple wideband controller formats:
//   - AEM X-Series (0x180) - Most common
//   - Innovate LC-2 (0x190)
//   - PLX SM-AFR (0x181)
//   - 14point7 Spartan (0x182)
//
// Use cases:
//   - ECU without integrated wideband controller
//   - External wideband sensor (AEM, Innovate, etc.)
//   - Wideband data bridging from one CAN bus to another
//
// Reference: SCG-ECU 2.0 CAN Dashboard Master Reference
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════
// WIDEBAND FORMAT CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

namespace wideband {
// CAN IDs for different wideband controllers
constexpr uint32_t AEM_XSERIES_ID   = 0x180;
constexpr uint32_t INNOVATE_LC2_ID  = 0x190;
constexpr uint32_t PLX_SM_AFR_ID    = 0x181;
constexpr uint32_t SPARTAN_14POINT7 = 0x182;

// Lambda limits (safety bounds)
constexpr float LAMBDA_MIN = 0.50f;  // Rich limit (AFR ~7.35)
constexpr float LAMBDA_MAX = 2.00f;  // Lean limit (AFR ~29.4)

// AFR for gasoline
constexpr float STOICH_AFR = 14.7f;
}  // namespace wideband

// ═══════════════════════════════════════════════════════════════════════════════
// Wideband Data Format
// ═══════════════════════════════════════════════════════════════════════════════

enum class WidebandFormat {
    AEM_XSERIES,      // AEM X-Series UEGO (most common)
    INNOVATE_LC2,     // Innovate LC-2
    PLX_SM_AFR,       // PLX SM-AFR
    SPARTAN_14POINT7  // 14point7 Spartan
};

// ═══════════════════════════════════════════════════════════════════════════════
// Wideband Forwarder Class
// ═══════════════════════════════════════════════════════════════════════════════

class WidebandForwarder {
public:
    explicit WidebandForwarder(CanInterface& interface);
    ~WidebandForwarder();

    // ═══════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════

    // Set wideband format (determines CAN ID and encoding)
    void setFormat(WidebandFormat format);
    WidebandFormat getFormat() const { return m_format; }

    // Set output CAN ID (override default for format)
    void setCanId(uint32_t id) { m_canId = id; }
    uint32_t getCanId() const { return m_canId; }

    // Set transmission rate (default 20Hz)
    void setRate(uint32_t rateHz);
    uint32_t getRate() const { return m_rateHz; }

    // Enable/disable forwarder
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // ═══════════════════════════════════════════════════════════════════════
    // DATA INPUT
    // ═══════════════════════════════════════════════════════════════════════

    // Set lambda value (0.5 - 2.0, clamped to bounds)
    void setLambda(float lambda);
    float getLambda() const { return m_lambda; }

    // Set AFR value (converted to lambda internally)
    void setAFR(float afr);
    float getAFR() const { return m_lambda * wideband::STOICH_AFR; }

    // Set additional data (sensor-specific)
    void setO2Millivolts(uint16_t mv) { m_o2Millivolts = mv; }
    void setSensorStatus(uint8_t status) { m_sensorStatus = status; }
    void setHeaterStatus(uint8_t status) { m_heaterStatus = status; }

    // ═══════════════════════════════════════════════════════════════════════
    // OPERATION
    // ═══════════════════════════════════════════════════════════════════════

    // Start automatic transmission at configured rate
    void start();

    // Stop automatic transmission
    void stop();

    // Send single frame immediately
    bool sendNow();

    // Check if running
    bool isRunning() const { return m_running; }

    // ═══════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════

    uint32_t getFramesSent() const { return m_framesSent; }
    uint32_t getFramesFailed() const { return m_framesFailed; }

private:
    // Build CAN frame for current format
    CanFrame buildFrame() const;

    // Build format-specific frames
    CanFrame buildAEMFrame() const;
    CanFrame buildInnovateFrame() const;
    CanFrame buildPLXFrame() const;
    CanFrame buildSpartanFrame() const;

    // Transmission thread function
    void transmitLoop();

    CanInterface& m_interface;

    // Configuration
    WidebandFormat m_format = WidebandFormat::AEM_XSERIES;
    uint32_t m_canId        = wideband::AEM_XSERIES_ID;
    uint32_t m_rateHz       = 20;  // 20Hz default
    std::atomic<bool> m_enabled{false};

    // Data
    std::atomic<float> m_lambda{1.0f};
    std::atomic<uint16_t> m_o2Millivolts{450};  // Stoich ~450mV
    std::atomic<uint8_t> m_sensorStatus{0};
    std::atomic<uint8_t> m_heaterStatus{0};

    // Transmission
    std::atomic<bool> m_running{false};
    std::thread m_txThread;
    std::mutex m_mutex;

    // Statistics
    std::atomic<uint32_t> m_framesSent{0};
    std::atomic<uint32_t> m_framesFailed{0};
};

}  // namespace speeduino

#endif  // CAN_SERVICE_WIDEBAND_FORWARDER_HPP
