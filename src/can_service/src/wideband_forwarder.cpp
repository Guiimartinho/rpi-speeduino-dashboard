#include "can_service/wideband_forwarder.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <cmath>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// WidebandForwarder Implementation
// ═══════════════════════════════════════════════════════════════════════════════

WidebandForwarder::WidebandForwarder(CanInterface& interface)
    : m_interface(interface)
{
}

WidebandForwarder::~WidebandForwarder() {
    stop();
}

void WidebandForwarder::setFormat(WidebandFormat format) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format = format;

    // Set default CAN ID for format
    switch (format) {
        case WidebandFormat::AEM_XSERIES:
            m_canId = wideband::AEM_XSERIES_ID;
            break;
        case WidebandFormat::INNOVATE_LC2:
            m_canId = wideband::INNOVATE_LC2_ID;
            break;
        case WidebandFormat::PLX_SM_AFR:
            m_canId = wideband::PLX_SM_AFR_ID;
            break;
        case WidebandFormat::SPARTAN_14POINT7:
            m_canId = wideband::SPARTAN_14POINT7;
            break;
    }

    LOG_INFO("Wideband: Format set to " + std::to_string(static_cast<int>(format)) +
             ", CAN ID 0x" + std::to_string(m_canId));
}

void WidebandForwarder::setRate(uint32_t rateHz) {
    // Clamp to reasonable range (1-50 Hz)
    m_rateHz = std::clamp(rateHz, 1U, 50U);
    LOG_DEBUG("Wideband: Rate set to " + std::to_string(m_rateHz) + " Hz");
}

void WidebandForwarder::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled && m_running) {
        stop();
    }
}

void WidebandForwarder::setLambda(float lambda) {
    // Clamp to safe range
    m_lambda = std::clamp(lambda, wideband::LAMBDA_MIN, wideband::LAMBDA_MAX);
}

void WidebandForwarder::setAFR(float afr) {
    setLambda(afr / wideband::STOICH_AFR);
}

void WidebandForwarder::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    m_txThread = std::thread(&WidebandForwarder::transmitLoop, this);
    LOG_INFO("Wideband: Forwarder started at " + std::to_string(m_rateHz) + " Hz");
}

void WidebandForwarder::stop() {
    m_running = false;
    if (m_txThread.joinable()) {
        m_txThread.join();
    }
    LOG_INFO("Wideband: Forwarder stopped");
}

bool WidebandForwarder::sendNow() {
    if (!m_enabled) {
        return false;
    }

    CanFrame frame = buildFrame();
    bool success = m_interface.send(frame);

    if (success) {
        m_framesSent++;
    } else {
        m_framesFailed++;
        LOG_WARN("Wideband: Failed to send frame");
    }

    return success;
}

void WidebandForwarder::transmitLoop() {
    auto intervalUs = std::chrono::microseconds(1000000 / m_rateHz);

    while (m_running) {
        auto start = std::chrono::steady_clock::now();

        if (m_enabled) {
            sendNow();
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleepTime = intervalUs - elapsed;

        if (sleepTime > std::chrono::microseconds(0)) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

CanFrame WidebandForwarder::buildFrame() const {
    switch (m_format) {
        case WidebandFormat::AEM_XSERIES:
            return buildAEMFrame();
        case WidebandFormat::INNOVATE_LC2:
            return buildInnovateFrame();
        case WidebandFormat::PLX_SM_AFR:
            return buildPLXFrame();
        case WidebandFormat::SPARTAN_14POINT7:
            return buildSpartanFrame();
        default:
            return buildAEMFrame();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// AEM X-Series Frame Format (CAN ID 0x180)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0-1: Lambda × 10000 (Big Endian)
//           Example: Lambda 1.0 = 10000 = 0x2710
//           Example: Lambda 0.85 = 8500 = 0x2134
// Byte 2-3: O2 sensor millivolts (Big Endian)
// Byte 4:   Sensor status (0 = OK)
// Byte 5:   Heater status (0 = OK, warming up = 1-100%)
// Byte 6:   Reserved
// Byte 7:   Reserved

CanFrame WidebandForwarder::buildAEMFrame() const {
    CanFrame frame;
    frame.id = m_canId;
    frame.dlc = 8;
    frame.data.fill(0x00);

    // Lambda × 10000 (Big Endian)
    uint16_t lambdaScaled = static_cast<uint16_t>(m_lambda.load() * 10000.0f);
    frame.data[0] = (lambdaScaled >> 8) & 0xFF;
    frame.data[1] = lambdaScaled & 0xFF;

    // O2 millivolts (Big Endian)
    uint16_t o2mv = m_o2Millivolts.load();
    frame.data[2] = (o2mv >> 8) & 0xFF;
    frame.data[3] = o2mv & 0xFF;

    // Status
    frame.data[4] = m_sensorStatus.load();
    frame.data[5] = m_heaterStatus.load();

    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Innovate LC-2 Frame Format (CAN ID 0x190)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0-1: AFR × 10 (Big Endian)
//           Example: AFR 14.7 = 147 = 0x0093
// Byte 2-3: Lambda × 1000 (Big Endian)
// Byte 4:   Warmup status (0-100%)
// Byte 5:   Error flags
// Byte 6-7: Reserved

CanFrame WidebandForwarder::buildInnovateFrame() const {
    CanFrame frame;
    frame.id = m_canId;
    frame.dlc = 8;
    frame.data.fill(0x00);

    float lambda = m_lambda.load();
    float afr = lambda * wideband::STOICH_AFR;

    // AFR × 10 (Big Endian)
    uint16_t afrScaled = static_cast<uint16_t>(afr * 10.0f);
    frame.data[0] = (afrScaled >> 8) & 0xFF;
    frame.data[1] = afrScaled & 0xFF;

    // Lambda × 1000 (Big Endian)
    uint16_t lambdaScaled = static_cast<uint16_t>(lambda * 1000.0f);
    frame.data[2] = (lambdaScaled >> 8) & 0xFF;
    frame.data[3] = lambdaScaled & 0xFF;

    // Warmup status (inverse of heater status for Innovate)
    frame.data[4] = 100 - m_heaterStatus.load();

    // Error flags
    frame.data[5] = m_sensorStatus.load();

    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PLX SM-AFR Frame Format (CAN ID 0x181)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0-1: AFR × 100 (Little Endian)
// Byte 2:   Sensor status
// Byte 3-7: Reserved

CanFrame WidebandForwarder::buildPLXFrame() const {
    CanFrame frame;
    frame.id = m_canId;
    frame.dlc = 8;
    frame.data.fill(0x00);

    float afr = m_lambda.load() * wideband::STOICH_AFR;

    // AFR × 100 (Little Endian for PLX)
    uint16_t afrScaled = static_cast<uint16_t>(afr * 100.0f);
    frame.data[0] = afrScaled & 0xFF;
    frame.data[1] = (afrScaled >> 8) & 0xFF;

    // Status
    frame.data[2] = m_sensorStatus.load();

    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 14point7 Spartan Frame Format (CAN ID 0x182)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0-1: Lambda × 1000 (Big Endian)
// Byte 2-3: O2 millivolts (Big Endian)
// Byte 4:   Pump current (signed, offset 128)
// Byte 5:   Heater duty (0-100%)
// Byte 6:   Status
// Byte 7:   Reserved

CanFrame WidebandForwarder::buildSpartanFrame() const {
    CanFrame frame;
    frame.id = m_canId;
    frame.dlc = 8;
    frame.data.fill(0x00);

    // Lambda × 1000 (Big Endian)
    uint16_t lambdaScaled = static_cast<uint16_t>(m_lambda.load() * 1000.0f);
    frame.data[0] = (lambdaScaled >> 8) & 0xFF;
    frame.data[1] = lambdaScaled & 0xFF;

    // O2 millivolts (Big Endian)
    uint16_t o2mv = m_o2Millivolts.load();
    frame.data[2] = (o2mv >> 8) & 0xFF;
    frame.data[3] = o2mv & 0xFF;

    // Pump current (mock value, offset 128 = 0)
    frame.data[4] = 128;

    // Heater duty
    frame.data[5] = m_heaterStatus.load();

    // Status
    frame.data[6] = m_sensorStatus.load();

    return frame;
}

} // namespace speeduino
