import QtQuick
import QtQuick.Layouts
import "../components"

/**
 * DashScreen.qml - Main dashboard with LARGE gauges
 *
 * Layout optimizado para 800x480:
 * - Shift lights no topo (fino)
 * - RPM e Speed GRANDES ocupando a maior parte da tela
 * - Gear indicator integrado
 * - Compact gauges no rodapé
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // DATA PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property real rpm: 3250
    property real speed: 85
    property real coolantTemp: 85
    property real mapKpa: 98
    property real tps: 35
    property real afr: 14.7
    property real boostPsi: 0.5
    property real oilTemp: 90
    property real oilPressure: 45
    property real fuelPressure: 43
    property real iat: 35
    property real batteryVoltage: 14.2
    property real ignitionAdvance: 28
    property int gear: 3

    property real redlineRpm: 7000
    property real shiftRpm: 6500

    property int displayMode: 0  // 0=Sport, 1=Eco, 2=Minimal, 3=Full

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: "#0a0a0a"
    }

    // ═══════════════════════════════════════════════════════════════════════
    // SPORT / FULL MODE LAYOUT
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 0 || displayMode === 3

        // ─────────────────────────────────────────────────────────────────
        // SHIFT LIGHTS (topo)
        // ─────────────────────────────────────────────────────────────────

        ShiftLightBar {
            id: shiftLights
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 4
            rpm: root.rpm
            redlineRpm: root.redlineRpm
            activationRpm: root.shiftRpm * 0.65
            ledCount: 16
            ledSize: 12
            ledSpacing: 6
            centerOut: true
            showBackground: false
            height: 24
        }

        // ─────────────────────────────────────────────────────────────────
        // MAIN GAUGES AREA
        // ─────────────────────────────────────────────────────────────────

        Item {
            id: mainGaugesArea
            anchors.top: shiftLights.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: compactGaugesRow.top
            anchors.margins: 8

            // RPM GAUGE (esquerda - GRANDE)
            ArcGauge {
                id: rpmGauge
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 20
                gaugeSize: Math.min(parent.height - 20, (parent.width / 2) - 60)
                value: root.rpm
                minValue: 0
                maxValue: root.redlineRpm
                warningValue: root.shiftRpm
                criticalValue: root.redlineRpm * 0.95
                label: "RPM"
                decimals: 0
                accentColor: "#00E676"
                majorTickCount: 7
            }

            // GEAR INDICATOR (centro)
            GearIndicator {
                anchors.centerIn: parent
                gear: root.gear
                size: 90
                showLabel: true
            }

            // SPEED GAUGE (direita - GRANDE)
            ArcGauge {
                id: speedGauge
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 20
                gaugeSize: Math.min(parent.height - 20, (parent.width / 2) - 60)
                value: root.speed
                minValue: 0
                maxValue: 260
                warningValue: 180
                criticalValue: 220
                label: "km/h"
                decimals: 0
                accentColor: "#00B8D4"
                majorTickCount: 13
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // COMPACT GAUGES ROW (rodapé)
        // ─────────────────────────────────────────────────────────────────

        Row {
            id: compactGaugesRow
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            height: 60
            spacing: 8

            CompactGauge {
                width: (parent.width - (parent.spacing * 4)) / 5
                height: parent.height
                value: root.coolantTemp
                minValue: 0
                maxValue: 120
                warningValue: 100
                criticalValue: 110
                label: "CLT"
                unit: "°C"
                accentColor: "#00E676"
            }

            CompactGauge {
                width: (parent.width - (parent.spacing * 4)) / 5
                height: parent.height
                value: root.mapKpa
                minValue: 0
                maxValue: 250
                warningValue: 200
                criticalValue: 230
                label: "MAP"
                unit: "kPa"
                accentColor: "#FF6D00"
            }

            CompactGauge {
                width: (parent.width - (parent.spacing * 4)) / 5
                height: parent.height
                value: root.tps
                minValue: 0
                maxValue: 100
                warningValue: 101
                criticalValue: 102
                label: "TPS"
                unit: "%"
                accentColor: "#00E676"
            }

            CompactGauge {
                width: (parent.width - (parent.spacing * 4)) / 5
                height: parent.height
                value: root.afr
                minValue: 10
                maxValue: 20
                warningValue: 16
                criticalValue: 17
                label: "AFR"
                unit: ""
                decimals: 1
                accentColor: "#AA00FF"
            }

            CompactGauge {
                width: (parent.width - (parent.spacing * 4)) / 5
                height: parent.height
                value: displayMode === 3 ? root.oilTemp : root.ignitionAdvance
                minValue: displayMode === 3 ? 0 : -10
                maxValue: displayMode === 3 ? 150 : 50
                warningValue: displayMode === 3 ? 120 : 45
                criticalValue: displayMode === 3 ? 140 : 48
                label: displayMode === 3 ? "OIL T" : "IGN"
                unit: "°"
                accentColor: displayMode === 3 ? "#FFB300" : "#00B8D4"
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ECO MODE LAYOUT - Speed maior, RPM menor
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 1

        // Speed CENTRAL e GRANDE
        ArcGauge {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -20
            gaugeSize: Math.min(parent.height - 100, parent.width * 0.5)
            value: root.speed
            minValue: 0
            maxValue: 260
            warningValue: 180
            criticalValue: 220
            label: "km/h"
            decimals: 0
            accentColor: "#00B8D4"
            majorTickCount: 13
        }

        // RPM bar no topo
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            height: 40
            radius: 8
            color: "#141414"

            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                Text {
                    text: "RPM"
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 12
                    font.family: "Roboto, sans-serif"
                    anchors.verticalCenter: parent.verticalCenter
                }

                // RPM bar
                Item {
                    width: parent.width - 120
                    height: 12
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "#333333"
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * (root.rpm / root.redlineRpm)
                        radius: 6
                        color: root.rpm > root.shiftRpm ? "#FFB300" :
                               root.rpm > root.redlineRpm * 0.95 ? "#FF3D00" : "#00E676"

                        Behavior on width { NumberAnimation { duration: 100 } }
                    }
                }

                Text {
                    text: Math.round(root.rpm)
                    color: "#00E676"
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Gear + info no rodapé
        Row {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 16
            spacing: 32

            // Gear
            Column {
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.gear === 0 ? "N" : root.gear === -1 ? "R" : root.gear.toString()
                    color: root.gear === -1 ? "#FF9100" : "#00B8D4"
                    font.pixelSize: 36
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "GEAR"
                    color: Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 10
                }
            }

            Rectangle { width: 1; height: 50; color: "#333333" }

            // CLT
            Column {
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(root.coolantTemp) + "°"
                    color: root.coolantTemp > 100 ? "#FF3D00" : "#00E676"
                    font.pixelSize: 28
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "CLT"
                    color: Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 10
                }
            }

            Rectangle { width: 1; height: 50; color: "#333333" }

            // AFR
            Column {
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.afr.toFixed(1)
                    color: "#AA00FF"
                    font.pixelSize: 28
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "AFR"
                    color: Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 10
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MINIMAL MODE - Speed gigante
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 2

        Column {
            anchors.centerIn: parent
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Math.round(root.speed)
                color: "#00B8D4"
                font.pixelSize: 140
                font.bold: true
                font.family: "Roboto Mono, monospace"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "km/h"
                color: Qt.rgba(1, 1, 1, 0.56)
                font.pixelSize: 24
                font.family: "Roboto, sans-serif"
            }
        }

        // Gear no canto inferior direito
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 20
            width: 60
            height: 60
            radius: 12
            color: "#141414"
            border.color: root.gear === -1 ? "#FF9100" : "#333333"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: root.gear === 0 ? "N" : root.gear === -1 ? "R" : root.gear.toString()
                color: root.gear === -1 ? "#FF9100" : "#00B8D4"
                font.pixelSize: 28
                font.bold: true
                font.family: "Roboto Mono, monospace"
            }
        }

        // Warnings only
        Row {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
            spacing: 8

            Rectangle {
                visible: root.coolantTemp > 100
                width: 50
                height: 30
                radius: 6
                color: "#FF3D00"

                Text {
                    anchors.centerIn: parent
                    text: "TEMP"
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }

                SequentialAnimation on opacity {
                    running: root.coolantTemp > 100
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 200 }
                    NumberAnimation { to: 1.0; duration: 200 }
                }
            }
        }
    }
}
