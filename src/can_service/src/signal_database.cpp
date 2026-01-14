#include "can_service/can_parser.hpp"
#include "common/logger.hpp"

// ═══════════════════════════════════════════════════════════════════════════════
// CAN Signal Database Implementation
// ISO 26262 ASIL-B: Complete signal definitions for automotive protocols
//
// Protocols supported:
//   - Haltech (IC-7/IC-10) - Big Endian, 50Hz critical data
//   - BMW E46 PT-CAN - Little Endian, 30Hz
//   - VAG (VW/Audi/Gol Quadrado) - Little Endian, 30Hz
//
// Reference: SCG-ECU 2.0 CAN Dashboard Master Reference (Jan 2026)
// ═══════════════════════════════════════════════════════════════════════════════

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// HALTECH PROTOCOL (Big Endian - MSB First)
// Source: Haltech IC-7/IC-10 CAN Stream documentation
// ═══════════════════════════════════════════════════════════════════════════════
//
// IMPORTANT: Haltech encodes pressures with 1 atm (1013 mbar = 101.3 kPa) offset!
// Formula: pressure_kPa = (raw_value × 0.1) - 101.3
//          pressure_PSI = (raw_value - 1013) / 68.94
//
// Temperatures are in Kelvin × 10
// Formula: temp_C = (raw_value × 0.1) - 273.15

std::vector<CanSignalDef> createHaltechSignals() {
    std::vector<CanSignalDef> signals;

    // ═══════════════════════════════════════════════════════════════════════
    // DATA1 (0x360) - Core Engine Data @ 50Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: RPM (direct value, Big Endian)
    // Bytes 2-3: MAP (kPa × 10, Big Endian)
    // Bytes 4-5: TPS (% × 10, Big Endian) - Note: doc says ×0.2 but ×0.1 is standard
    // Bytes 6-7: Coolant pressure (not commonly used)
    signals.push_back({"rpm", 0x360, 0, 0, 16, true, false, 1.0, 0, "rpm"});
    signals.push_back({"map", 0x360, 2, 0, 16, true, false, 0.1, 0, "kPa"});
    signals.push_back({"tps", 0x360, 4, 0, 16, true, false, 0.1, 0, "%"});

    // ═══════════════════════════════════════════════════════════════════════
    // DATA2 (0x361) - Pressures and Load @ 50Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Fuel pressure (kPa × 10 + 1013 offset)
    // Bytes 2-3: Oil pressure (kPa × 10 + 1013 offset)
    // Bytes 4-5: Fuel load (% × 10)
    // Bytes 6-7: Wastegate pressure (not commonly used)
    //
    // CRITICAL: Apply -101.3 offset for gauge (1 atm) pressures
    signals.push_back({"fuel_pressure", 0x361, 0, 0, 16, true, false, 0.1, -101.3, "kPa"});
    signals.push_back({"oil_pressure", 0x361, 2, 0, 16, true, false, 0.1, -101.3, "kPa"});
    signals.push_back({"fuel_load", 0x361, 4, 0, 16, true, false, 0.1, 0, "%"});

    // ═══════════════════════════════════════════════════════════════════════
    // DATA3 (0x362) - Injector and Ignition @ 50Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Injector duty cycle (% × 10)
    // Bytes 2-3: Reserved
    // Bytes 4-5: Ignition advance (degrees × 10, SIGNED)
    // Bytes 6-7: Reserved
    signals.push_back({"injector_duty", 0x362, 0, 0, 16, true, false, 0.1, 0, "%"});
    signals.push_back({"ignition_advance", 0x362, 4, 0, 16, true, true, 0.1, 0, "deg"});

    // ═══════════════════════════════════════════════════════════════════════
    // PW (0x364) - Pulse Widths @ 50Hz (Injector timing)
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: PW1 (microseconds, cylinder 1)
    // Bytes 2-3: PW2 (microseconds, cylinder 2)
    // Bytes 4-5: PW3 (microseconds, cylinder 3)
    // Bytes 6-7: PW4 (microseconds, cylinder 4)
    signals.push_back({"pw1", 0x364, 0, 0, 16, true, false, 1.0, 0, "us"});
    signals.push_back({"pw2", 0x364, 2, 0, 16, true, false, 1.0, 0, "us"});
    signals.push_back({"pw3", 0x364, 4, 0, 16, true, false, 1.0, 0, "us"});
    signals.push_back({"pw4", 0x364, 6, 0, 16, true, false, 1.0, 0, "us"});

    // ═══════════════════════════════════════════════════════════════════════
    // LAMBDA (0x368) - O2 Sensors @ 15Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Lambda 1 (× 1000, so 1000 = stoich)
    // Bytes 2-3: Lambda 2 (× 1000)
    // Bytes 4-7: Reserved
    signals.push_back({"lambda1", 0x368, 0, 0, 16, true, false, 0.001, 0, ""});
    signals.push_back({"lambda2", 0x368, 2, 0, 16, true, false, 0.001, 0, ""});

    // ═══════════════════════════════════════════════════════════════════════
    // VSS (0x370) - Speed, Gear, VVT @ 15Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Vehicle speed (km/h × 10)
    // Byte 2: Reserved
    // Byte 3: Gear (0=N, 1-6=gears, 7=R in some configs)
    // Bytes 4-5: VVT Intake angle (degrees × 10, SIGNED)
    // Bytes 6-7: VVT Exhaust angle (degrees × 10, SIGNED)
    signals.push_back({"vehicle_speed", 0x370, 0, 0, 16, true, false, 0.1, 0, "km/h"});
    signals.push_back({"gear", 0x370, 3, 0, 8, true, false, 1.0, 0, ""});
    signals.push_back({"vvt_intake", 0x370, 4, 0, 16, true, true, 0.1, 0, "deg"});
    signals.push_back({"vvt_exhaust", 0x370, 6, 0, 16, true, true, 0.1, 0, "deg"});

    // ═══════════════════════════════════════════════════════════════════════
    // DATA4 (0x372) - Battery, Boost, Baro @ 10Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Reserved
    // Byte 1: Battery voltage (V × 10)
    // Bytes 2-3: Reserved
    // Bytes 4-5: Boost target (kPa × 10)
    // Bytes 6-7: Barometric pressure (kPa × 10)
    signals.push_back({"battery_voltage", 0x372, 1, 0, 8, true, false, 0.1, 0, "V"});
    signals.push_back({"boost_target", 0x372, 4, 0, 16, true, false, 0.1, 0, "kPa"});
    signals.push_back({"baro", 0x372, 6, 0, 16, true, false, 0.1, 0, "kPa"});

    // ═══════════════════════════════════════════════════════════════════════
    // DATA5 (0x3E0) - Temperatures @ 10Hz (Kelvin × 10)
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Coolant temp (Kelvin × 10)
    // Bytes 2-3: Intake air temp (Kelvin × 10)
    // Bytes 4-5: Fuel temp (Kelvin × 10)
    // Bytes 6-7: Reserved
    //
    // Conversion: Celsius = (raw / 10) - 273.15
    signals.push_back({"coolant_temp", 0x3E0, 0, 0, 16, true, false, 0.1, -273.15, "C"});
    signals.push_back({"intake_temp", 0x3E0, 2, 0, 16, true, false, 0.1, -273.15, "C"});
    signals.push_back({"fuel_temp", 0x3E0, 4, 0, 16, true, false, 0.1, -273.15, "C"});

    return signals;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BMW E46/E39/E38 PT-CAN PROTOCOL (Little Endian - LSB First)
// Source: BMW PT-CAN documentation, compatible with original BMW clusters
// ═══════════════════════════════════════════════════════════════════════════════
//
// Temperature formula: temp_C = ((raw × 3) / 4) - 48
//                      Or: temp_C = raw × 0.75 - 48
//
// RPM formula: rpm = raw / 6.4 = raw × 0.15625
//
// TPS formula: tps_pct = map(raw, 1, 254, 0, 100) ≈ raw × 0.392157

std::vector<CanSignalDef> createBMWSignals() {
    std::vector<CanSignalDef> signals;

    // ═══════════════════════════════════════════════════════════════════════
    // DME1 (0x316) - Engine Status @ 30Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Status flags (0x05 = engine running)
    //         Bit 0: Engine running
    //         Bit 2: Motor status OK
    // Byte 1: Indexed torque (0x0C typical)
    // Bytes 2-3: RPM × 6.4 (Little Endian)
    // Byte 4: Indicated torque
    // Byte 5: Torque loss
    // Byte 6: Reserved
    // Byte 7: Theoretical torque
    signals.push_back({"rpm", 0x316, 2, 0, 16, false, false, 0.15625, 0, "rpm"});
    signals.push_back({"torque_indexed", 0x316, 1, 0, 8, false, false, 1.0, 0, "%"});
    signals.push_back({"torque_indicated", 0x316, 4, 0, 8, false, false, 1.0, 0, "%"});
    signals.push_back({"torque_loss", 0x316, 5, 0, 8, false, false, 1.0, 0, "%"});
    // Status flags as individual bits
    signals.push_back({"engine_running", 0x316, 0, 0, 1, false, false, 1.0, 0, ""});
    signals.push_back({"motor_status_ok", 0x316, 0, 2, 1, false, false, 1.0, 0, ""});

    // ═══════════════════════════════════════════════════════════════════════
    // DME2 (0x329) - Temperature and Throttle @ 30Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Multiplexed info (0x11 typical)
    // Byte 1: Coolant temperature ((CLT + 48) × 4 / 3)
    // Byte 2: Barometric pressure (kPa direct)
    // Byte 3: Status flags
    //         Bit 3 (0x08): Motor running
    // Byte 4: Virtual TPS cruise (not used)
    // Byte 5: TPS (1-254 maps to 0-100%)
    // Byte 6: Brake status flags
    // Byte 7: Reserved
    signals.push_back({"coolant_temp", 0x329, 1, 0, 8, false, false, 0.75, -48.0, "C"});
    signals.push_back({"baro", 0x329, 2, 0, 8, false, false, 1.0, 0, "kPa"});
    signals.push_back({"tps", 0x329, 5, 0, 8, false, false, 0.392157, 0, "%"});
    // Status flags
    signals.push_back({"motor_running_329", 0x329, 3, 3, 1, false, false, 1.0, 0, ""});
    signals.push_back({"brake_pressed", 0x329, 6, 0, 1, false, false, 1.0, 0, ""});

    // ═══════════════════════════════════════════════════════════════════════
    // DME4 (0x545) - Diagnostics and Consumption @ 10Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: CEL/MIL status flags
    //         Bit 1 (0x02): MIL (Check Engine Light) ON
    //         Bit 4 (0x10): Cruise control active
    // Bytes 1-2: Fuel consumption (L/h × 100, Little Endian)
    // Byte 3: Overheat warning
    //         0x08: Overheat active (CLT > 120°C)
    // Byte 4: Oil temperature (same formula as coolant)
    // Bytes 5-7: Reserved
    signals.push_back({"cel_status", 0x545, 0, 1, 1, false, false, 1.0, 0, ""});
    signals.push_back({"cruise_active", 0x545, 0, 4, 1, false, false, 1.0, 0, ""});
    signals.push_back({"fuel_consumption", 0x545, 1, 0, 16, false, false, 0.01, 0, "L/h"});
    signals.push_back({"overheat_warning", 0x545, 3, 3, 1, false, false, 1.0, 0, ""});
    signals.push_back({"oil_temp", 0x545, 4, 0, 8, false, false, 0.75, -48.0, "C"});

    return signals;
}

// ═══════════════════════════════════════════════════════════════════════════════
// VAG PROTOCOL (VW/Audi - Gol Quadrado compatible)
// Little Endian - LSB First
// Source: VAG cluster CAN protocol documentation
// ═══════════════════════════════════════════════════════════════════════════════
//
// RPM formula: rpm = raw / 4 = raw × 0.25
// VSS formula: vss = raw / 133 ≈ raw × 0.0075188
//
// Note: VAG protocol is simpler than BMW, provides only essential data

std::vector<CanSignalDef> createVAGSignals() {
    std::vector<CanSignalDef> signals;

    // ═══════════════════════════════════════════════════════════════════════
    // RPM (0x280) - Engine Speed @ 30Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Header (0x49)
    // Byte 1: Header (0x0E)
    // Bytes 2-3: RPM × 4 (Little Endian)
    // Byte 4: Footer (0x0E)
    // Byte 5: Footer (0x00)
    // Byte 6: Footer (0x1B)
    // Byte 7: Footer (0x0E)
    signals.push_back({"rpm", 0x280, 2, 0, 16, false, false, 0.25, 0, "rpm"});

    // ═══════════════════════════════════════════════════════════════════════
    // VSS (0x5A0) - Vehicle Speed @ 30Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Header (0xFF)
    // Bytes 1-2: Speed × 133 (Little Endian)
    // Bytes 3-6: Reserved (0x00)
    // Byte 7: Footer (0xAD)
    signals.push_back({"vehicle_speed", 0x5A0, 1, 0, 16, false, false, 0.0075188, 0, "km/h"});

    return signals;
}

std::vector<CanSignalDef> getSignalsForProtocol(const std::string& protocol) {
    if (protocol == "haltech") {
        return createHaltechSignals();
    } else if (protocol == "bmw") {
        return createBMWSignals();
    } else if (protocol == "vag") {
        return createVAGSignals();
    } else {
        LOG_WARN("Unknown protocol '" + protocol + "', defaulting to Haltech");
        return createHaltechSignals();
    }
}

} // namespace speeduino
