#ifndef CAN_SERVICE_BMW_CLUSTER_SIM_HPP
#define CAN_SERVICE_BMW_CLUSTER_SIM_HPP

#include "can_service/can_interface.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// BMW Cluster Simulator
// ISO 26262 ASIL-B: Simulate BMW ECU messages for original cluster support
//
// When using an original BMW E46/E39/E38 cluster with a non-BMW ECU (like
// Speeduino), the cluster expects certain CAN messages from other BMW systems.
// This simulator generates those messages to prevent cluster errors.
//
// Simulated systems:
//   - ASC (Active Stability Control) - Traction control status
//   - EGS (Electronic Gearbox) - Automatic transmission status
//   - SAS (Steering Angle Sensor) - Steering wheel position
//   - ABS - Wheel speed sensors (for cluster speedometer)
//
// Reference: BMW E46 PT-CAN documentation
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════
// BMW CAN ID CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

namespace bmw {
// ASC (Traction Control)
constexpr uint32_t ASC1_ID = 0x153;  // ASC status
constexpr uint32_t ASC2_ID = 0x154;  // ASC wheel speeds

// EGS (Automatic Transmission)
constexpr uint32_t EGS_ID = 0x43F;  // Gear position, sport mode

// SAS (Steering Angle)
constexpr uint32_t SAS_ID = 0x1D2;  // Steering angle

// ABS (Wheel Speeds)
constexpr uint32_t ABS1_ID = 0x1F0;  // Front wheel speeds
constexpr uint32_t ABS2_ID = 0x1F5;  // Rear wheel speeds

// Transmission rates
constexpr uint32_t ASC_RATE_HZ = 10;
constexpr uint32_t EGS_RATE_HZ = 10;
constexpr uint32_t SAS_RATE_HZ = 50;
constexpr uint32_t ABS_RATE_HZ = 20;
}  // namespace bmw

// ═══════════════════════════════════════════════════════════════════════════════
// Gear Position Enum
// ═══════════════════════════════════════════════════════════════════════════════

enum class BMWGear : uint8_t {
    PARK    = 0x50,  // 'P'
    REVERSE = 0x52,  // 'R'
    NEUTRAL = 0x4E,  // 'N'
    DRIVE   = 0x44,  // 'D'
    GEAR_1  = 0x31,  // '1'
    GEAR_2  = 0x32,  // '2'
    GEAR_3  = 0x33,  // '3'
    GEAR_4  = 0x34,  // '4'
    GEAR_5  = 0x35,  // '5'
    GEAR_6  = 0x36,  // '6'
    MANUAL  = 0x4D,  // 'M' (manual mode)
};

// ═══════════════════════════════════════════════════════════════════════════════
// BMW Cluster Simulator Class
// ═══════════════════════════════════════════════════════════════════════════════

class BMWClusterSim {
public:
    explicit BMWClusterSim(CanInterface& interface);
    ~BMWClusterSim();

    // ═══════════════════════════════════════════════════════════════════════
    // ENABLE/DISABLE SIMULATION MODULES
    // ═══════════════════════════════════════════════════════════════════════

    void enableASC(bool enable) { m_ascEnabled = enable; }
    void enableEGS(bool enable) { m_egsEnabled = enable; }
    void enableSAS(bool enable) { m_sasEnabled = enable; }
    void enableABS(bool enable) { m_absEnabled = enable; }

    bool isASCEnabled() const { return m_ascEnabled; }
    bool isEGSEnabled() const { return m_egsEnabled; }
    bool isSASEnabled() const { return m_sasEnabled; }
    bool isABSEnabled() const { return m_absEnabled; }

    // ═══════════════════════════════════════════════════════════════════════
    // ASC (TRACTION CONTROL) DATA
    // ═══════════════════════════════════════════════════════════════════════

    // DSC (Dynamic Stability Control) states
    void setDSCOff(bool off) { m_dscOff = off; }
    void setDSCActive(bool active) { m_dscActive = active; }
    void setTractionActive(bool active) { m_tractionActive = active; }
    void setABSActive(bool active) { m_absActive = active; }

    // ═══════════════════════════════════════════════════════════════════════
    // EGS (TRANSMISSION) DATA
    // ═══════════════════════════════════════════════════════════════════════

    // Set current gear (for cluster display)
    void setGear(BMWGear gear) { m_gear = gear; }
    void setGear(uint8_t gear);  // 0=N, 1-6=gears, 7=R

    // Set sport mode indicator
    void setSportMode(bool sport) { m_sportMode = sport; }

    // ═══════════════════════════════════════════════════════════════════════
    // SAS (STEERING ANGLE) DATA
    // ═══════════════════════════════════════════════════════════════════════

    // Set steering angle (-720 to +720 degrees, 0 = center)
    void setSteeringAngle(int16_t angleDeg);

    // Set steering rate (degrees per second)
    void setSteeringRate(int16_t rateDegsPerSec) { m_steeringRate = rateDegsPerSec; }

    // ═══════════════════════════════════════════════════════════════════════
    // ABS (WHEEL SPEED) DATA
    // ═══════════════════════════════════════════════════════════════════════

    // Set individual wheel speeds (km/h × 10)
    void setWheelSpeedFL(uint16_t speed) { m_wheelSpeedFL = speed; }
    void setWheelSpeedFR(uint16_t speed) { m_wheelSpeedFR = speed; }
    void setWheelSpeedRL(uint16_t speed) { m_wheelSpeedRL = speed; }
    void setWheelSpeedRR(uint16_t speed) { m_wheelSpeedRR = speed; }

    // Set all wheel speeds to same value (convenience)
    void setAllWheelSpeeds(uint16_t speed);

    // ═══════════════════════════════════════════════════════════════════════
    // OPERATION
    // ═══════════════════════════════════════════════════════════════════════

    // Start simulation (begins transmitting all enabled modules)
    void start();

    // Stop simulation
    void stop();

    // Check if running
    bool isRunning() const { return m_running; }

    // Manual tick (send all frames once)
    void tick();

    // ═══════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════

    uint32_t getFramesSent() const { return m_framesSent; }
    uint32_t getFramesFailed() const { return m_framesFailed; }

private:
    // Build and send individual frames
    void sendASC1();
    void sendEGS();
    void sendSAS();
    void sendABS();

    // Transmission thread
    void transmitLoop();

    CanInterface& m_interface;

    // Module enable flags
    std::atomic<bool> m_ascEnabled{true};
    std::atomic<bool> m_egsEnabled{true};
    std::atomic<bool> m_sasEnabled{false};  // Optional
    std::atomic<bool> m_absEnabled{false};  // Optional

    // ASC data
    std::atomic<bool> m_dscOff{false};
    std::atomic<bool> m_dscActive{false};
    std::atomic<bool> m_tractionActive{false};
    std::atomic<bool> m_absActive{false};

    // EGS data
    std::atomic<BMWGear> m_gear{BMWGear::NEUTRAL};
    std::atomic<bool> m_sportMode{false};

    // SAS data
    std::atomic<int16_t> m_steeringAngle{0};
    std::atomic<int16_t> m_steeringRate{0};

    // ABS data
    std::atomic<uint16_t> m_wheelSpeedFL{0};
    std::atomic<uint16_t> m_wheelSpeedFR{0};
    std::atomic<uint16_t> m_wheelSpeedRL{0};
    std::atomic<uint16_t> m_wheelSpeedRR{0};

    // Transmission
    std::atomic<bool> m_running{false};
    std::thread m_txThread;
    std::mutex m_mutex;

    // Timing counters (for different rates)
    uint32_t m_tickCounter = 0;

    // Statistics
    std::atomic<uint32_t> m_framesSent{0};
    std::atomic<uint32_t> m_framesFailed{0};
};

}  // namespace speeduino

#endif  // CAN_SERVICE_BMW_CLUSTER_SIM_HPP
