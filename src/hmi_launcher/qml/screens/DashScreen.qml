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

    property int displayMode: 0  // 0=Sport, 1=Street, 2=Tuning, 3=Diagnostic

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
                    { label: "SPORT", icon: "S" },
                    { label: "STREET", icon: "R" },
                    { label: "TUNING", icon: "T" },
                    { label: "DIAG", icon: "D" }
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

        // Progressive Shift Bar (estilo barra de progresso)
        Components.ProgressiveShiftBar {
            id: shiftBar
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 4
            width: parent.width * 0.85
            rpm: engineData.rpm
            minRpm: 2000
            shiftRpm: 7000
            showGlow: true
            showShiftLabel: true
            barHeight: 16
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
                font.pixelSize: Math.min(parent.parent.width * 0.22, 110)
                font.bold: true
                font.family: "Roboto Mono"

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

        // RPM Bar at top (Progressive style)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            height: 50
            radius: 10
            color: "#141414"

            Row {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Components.ProgressiveShiftBar {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 80
                    rpm: engineData.rpm
                    minRpm: 800
                    shiftRpm: 7000
                    barHeight: 20
                    showGlow: false
                    showShiftLabel: true
                    showDividers: false
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: engineData.rpm.toFixed(0)
                    color: engineData.rpm > 6500 ? "#FF9800" : "#00E676"
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Roboto Mono"
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
    // MODE 2: TUNING - VE Table, AFR Histogram, dados para tunagem
    // ═══════════════════════════════════════════════════════════════════════

    Loader {
        id: tuningModeLoader
        anchors.fill: parent
        anchors.topMargin: 42
        active: displayMode === 2
        sourceComponent: tuningModeComponent
    }

    Component {
        id: tuningModeComponent

        Item {
            // Toggle 2D/3D mode
            property int veMapMode: 0  // 0=2D, 1=3D

        // Header row com dados principais
        Rectangle {
            id: tuningHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            height: 40
            radius: 8
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 16

                // RPM
                Row {
                    spacing: 4
                    Text { text: "RPM:"; color: "#666666"; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: engineData.rpm.toFixed(0); color: "#00E676"; font.pixelSize: 18; font.bold: true; font.family: "Roboto Mono"; anchors.verticalCenter: parent.verticalCenter }
                }

                // MAP
                Row {
                    spacing: 4
                    Text { text: "MAP:"; color: "#666666"; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: engineData.mapKpa.toFixed(0) + " kPa"; color: "#FF9800"; font.pixelSize: 18; font.bold: true; font.family: "Roboto Mono"; anchors.verticalCenter: parent.verticalCenter }
                }

                // TPS
                Row {
                    spacing: 4
                    Text { text: "TPS:"; color: "#666666"; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: engineData.tps.toFixed(0) + "%"; color: "#00BCD4"; font.pixelSize: 18; font.bold: true; font.family: "Roboto Mono"; anchors.verticalCenter: parent.verticalCenter }
                }

                // VE
                Row {
                    spacing: 4
                    Text { text: "VE:"; color: "#666666"; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: engineData.ve.toFixed(0) + "%"; color: "#AA00FF"; font.pixelSize: 18; font.bold: true; font.family: "Roboto Mono"; anchors.verticalCenter: parent.verticalCenter }
                }

                Item { Layout.fillWidth: true }

                // SYNC indicator
                Rectangle {
                    width: 50
                    height: 24
                    radius: 12
                    color: engineData.synced ? "#4CAF50" : "#F44336"
                    Text {
                        anchors.centerIn: parent
                        text: "SYNC"
                        color: "#FFFFFF"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                // Toggle 2D/3D
                Row {
                    spacing: 2
                    Repeater {
                        model: ["2D", "3D"]
                        Rectangle {
                            width: 36
                            height: 24
                            radius: 6
                            color: veMapMode === index ? "#00AAFF" : "#333333"
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: veMapMode === index ? "#000000" : "#888888"
                                font.pixelSize: 10
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: veMapMode = index
                            }
                        }
                    }
                }
            }
        }

        // Main content - VE Table maximizado + sidebar compacta
        RowLayout {
            anchors.top: tuningHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            anchors.topMargin: 4
            spacing: 6

            // LEFT: VE Table (2D or 3D) - Maximizado
            Components.VETableMini {
                visible: veMapMode === 0
                Layout.fillHeight: true
                Layout.fillWidth: true
                rpm: engineData.rpm
                mapKpa: engineData.mapKpa
                veValue: engineData.ve
                gridSize: 8
                showValues: false
                showTrail: true
                showLabels: true
            }

            Components.VETable3D {
                visible: veMapMode === 1
                Layout.fillHeight: true
                Layout.fillWidth: true
                rpm: engineData.rpm
                mapKpa: engineData.mapKpa
                veValue: engineData.ve
                animated: true
            }

            // RIGHT: Sidebar compacta com AFR vertical + dados
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 110
                radius: 8
                color: "#141414"

                Column {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    // AFR Vertical Bar
                    Item {
                        width: parent.width
                        height: parent.height * 0.45

                        // Barra de fundo (escala 10-18)
                        Rectangle {
                            id: afrBarBg
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 16
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 4
                            width: 24
                            radius: 4
                            color: "#0a0a0a"
                            border.color: "#333333"
                            border.width: 1

                            // Zona rica (verde escuro) 10-12
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: parent.height * 0.25
                                color: "#1B5E20"
                                radius: 4
                            }

                            // Zona stoich (verde) 12-15
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: parent.height * 0.375
                                height: parent.height * 0.375
                                color: "#2E7D32"
                            }

                            // Zona lean (amarelo/vermelho) 15-18
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: parent.height * 0.25
                                color: "#E65100"
                                radius: 4
                            }

                            // Marcador de target
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: parent.height * (1 - (engineData.afrTarget - 10) / 8) - 2
                                width: parent.width + 8
                                height: 4
                                radius: 2
                                color: "#00AAFF"
                            }

                            // Marcador atual
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: Math.max(0, Math.min(parent.height - 8, parent.height * (1 - (afr - 10) / 8) - 4))
                                width: parent.width + 4
                                height: 8
                                radius: 4
                                color: (afr > 15 || afr < 11) ? "#F44336" : (afr > 14.2 || afr < 12) ? "#FF9800" : "#4CAF50"
                                border.color: "#FFFFFF"
                                border.width: 2
                            }
                        }

                        // Label AFR
                        Text {
                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "AFR"
                            color: "#888888"
                            font.pixelSize: 9
                            font.bold: true
                        }

                        // Valor AFR atual
                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: afrBarBg.verticalCenter
                            text: afr.toFixed(1)
                            color: (afr > 15 || afr < 11) ? "#F44336" : (afr > 14.2 || afr < 12) ? "#FF9800" : "#4CAF50"
                            font.pixelSize: 16
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // Escala
                        Column {
                            anchors.left: parent.left
                            anchors.top: afrBarBg.top
                            anchors.bottom: afrBarBg.bottom
                            width: 18

                            Text { text: "18"; color: "#666"; font.pixelSize: 8; height: parent.height / 4; verticalAlignment: Text.AlignTop }
                            Text { text: "15"; color: "#666"; font.pixelSize: 8; height: parent.height / 4; verticalAlignment: Text.AlignVCenter }
                            Text { text: "12"; color: "#666"; font.pixelSize: 8; height: parent.height / 4; verticalAlignment: Text.AlignVCenter }
                            Text { text: "10"; color: "#666"; font.pixelSize: 8; height: parent.height / 4; verticalAlignment: Text.AlignBottom }
                        }
                    }

                    // Separator
                    Rectangle { width: parent.width; height: 1; color: "#2a2a2a" }

                    // Dados compactos em grid
                    Grid {
                        width: parent.width
                        columns: 2
                        rowSpacing: 2
                        columnSpacing: 4

                        // IGN
                        Text { text: "IGN"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.ignitionAdvance.toFixed(0) + "°"
                            color: "#E91E63"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // CLT
                        Text { text: "CLT"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.coolantTemp.toFixed(0) + "°"
                            color: engineData.coolantTemp > 100 ? "#F44336" : "#00E676"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // IAT
                        Text { text: "IAT"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.intakeTemp.toFixed(0) + "°"
                            color: engineData.intakeTemp > 50 ? "#FF9800" : "#00BCD4"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // INJ%
                        Text { text: "INJ"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.injectorDuty.toFixed(0) + "%"
                            color: engineData.injectorDuty > 85 ? "#F44336" : "#FF9800"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // Lambda
                        Text { text: "λ"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.lambda.toFixed(2)
                            color: "#AA00FF"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }

                        // TGT (AFR Target)
                        Text { text: "TGT"; color: "#666"; font.pixelSize: 8; font.bold: true }
                        Text {
                            text: engineData.afrTarget.toFixed(1)
                            color: "#00AAFF"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Roboto Mono"
                        }
                    }
                }
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
            anchors.fill: parent

            // Reference to parent's engineData
            property var eData: dashScreen.engineData
            property real afrVal: dashScreen.afr

            GridLayout {
                anchors.fill: parent
                anchors.margins: 6
                columns: 5
                rows: 6
                columnSpacing: 4
                rowSpacing: 4

                // Row 1
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "RPM"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData ? eData.rpm.toFixed(0) : "0"; color: "#00E676"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "SPEED"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.vehicleSpeed.toFixed(0) : "0"; color: "#00B8D4"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " km/h"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "GEAR"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData ? (eData.gear === 0 ? "N" : eData.gear.toString()) : "N"; color: "#00B8D4"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "CLT"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.coolantTemp.toFixed(0) : "0"; color: eData && eData.coolantTemp > 110 ? "#F44336" : eData && eData.coolantTemp > 100 ? "#FF9800" : "#00E676"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °C"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "IAT"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.intakeTemp.toFixed(0) : "0"; color: "#00B8D4"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °C"; color: "#555"; font.pixelSize: 9 } }
                    }
                }

                // Row 2
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "MAP"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.mapKpa.toFixed(0) : "0"; color: "#FF9800"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " kPa"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "TPS"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.tps.toFixed(0) : "0"; color: "#00E676"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "AFR"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: afrVal ? afrVal.toFixed(1) : "14.7"; color: "#AA00FF"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "AFR TGT"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData ? eData.afrTarget.toFixed(1) : "14.7"; color: "#9C27B0"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "VE"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ve.toFixed(0) : "0"; color: "#00BCD4"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }

                // Row 3
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "IGN ADV"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ignitionAdvance.toFixed(1) : "0"; color: "#E91E63"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "INJ DC"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.injectorDuty.toFixed(0) : "0"; color: "#FF9800"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "BATT"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.batteryVoltage.toFixed(1) : "12.0"; color: "#00E676"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " V"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "FUEL P"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? (eData.fuelPressure / 100).toFixed(1) : "0"; color: "#00BCD4"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " bar"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "OIL P"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? (eData.oilPressure / 100).toFixed(1) : "0"; color: "#FFB300"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " bar"; color: "#555"; font.pixelSize: 9 } }
                    }
                }

                // Row 4
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "F TRM 1"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.fuelTrimCyl1.toFixed(0) : "0"; color: "#4CAF50"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "F TRM 2"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.fuelTrimCyl2.toFixed(0) : "0"; color: "#4CAF50"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "F TRM 3"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.fuelTrimCyl3.toFixed(0) : "0"; color: "#4CAF50"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "F TRM 4"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.fuelTrimCyl4.toFixed(0) : "0"; color: "#4CAF50"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "OIL T"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.oilTemp.toFixed(0) : "0"; color: "#FFB300"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °C"; color: "#555"; font.pixelSize: 9 } }
                    }
                }

                // Row 5
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "I TRM 1"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ignTrimCyl1.toFixed(1) : "0"; color: "#E91E63"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "I TRM 2"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ignTrimCyl2.toFixed(1) : "0"; color: "#E91E63"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "I TRM 3"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ignTrimCyl3.toFixed(1) : "0"; color: "#E91E63"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "I TRM 4"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.ignTrimCyl4.toFixed(1) : "0"; color: "#E91E63"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " °"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "ENGINE"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData && eData.engineRunning ? "RUN" : "OFF"; color: eData && eData.engineRunning ? "#4CAF50" : "#9E9E9E"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }

                // Row 6
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "IDLE TGT"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.idleTargetRpm.toFixed(0) : "0"; color: "#9E9E9E"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " rpm"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "IAC"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.idleValveDuty.toFixed(0) : "0"; color: "#9E9E9E"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " %"; color: "#555"; font.pixelSize: 9 } }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "ERRORS"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData ? eData.errorCount.toFixed(0) : "0"; color: eData && eData.errorCount > 0 ? "#F44336" : "#4CAF50"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "SYNC"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: eData && eData.synced ? "OK" : "LOST"; color: eData && eData.synced ? "#4CAF50" : "#F44336"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 6; color: "#141414"; border.color: "#1e1e1e"
                    Column { anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "CONS"; color: "#666"; font.pixelSize: 10; font.bold: true }
                        Row { anchors.horizontalCenter: parent.horizontalCenter; Text { text: eData ? eData.fuelConsumption.toFixed(1) : "0"; color: "#9E9E9E"; font.pixelSize: 20; font.bold: true; font.family: "Roboto Mono" } Text { text: " L/h"; color: "#555"; font.pixelSize: 9 } }
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

        // Low Oil Pressure Warning (disabled - no real data yet)
        Rectangle {
            visible: false // engineData.lowOilPressure
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

        // Low Fuel Pressure Warning (disabled - no real data yet)
        Rectangle {
            visible: false // engineData.lowFuelPressure
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
