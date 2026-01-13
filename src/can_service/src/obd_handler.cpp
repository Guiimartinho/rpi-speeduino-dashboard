#include "can_service/obd_handler.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <unordered_map>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// DTC Implementation
// ═══════════════════════════════════════════════════════════════════════════════

DTC DTC::fromBytes(uint8_t high, uint8_t low) {
    DTC dtc;

    // First 2 bits determine the category (P, C, B, U)
    // Next 2 bits are the first digit
    // Remaining 12 bits are the code
    char category;
    switch ((high >> 6) & 0x03) {
        case 0: category = 'P'; break;  // Powertrain
        case 1: category = 'C'; break;  // Chassis
        case 2: category = 'B'; break;  // Body
        case 3: category = 'U'; break;  // Network
        default: category = 'P';
    }

    uint8_t firstDigit = (high >> 4) & 0x03;
    uint8_t secondDigit = high & 0x0F;
    uint8_t thirdDigit = (low >> 4) & 0x0F;
    uint8_t fourthDigit = low & 0x0F;

    std::ostringstream oss;
    oss << category << static_cast<int>(firstDigit)
        << std::hex << std::uppercase
        << static_cast<int>(secondDigit)
        << static_cast<int>(thirdDigit)
        << static_cast<int>(fourthDigit);

    dtc.code = oss.str();
    dtc.description = getDescription(dtc.code);

    return dtc;
}

std::string DTC::getDescription(const std::string& code) {
    // Known DTCs from SCG-ECU 2.0 documentation
    static const std::unordered_map<std::string, std::string> dtcDatabase = {
        // MAP Sensor
        {"P0105", "MAP sensor malfunction"},
        {"P0107", "MAP sensor low input"},
        {"P0108", "MAP sensor high input"},

        // IAT Sensor
        {"P0110", "IAT sensor circuit malfunction"},
        {"P0112", "IAT sensor low input"},
        {"P0113", "IAT sensor high input"},

        // CLT Sensor
        {"P0115", "CLT sensor circuit malfunction"},
        {"P0117", "CLT sensor low input"},
        {"P0118", "CLT sensor high input"},

        // TPS
        {"P0120", "TPS circuit malfunction"},
        {"P0121", "TPS range/performance problem"},
        {"P0122", "TPS low input"},
        {"P0123", "TPS high input"},

        // O2 Sensor
        {"P0130", "O2 sensor circuit malfunction (Bank 1 Sensor 1)"},
        {"P0131", "O2 sensor low voltage (Bank 1 Sensor 1)"},
        {"P0132", "O2 sensor high voltage (Bank 1 Sensor 1)"},
        {"P0133", "O2 sensor slow response (Bank 1 Sensor 1)"},

        // Fuel System
        {"P0171", "System too lean (Bank 1)"},
        {"P0172", "System too rich (Bank 1)"},
        {"P0174", "System too lean (Bank 2)"},
        {"P0175", "System too rich (Bank 2)"},

        // Engine Protection
        {"P0217", "Engine overtemp condition"},
        {"P0219", "Engine overspeed condition"},

        // Fuel Pump
        {"P0230", "Fuel pump primary circuit malfunction"},
        {"P0231", "Fuel pump secondary circuit low"},
        {"P0232", "Fuel pump secondary circuit high"},

        // Crankshaft Position Sensor
        {"P0335", "Crankshaft position sensor A circuit"},
        {"P0336", "Crankshaft position sensor A range/performance"},
        {"P0337", "Crankshaft position sensor A low input"},
        {"P0338", "Crankshaft position sensor A high input"},

        // Camshaft Position Sensor
        {"P0340", "Camshaft position sensor A circuit (Bank 1)"},
        {"P0341", "Camshaft position sensor A range/performance"},
        {"P0342", "Camshaft position sensor A low input"},
        {"P0343", "Camshaft position sensor A high input"},

        // Ignition
        {"P0351", "Ignition coil A primary/secondary circuit"},
        {"P0352", "Ignition coil B primary/secondary circuit"},
        {"P0353", "Ignition coil C primary/secondary circuit"},
        {"P0354", "Ignition coil D primary/secondary circuit"},

        // Oil Pressure
        {"P0520", "Engine oil pressure sensor/switch circuit"},
        {"P0521", "Engine oil pressure sensor range/performance"},
        {"P0522", "Engine oil pressure sensor low voltage"},
        {"P0523", "Engine oil pressure sensor high voltage"},
        {"P0524", "Engine oil pressure too low"},

        // System Voltage
        {"P0560", "System voltage malfunction"},
        {"P0562", "System voltage low"},
        {"P0563", "System voltage high"},

        // Idle Control
        {"P0505", "Idle air control system malfunction"},
        {"P0506", "Idle air control system RPM lower than expected"},
        {"P0507", "Idle air control system RPM higher than expected"},

        // Knock Sensor
        {"P0325", "Knock sensor 1 circuit (Bank 1)"},
        {"P0326", "Knock sensor 1 range/performance"},
        {"P0327", "Knock sensor 1 low input"},
        {"P0328", "Knock sensor 1 high input"},

        // Manufacturer Specific
        {"P1000", "OBD system readiness not complete"},
    };

    auto it = dtcDatabase.find(code);
    if (it != dtcDatabase.end()) {
        return it->second;
    }
    return "Unknown DTC";
}

// ═══════════════════════════════════════════════════════════════════════════════
// OBDHandler Implementation
// ═══════════════════════════════════════════════════════════════════════════════

OBDHandler::OBDHandler(CanInterface& interface)
    : m_interface(interface)
{
}

CanFrame OBDHandler::buildRequest(uint8_t mode, uint8_t pid1, uint8_t pid2) {
    CanFrame frame;
    frame.id = m_useBroadcast ? obd::REQUEST_BROADCAST : obd::REQUEST_ECU;
    frame.dlc = 8;
    frame.data.fill(0x00);

    // ISO 15765-2 single frame format
    if (pid2 != 0) {
        // 3 bytes: mode + 2 PIDs (for Mode 22)
        frame.data[0] = 0x03;
        frame.data[1] = mode;
        frame.data[2] = pid1;
        frame.data[3] = pid2;
    } else if (pid1 != 0) {
        // 2 bytes: mode + PID
        frame.data[0] = 0x02;
        frame.data[1] = mode;
        frame.data[2] = pid1;
    } else {
        // 1 byte: mode only
        frame.data[0] = 0x01;
        frame.data[1] = mode;
    }

    return frame;
}

std::optional<CanFrame> OBDHandler::sendAndReceive(const CanFrame& request,
                                                    uint32_t timeout_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (timeout_ms == 0) {
        timeout_ms = m_timeout_ms;
    }

    m_requestCount++;

    // Send request
    if (!m_interface.send(request)) {
        LOG_ERROR("OBD-II: Failed to send request");
        m_errorCount++;
        return std::nullopt;
    }

    // Wait for response
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::milliseconds(timeout_ms)) {
            LOG_WARN("OBD-II: Request timeout");
            m_timeoutCount++;
            return std::nullopt;
        }

        auto maybeResponse = m_interface.receive(10);
        if (maybeResponse) {
            const auto& response = *maybeResponse;
            // Check if it's an OBD-II response
            if (response.id == obd::RESPONSE_ECU) {
                // Verify it's a positive response (mode + 0x40)
                uint8_t responseMode = response.data[1];
                uint8_t requestMode = request.data[1];
                if (responseMode == (requestMode + 0x40)) {
                    return response;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::optional<OBDLiveData> OBDHandler::requestPID(uint8_t pid, uint32_t timeout_ms) {
    CanFrame request = buildRequest(obd::MODE_01_LIVE_DATA, pid);

    auto response = sendAndReceive(request, timeout_ms);
    if (!response) {
        return std::nullopt;
    }

    return parseLiveData(pid, *response);
}

std::vector<OBDLiveData> OBDHandler::requestPIDs(const std::vector<uint8_t>& pids,
                                                  uint32_t timeout_ms) {
    std::vector<OBDLiveData> results;
    results.reserve(pids.size());

    for (uint8_t pid : pids) {
        auto data = requestPID(pid, timeout_ms);
        if (data) {
            results.push_back(*data);
        }
    }

    return results;
}

std::vector<uint8_t> OBDHandler::getSupportedPIDs() {
    std::vector<uint8_t> supported;

    auto response = requestPID(obd::PID_SUPPORTED_01_20);
    if (!response) {
        return supported;
    }

    // Parse bitmap (4 bytes, MSB first)
    // Each bit represents a PID (1-32)
    // PID 1 is bit 7 of byte 0, PID 8 is bit 0 of byte 0, etc.
    // TODO: Implement full bitmap parsing

    return supported;
}

OBDLiveData OBDHandler::parseLiveData(uint8_t pid, const CanFrame& response) {
    OBDLiveData data;
    data.pid = pid;

    // Response format: [length, mode+0x40, pid, A, B, C, D, ...]
    const uint8_t* values = &response.data[3];
    uint8_t len = response.data[0] - 2;  // Subtract mode and PID bytes

    data.value = decodePID(pid, values, len);

    // Set unit based on PID
    switch (pid) {
        case obd::PID_COOLANT_TEMP:
        case obd::PID_INTAKE_TEMP:
        case obd::PID_AMBIENT_TEMP:
        case obd::PID_OIL_TEMP:
            data.unit = "C";
            break;
        case obd::PID_RPM:
            data.unit = "rpm";
            break;
        case obd::PID_SPEED:
            data.unit = "km/h";
            break;
        case obd::PID_MAP:
        case obd::PID_BARO_PRESSURE:
            data.unit = "kPa";
            break;
        case obd::PID_ENGINE_LOAD:
        case obd::PID_TPS:
        case obd::PID_FUEL_LEVEL:
            data.unit = "%";
            break;
        case obd::PID_TIMING_ADVANCE:
            data.unit = "deg";
            break;
        case obd::PID_CONTROL_VOLTAGE:
            data.unit = "V";
            break;
        case obd::PID_MAF:
            data.unit = "g/s";
            break;
        case obd::PID_O2_VOLTAGE:
            data.unit = "V";
            break;
        case obd::PID_FUEL_PRESSURE:
            data.unit = "kPa";
            break;
        case obd::PID_RUNTIME:
        case obd::PID_DISTANCE_MIL:
            data.unit = "s";
            break;
        default:
            data.unit = "";
    }

    return data;
}

double OBDHandler::decodePID(uint8_t pid, const uint8_t* data, uint8_t len) {
    // ISO 15031-5 PID formulas
    switch (pid) {
        // Temperature: A - 40
        case obd::PID_COOLANT_TEMP:
        case obd::PID_INTAKE_TEMP:
        case obd::PID_AMBIENT_TEMP:
        case obd::PID_OIL_TEMP:
            return static_cast<double>(data[0]) - 40.0;

        // RPM: ((A * 256) + B) / 4
        case obd::PID_RPM:
            if (len >= 2) {
                return (static_cast<double>(data[0]) * 256.0 + data[1]) / 4.0;
            }
            break;

        // Speed: A (direct km/h)
        case obd::PID_SPEED:
            return static_cast<double>(data[0]);

        // Load, TPS, Fuel Level: A / 2.55 (percentage)
        case obd::PID_ENGINE_LOAD:
        case obd::PID_TPS:
        case obd::PID_FUEL_LEVEL:
            return static_cast<double>(data[0]) / 2.55;

        // MAP, Baro: A (direct kPa)
        case obd::PID_MAP:
        case obd::PID_BARO_PRESSURE:
            return static_cast<double>(data[0]);

        // Timing advance: (A / 2) - 64
        case obd::PID_TIMING_ADVANCE:
            return (static_cast<double>(data[0]) / 2.0) - 64.0;

        // Fuel trim: (A - 128) / 1.28
        case obd::PID_FUEL_TRIM_SHORT:
        case obd::PID_FUEL_TRIM_LONG:
            return (static_cast<double>(data[0]) - 128.0) / 1.28;

        // Control voltage: ((A * 256) + B) / 1000
        case obd::PID_CONTROL_VOLTAGE:
            if (len >= 2) {
                return (static_cast<double>(data[0]) * 256.0 + data[1]) / 1000.0;
            }
            break;

        // MAF: ((A * 256) + B) / 100
        case obd::PID_MAF:
            if (len >= 2) {
                return (static_cast<double>(data[0]) * 256.0 + data[1]) / 100.0;
            }
            break;

        // O2 voltage: A / 200
        case obd::PID_O2_VOLTAGE:
            return static_cast<double>(data[0]) / 200.0;

        // Fuel pressure: A * 3
        case obd::PID_FUEL_PRESSURE:
            return static_cast<double>(data[0]) * 3.0;

        // Runtime: (A * 256) + B (seconds)
        case obd::PID_RUNTIME:
            if (len >= 2) {
                return static_cast<double>(data[0]) * 256.0 + data[1];
            }
            break;

        // Distance with MIL: (A * 256) + B (km)
        case obd::PID_DISTANCE_MIL:
            if (len >= 2) {
                return static_cast<double>(data[0]) * 256.0 + data[1];
            }
            break;

        default:
            // Unknown PID - return raw first byte
            return static_cast<double>(data[0]);
    }

    return 0.0;
}

OBDDTCList OBDHandler::readDTCs() {
    OBDDTCList dtcList;

    CanFrame request = buildRequest(obd::MODE_03_READ_DTCS);
    auto response = sendAndReceive(request);

    if (!response) {
        return dtcList;
    }

    // Response format: [length, 0x43, count, DTC1_high, DTC1_low, DTC2_high, ...]
    if (response->data[1] != 0x43) {
        LOG_WARN("OBD-II: Unexpected response mode for DTCs");
        return dtcList;
    }

    dtcList.dtcCount = response->data[2];
    dtcList.milOn = (dtcList.dtcCount > 0);

    // Parse DTCs (up to 3 per frame)
    for (int i = 0; i < std::min(static_cast<int>(dtcList.dtcCount), 3); i++) {
        uint8_t high = response->data[3 + i * 2];
        uint8_t low = response->data[4 + i * 2];

        if (high != 0 || low != 0) {
            DTC dtc = DTC::fromBytes(high, low);
            dtcList.confirmed.push_back(dtc);
        }
    }

    LOG_INFO("OBD-II: Read " + std::to_string(dtcList.confirmed.size()) + " DTCs");
    return dtcList;
}

std::vector<DTC> OBDHandler::readPendingDTCs() {
    std::vector<DTC> pending;

    CanFrame request = buildRequest(obd::MODE_07_PENDING_DTCS);
    auto response = sendAndReceive(request);

    if (!response) {
        return pending;
    }

    // Response format: [length, 0x47, count, DTC1_high, DTC1_low, ...]
    if (response->data[1] != 0x47) {
        LOG_WARN("OBD-II: Unexpected response mode for pending DTCs");
        return pending;
    }

    uint8_t count = response->data[2];

    for (int i = 0; i < std::min(static_cast<int>(count), 3); i++) {
        uint8_t high = response->data[3 + i * 2];
        uint8_t low = response->data[4 + i * 2];

        if (high != 0 || low != 0) {
            DTC dtc = DTC::fromBytes(high, low);
            dtc.isPending = true;
            pending.push_back(dtc);
        }
    }

    return pending;
}

bool OBDHandler::clearDTCs() {
    CanFrame request = buildRequest(obd::MODE_04_CLEAR_DTCS);
    auto response = sendAndReceive(request);

    if (!response) {
        return false;
    }

    // Response should be 0x44 (Mode 04 + 0x40)
    bool success = (response->data[1] == 0x44);

    if (success) {
        LOG_INFO("OBD-II: DTCs cleared successfully");
    } else {
        LOG_WARN("OBD-II: Failed to clear DTCs");
    }

    return success;
}

std::optional<std::string> OBDHandler::getVIN() {
    CanFrame request = buildRequest(obd::MODE_09_VEHICLE_INFO, obd::PID_VIN);
    auto response = sendAndReceive(request, 500);  // Longer timeout for multi-frame

    if (!response) {
        return std::nullopt;
    }

    // VIN is typically multi-frame (17 characters)
    // For now, handle single-frame response
    // Full implementation would need ISO-TP for multi-frame

    if (response->data[1] != 0x49) {
        return std::nullopt;
    }

    // Simple single-frame extraction
    std::string vin;
    for (int i = 4; i < 8 && response->data[i] != 0; i++) {
        vin += static_cast<char>(response->data[i]);
    }

    // TODO: Implement full multi-frame VIN reading with ISO-TP
    if (vin.length() < 17) {
        LOG_DEBUG("OBD-II: Partial VIN received (multi-frame not implemented)");
    }

    return vin.empty() ? std::nullopt : std::make_optional(vin);
}

std::optional<std::string> OBDHandler::getECUName() {
    CanFrame request = buildRequest(obd::MODE_09_VEHICLE_INFO, obd::PID_ECU_NAME);
    auto response = sendAndReceive(request, 500);

    if (!response) {
        return std::nullopt;
    }

    if (response->data[1] != 0x49) {
        return std::nullopt;
    }

    std::string name;
    for (int i = 4; i < 8 && response->data[i] != 0; i++) {
        name += static_cast<char>(response->data[i]);
    }

    return name.empty() ? std::nullopt : std::make_optional(name);
}

std::optional<double> OBDHandler::readAuxChannel(uint8_t channel) {
    // Mode 22 with 0x77XX for AUX channels
    CanFrame request = buildRequest(obd::MODE_22_CUSTOM, channel, obd::CUSTOM_AUX_PREFIX);
    auto response = sendAndReceive(request);

    if (!response) {
        return std::nullopt;
    }

    if (response->data[1] != 0x62) {  // Mode 22 + 0x40
        return std::nullopt;
    }

    // Value is in bytes 4-5 (16-bit)
    uint16_t raw = (static_cast<uint16_t>(response->data[4]) << 8) | response->data[5];
    return static_cast<double>(raw);
}

std::optional<double> OBDHandler::readStatusField(uint8_t fieldIndex) {
    // Mode 22 with 0x78XX for currentStatus fields
    CanFrame request = buildRequest(obd::MODE_22_CUSTOM, fieldIndex, obd::CUSTOM_STATUS_PREFIX);
    auto response = sendAndReceive(request);

    if (!response) {
        return std::nullopt;
    }

    if (response->data[1] != 0x62) {
        return std::nullopt;
    }

    // Value is in bytes 4-5 (16-bit)
    uint16_t raw = (static_cast<uint16_t>(response->data[4]) << 8) | response->data[5];
    return static_cast<double>(raw);
}

} // namespace speeduino
