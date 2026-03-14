#ifndef CAN_SERVICE_OBD_HANDLER_HPP
#define CAN_SERVICE_OBD_HANDLER_HPP

#include "can_service/can_interface.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// OBD-II Handler
// ISO 26262 ASIL-B: Complete OBD-II protocol implementation
//
// Supports:
//   - Mode 01: Live data (PIDs)
//   - Mode 03: Read confirmed DTCs
//   - Mode 04: Clear DTCs
//   - Mode 07: Read pending DTCs
//   - Mode 09: Vehicle information (VIN, ECU name)
//   - Mode 22: Custom PIDs (Speeduino extension)
//
// Reference: ISO 15031-5 (OBD-II PIDs), SAE J1979
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════
// OBD-II CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

namespace obd {
// CAN IDs
constexpr uint32_t REQUEST_BROADCAST = 0x7DF;  // Broadcast request
constexpr uint32_t REQUEST_ECU       = 0x7E0;  // ECU-specific request
constexpr uint32_t RESPONSE_ECU      = 0x7E8;  // ECU response

// OBD-II Modes
constexpr uint8_t MODE_01_LIVE_DATA    = 0x01;
constexpr uint8_t MODE_02_FREEZE_FRAME = 0x02;
constexpr uint8_t MODE_03_READ_DTCS    = 0x03;
constexpr uint8_t MODE_04_CLEAR_DTCS   = 0x04;
constexpr uint8_t MODE_05_O2_TEST      = 0x05;
constexpr uint8_t MODE_06_TEST_RESULTS = 0x06;
constexpr uint8_t MODE_07_PENDING_DTCS = 0x07;
constexpr uint8_t MODE_09_VEHICLE_INFO = 0x09;
constexpr uint8_t MODE_22_CUSTOM       = 0x22;  // Enhanced diagnostics

// Mode 01 PIDs (Live Data)
constexpr uint8_t PID_SUPPORTED_01_20 = 0x00;
constexpr uint8_t PID_ENGINE_LOAD     = 0x04;
constexpr uint8_t PID_COOLANT_TEMP    = 0x05;
constexpr uint8_t PID_FUEL_TRIM_SHORT = 0x06;
constexpr uint8_t PID_FUEL_TRIM_LONG  = 0x07;
constexpr uint8_t PID_FUEL_PRESSURE   = 0x0A;
constexpr uint8_t PID_MAP             = 0x0B;
constexpr uint8_t PID_RPM             = 0x0C;
constexpr uint8_t PID_SPEED           = 0x0D;
constexpr uint8_t PID_TIMING_ADVANCE  = 0x0E;
constexpr uint8_t PID_INTAKE_TEMP     = 0x0F;
constexpr uint8_t PID_MAF             = 0x10;
constexpr uint8_t PID_TPS             = 0x11;
constexpr uint8_t PID_O2_VOLTAGE      = 0x14;
constexpr uint8_t PID_OBD_STANDARD    = 0x1C;
constexpr uint8_t PID_RUNTIME         = 0x1F;
constexpr uint8_t PID_DISTANCE_MIL    = 0x21;
constexpr uint8_t PID_FUEL_LEVEL      = 0x2F;
constexpr uint8_t PID_BARO_PRESSURE   = 0x33;
constexpr uint8_t PID_CONTROL_VOLTAGE = 0x42;
constexpr uint8_t PID_AMBIENT_TEMP    = 0x46;
constexpr uint8_t PID_OIL_TEMP        = 0x5C;

// Mode 09 PIDs (Vehicle Info)
constexpr uint8_t PID_VIN      = 0x02;
constexpr uint8_t PID_ECU_NAME = 0x0A;

// Mode 22 Custom PIDs (Speeduino/SCG-ECU)
constexpr uint8_t CUSTOM_AUX_PREFIX    = 0x77;
constexpr uint8_t CUSTOM_STATUS_PREFIX = 0x78;
}  // namespace obd

// ═══════════════════════════════════════════════════════════════════════════════
// DTC (Diagnostic Trouble Code) Structure
// ═══════════════════════════════════════════════════════════════════════════════

struct DTC {
    std::string code;         // P0XXX, C0XXX, B0XXX, U0XXX
    std::string description;  // Human-readable description
    bool isPending = false;   // Pending vs confirmed

    // Parse DTC from 2 bytes (ISO 15031-6)
    static DTC fromBytes(uint8_t high, uint8_t low);

    // Get description for known codes
    static std::string getDescription(const std::string& code);
};

// ═══════════════════════════════════════════════════════════════════════════════
// OBD-II Response Types
// ═══════════════════════════════════════════════════════════════════════════════

struct OBDLiveData {
    uint8_t pid  = 0;
    double value = 0.0;
    std::string unit;
};

struct OBDVehicleInfo {
    std::string vin;
    std::string ecuName;
};

struct OBDDTCList {
    std::vector<DTC> confirmed;
    std::vector<DTC> pending;
    bool milOn       = false;
    uint8_t dtcCount = 0;
};

// Response variant
using OBDResponse = std::variant<OBDLiveData, OBDVehicleInfo, OBDDTCList,
                                 bool  // For clear DTCs success/fail
                                 >;

// ═══════════════════════════════════════════════════════════════════════════════
// OBD-II Handler Class
// ═══════════════════════════════════════════════════════════════════════════════

class OBDHandler {
public:
    explicit OBDHandler(CanInterface& interface);
    ~OBDHandler() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 01: Live Data Requests
    // ═══════════════════════════════════════════════════════════════════════

    // Request a single PID
    std::optional<OBDLiveData> requestPID(uint8_t pid, uint32_t timeout_ms = 100);

    // Request multiple PIDs (batched)
    std::vector<OBDLiveData> requestPIDs(const std::vector<uint8_t>& pids,
                                         uint32_t timeout_ms = 100);

    // Get supported PIDs (queries PID 0x00)
    std::vector<uint8_t> getSupportedPIDs();

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 03/07: DTC Reading
    // ═══════════════════════════════════════════════════════════════════════

    // Read confirmed DTCs
    OBDDTCList readDTCs();

    // Read pending DTCs
    std::vector<DTC> readPendingDTCs();

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 04: Clear DTCs
    // ═══════════════════════════════════════════════════════════════════════

    // Clear all DTCs and reset MIL
    bool clearDTCs();

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 09: Vehicle Information
    // ═══════════════════════════════════════════════════════════════════════

    // Get VIN (17 characters)
    std::optional<std::string> getVIN();

    // Get ECU name
    std::optional<std::string> getECUName();

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 22: Custom PIDs (Speeduino/SCG-ECU Extension)
    // ═══════════════════════════════════════════════════════════════════════

    // Read AUX channel (0x77XX)
    std::optional<double> readAuxChannel(uint8_t channel);

    // Read currentStatus field by index (0x78XX)
    std::optional<double> readStatusField(uint8_t fieldIndex);

    // ═══════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════

    void setTimeout(uint32_t timeout_ms) { m_timeout_ms = timeout_ms; }
    void setRetryCount(uint8_t count) { m_retryCount = count; }
    void setBroadcast(bool broadcast) { m_useBroadcast = broadcast; }

    // Statistics
    uint32_t getRequestCount() const { return m_requestCount; }
    uint32_t getTimeoutCount() const { return m_timeoutCount; }
    uint32_t getErrorCount() const { return m_errorCount; }

private:
    // Build request frame
    CanFrame buildRequest(uint8_t mode, uint8_t pid1 = 0, uint8_t pid2 = 0);

    // Send request and wait for response
    std::optional<CanFrame> sendAndReceive(const CanFrame& request, uint32_t timeout_ms = 0);

    // Parse response for Mode 01
    OBDLiveData parseLiveData(uint8_t pid, const CanFrame& response);

    // Parse multi-frame response (for VIN, etc.)
    std::string parseMultiFrame(const std::vector<CanFrame>& frames);

    // Decode PID value
    static double decodePID(uint8_t pid, const uint8_t* data, uint8_t len);

    CanInterface& m_interface;
    uint32_t m_timeout_ms = 100;
    uint8_t m_retryCount  = 3;
    bool m_useBroadcast   = true;

    // Statistics
    uint32_t m_requestCount = 0;
    uint32_t m_timeoutCount = 0;
    uint32_t m_errorCount   = 0;

    std::mutex m_mutex;
};

// ═══════════════════════════════════════════════════════════════════════════════
// DTC Database (Known Codes)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Common DTCs from SCG-ECU 2.0 documentation:
//   P0105 - MAP sensor malfunction
//   P0107 - MAP sensor low input
//   P0108 - MAP sensor high input
//   P0110 - IAT sensor circuit malfunction
//   P0112 - IAT sensor low input
//   P0113 - IAT sensor high input
//   P0115 - CLT sensor circuit malfunction
//   P0117 - CLT sensor low input
//   P0118 - CLT sensor high input
//   P0120 - TPS circuit malfunction
//   P0130 - O2 sensor circuit malfunction
//   P0171 - System too lean (Bank 1)
//   P0172 - System too rich (Bank 1)
//   P0217 - Engine overtemp condition
//   P0219 - Engine overspeed condition
//   P0230 - Fuel pump primary circuit
//   P0335 - Crank position sensor A circuit
//   P0336 - Crank position sensor A range
//   P0340 - Cam position sensor A circuit
//   P0520 - Engine oil pressure sensor
//   P0562 - System voltage low
//   P0563 - System voltage high
//   P1000 - OBD system readiness not complete

}  // namespace speeduino

#endif  // CAN_SERVICE_OBD_HANDLER_HPP
