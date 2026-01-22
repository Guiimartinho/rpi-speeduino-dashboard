import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import "../styles" as Styles
import "../state" as State
import "../components" as Components

/**
 * DashScreen.qml
 * Professional Speeduino ECU Dashboard with modern UI/UX
 * Optimized layout - No overlapping elements
 */
Item {
    id: dashScreen

    // ═══════════════════════════════════════════════════════════════════════
    // ENGINE DATA
    // ═══════════════════════════════════════════════════════════════════════

    property var engineData: ({
        rpm: 0, coolantTemp: 0, intakeTemp: 0, tps: 0, mapKpa: 0,
        lambda: 1.0, ignitionAdvance: 0, injectorDuty: 0, vehicleSpeed: 0,
        gear: 0, canOk: false, celOn: false, overheat: false,
        batteryVoltage: 12.0, oilPressure: 0, oilTemp: 0, boostPsi: 0,
        veValue: 0, sparkDwell: 0, loopsPerSec: 0, freeRam: 0,
        targetAfr: 14.7, afrCorrection: 0,
        // New fields from Speeduino Native CAN
        fuelPressure: 0, boostTarget: 0, fuelConsumption: 0,
        ve: 0, afrTarget: 14.7,
        // Per-cylinder trims
        fuelTrimCyl1: 0, fuelTrimCyl2: 0, fuelTrimCyl3: 0, fuelTrimCyl4: 0,
        ignTrimCyl1: 0, ignTrimCyl2: 0, ignTrimCyl3: 0, ignTrimCyl4: 0,
        // Idle control
        idleTargetRpm: 0, idleValveDuty: 0,
        // Diagnostic
        errorCount: 0, synced: false,
        // Status flags
        engineRunning: false, revLimiterActive: false, launchControlActive: false, flatShiftActive: false,
        clutchIn: false, brakeOn: false, cruiseOn: false,
        lowOilPressure: false, lowFuelPressure: false, dfcoActive: false, fanOn: false
    })

    property int displayMode: 0  // 0=Sport, 1=Street, 2=Track, 3=Diagnostic

    // Calculated values
    property real afr: engineData.lambda * 14.7
    property real boostPsi: (engineData.mapKpa - 101.3) * 0.145
    property bool isBoost: boostPsi > 0.5
    property bool compactMode: width < 900

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND WITH GRADIENT
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d0d0d" }
            GradientStop { position: 0.5; color: "#0a0a0a" }
            GradientStop { position: 1.0; color: "#080808" }
        }
    }

    // Subtle grid pattern overlay
    Canvas {
        anchors.fill: parent
        opacity: 0.03
        onPaint: {
            var ctx = getContext("2d")
            ctx.strokeStyle = "#ffffff"
            ctx.lineWidth = 0.5
            var step = 40
            for (var x = 0; x < width; x += step) {
                ctx.beginPath()
                ctx.moveTo(x, 0)
                ctx.lineTo(x, height)
                ctx.stroke()
            }
            for (var y = 0; y < height; y += step) {
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE SELECTOR - Floating pills (Top Center)
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: modeSelector
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 6
        width: modeRow.width + 12
        height: 32
        radius: 16
        color: "#1a1a1a"
        border.color: "#2a2a2a"
        border.width: 1
        z: 100

        Row {
            id: modeRow
            anchors.centerIn: parent
            spacing: 4

            Repeater {
                model: [
                    { label: "SPORT", icon: "⚡" },
                    { label: "STREET", icon: "🛣" },
                    { label: "TRACK", icon: "🏁" },
                    { label: "DIAG", icon: "📊" }
                ]

                Rectangle {
                    width: 64
                    height: 24
                    radius: 12
                    color: "transparent"

                    // Gradient background when active
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        visible: displayMode === index
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#00E676" }
                            GradientStop { position: 1.0; color: "#00C853" }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: displayMode === index ? "#000000" : "#666666"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: displayMode = index
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 0: SPORT - Optimized Racing Layout
    // Removed redundant speed gauge, added boost display
    // ═══════════════════════════════════════════════════════════════════════

    Loader {
        id: sportModeLoader
        anchors.fill: parent
        anchors.topMargin: 42
        active: displayMode === 0
        sourceComponent: sportModeComponent
    }

    Component {
        id: sportModeComponent

        Item {

        // Progressive Shift Light Bar
        Item {
            id: shiftBar
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.75
            height: 18

            Row {
                anchors.centerIn: parent
                spacing: 3

                Repeater {
                    model: 15

                    Rectangle {
                        width: 22
                        height: 10
                        radius: 2

                        property real threshold: 4000 + (index * 250)
                        property bool active: engineData.rpm >= threshold

                        color: {
                            if (!active) return "#1a1a1a"
                            if (index < 5) return "#00E676"
                            if (index < 10) return "#FFEB3B"
                            if (index < 13) return "#FF9800"
                            return "#F44336"
                        }

                        opacity: active ? 1.0 : 0.3

                        // Glow effect for active LEDs
                        Rectangle {
                            visible: parent.active && index >= 10
                            anchors.fill: parent
                            anchors.margins: -3
                            radius: 5
                            color: parent.color
                            opacity: 0.3
                            z: -1
                        }

                        Behavior on color { ColorAnimation { duration: 50 } }
                        Behavior on opacity { NumberAnimation { duration: 50 } }
                    }
                }
            }
        }

        // Main gauges area - Optimized 3-column layout
        RowLayout {
            anchors.top: shiftBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: bottomBar.top
            anchors.margins: 12
            anchors.topMargin: 4
            spacing: 12

            // LEFT: RPM Gauge (45%)
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.42

                Components.ArcGauge {
                    anchors.centerIn: parent
                    gaugeSize: Math.min(parent.width, parent.height) - 16
                    value: engineData.rpm
                    minValue: 0
                    maxValue: 8000
                    warningValue: 6500
                    criticalValue: 7500
                    label: "RPM"
                    decimals: 0
                    accentColor: "#00E676"
                    majorTickCount: 8
                    showLabels: !compactMode
                }
            }

            // CENTER: Gear + Boost (16%)
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.16

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    // Large Gear Display
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 80
                        height: 80
                        radius: 12
                        color: "#141414"
                        border.color: engineData.gear === 0 ? "#FF9800" : engineData.gear === -1 ? "#F44336" : "#00E676"
                        border.width: 2

                        Text {
                            anchors.centerIn: parent
                            text: engineData.gear === 0 ? "N" : engineData.gear === -1 ? "R" : engineData.gear.toString()
                            color: parent.border.color
                            font.pixelSize: 48
                            font.bold: true
                            font.family: "Roboto Mono, Consolas, monospace"
                        }
                    }

                    // Boost/Vacuum Display
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 80
                        height: 50
                        radius: 8
                        color: "#141414"
                        border.color: isBoost ? "#FF9800" : "#00BCD4"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: isBoost ? "BOOST" : "VAC"
                                color: "#666666"
                                font.pixelSize: 8
                                font.bold: true
                                font.letterSpacing: 1
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Math.abs(boostPsi).toFixed(1)
                                color: isBoost ? "#FF9800" : "#00BCD4"
                                font.pixelSize: 20
                                font.bold: true
                                font.family: "Roboto Mono, Consolas, monospace"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "psi"
                                color: "#555555"
                                font.pixelSize: 8
                            }
                        }
                    }
                }
            }

            // RIGHT: Speed Digital (42%)
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.42

                Column {
                    anchors.centerIn: parent
                    spacing: 0

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: engineData.vehicleSpeed.toFixed(0)
                        color: "#00B8D4"
                        font.pixelSize: Math.min(parent.parent.width * 0.45, 120)
                        font.bold: true
                        font.family: "Roboto Mono, Consolas, monospace"

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            blurEnabled: true
                            blur: 0.2
                            blurMax: 12
                            brightness: 0.05
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "km/h"
                        color: "#555555"
                        font.pixelSize: 16
                        font.letterSpacing: 3
                    }
                }
            }
        }

        // Bottom status bar with 8 metrics (2 rows x 4)
        Rectangle {
            id: bottomBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            height: 100
            radius: 10
            color: "#141414"
            border.color: "#1e1e1e"
            border.width: 1

            GridLayout {
                anchors.fill: parent
                anchors.margins: 8
                columns: 4
                rows: 2
                columnSpacing: 6
                rowSpacing: 4

                // Row 1: CLT, AFR, FUEL P, INJ
                Repeater {
                    model: [
                        { label: "CLT", value: engineData.coolantTemp.toFixed(0), unit: "°C",
                          color: engineData.coolantTemp > 110 ? "#F44336" : engineData.coolantTemp > 100 ? "#FF9800" : "#00E676",
                          warn: engineData.coolantTemp > 100 },
                        { label: "AFR", value: afr.toFixed(1), unit: "",
                          color: (afr > 14.0 || afr < 10.0) ? "#F44336" : (afr > 13.5 || afr < 10.5) ? "#FF9800" : "#AA00FF",
                          warn: afr > 13.5 || afr < 10.5 },
                        { label: "FUEL P", value: (engineData.fuelPressure / 100).toFixed(1), unit: "bar",
                          color: engineData.lowFuelPressure ? "#F44336" : "#00BCD4", warn: engineData.lowFuelPressure },
                        { label: "INJ", value: engineData.injectorDuty.toFixed(0), unit: "%",
                          color: engineData.injectorDuty > 85 ? "#F44336" : "#FF9800", warn: engineData.injectorDuty > 85 },
                        // Row 2: OIL T, OIL P, MAP, BATT
                        { label: "OIL T", value: engineData.oilTemp.toFixed(0), unit: "°C",
                          color: engineData.oilTemp > 140 ? "#F44336" : engineData.oilTemp > 120 ? "#FF9800" : "#FFB300",
                          warn: engineData.oilTemp > 120 },
                        { label: "OIL P", value: (engineData.oilPressure / 100).toFixed(1), unit: "bar",
                          color: engineData.lowOilPressure ? "#F44336" : "#FFB300", warn: engineData.lowOilPressure },
                        { label: "MAP", value: engineData.mapKpa.toFixed(0), unit: "kPa",
                          color: "#FF9800", warn: false },
                        { label: "BATT", value: engineData.batteryVoltage.toFixed(1), unit: "V",
                          color: engineData.batteryVoltage < 11.5 ? "#F44336" : engineData.batteryVoltage < 12.5 ? "#FF9800" : "#00E676",
                          warn: engineData.batteryVoltage < 12.5 }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 6
                        color: modelData.warn ? Qt.rgba(244/255, 67/255, 54/255, 0.12) : "#1a1a1a"
                        border.color: modelData.warn ? "#F44336" : "#252525"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: "#666666"
                                font.pixelSize: 8
                                font.bold: true
                                font.letterSpacing: 0.5
                            }

                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 2

                                Text {
                                    text: modelData.value
                                    color: modelData.color
                                    font.pixelSize: 18
                                    font.bold: true
                                    font.family: "Roboto Mono, Consolas, monospace"
                                }

                                Text {
                                    text: modelData.unit
                                    color: "#555555"
                                    font.pixelSize: 9
                                    anchors.bottom: parent.children[0].bottom
                                    anchors.bottomMargin: 1
                                }
                            }
                        }
                    }
                }
            }
        }

        // Status indicator row (compact, inside bottom margin)
        Row {
            anchors.bottom: bottomBar.top
            anchors.horizontalCenter: bottomBar.horizontalCenter
            anchors.bottomMargin: 4
            spacing: 8
            z: 10

            // Engine Running
            Rectangle {
                visible: engineData.engineRunning
                width: 50
                height: 18
                radius: 9
                color: "#4CAF50"
                Text {
                    anchors.centerIn: parent
                    text: "RUN"
                    color: "#FFFFFF"
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            // Launch Control
            Rectangle {
                visible: engineData.launchControlActive
                width: 55
                height: 18
                radius: 9
                color: "#00E676"
                Text {
                    anchors.centerIn: parent
                    text: "LAUNCH"
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
                SequentialAnimation on opacity {
                    running: engineData.launchControlActive
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.5; duration: 200 }
                    NumberAnimation { to: 1.0; duration: 200 }
                }
            }

            // Flat Shift
            Rectangle {
                visible: engineData.flatShiftActive
                width: 45
                height: 18
                radius: 9
                color: "#FFEB3B"
                Text {
                    anchors.centerIn: parent
                    text: "FLAT"
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            // Rev Limiter
            Rectangle {
                visible: engineData.revLimiterActive
                width: 45
                height: 18
                radius: 9
                color: "#F44336"
                Text {
                    anchors.centerIn: parent
                    text: "REV"
                    color: "#FFFFFF"
                    font.pixelSize: 8
                    font.bold: true
                }
                SequentialAnimation on opacity {
                    running: engineData.revLimiterActive
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 100 }
                    NumberAnimation { to: 1.0; duration: 100 }
                }
            }

            // DFCO
            Rectangle {
                visible: engineData.dfcoActive
                width: 45
                height: 18
                radius: 9
                color: "#2196F3"
                Text {
                    anchors.centerIn: parent
                    text: "DFCO"
                    color: "#FFFFFF"
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            // Fan
            Rectangle {
                visible: engineData.fanOn
                width: 40
                height: 18
                radius: 9
                color: "#00BCD4"
                Text {
                    anchors.centerIn: parent
                    text: "FAN"
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 1: STREET - Minimal, speed focused
    // ═══════════════════════════════════════════════════════════════════════

    Loader {
        id: streetModeLoader
        anchors.fill: parent
        anchors.topMargin: 42
        active: displayMode === 1
        sourceComponent: streetModeComponent
    }

    Component {
        id: streetModeComponent

        Item {

        // Giant speed display
        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -30
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: engineData.vehicleSpeed.toFixed(0)
                color: "#00B8D4"
                font.pixelSize: Math.min(parent.parent.width * 0.28, 180)
                font.bold: true
                font.family: "Roboto Mono, Consolas, monospace"

                layer.enabled: true
                layer.effect: MultiEffect {
                    blurEnabled: true
                    blur: 0.3
                    blurMax: 16
                    brightness: 0.1
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "km/h"
                color: "#444444"
                font.pixelSize: 24
                font.letterSpacing: 4
            }
        }

        // RPM Bar at top
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            height: 45
            radius: 10
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Text {
                    text: "RPM"
                    color: "#666666"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }

                // RPM Bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 5
                    color: "#1a1a1a"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * Math.min(1, engineData.rpm / 8000)
                        radius: 5

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#00E676" }
                            GradientStop { position: 0.7; color: "#FFEB3B" }
                            GradientStop { position: 0.9; color: "#FF9800" }
                            GradientStop { position: 1.0; color: "#F44336" }
                        }

                        Behavior on width { NumberAnimation { duration: 80 } }
                    }

                    // Tick marks
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: 8
                            Rectangle {
                                width: 1
                                height: parent.height
                                x: parent.width * (index / 8)
                                color: "#333333"
                            }
                        }
                    }
                }

                Text {
                    text: engineData.rpm.toFixed(0)
                    color: engineData.rpm > 6500 ? "#FF9800" : "#00E676"
                    font.pixelSize: 22
                    font.bold: true
                    font.family: "Roboto Mono, Consolas, monospace"
                    Layout.preferredWidth: 55
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Bottom info strip with 5 items
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            height: 55
            radius: 10
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 24

                Repeater {
                    model: [
                        { label: "GEAR", value: engineData.gear === 0 ? "N" : engineData.gear.toString(),
                          color: engineData.gear === 0 ? "#FF9800" : "#00B8D4" },
                        { label: "CLT", value: engineData.coolantTemp.toFixed(0) + "°",
                          color: engineData.coolantTemp > 110 ? "#F44336" : engineData.coolantTemp > 100 ? "#FF9800" : "#00E676" },
                        { label: "AFR", value: afr.toFixed(1),
                          color: (afr > 14.0 || afr < 10.0) ? "#F44336" : (afr > 13.5 || afr < 10.5) ? "#FF9800" : "#AA00FF" },
                        { label: "OIL", value: engineData.oilTemp.toFixed(0) + "°",
                          color: engineData.oilTemp > 140 ? "#F44336" : engineData.oilTemp > 120 ? "#FF9800" : "#FFB300" },
                        { label: "BATT", value: engineData.batteryVoltage.toFixed(1) + "V",
                          color: engineData.batteryVoltage < 11.5 ? "#F44336" : engineData.batteryVoltage < 12.5 ? "#FF9800" : "#00E676" }
                    ]

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: "#555555"
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 1
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 22
                                font.bold: true
                                font.family: "Roboto Mono, Consolas, monospace"
                            }
                        }
                    }
                }
            }
        }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 2: TRACK - All critical data visible (Fixed 4 rows)
    // ═══════════════════════════════════════════════════════════════════════

    Loader {
        id: trackModeLoader
        anchors.fill: parent
        anchors.topMargin: 42
        active: displayMode === 2
        sourceComponent: trackModeComponent
    }

    Component {
        id: trackModeComponent

        Item {

        GridLayout {
            anchors.fill: parent
            anchors.margins: 10
            columns: 4
            rows: 4
            columnSpacing: 6
            rowSpacing: 6

            // Row 1: Main gauges
            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.columnSpan: 2
                label: "RPM"
                value: engineData.rpm.toFixed(0)
                unit: ""
                progress: engineData.rpm / 8000
                accentColor: engineData.rpm > 6500 ? "#FF9800" : "#00E676"
                showBar: true
                large: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "SPEED"
                value: engineData.vehicleSpeed.toFixed(0)
                unit: "km/h"
                progress: engineData.vehicleSpeed / 280
                accentColor: "#00B8D4"
                showBar: true
                large: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "GEAR"
                value: engineData.gear === 0 ? "N" : engineData.gear.toString()
                unit: ""
                accentColor: engineData.gear === 0 ? "#FF9800" : "#00B8D4"
                large: true
                centered: true
            }

            // Row 2: Engine params
            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "CLT"
                value: engineData.coolantTemp.toFixed(0)
                unit: "°C"
                progress: engineData.coolantTemp / 120
                accentColor: engineData.coolantTemp > 110 ? "#F44336" : engineData.coolantTemp > 100 ? "#FF9800" : "#00E676"
                warning: engineData.coolantTemp > 100
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IAT"
                value: engineData.intakeTemp.toFixed(0)
                unit: "°C"
                progress: (engineData.intakeTemp + 20) / 100
                accentColor: engineData.intakeTemp > 50 ? "#FF9800" : "#00B8D4"
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "MAP"
                value: engineData.mapKpa.toFixed(0)
                unit: "kPa"
                progress: engineData.mapKpa / 250
                accentColor: "#FF9800"
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: isBoost ? "BOOST" : "VAC"
                value: boostPsi.toFixed(1)
                unit: "psi"
                accentColor: isBoost ? "#FF9800" : "#00B8D4"
            }

            // Row 3: Fuel & ignition & temps
            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "AFR"
                value: afr.toFixed(1)
                unit: ""
                accentColor: (afr > 14.0 || afr < 10.0) ? "#F44336" : (afr > 13.5 || afr < 10.5) ? "#FF9800" : "#AA00FF"
                warning: afr > 13.5 || afr < 10.5
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "OIL T"
                value: engineData.oilTemp.toFixed(0)
                unit: "°C"
                progress: engineData.oilTemp / 150
                accentColor: engineData.oilTemp > 140 ? "#F44336" : engineData.oilTemp > 120 ? "#FF9800" : "#FFB300"
                warning: engineData.oilTemp > 120
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "OIL P"
                value: (engineData.oilPressure / 100).toFixed(1)
                unit: "bar"
                accentColor: engineData.lowOilPressure ? "#F44336" : "#FFB300"
                warning: engineData.lowOilPressure
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "FUEL P"
                value: (engineData.fuelPressure / 100).toFixed(1)
                unit: "bar"
                accentColor: engineData.lowFuelPressure ? "#F44336" : "#00BCD4"
                warning: engineData.lowFuelPressure
            }

            // Row 4: IGN, INJ, TPS, BATT
            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IGN"
                value: engineData.ignitionAdvance.toFixed(1)
                unit: "°"
                accentColor: "#E91E63"
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "INJ"
                value: engineData.injectorDuty.toFixed(0)
                unit: "%"
                progress: engineData.injectorDuty / 100
                accentColor: engineData.injectorDuty > 85 ? "#F44336" : "#FF9800"
                warning: engineData.injectorDuty > 85
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "TPS"
                value: engineData.tps.toFixed(0)
                unit: "%"
                progress: engineData.tps / 100
                accentColor: "#00E676"
                showBar: true
            }

            Components.GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "BATT"
                value: engineData.batteryVoltage.toFixed(1)
                unit: "V"
                accentColor: engineData.batteryVoltage < 11.5 ? "#F44336" : engineData.batteryVoltage < 12.5 ? "#FF9800" : "#00E676"
                warning: engineData.batteryVoltage < 12.5
            }
        }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 3: DIAGNOSTIC - Full ECU data (Optimized spacing)
    // ═══════════════════════════════════════════════════════════════════════

    Loader {
        id: diagModeLoader
        anchors.fill: parent
        anchors.topMargin: 42
        active: displayMode === 3
        sourceComponent: diagModeComponent
    }

    Component {
        id: diagModeComponent

        Item {

        GridLayout {
            anchors.fill: parent
            anchors.margins: 6
            columns: 5
            rows: 6
            columnSpacing: 5
            rowSpacing: 5

            Repeater {
                model: [
                    // Row 1: Core data
                    { label: "RPM", value: engineData.rpm.toFixed(0), unit: "", color: "#00E676" },
                    { label: "SPEED", value: engineData.vehicleSpeed.toFixed(0), unit: "km/h", color: "#00B8D4" },
                    { label: "GEAR", value: engineData.gear === 0 ? "N" : engineData.gear.toString(), unit: "", color: "#00B8D4" },
                    { label: "CLT", value: engineData.coolantTemp.toFixed(0), unit: "°C", color: engineData.coolantTemp > 110 ? "#F44336" : engineData.coolantTemp > 100 ? "#FF9800" : "#00E676" },
                    { label: "IAT", value: engineData.intakeTemp.toFixed(0), unit: "°C", color: "#00B8D4" },

                    // Row 2: MAP/TPS/AFR
                    { label: "MAP", value: engineData.mapKpa.toFixed(0), unit: "kPa", color: "#FF9800" },
                    { label: "TPS", value: engineData.tps.toFixed(0), unit: "%", color: "#00E676" },
                    { label: "AFR", value: afr.toFixed(1), unit: "", color: (afr > 14.0 || afr < 10.0) ? "#F44336" : (afr > 13.5 || afr < 10.5) ? "#FF9800" : "#AA00FF" },
                    { label: "AFR TGT", value: engineData.afrTarget.toFixed(1), unit: "", color: "#9C27B0" },
                    { label: "VE", value: engineData.ve.toFixed(0), unit: "%", color: "#00BCD4" },

                    // Row 3: Ignition/Injection/Pressures
                    { label: "IGN ADV", value: engineData.ignitionAdvance.toFixed(1), unit: "°", color: "#E91E63" },
                    { label: "INJ DC", value: engineData.injectorDuty.toFixed(0), unit: "%", color: "#FF9800" },
                    { label: "BATT", value: engineData.batteryVoltage.toFixed(1), unit: "V", color: engineData.batteryVoltage < 11.5 ? "#F44336" : engineData.batteryVoltage < 12.5 ? "#FF9800" : "#00E676" },
                    { label: "FUEL P", value: (engineData.fuelPressure / 100).toFixed(1), unit: "bar", color: engineData.lowFuelPressure ? "#F44336" : "#00BCD4" },
                    { label: "OIL P", value: (engineData.oilPressure / 100).toFixed(1), unit: "bar", color: engineData.lowOilPressure ? "#F44336" : "#FFB300" },

                    // Row 4: Per-cylinder fuel trims
                    { label: "F TRM 1", value: engineData.fuelTrimCyl1.toFixed(0), unit: "%", color: Math.abs(engineData.fuelTrimCyl1) > 10 ? "#FF9800" : "#4CAF50" },
                    { label: "F TRM 2", value: engineData.fuelTrimCyl2.toFixed(0), unit: "%", color: Math.abs(engineData.fuelTrimCyl2) > 10 ? "#FF9800" : "#4CAF50" },
                    { label: "F TRM 3", value: engineData.fuelTrimCyl3.toFixed(0), unit: "%", color: Math.abs(engineData.fuelTrimCyl3) > 10 ? "#FF9800" : "#4CAF50" },
                    { label: "F TRM 4", value: engineData.fuelTrimCyl4.toFixed(0), unit: "%", color: Math.abs(engineData.fuelTrimCyl4) > 10 ? "#FF9800" : "#4CAF50" },
                    { label: "OIL T", value: engineData.oilTemp.toFixed(0), unit: "°C", color: engineData.oilTemp > 140 ? "#F44336" : engineData.oilTemp > 120 ? "#FF9800" : "#FFB300" },

                    // Row 5: Per-cylinder ignition trims
                    { label: "I TRM 1", value: engineData.ignTrimCyl1.toFixed(1), unit: "°", color: Math.abs(engineData.ignTrimCyl1) > 5 ? "#FF9800" : "#E91E63" },
                    { label: "I TRM 2", value: engineData.ignTrimCyl2.toFixed(1), unit: "°", color: Math.abs(engineData.ignTrimCyl2) > 5 ? "#FF9800" : "#E91E63" },
                    { label: "I TRM 3", value: engineData.ignTrimCyl3.toFixed(1), unit: "°", color: Math.abs(engineData.ignTrimCyl3) > 5 ? "#FF9800" : "#E91E63" },
                    { label: "I TRM 4", value: engineData.ignTrimCyl4.toFixed(1), unit: "°", color: Math.abs(engineData.ignTrimCyl4) > 5 ? "#FF9800" : "#E91E63" },
                    { label: "ENGINE", value: engineData.engineRunning ? "RUN" : "OFF", unit: "", color: engineData.engineRunning ? "#4CAF50" : "#9E9E9E" },

                    // Row 6: Idle/Diag/Status
                    { label: "IDLE TGT", value: engineData.idleTargetRpm.toFixed(0), unit: "rpm", color: "#9E9E9E" },
                    { label: "IAC", value: engineData.idleValveDuty.toFixed(0), unit: "%", color: "#9E9E9E" },
                    { label: "ERRORS", value: engineData.errorCount.toFixed(0), unit: "", color: engineData.errorCount > 0 ? "#F44336" : "#4CAF50" },
                    { label: "SYNC", value: engineData.synced ? "OK" : "LOST", unit: "", color: engineData.synced ? "#4CAF50" : "#F44336" },
                    { label: "CONS", value: engineData.fuelConsumption.toFixed(1), unit: "L/h", color: "#9E9E9E" }
                ]

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 6
                    color: "#141414"
                    border.color: "#1e1e1e"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: "#666666"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.5
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 2

                            Text {
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 22
                                font.bold: true
                                font.family: "Roboto Mono, Consolas, monospace"
                            }

                            Text {
                                visible: modelData.unit !== ""
                                text: modelData.unit
                                color: "#555555"
                                font.pixelSize: 9
                                anchors.bottom: parent.children[0].bottom
                                anchors.bottomMargin: 2
                            }
                        }
                    }
                }
            }
        }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CRITICAL WARNINGS - Bottom Right (z=1001)
    // Only life-threatening: Overheat + Low Oil Pressure
    // ═══════════════════════════════════════════════════════════════════════

    Column {
        id: criticalWarnings
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        spacing: 6
        z: 1001

        // Overheat Warning
        Rectangle {
            visible: engineData.overheat
            width: 140
            height: 32
            radius: 16
            color: "#F44336"

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "🔥"
                    font.pixelSize: 12
                }

                Text {
                    text: "OVERHEAT"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.overheat
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 150 }
                NumberAnimation { to: 1.0; duration: 150 }
            }
        }

        // Low Oil Pressure Warning
        Rectangle {
            visible: engineData.lowOilPressure
            width: 160
            height: 32
            radius: 16
            color: "#F44336"

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "🛢"
                    font.pixelSize: 12
                }

                Text {
                    text: "LOW OIL P"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.lowOilPressure
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 100 }
                NumberAnimation { to: 1.0; duration: 100 }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // NON-CRITICAL WARNINGS - Top Center Toast (z=999)
    // CEL, Low Fuel, High Oil Temp - Auto-hide behavior
    // ═══════════════════════════════════════════════════════════════════════

    Column {
        id: toastWarnings
        anchors.top: modeSelector.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 6
        spacing: 4
        z: 999

        // CEL Warning
        Rectangle {
            visible: engineData.celOn
            width: celText.width + 28
            height: 28
            radius: 14
            color: "#FF9800"

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "⚠"
                    font.pixelSize: 12
                }

                Text {
                    id: celText
                    text: "CHECK ENGINE"
                    color: "#000000"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.celOn
                loops: Animation.Infinite
                NumberAnimation { to: 0.6; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }

        // Low Fuel Pressure Warning
        Rectangle {
            visible: engineData.lowFuelPressure
            width: lowFuelText.width + 28
            height: 28
            radius: 14
            color: "#FF9800"

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "⛽"
                    font.pixelSize: 12
                }

                Text {
                    id: lowFuelText
                    text: "LOW FUEL P"
                    color: "#000000"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.lowFuelPressure
                loops: Animation.Infinite
                NumberAnimation { to: 0.5; duration: 400 }
                NumberAnimation { to: 1.0; duration: 400 }
            }
        }

        // High Oil Temp Warning
        Rectangle {
            visible: engineData.oilTemp > 120
            width: highOilTempText.width + 28
            height: 28
            radius: 14
            color: engineData.oilTemp > 140 ? "#F44336" : "#FF9800"

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "🌡"
                    font.pixelSize: 12
                }

                Text {
                    id: highOilTempText
                    text: engineData.oilTemp > 140 ? "OIL CRITICAL" : "OIL TEMP"
                    color: engineData.oilTemp > 140 ? "#FFFFFF" : "#000000"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.oilTemp > 120
                loops: Animation.Infinite
                NumberAnimation { to: engineData.oilTemp > 140 ? 0.3 : 0.6; duration: engineData.oilTemp > 140 ? 100 : 400 }
                NumberAnimation { to: 1.0; duration: engineData.oilTemp > 140 ? 100 : 400 }
            }
        }
    }

}
