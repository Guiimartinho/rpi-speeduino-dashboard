#include "can_service/bmw_cluster_sim.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <chrono>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// BMWClusterSim Implementation
// ═══════════════════════════════════════════════════════════════════════════════

BMWClusterSim::BMWClusterSim(CanInterface& interface)
    : m_interface(interface)
{
}

BMWClusterSim::~BMWClusterSim() {
    stop();
}

void BMWClusterSim::setGear(uint8_t gear) {
    switch (gear) {
        case 0:  m_gear = BMWGear::NEUTRAL; break;
        case 1:  m_gear = BMWGear::GEAR_1; break;
        case 2:  m_gear = BMWGear::GEAR_2; break;
        case 3:  m_gear = BMWGear::GEAR_3; break;
        case 4:  m_gear = BMWGear::GEAR_4; break;
        case 5:  m_gear = BMWGear::GEAR_5; break;
        case 6:  m_gear = BMWGear::GEAR_6; break;
        case 7:  m_gear = BMWGear::REVERSE; break;
        default: m_gear = BMWGear::NEUTRAL; break;
    }
}

void BMWClusterSim::setSteeringAngle(int16_t angleDeg) {
    // Clamp to ±720 degrees (2 full turns)
    m_steeringAngle = std::clamp(angleDeg, static_cast<int16_t>(-720),
                                           static_cast<int16_t>(720));
}

void BMWClusterSim::setAllWheelSpeeds(uint16_t speed) {
    m_wheelSpeedFL = speed;
    m_wheelSpeedFR = speed;
    m_wheelSpeedRL = speed;
    m_wheelSpeedRR = speed;
}

void BMWClusterSim::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    m_txThread = std::thread(&BMWClusterSim::transmitLoop, this);

    LOG_INFO("BMW Cluster Sim: Started (ASC=" + std::to_string(m_ascEnabled.load()) +
             ", EGS=" + std::to_string(m_egsEnabled.load()) +
             ", SAS=" + std::to_string(m_sasEnabled.load()) +
             ", ABS=" + std::to_string(m_absEnabled.load()) + ")");
}

void BMWClusterSim::stop() {
    m_running = false;
    if (m_txThread.joinable()) {
        m_txThread.join();
    }
    LOG_INFO("BMW Cluster Sim: Stopped");
}

void BMWClusterSim::tick() {
    m_tickCounter++;

    // ASC at 10Hz (every 5th tick at 50Hz base)
    if (m_ascEnabled && (m_tickCounter % 5) == 0) {
        sendASC1();
    }

    // EGS at 10Hz
    if (m_egsEnabled && (m_tickCounter % 5) == 1) {
        sendEGS();
    }

    // SAS at 50Hz (every tick)
    if (m_sasEnabled) {
        sendSAS();
    }

    // ABS at 20Hz (every 2-3 ticks)
    if (m_absEnabled && (m_tickCounter % 2) == 0) {
        sendABS();
    }
}

void BMWClusterSim::transmitLoop() {
    // Base rate 50Hz (for SAS), other modules derived from counter
    constexpr auto interval = std::chrono::milliseconds(20);

    while (m_running) {
        auto start = std::chrono::steady_clock::now();

        tick();

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleepTime = interval - elapsed;

        if (sleepTime > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ASC1 (0x153) - Traction Control Status
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0: DSC status flags
//         Bit 0: DSC off (yellow light on cluster)
//         Bit 1: DSC active (flashing yellow)
//         Bit 2: Traction control active
//         Bit 3: ABS active
// Byte 1: Counter (rolling 0-255)
// Byte 2-7: Reserved / other status

void BMWClusterSim::sendASC1() {
    CanFrame frame;
    frame.id = bmw::ASC1_ID;
    frame.dlc = 8;
    frame.data.fill(0x00);

    // Build status byte
    uint8_t status = 0;
    if (m_dscOff) status |= 0x01;
    if (m_dscActive) status |= 0x02;
    if (m_tractionActive) status |= 0x04;
    if (m_absActive) status |= 0x08;

    frame.data[0] = status;
    frame.data[1] = static_cast<uint8_t>(m_tickCounter & 0xFF);  // Rolling counter

    if (m_interface.send(frame)) {
        m_framesSent++;
    } else {
        m_framesFailed++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// EGS (0x43F) - Transmission Status
// ═══════════════════════════════════════════════════════════════════════════════
//
// Byte 0: Gear position ASCII character (P/R/N/D/1-6/M)
// Byte 1: Sport mode flag
//         Bit 0: Sport mode active (S indicator on cluster)
// Byte 2: Target gear (for auto shifting display)
// Byte 3: Counter
// Byte 4-7: Reserved

void BMWClusterSim::sendEGS() {
    CanFrame frame;
    frame.id = bmw::EGS_ID;
    frame.dlc = 8;
    frame.data.fill(0x00);

    frame.data[0] = static_cast<uint8_t>(m_gear.load());
    frame.data[1] = m_sportMode ? 0x01 : 0x00;
    frame.data[2] = frame.data[0];  // Target = current for manual
    frame.data[3] = static_cast<uint8_t>(m_tickCounter & 0xFF);

    if (m_interface.send(frame)) {
        m_framesSent++;
    } else {
        m_framesFailed++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SAS (0x1D2) - Steering Angle Sensor
// ═══════════════════════════════════════════════════════════════════════════════
//
// Bytes 0-1: Steering angle (signed, Little Endian)
//            Range: -7200 to +7200 (0.1 degree resolution)
//            Center: 0
//            Left: negative
//            Right: positive
// Bytes 2-3: Steering rate (signed, Little Endian)
//            Degrees per second × 10
// Byte 4: Status
//         Bit 0: Calibrated
//         Bit 1: Valid
// Byte 5-7: Reserved

void BMWClusterSim::sendSAS() {
    CanFrame frame;
    frame.id = bmw::SAS_ID;
    frame.dlc = 8;
    frame.data.fill(0x00);

    // Angle × 10 for 0.1 degree resolution
    int16_t angleScaled = m_steeringAngle.load() * 10;
    frame.data[0] = angleScaled & 0xFF;
    frame.data[1] = (angleScaled >> 8) & 0xFF;

    // Rate × 10
    int16_t rateScaled = m_steeringRate.load() * 10;
    frame.data[2] = rateScaled & 0xFF;
    frame.data[3] = (rateScaled >> 8) & 0xFF;

    // Status: calibrated and valid
    frame.data[4] = 0x03;

    if (m_interface.send(frame)) {
        m_framesSent++;
    } else {
        m_framesFailed++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ABS1 (0x1F0) - Front Wheel Speeds
// ═══════════════════════════════════════════════════════════════════════════════
//
// Bytes 0-1: Front Left wheel speed (km/h × 16, Little Endian)
// Bytes 2-3: Front Right wheel speed
// Byte 4: Status flags
// Byte 5-7: Reserved

void BMWClusterSim::sendABS() {
    // Front wheels (0x1F0)
    CanFrame frameFront;
    frameFront.id = bmw::ABS1_ID;
    frameFront.dlc = 8;
    frameFront.data.fill(0x00);

    // Speed × 16 for resolution
    uint16_t flSpeed = (m_wheelSpeedFL.load() * 16) / 10;  // Convert from ×10 to ×16
    uint16_t frSpeed = (m_wheelSpeedFR.load() * 16) / 10;

    frameFront.data[0] = flSpeed & 0xFF;
    frameFront.data[1] = (flSpeed >> 8) & 0xFF;
    frameFront.data[2] = frSpeed & 0xFF;
    frameFront.data[3] = (frSpeed >> 8) & 0xFF;
    frameFront.data[4] = 0x00;  // Status OK

    if (m_interface.send(frameFront)) {
        m_framesSent++;
    } else {
        m_framesFailed++;
    }

    // Rear wheels (0x1F5)
    CanFrame frameRear;
    frameRear.id = bmw::ABS2_ID;
    frameRear.dlc = 8;
    frameRear.data.fill(0x00);

    uint16_t rlSpeed = (m_wheelSpeedRL.load() * 16) / 10;
    uint16_t rrSpeed = (m_wheelSpeedRR.load() * 16) / 10;

    frameRear.data[0] = rlSpeed & 0xFF;
    frameRear.data[1] = (rlSpeed >> 8) & 0xFF;
    frameRear.data[2] = rrSpeed & 0xFF;
    frameRear.data[3] = (rrSpeed >> 8) & 0xFF;
    frameRear.data[4] = 0x00;  // Status OK

    if (m_interface.send(frameRear)) {
        m_framesSent++;
    } else {
        m_framesFailed++;
    }
}

} // namespace speeduino
