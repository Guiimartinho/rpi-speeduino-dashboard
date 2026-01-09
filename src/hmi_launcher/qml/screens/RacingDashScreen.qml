import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State
import "../components" as Components

/**
 * RacingDashScreen.qml
 * Performance-focused dashboard for track use
 *
 * Features:
 * - Large speedometer and tachometer
 * - G-Force meter for cornering analysis
 * - Lap timer with sector times
 * - Performance meter (0-60, 1/4 mile)
 * - Warning system integration
 */
Item {
    id: racingDash

    // Engine data from DataProvider
    property var engineData: ({
        rpm: 0,
        coolantTemp: 0,
        intakeTemp: 0,
        tps: 0,
        mapKpa: 0,
        lambda: 1.0,
        ignitionAdvance: 0,
        injectorDuty: 0,
        vehicleSpeed: 0,
        gear: 0,
        canOk: false,
        celOn: false,
        overheat: false
    })

    // G-Force data (from accelerometer)
    property real lateralG: 0
    property real longitudinalG: 0

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Responsive layout
    property bool compactMode: width < 900

    // Main layout
    RowLayout {
        anchors.fill: parent
        anchors.margins: Styles.Theme.spacingSm
        spacing: Styles.Theme.spacingSm

        // Left column - Primary gauges
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.4
            spacing: Styles.Theme.spacingSm

            // Large RPM gauge
            Components.SafeGauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.55

                label: "RPM"
                unit: ""
                value: racingDash.engineData.rpm
                minValue: 0
                maxValue: 8000
                warningThreshold: 6500
                criticalThreshold: 7500
                precision: 0
                gaugeColor: Styles.Theme.accentPrimary
                showArc: true
                arcStartAngle: -140
                arcEndAngle: 140
                isPrimary: true

                // Shift light indicator
                Rectangle {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    width: 60
                    height: 20
                    radius: 4
                    color: racingDash.engineData.rpm > 6500 ?
                           (racingDash.engineData.rpm > 7200 ? "#FF0000" : "#FFC107") :
                           "#333333"
                    visible: racingDash.engineData.rpm > 5500

                    Text {
                        anchors.centerIn: parent
                        text: "SHIFT"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    SequentialAnimation on opacity {
                        running: racingDash.engineData.rpm > 7000
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 100 }
                        NumberAnimation { to: 1.0; duration: 100 }
                    }
                }
            }

            // Speed and Gear display
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Styles.Theme.spacingSm

                // Speed
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    label: "SPEED"
                    unit: "km/h"
                    value: racingDash.engineData.vehicleSpeed
                    minValue: 0
                    maxValue: 280
                    precision: 0
                    gaugeColor: Styles.Theme.accentSecondary
                    showArc: true
                    isPrimary: true
                }

                // Large gear indicator
                Rectangle {
                    Layout.preferredWidth: 80
                    Layout.fillHeight: true
                    color: "#1a1a1a"
                    radius: 12
                    border.color: "#333333"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            text: "GEAR"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 10
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: racingDash.engineData.gear === 0 ? "N" : racingDash.engineData.gear.toString()
                            color: racingDash.engineData.gear === 0 ? "#FFC107" : "#00E676"
                            font.pixelSize: 48
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }

        // Center column - G-Force and timing
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.3
            spacing: Styles.Theme.spacingSm

            // G-Force Meter
            Components.GForceMeter {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.5

                lateralG: racingDash.lateralG
                longitudinalG: racingDash.longitudinalG
            }

            // Lap Timer
            Components.LapTimer {
                Layout.fillWidth: true
                Layout.fillHeight: true

                speed: racingDash.engineData.vehicleSpeed
                numSectors: 3
            }
        }

        // Right column - Performance and warnings
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.3
            spacing: Styles.Theme.spacingSm

            // Performance Meter
            Components.PerformanceMeter {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.55

                speed: racingDash.engineData.vehicleSpeed
                rpm: racingDash.engineData.rpm
            }

            // Critical gauges row
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Styles.Theme.spacingXs

                // Coolant temp mini gauge
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#1a1a1a"
                    radius: 8
                    border.color: racingDash.engineData.coolantTemp > 100 ? "#FF5252" : "#333333"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            text: "CLT"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 9
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: racingDash.engineData.coolantTemp.toFixed(0) + "\u00B0"
                            color: racingDash.engineData.coolantTemp > 100 ? "#FF5252" :
                                   racingDash.engineData.coolantTemp > 95 ? "#FFC107" : "#00E676"
                            font.pixelSize: 20
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                // AFR mini gauge
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#1a1a1a"
                    radius: 8
                    border.color: "#333333"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            text: "AFR"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 9
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            property real afr: racingDash.engineData.lambda * 14.7
                            text: afr.toFixed(1)
                            color: afr > 16 || afr < 11 ? "#FF5252" :
                                   afr > 15 || afr < 12 ? "#FFC107" : "#00E676"
                            font.pixelSize: 20
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                // Boost mini gauge
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#1a1a1a"
                    radius: 8
                    border.color: "#333333"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            text: "BOOST"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 9
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            property real boostPsi: (racingDash.engineData.mapKpa - 101.3) * 0.145
                            text: boostPsi > 0 ? boostPsi.toFixed(1) : "VAC"
                            color: boostPsi > 15 ? "#FFC107" : "#2196F3"
                            font.pixelSize: 20
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }
    }

    // Warning overlay
    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 10
        width: warningRow.width + 40
        height: 36
        radius: 8
        color: Qt.rgba(1, 0, 0, 0.8)
        visible: racingDash.engineData.celOn || racingDash.engineData.overheat

        Row {
            id: warningRow
            anchors.centerIn: parent
            spacing: 16

            Text {
                text: racingDash.engineData.celOn ? "CEL" : ""
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
                visible: racingDash.engineData.celOn
            }

            Text {
                text: racingDash.engineData.overheat ? "OVERHEAT" : ""
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
                visible: racingDash.engineData.overheat
            }
        }

        SequentialAnimation on opacity {
            running: racingDash.engineData.celOn || racingDash.engineData.overheat
            loops: Animation.Infinite
            NumberAnimation { to: 0.5; duration: 300 }
            NumberAnimation { to: 1.0; duration: 300 }
        }
    }
}
