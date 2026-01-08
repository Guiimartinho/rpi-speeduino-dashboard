#include "can_service/can_parser.hpp"
#include "common/logger.hpp"

// This file provides the signal database implementation
// Additional signal-related functionality can be added here

namespace speeduino {

// Factory function to create default signal sets

std::vector<CanSignalDef> createHaltechSignals() {
    std::vector<CanSignalDef> signals;

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

    // DATA5 (0x3E0) - Temps @ 10Hz (Kelvin x 10)
    signals.push_back({"coolant_temp", 0x3E0, 0, 0, 16, true, false, 0.1, -273.0, "C"});
    signals.push_back({"intake_temp", 0x3E0, 2, 0, 16, true, false, 0.1, -273.0, "C"});
    signals.push_back({"fuel_temp", 0x3E0, 4, 0, 16, true, false, 0.1, -273.0, "C"});

    return signals;
}

std::vector<CanSignalDef> createBMWSignals() {
    std::vector<CanSignalDef> signals;

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

    // CEL flag from DME4 byte 0
    signals.push_back({"cel_status", 0x545, 0, 1, 1, true, false, 1.0, 0, ""});

    return signals;
}

std::vector<CanSignalDef> createVAGSignals() {
    std::vector<CanSignalDef> signals;

    // RPM (0x280) @ 30Hz
    signals.push_back({"rpm", 0x280, 2, 0, 16, false, false, 0.25, 0, "rpm"});

    // VSS (0x5A0) @ 30Hz
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
