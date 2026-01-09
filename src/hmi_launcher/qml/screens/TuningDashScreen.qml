import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State
import "../components" as Components

/**
 * TuningDashScreen.qml
 * Diagnostics and tuning focused dashboard
 *
 * Features:
 * - VE/Advance/AFR map visualization
 * - Real-time data charts
 * - Data logger controls
 * - Peak recall for session analysis
 * - Trip computer
 */
Item {
    id: tuningDash

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
        batteryVoltage: 12.0,
        oilPressure: 40,
        oilTemp: 90,
        boostPsi: 0,
        canOk: false,
        celOn: false,
        overheat: false
    })

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Tab selector for different views
    property int activeTab: 0  // 0=Maps, 1=Charts, 2=Logger

    // Header with tab buttons
    Row {
        id: tabHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Styles.Theme.spacingSm
        height: 36
        spacing: 4

        Repeater {
            model: ["VE MAPS", "CHARTS", "LOGGER", "PEAKS", "TRIP"]

            Rectangle {
                width: (tabHeader.width - 16) / 5
                height: 36
                radius: 6
                color: activeTab === index ? "#2196F3" : "#333333"

                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: activeTab === index ? "#000000" : Qt.rgba(1, 1, 1, 0.72)
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: activeTab = index
                }
            }
        }
    }

    // Content area
    Item {
        anchors.top: tabHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Styles.Theme.spacingSm

        // Tab 0: VE Maps
        RowLayout {
            anchors.fill: parent
            spacing: Styles.Theme.spacingSm
            visible: activeTab === 0

            // VE Map Display
            Components.VEMapDisplay {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.6

                rpm: tuningDash.engineData.rpm
                mapKpa: tuningDash.engineData.mapKpa
                tps: tuningDash.engineData.tps
            }

            // Side panel with key gauges
            ColumnLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.4
                spacing: Styles.Theme.spacingSm

                // RPM
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "RPM"
                    unit: ""
                    value: tuningDash.engineData.rpm
                    minValue: 0
                    maxValue: 8000
                    warningThreshold: 6500
                    precision: 0
                    gaugeColor: Styles.Theme.accentPrimary
                    showBar: true
                }

                // MAP
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "MAP"
                    unit: "kPa"
                    value: tuningDash.engineData.mapKpa
                    minValue: 0
                    maxValue: 250
                    precision: 0
                    gaugeColor: Styles.Theme.accentOrange
                    showBar: true
                }

                // AFR
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "AFR"
                    unit: ""
                    value: tuningDash.engineData.lambda * 14.7
                    minValue: 10
                    maxValue: 20
                    warningThreshold: 12
                    warningThresholdHigh: 16
                    precision: 1
                    gaugeColor: Styles.Theme.accentPurple
                    showBar: true
                }

                // Ignition
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "IGN ADV"
                    unit: "\u00B0"
                    value: tuningDash.engineData.ignitionAdvance
                    minValue: -10
                    maxValue: 50
                    precision: 1
                    gaugeColor: Styles.Theme.accentPink
                    showBar: true
                }
            }
        }

        // Tab 1: Charts
        RowLayout {
            anchors.fill: parent
            spacing: Styles.Theme.spacingSm
            visible: activeTab === 1

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Styles.Theme.spacingSm

                // RPM Chart
                Components.ChartGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "RPM"
                    value: tuningDash.engineData.rpm
                    minValue: 0
                    maxValue: 8000
                    lineColor: "#00E676"
                }

                // AFR Chart
                Components.ChartGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "AFR"
                    value: tuningDash.engineData.lambda * 14.7
                    minValue: 10
                    maxValue: 20
                    lineColor: "#9C27B0"
                    targetValue: 14.7
                    showTarget: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Styles.Theme.spacingSm

                // MAP Chart
                Components.ChartGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "MAP"
                    value: tuningDash.engineData.mapKpa
                    minValue: 0
                    maxValue: 250
                    lineColor: "#FF9800"
                }

                // CLT Chart
                Components.ChartGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "CLT"
                    value: tuningDash.engineData.coolantTemp
                    minValue: 0
                    maxValue: 120
                    lineColor: "#F44336"
                    warningThreshold: 100
                }
            }
        }

        // Tab 2: Data Logger
        RowLayout {
            anchors.fill: parent
            spacing: Styles.Theme.spacingSm
            visible: activeTab === 2

            // Logger control
            Components.DataLogger {
                Layout.fillHeight: true
                Layout.preferredWidth: 280
            }

            // Live data panel
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 8
                columnSpacing: 8

                Repeater {
                    model: [
                        { label: "RPM", value: tuningDash.engineData.rpm.toFixed(0), unit: "" },
                        { label: "MAP", value: tuningDash.engineData.mapKpa.toFixed(0), unit: "kPa" },
                        { label: "TPS", value: tuningDash.engineData.tps.toFixed(0), unit: "%" },
                        { label: "CLT", value: tuningDash.engineData.coolantTemp.toFixed(0), unit: "\u00B0C" },
                        { label: "IAT", value: tuningDash.engineData.intakeTemp.toFixed(0), unit: "\u00B0C" },
                        { label: "AFR", value: (tuningDash.engineData.lambda * 14.7).toFixed(1), unit: "" },
                        { label: "IGN", value: tuningDash.engineData.ignitionAdvance.toFixed(1), unit: "\u00B0" },
                        { label: "INJ", value: tuningDash.engineData.injectorDuty.toFixed(0), unit: "%" },
                        { label: "BATT", value: tuningDash.engineData.batteryVoltage.toFixed(1), unit: "V" }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1a1a1a"
                        radius: 8
                        border.color: "#333333"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                text: modelData.label
                                color: Qt.rgba(1, 1, 1, 0.56)
                                font.pixelSize: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 4

                                Text {
                                    text: modelData.value
                                    color: Qt.rgba(1, 1, 1, 0.96)
                                    font.pixelSize: 24
                                    font.bold: true
                                    font.family: "Roboto Mono, monospace"
                                }

                                Text {
                                    text: modelData.unit
                                    color: Qt.rgba(1, 1, 1, 0.56)
                                    font.pixelSize: 12
                                    anchors.baseline: parent.children[0].baseline
                                    visible: modelData.unit !== ""
                                }
                            }
                        }
                    }
                }
            }
        }

        // Tab 3: Peak Recall
        RowLayout {
            anchors.fill: parent
            spacing: Styles.Theme.spacingSm
            visible: activeTab === 3

            // Peak Recall component
            Components.PeakRecall {
                Layout.fillHeight: true
                Layout.preferredWidth: 300

                rpm: tuningDash.engineData.rpm
                speed: tuningDash.engineData.vehicleSpeed
                coolantTemp: tuningDash.engineData.coolantTemp
                oilTemp: tuningDash.engineData.oilTemp
                oilPressure: tuningDash.engineData.oilPressure
                boostPsi: tuningDash.engineData.boostPsi
                afr: tuningDash.engineData.lambda * 14.7
                iat: tuningDash.engineData.intakeTemp
                batteryVoltage: tuningDash.engineData.batteryVoltage
            }

            // Current values for comparison
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                rowSpacing: 8
                columnSpacing: 8

                Text {
                    Layout.columnSpan: 2
                    text: "CURRENT VALUES"
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 12
                    font.bold: true
                }

                Repeater {
                    model: [
                        { label: "RPM", value: tuningDash.engineData.rpm.toFixed(0) },
                        { label: "SPEED", value: tuningDash.engineData.vehicleSpeed.toFixed(0) + " km/h" },
                        { label: "CLT", value: tuningDash.engineData.coolantTemp.toFixed(0) + "\u00B0C" },
                        { label: "OIL TEMP", value: tuningDash.engineData.oilTemp.toFixed(0) + "\u00B0C" },
                        { label: "OIL PSI", value: tuningDash.engineData.oilPressure.toFixed(0) + " psi" },
                        { label: "BOOST", value: tuningDash.engineData.boostPsi.toFixed(1) + " psi" },
                        { label: "AFR", value: (tuningDash.engineData.lambda * 14.7).toFixed(1) },
                        { label: "BATT", value: tuningDash.engineData.batteryVoltage.toFixed(1) + "V" }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        height: 50
                        color: "#1a1a1a"
                        radius: 6
                        border.color: "#333333"
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 8

                            Text {
                                text: modelData.label
                                color: Qt.rgba(1, 1, 1, 0.56)
                                font.pixelSize: 10
                                width: parent.width * 0.4
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: modelData.value
                                color: Qt.rgba(1, 1, 1, 0.96)
                                font.pixelSize: 16
                                font.bold: true
                                font.family: "Roboto Mono, monospace"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }

        // Tab 4: Trip Computer
        RowLayout {
            anchors.fill: parent
            spacing: Styles.Theme.spacingSm
            visible: activeTab === 4

            // Trip Computer component
            Components.TripComputer {
                Layout.fillHeight: true
                Layout.preferredWidth: 300

                speed: tuningDash.engineData.vehicleSpeed
                rpm: tuningDash.engineData.rpm
                tps: tuningDash.engineData.tps
                mapKpa: tuningDash.engineData.mapKpa
            }

            // Driving stats
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Styles.Theme.spacingSm

                // Speed gauge
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "SPEED"
                    unit: "km/h"
                    value: tuningDash.engineData.vehicleSpeed
                    minValue: 0
                    maxValue: 280
                    precision: 0
                    gaugeColor: Styles.Theme.accentSecondary
                    showArc: true
                    isPrimary: true
                }

                // RPM gauge
                Components.SafeGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "RPM"
                    unit: ""
                    value: tuningDash.engineData.rpm
                    minValue: 0
                    maxValue: 8000
                    warningThreshold: 6500
                    precision: 0
                    gaugeColor: Styles.Theme.accentPrimary
                    showArc: true
                    isPrimary: true
                }
            }
        }
    }
}
