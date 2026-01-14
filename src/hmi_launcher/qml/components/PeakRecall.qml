import QtQuick

/**
 * PeakRecall.qml - Peak/Min-Max value tracking and display
 *
 * Features:
 * - Tracks maximum and minimum values for engine parameters
 * - Session timer with duration display
 * - Reset capability with visual feedback
 * - Warning indicators for dangerous values
 * - Optimized for 60fps performance
 */
Item {
    id: root

    // Input values to track
    property real rpm: 0
    property real speed: 0
    property real coolantTemp: 0
    property real oilTemp: 0
    property real oilPressure: 0
    property real boostPsi: 0
    property real afr: 14.7
    property real iat: 0
    property real batteryVoltage: 12.0
    property real egt: 0

    // Peak values (max)
    property real peakRpm: 0
    property real peakSpeed: 0
    property real peakCoolantTemp: 0
    property real peakOilTemp: 0
    property real peakBoostPsi: 0
    property real peakIat: 0
    property real peakEgt: 0

    // Min values
    property real minOilPressure: 999
    property real minAfr: 20
    property real maxAfr: 10
    property real minBatteryVoltage: 15

    // Session tracking
    property int sessionStartTime: 0
    property bool isTracking: true
    property int sessionDuration: 0

    // Warning thresholds
    property real coolantWarningThreshold: 100
    property real oilTempWarningThreshold: 120
    property real oilPressureWarningThreshold: 20
    property real afrLeanWarningThreshold: 16
    property real afrRichWarningThreshold: 11.5
    property real batteryWarningThreshold: 12
    property real egtWarningThreshold: 900

    // Update peaks when values change
    onRpmChanged: if (isTracking && rpm > peakRpm) peakRpm = rpm
    onSpeedChanged: if (isTracking && speed > peakSpeed) peakSpeed = speed
    onCoolantTempChanged: if (isTracking && coolantTemp > peakCoolantTemp) peakCoolantTemp = coolantTemp
    onOilTempChanged: if (isTracking && oilTemp > peakOilTemp) peakOilTemp = oilTemp
    onBoostPsiChanged: if (isTracking && boostPsi > peakBoostPsi) peakBoostPsi = boostPsi
    onIatChanged: if (isTracking && iat > peakIat) peakIat = iat
    onEgtChanged: if (isTracking && egt > peakEgt) peakEgt = egt

    onOilPressureChanged: if (isTracking && oilPressure < minOilPressure && oilPressure > 0) minOilPressure = oilPressure
    onAfrChanged: {
        if (isTracking && afr > 0) {
            if (afr < minAfr) minAfr = afr
            if (afr > maxAfr) maxAfr = afr
        }
    }
    onBatteryVoltageChanged: if (isTracking && batteryVoltage < minBatteryVoltage && batteryVoltage > 0) minBatteryVoltage = batteryVoltage

    // Reset all peaks
    function resetPeaks() {
        peakRpm = 0
        peakSpeed = 0
        peakCoolantTemp = coolantTemp
        peakOilTemp = oilTemp
        peakBoostPsi = boostPsi
        peakIat = iat
        peakEgt = egt
        minOilPressure = oilPressure > 0 ? oilPressure : 999
        minAfr = afr > 0 ? afr : 20
        maxAfr = afr > 0 ? afr : 10
        minBatteryVoltage = batteryVoltage > 0 ? batteryVoltage : 15
        sessionStartTime = Date.now()
        sessionDuration = 0
    }

    // Toggle tracking
    function toggleTracking() {
        isTracking = !isTracking
    }

    // Format duration as HH:MM:SS
    function formatDuration(seconds) {
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = seconds % 60
        return (h > 0 ? h + ":" : "") +
               (m < 10 ? "0" : "") + m + ":" +
               (s < 10 ? "0" : "") + s
    }

    Component.onCompleted: {
        sessionStartTime = Date.now()
    }

    // Session timer
    Timer {
        interval: 1000
        running: isTracking
        repeat: true
        onTriggered: {
            sessionDuration = Math.floor((Date.now() - sessionStartTime) / 1000)
        }
    }

    // Visual display
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 12
        border.color: isTracking ? "#00E676" : "#666666"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "PEAK RECALL"
                    color: "#00E676"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: parent.width - 280; height: 1 }  // Spacer

                // Session timer
                Text {
                    text: formatDuration(sessionDuration)
                    color: isTracking ? Qt.rgba(1, 1, 1, 0.72) : Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 12
                    font.family: "Roboto Mono, monospace"
                }

                // Pause/Resume button
                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    color: pauseMouseArea.pressed ? "#444444" : "#333333"

                    Text {
                        anchors.centerIn: parent
                        text: isTracking ? "\u275A\u275A" : "\u25B6"
                        color: isTracking ? "#FFC107" : "#00E676"
                        font.pixelSize: 10
                    }

                    MouseArea {
                        id: pauseMouseArea
                        anchors.fill: parent
                        onClicked: toggleTracking()
                    }
                }

                // Reset button
                Rectangle {
                    width: 60
                    height: 24
                    radius: 4
                    color: resetMouseArea.pressed ? "#FF5252" : "#333333"

                    Text {
                        anchors.centerIn: parent
                        text: "RESET"
                        color: Qt.rgba(1, 1, 1, 0.87)
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        id: resetMouseArea
                        anchors.fill: parent
                        onClicked: root.resetPeaks()
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#333333"
            }

            // Peak values grid
            Grid {
                width: parent.width
                columns: 2
                rowSpacing: 6
                columnSpacing: 16

                PeakItem {
                    label: "MAX RPM"
                    value: peakRpm.toFixed(0)
                    unit: ""
                    isMax: true
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX SPEED"
                    value: peakSpeed.toFixed(0)
                    unit: "km/h"
                    isMax: true
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX CLT"
                    value: peakCoolantTemp.toFixed(0)
                    unit: "\u00B0C"
                    isMax: true
                    isWarning: peakCoolantTemp > coolantWarningThreshold
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX OIL TEMP"
                    value: peakOilTemp.toFixed(0)
                    unit: "\u00B0C"
                    isMax: true
                    isWarning: peakOilTemp > oilTempWarningThreshold
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MIN OIL PSI"
                    value: minOilPressure < 999 ? minOilPressure.toFixed(0) : "--"
                    unit: "psi"
                    isMax: false
                    isWarning: minOilPressure < oilPressureWarningThreshold && minOilPressure < 999
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX BOOST"
                    value: peakBoostPsi.toFixed(1)
                    unit: "psi"
                    isMax: true
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "AFR RANGE"
                    value: (minAfr < 20 ? minAfr.toFixed(1) : "--") + "-" + (maxAfr > 10 ? maxAfr.toFixed(1) : "--")
                    unit: ""
                    isMax: true
                    isWarning: minAfr < afrRichWarningThreshold || maxAfr > afrLeanWarningThreshold
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MIN BATT"
                    value: minBatteryVoltage < 15 ? minBatteryVoltage.toFixed(1) : "--"
                    unit: "V"
                    isMax: false
                    isWarning: minBatteryVoltage < batteryWarningThreshold
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX IAT"
                    value: peakIat.toFixed(0)
                    unit: "\u00B0C"
                    isMax: true
                    isWarning: peakIat > 60
                    width: (parent.width - parent.columnSpacing) / 2
                }

                PeakItem {
                    label: "MAX EGT"
                    value: peakEgt > 0 ? peakEgt.toFixed(0) : "--"
                    unit: "\u00B0C"
                    isMax: true
                    isWarning: peakEgt > egtWarningThreshold
                    width: (parent.width - parent.columnSpacing) / 2
                }
            }
        }
    }

    // Peak item component
    component PeakItem: Item {
        property string label: ""
        property string value: ""
        property string unit: ""
        property bool isMax: true
        property bool isWarning: false

        height: 32

        Column {
            anchors.fill: parent
            spacing: 2

            Text {
                text: label
                color: Qt.rgba(1, 1, 1, 0.56)
                font.pixelSize: 10
                font.family: "Roboto, sans-serif"
            }

            Row {
                spacing: 4

                Text {
                    text: isMax ? "\u25B2" : "\u25BC"
                    color: isWarning ? "#FF5252" : (isMax ? "#00E676" : "#FFC107")
                    font.pixelSize: 10
                }

                Text {
                    text: value
                    color: isWarning ? "#FF5252" : Qt.rgba(1, 1, 1, 0.96)
                    font.pixelSize: 16
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: unit
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 12
                    font.family: "Roboto, sans-serif"
                    visible: unit !== ""
                }
            }
        }
    }
}
