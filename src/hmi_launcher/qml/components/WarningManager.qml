pragma Singleton
import QtQuick

/**
 * WarningManager.qml - Centralized warning and alarm system
 *
 * Features:
 * - Configurable warning thresholds for all parameters
 * - Three severity levels: Warning, Critical, Emergency
 * - Active warning tracking with timestamps
 * - Warning history logging
 * - Audio alert triggers
 * - Visual alert states for UI binding
 */
QtObject {
    id: root

    // Warning severity levels
    enum Severity {
        None = 0,
        Info = 1,
        Warning = 2,
        Critical = 3,
        Emergency = 4
    }

    // Current input values (set by external binding)
    property real rpm: 0
    property real coolantTemp: 0
    property real oilTemp: 0
    property real oilPressure: 0
    property real fuelPressure: 0
    property real batteryVoltage: 0
    property real afr: 14.7
    property real iat: 0
    property real boostPsi: 0
    property real egt: 0
    property bool celOn: false
    property bool knockDetected: false
    property int gear: 0
    property real speed: 0

    // Threshold configuration
    property var thresholds: ({
        rpm: { warning: 6500, critical: 7000, emergency: 7500 },
        coolantTemp: { warning: 95, critical: 105, emergency: 115 },
        oilTemp: { warning: 110, critical: 120, emergency: 130 },
        oilPressure: { warningLow: 25, criticalLow: 15, emergencyLow: 10 },
        fuelPressure: { warningLow: 35, criticalLow: 30, emergencyLow: 25 },
        batteryVoltage: { warningLow: 12.5, criticalLow: 11.8, warningHigh: 15.0, criticalHigh: 15.5 },
        afr: { warningLean: 15.5, criticalLean: 16.5, warningRich: 11.0, criticalRich: 10.0 },
        iat: { warning: 50, critical: 60, emergency: 70 },
        boostPsi: { warning: 15, critical: 18, emergency: 22 },
        egt: { warning: 850, critical: 950, emergency: 1050 }
    })

    // Active warnings list
    property var activeWarnings: []
    property int activeWarningCount: activeWarnings.length
    property int highestSeverity: 0

    // Warning history (for logging)
    property var warningHistory: []
    property int maxHistorySize: 100

    // Individual warning states (for easy UI binding)
    property bool rpmWarning: false
    property bool rpmCritical: false
    property bool coolantWarning: false
    property bool coolantCritical: false
    property bool oilTempWarning: false
    property bool oilTempCritical: false
    property bool oilPressWarning: false
    property bool oilPressCritical: false
    property bool fuelPressWarning: false
    property bool fuelPressCritical: false
    property bool batteryWarning: false
    property bool batteryCritical: false
    property bool afrWarning: false
    property bool afrCritical: false
    property bool iatWarning: false
    property bool boostWarning: false
    property bool boostCritical: false
    property bool egtWarning: false
    property bool egtCritical: false

    // Aggregate states
    property bool anyWarning: activeWarningCount > 0
    property bool anyCritical: highestSeverity >= 3
    property bool anyEmergency: highestSeverity >= 4

    // Signals for external handlers
    signal warningTriggered(string parameter, int severity, string message)
    signal warningCleared(string parameter)
    signal emergencyAlert(string message)

    // Check all warnings
    function checkWarnings() {
        var newWarnings = []
        highestSeverity = 0

        // RPM check
        rpmWarning = rpm >= thresholds.rpm.warning && rpm < thresholds.rpm.critical
        rpmCritical = rpm >= thresholds.rpm.critical
        if (rpm >= thresholds.rpm.emergency) {
            addWarning(newWarnings, "RPM", 4, "RPM at " + rpm.toFixed(0) + " - OVER REV!")
        } else if (rpmCritical) {
            addWarning(newWarnings, "RPM", 3, "RPM at " + rpm.toFixed(0) + " - Shift!")
        } else if (rpmWarning) {
            addWarning(newWarnings, "RPM", 2, "High RPM: " + rpm.toFixed(0))
        }

        // Coolant temperature check
        coolantWarning = coolantTemp >= thresholds.coolantTemp.warning && coolantTemp < thresholds.coolantTemp.critical
        coolantCritical = coolantTemp >= thresholds.coolantTemp.critical
        if (coolantTemp >= thresholds.coolantTemp.emergency) {
            addWarning(newWarnings, "CLT", 4, "OVERHEAT! " + coolantTemp.toFixed(0) + "\u00B0C")
        } else if (coolantCritical) {
            addWarning(newWarnings, "CLT", 3, "Coolant critical: " + coolantTemp.toFixed(0) + "\u00B0C")
        } else if (coolantWarning) {
            addWarning(newWarnings, "CLT", 2, "Coolant high: " + coolantTemp.toFixed(0) + "\u00B0C")
        }

        // Oil temperature check
        oilTempWarning = oilTemp >= thresholds.oilTemp.warning && oilTemp < thresholds.oilTemp.critical
        oilTempCritical = oilTemp >= thresholds.oilTemp.critical
        if (oilTemp >= thresholds.oilTemp.emergency) {
            addWarning(newWarnings, "OIL_TEMP", 4, "Oil overheat! " + oilTemp.toFixed(0) + "\u00B0C")
        } else if (oilTempCritical) {
            addWarning(newWarnings, "OIL_TEMP", 3, "Oil temp critical: " + oilTemp.toFixed(0) + "\u00B0C")
        } else if (oilTempWarning) {
            addWarning(newWarnings, "OIL_TEMP", 2, "Oil temp high: " + oilTemp.toFixed(0) + "\u00B0C")
        }

        // Oil pressure check (low values are bad)
        oilPressWarning = oilPressure > 0 && oilPressure <= thresholds.oilPressure.warningLow && oilPressure > thresholds.oilPressure.criticalLow
        oilPressCritical = oilPressure > 0 && oilPressure <= thresholds.oilPressure.criticalLow
        if (oilPressure > 0 && oilPressure <= thresholds.oilPressure.emergencyLow && rpm > 1000) {
            addWarning(newWarnings, "OIL_PRESS", 4, "OIL PRESSURE EMERGENCY! " + oilPressure.toFixed(0) + " psi")
        } else if (oilPressCritical && rpm > 1000) {
            addWarning(newWarnings, "OIL_PRESS", 3, "Low oil pressure: " + oilPressure.toFixed(0) + " psi")
        } else if (oilPressWarning && rpm > 1000) {
            addWarning(newWarnings, "OIL_PRESS", 2, "Oil pressure low: " + oilPressure.toFixed(0) + " psi")
        }

        // Battery voltage check
        batteryWarning = batteryVoltage < thresholds.batteryVoltage.warningLow ||
                         batteryVoltage > thresholds.batteryVoltage.warningHigh
        batteryCritical = batteryVoltage < thresholds.batteryVoltage.criticalLow ||
                          batteryVoltage > thresholds.batteryVoltage.criticalHigh
        if (batteryCritical) {
            addWarning(newWarnings, "BATTERY", 3, "Battery voltage: " + batteryVoltage.toFixed(1) + "V")
        } else if (batteryWarning) {
            addWarning(newWarnings, "BATTERY", 2, "Battery: " + batteryVoltage.toFixed(1) + "V")
        }

        // AFR check
        afrWarning = afr > thresholds.afr.warningLean || afr < thresholds.afr.warningRich
        afrCritical = afr > thresholds.afr.criticalLean || afr < thresholds.afr.criticalRich
        if (afrCritical && rpm > 2000) {
            var afrMsg = afr > thresholds.afr.criticalLean ? "TOO LEAN!" : "TOO RICH!"
            addWarning(newWarnings, "AFR", 3, afrMsg + " AFR: " + afr.toFixed(1))
        } else if (afrWarning && rpm > 2000) {
            addWarning(newWarnings, "AFR", 2, "AFR: " + afr.toFixed(1))
        }

        // IAT check
        iatWarning = iat >= thresholds.iat.warning
        if (iat >= thresholds.iat.emergency) {
            addWarning(newWarnings, "IAT", 3, "Intake air too hot: " + iat.toFixed(0) + "\u00B0C")
        } else if (iatWarning) {
            addWarning(newWarnings, "IAT", 2, "High IAT: " + iat.toFixed(0) + "\u00B0C")
        }

        // Boost check
        boostWarning = boostPsi >= thresholds.boostPsi.warning && boostPsi < thresholds.boostPsi.critical
        boostCritical = boostPsi >= thresholds.boostPsi.critical
        if (boostPsi >= thresholds.boostPsi.emergency) {
            addWarning(newWarnings, "BOOST", 4, "OVERBOOST! " + boostPsi.toFixed(1) + " psi")
        } else if (boostCritical) {
            addWarning(newWarnings, "BOOST", 3, "High boost: " + boostPsi.toFixed(1) + " psi")
        } else if (boostWarning) {
            addWarning(newWarnings, "BOOST", 2, "Boost: " + boostPsi.toFixed(1) + " psi")
        }

        // EGT check
        egtWarning = egt >= thresholds.egt.warning && egt < thresholds.egt.critical
        egtCritical = egt >= thresholds.egt.critical
        if (egt >= thresholds.egt.emergency) {
            addWarning(newWarnings, "EGT", 4, "EGT EMERGENCY! " + egt.toFixed(0) + "\u00B0C")
        } else if (egtCritical) {
            addWarning(newWarnings, "EGT", 3, "High EGT: " + egt.toFixed(0) + "\u00B0C")
        } else if (egtWarning) {
            addWarning(newWarnings, "EGT", 2, "EGT elevated: " + egt.toFixed(0) + "\u00B0C")
        }

        // CEL check
        if (celOn) {
            addWarning(newWarnings, "CEL", 2, "Check Engine Light")
        }

        // Knock detection
        if (knockDetected) {
            addWarning(newWarnings, "KNOCK", 3, "Knock detected!")
        }

        // Update active warnings and emit signals
        processWarningChanges(newWarnings)
    }

    function addWarning(list, param, severity, message) {
        list.push({
            parameter: param,
            severity: severity,
            message: message,
            timestamp: Date.now()
        })
        if (severity > highestSeverity) {
            highestSeverity = severity
        }
    }

    function processWarningChanges(newWarnings) {
        // Find new warnings
        for (var i = 0; i < newWarnings.length; i++) {
            var found = false
            for (var j = 0; j < activeWarnings.length; j++) {
                if (activeWarnings[j].parameter === newWarnings[i].parameter) {
                    found = true
                    break
                }
            }
            if (!found) {
                warningTriggered(newWarnings[i].parameter, newWarnings[i].severity, newWarnings[i].message)
                addToHistory(newWarnings[i])
                if (newWarnings[i].severity >= 4) {
                    emergencyAlert(newWarnings[i].message)
                }
            }
        }

        // Find cleared warnings
        for (var k = 0; k < activeWarnings.length; k++) {
            var stillActive = false
            for (var l = 0; l < newWarnings.length; l++) {
                if (activeWarnings[k].parameter === newWarnings[l].parameter) {
                    stillActive = true
                    break
                }
            }
            if (!stillActive) {
                warningCleared(activeWarnings[k].parameter)
            }
        }

        activeWarnings = newWarnings
    }

    function addToHistory(warning) {
        warningHistory.unshift({
            parameter: warning.parameter,
            severity: warning.severity,
            message: warning.message,
            timestamp: warning.timestamp
        })
        if (warningHistory.length > maxHistorySize) {
            warningHistory.pop()
        }
    }

    // Acknowledge/clear a specific warning
    function acknowledgeWarning(parameter) {
        for (var i = 0; i < activeWarnings.length; i++) {
            if (activeWarnings[i].parameter === parameter) {
                activeWarnings[i].acknowledged = true
                break
            }
        }
    }

    // Clear all warnings
    function clearAllWarnings() {
        activeWarnings = []
        highestSeverity = 0
    }

    // Clear history
    function clearHistory() {
        warningHistory = []
    }

    // Get severity color
    function getSeverityColor(severity) {
        switch (severity) {
            case 1: return "#2196F3"  // Info - Blue
            case 2: return "#FFC107"  // Warning - Amber
            case 3: return "#FF5252"  // Critical - Red
            case 4: return "#FF0000"  // Emergency - Bright Red
            default: return "#666666"
        }
    }

    // Get severity name
    function getSeverityName(severity) {
        switch (severity) {
            case 1: return "INFO"
            case 2: return "WARNING"
            case 3: return "CRITICAL"
            case 4: return "EMERGENCY"
            default: return "NONE"
        }
    }

    // Update threshold
    function setThreshold(parameter, type, value) {
        if (thresholds.hasOwnProperty(parameter)) {
            thresholds[parameter][type] = value
        }
    }

    // Timer for periodic checks
    property Timer checkTimer: Timer {
        interval: 100  // 10 Hz check rate
        running: true
        repeat: true
        onTriggered: checkWarnings()
    }
}
