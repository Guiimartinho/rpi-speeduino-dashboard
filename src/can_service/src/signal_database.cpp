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
    signals.push_back({"clutch_pressed", 0x329, 6, 1, 1, false, false, 1.0, 0, ""});

    // ═══════════════════════════════════════════════════════════════════════
    // DME4 (0x545) - Diagnostics and Consumption @ 5Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Bytes 0-1: Oil Temperature (12-bit, (OilTemp + 48) × 4)
    //            Decode: ((byte[1] << 8 | byte[0]) / 4) - 48
    // Byte 2: Coolant Temp backup (same as 0x329)
    // Byte 3: Ambient Temperature ((Temp + 48))
    // Bytes 4-5: Fuel Consumption (L/h × 10, Little Endian)
    // Byte 6: CEL Status (bit 0 = CEL ON)
    // Byte 7: Cruise Status (0x00 typically)
    signals.push_back({"oil_temp", 0x545, 0, 0, 16, false, false, 0.25, -48.0, "C"});
    signals.push_back({"coolant_temp_545", 0x545, 2, 0, 8, false, false, 0.75, -48.0, "C"});
    signals.push_back({"ambient_temp", 0x545, 3, 0, 8, false, false, 1.0, -48.0, "C"});
    signals.push_back({"fuel_consumption", 0x545, 4, 0, 16, false, false, 0.1, 0, "L/h"});
    signals.push_back({"cel_status", 0x545, 6, 0, 1, false, false, 1.0, 0, ""});
    signals.push_back({"cruise_status", 0x545, 7, 0, 8, false, false, 1.0, 0, ""});

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

// ═══════════════════════════════════════════════════════════════════════════════
// SPEEDUINO NATIVE CAN PROTOCOL (Little Endian - LSB First)
// Source: SCG-ECU 2.0 CAN Dashboard Master Reference (Jan 2026)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Primary engine data broadcast for HMI display
// Messages: 0x3E0, 0x3E1, 0x3E2, 0x3E3, 0x370
//
// Note: This is separate from Haltech signals which use different message IDs

std::vector<CanSignalDef> createSpeeduinoSignals() {
    std::vector<CanSignalDef> signals;

    // ═══════════════════════════════════════════════════════════════════════
    // 0x3E0 - Engine Data 1 @ 20Hz (Core engine data)
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0-1: RPM (direct, Little Endian)
    // Byte 2: MAP (kPa direct)
    // Byte 3: TPS × 2
    // Byte 4: CLT + 40
    // Byte 5: IAT + 40
    // Byte 6: Battery × 10
    // Byte 7: AFR Target × 10
    signals.push_back({"rpm", 0x3E0, 0, 0, 16, false, false, 1.0, 0, "rpm"});
    signals.push_back({"map", 0x3E0, 2, 0, 8, false, false, 1.0, 0, "kPa"});
    signals.push_back({"tps", 0x3E0, 3, 0, 8, false, false, 0.5, 0, "%"});
    signals.push_back({"coolant_temp", 0x3E0, 4, 0, 8, false, false, 1.0, -40.0, "C"});
    signals.push_back({"intake_temp", 0x3E0, 5, 0, 8, false, false, 1.0, -40.0, "C"});
    signals.push_back({"battery_voltage", 0x3E0, 6, 0, 8, false, false, 0.1, 0, "V"});
    signals.push_back({"afr_target", 0x3E0, 7, 0, 8, false, false, 0.1, 0, "AFR"});

    // ═══════════════════════════════════════════════════════════════════════
    // 0x3E1 - Engine Data 2 @ 20Hz (AFR, Ignition, Pressures)
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: AFR Actual × 10
    // Byte 1: Ignition Advance × 2
    // Byte 2-3: Fuel PW × 10 (ms, Little Endian)
    // Byte 4: VE (%)
    // Byte 5: Fuel Pressure × 10 (bar)
    // Byte 6: Oil Pressure × 10 (bar)
    // Byte 7: Boost Target (kPa)
    signals.push_back({"lambda1", 0x3E1, 0, 0, 8, false, false, 0.1, 0, "AFR"});
    signals.push_back({"ignition_advance", 0x3E1, 1, 0, 8, false, false, 0.5, 0, "deg"});
    signals.push_back({"pw1", 0x3E1, 2, 0, 16, false, false, 0.1, 0, "ms"});
    signals.push_back({"ve", 0x3E1, 4, 0, 8, false, false, 1.0, 0, "%"});
    signals.push_back({"fuel_pressure", 0x3E1, 5, 0, 8, false, false, 10.0, 0, "kPa"});  // bar×10 → kPa
    signals.push_back({"oil_pressure", 0x3E1, 6, 0, 8, false, false, 10.0, 0, "kPa"});   // bar×10 → kPa
    signals.push_back({"boost_target", 0x3E1, 7, 0, 8, false, false, 1.0, 0, "kPa"});

    // ═══════════════════════════════════════════════════════════════════════
    // 0x3E2 - Engine Status @ 10Hz (Status flags and idle control)
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0: Status flags 1
    //   Bit 0: Engine Running
    //   Bit 1: Cranking
    //   Bit 2: ASE Active
    //   Bit 3: Warmup Active
    //   Bit 4: Accel Enrich
    //   Bit 5: Decel Fuel Cut (DFCO)
    //   Bit 6: Rev Limiter
    //   Bit 7: Boost Cut
    // Byte 1: Status flags 2
    //   Bit 0: Fan On
    //   Bit 1: Fuel Pump
    //   Bit 2: CEL (Check Engine)
    //   Bit 3: EGO Heating
    //   Bit 4: Launch Control
    //   Bit 5: Flat Shift
    //   Bit 6: Nitrous Stage 1
    //   Bit 7: Nitrous Stage 2
    // Byte 2: Error Count
    // Byte 3: Sync Status
    // Byte 4: Idle Target RPM / 10
    // Byte 5: Idle Valve Duty
    signals.push_back({"engine_running", 0x3E2, 0, 0, 1, false, false, 1.0, 0, ""});
    signals.push_back({"cranking", 0x3E2, 0, 1, 1, false, false, 1.0, 0, ""});
    signals.push_back({"ase_active", 0x3E2, 0, 2, 1, false, false, 1.0, 0, ""});
    signals.push_back({"warmup_active", 0x3E2, 0, 3, 1, false, false, 1.0, 0, ""});
    signals.push_back({"accel_enrich", 0x3E2, 0, 4, 1, false, false, 1.0, 0, ""});
    signals.push_back({"dfco_active", 0x3E2, 0, 5, 1, false, false, 1.0, 0, ""});
    signals.push_back({"rev_limit_active", 0x3E2, 0, 6, 1, false, false, 1.0, 0, ""});
    signals.push_back({"boost_cut_active", 0x3E2, 0, 7, 1, false, false, 1.0, 0, ""});
    signals.push_back({"fan_on", 0x3E2, 1, 0, 1, false, false, 1.0, 0, ""});
    signals.push_back({"fuel_pump_on", 0x3E2, 1, 1, 1, false, false, 1.0, 0, ""});
    signals.push_back({"cel_flag", 0x3E2, 1, 2, 1, false, false, 1.0, 0, ""});
    signals.push_back({"ego_heating", 0x3E2, 1, 3, 1, false, false, 1.0, 0, ""});
    signals.push_back({"launch_active", 0x3E2, 1, 4, 1, false, false, 1.0, 0, ""});
    signals.push_back({"flat_shift_active", 0x3E2, 1, 5, 1, false, false, 1.0, 0, ""});
    signals.push_back({"nitrous_stage1", 0x3E2, 1, 6, 1, false, false, 1.0, 0, ""});
    signals.push_back({"nitrous_stage2", 0x3E2, 1, 7, 1, false, false, 1.0, 0, ""});
    signals.push_back({"error_count", 0x3E2, 2, 0, 8, false, false, 1.0, 0, ""});
    signals.push_back({"sync_status", 0x3E2, 3, 0, 8, false, false, 1.0, 0, ""});
    signals.push_back({"idle_target_rpm", 0x3E2, 4, 0, 8, false, false, 10.0, 0, "rpm"});
    signals.push_back({"idle_valve_duty", 0x3E2, 5, 0, 8, false, false, 1.0, 0, "%"});

    // ═══════════════════════════════════════════════════════════════════════
    // 0x3E3 - Fuel/Ignition Trims @ 5Hz (Per-cylinder corrections)
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0-3: Fuel trim per cylinder (raw - 100 = % correction)
    // Byte 4-7: Ignition trim per cylinder ((raw - 128) / 2 = degrees)
    signals.push_back({"fuel_trim_cyl1", 0x3E3, 0, 0, 8, false, false, 1.0, -100.0, "%"});
    signals.push_back({"fuel_trim_cyl2", 0x3E3, 1, 0, 8, false, false, 1.0, -100.0, "%"});
    signals.push_back({"fuel_trim_cyl3", 0x3E3, 2, 0, 8, false, false, 1.0, -100.0, "%"});
    signals.push_back({"fuel_trim_cyl4", 0x3E3, 3, 0, 8, false, false, 1.0, -100.0, "%"});
    signals.push_back({"ign_trim_cyl1", 0x3E3, 4, 0, 8, false, false, 0.5, -64.0, "deg"});
    signals.push_back({"ign_trim_cyl2", 0x3E3, 5, 0, 8, false, false, 0.5, -64.0, "deg"});
    signals.push_back({"ign_trim_cyl3", 0x3E3, 6, 0, 8, false, false, 0.5, -64.0, "deg"});
    signals.push_back({"ign_trim_cyl4", 0x3E3, 7, 0, 8, false, false, 0.5, -64.0, "deg"});

    // ═══════════════════════════════════════════════════════════════════════
    // 0x370 - VSS @ 15Hz (Vehicle speed and odometer)
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0-1: Speed × 100 (km/h, Little Endian)
    // Byte 2-5: Odometer (km, Little Endian)
    signals.push_back({"vehicle_speed", 0x370, 0, 0, 16, false, false, 0.01, 0, "km/h"});
    signals.push_back({"odometer", 0x370, 2, 0, 32, false, false, 1.0, 0, "km"});

    return signals;
}

// ═══════════════════════════════════════════════════════════════════════════════
// WIDEBAND CONTROLLER (AEM X-Series) - Little Endian
// Source: AEM X-Series CAN Protocol Documentation
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<CanSignalDef> createWidebandSignals() {
    std::vector<CanSignalDef> signals;

    // ═══════════════════════════════════════════════════════════════════════
    // 0x180 - Wideband Data @ 50Hz
    // ═══════════════════════════════════════════════════════════════════════
    // Byte 0-1: Lambda × 10000 (Little Endian)
    // Byte 2: O2 % × 100
    // Byte 3: Status (0=warmup, 1=ok, 2=error)
    // Byte 4: Heater Duty (0-100%)
    // Byte 5: Sensor Temp (°C)
    signals.push_back({"wideband_lambda", 0x180, 0, 0, 16, false, false, 0.0001, 0, ""});
    signals.push_back({"wideband_o2", 0x180, 2, 0, 8, false, false, 0.01, 0, "%"});
    signals.push_back({"wideband_status", 0x180, 3, 0, 8, false, false, 1.0, 0, ""});
    signals.push_back({"wideband_heater", 0x180, 4, 0, 8, false, false, 1.0, 0, "%"});
    signals.push_back({"wideband_temp", 0x180, 5, 0, 8, false, false, 1.0, 0, "C"});

    return signals;
}

std::vector<CanSignalDef> getSignalsForProtocol(const std::string& protocol) {
    if (protocol == "haltech") {
        return createHaltechSignals();
    } else if (protocol == "bmw") {
        return createBMWSignals();
    } else if (protocol == "vag") {
        return createVAGSignals();
    } else if (protocol == "speeduino") {
        return createSpeeduinoSignals();
    } else if (protocol == "wideband") {
        return createWidebandSignals();
    } else if (protocol == "speeduino_full") {
        // Combined Speeduino + Wideband for complete dashboard support
        auto signals = createSpeeduinoSignals();
        auto wideband = createWidebandSignals();
        signals.insert(signals.end(), wideband.begin(), wideband.end());
        return signals;
    } else {
        LOG_WARN("Unknown protocol '" + protocol + "', defaulting to Speeduino");
        return createSpeeduinoSignals();
    }
}

}  // namespace speeduino
