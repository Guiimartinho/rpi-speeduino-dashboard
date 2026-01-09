import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import "../styles" as Styles
import "../state" as State
import "../components" as Components

/**
 * DashScreen.qml
 * Professional Speeduino ECU Dashboard with modern UI/UX
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
        targetAfr: 14.7, afrCorrection: 0
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
    // MODE SELECTOR - Floating pills
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: modeSelector
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        width: modeRow.width + 16
        height: 36
        radius: 18
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
                    width: 70
                    height: 28
                    radius: 14
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
                        font.pixelSize: 10
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
    // MODE 0: SPORT - Clean racing style
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 0
        anchors.topMargin: 50

        // Progressive Shift Light Bar
        Item {
            id: shiftBar
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.7
            height: 20

            Row {
                anchors.centerIn: parent
                spacing: 4

                Repeater {
                    model: 15

                    Rectangle {
                        width: 20
                        height: 12
                        radius: 3

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
                            anchors.margins: -4
                            radius: 6
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

        // Main gauges area
        RowLayout {
            anchors.top: shiftBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: bottomBar.top
            anchors.margins: 16
            anchors.topMargin: 8
            spacing: 20

            // RPM Gauge
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.38

                Components.ArcGauge {
                    anchors.centerIn: parent
                    gaugeSize: Math.min(parent.width, parent.height) - 20
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

            // Center - Gear + Speed digital
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.24

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    // Large Gear Display
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 100
                        height: 100
                        radius: 16
                        color: "#141414"
                        border.color: engineData.gear === 0 ? "#FF9800" : "#00E676"
                        border.width: 2

                        Text {
                            anchors.centerIn: parent
                            text: engineData.gear === 0 ? "N" : engineData.gear === -1 ? "R" : engineData.gear.toString()
                            color: engineData.gear === 0 ? "#FF9800" : engineData.gear === -1 ? "#F44336" : "#00E676"
                            font.pixelSize: 56
                            font.bold: true
                            font.family: "Roboto Mono, Consolas, monospace"
                        }

                        // Glow
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -6
                            radius: 20
                            color: "transparent"
                            border.color: parent.border.color
                            border.width: 1
                            opacity: 0.3
                            z: -1
                        }
                    }

                    // Digital Speed
                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 0

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: engineData.vehicleSpeed.toFixed(0)
                            color: "#00B8D4"
                            font.pixelSize: 48
                            font.bold: true
                            font.family: "Roboto Mono, Consolas, monospace"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "km/h"
                            color: "#666666"
                            font.pixelSize: 14
                            font.letterSpacing: 2
                        }
                    }
                }
            }

            // Speed Gauge
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.38

                Components.ArcGauge {
                    anchors.centerIn: parent
                    gaugeSize: Math.min(parent.width, parent.height) - 20
                    value: engineData.vehicleSpeed
                    minValue: 0
                    maxValue: 280
                    warningValue: 200
                    criticalValue: 250
                    label: "SPEED"
                    unit: "km/h"
                    decimals: 0
                    accentColor: "#00B8D4"
                    majorTickCount: 7
                    showLabels: !compactMode
                }
            }
        }

        // Bottom status bar
        Rectangle {
            id: bottomBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            height: 70
            radius: 12
            color: "#141414"
            border.color: "#1e1e1e"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Gauge items
                Repeater {
                    model: [
                        { label: "CLT", value: engineData.coolantTemp.toFixed(0), unit: "°C",
                          color: engineData.coolantTemp > 100 ? "#F44336" : engineData.coolantTemp > 95 ? "#FF9800" : "#00E676",
                          warn: engineData.coolantTemp > 95 },
                        { label: "MAP", value: isBoost ? boostPsi.toFixed(1) : engineData.mapKpa.toFixed(0),
                          unit: isBoost ? "psi" : "kPa", color: "#FF9800", warn: false },
                        { label: "AFR", value: afr.toFixed(1), unit: "", color: "#AA00FF", warn: afr > 16 || afr < 11 },
                        { label: "IGN", value: engineData.ignitionAdvance.toFixed(1), unit: "°", color: "#E91E63", warn: false },
                        { label: "INJ", value: engineData.injectorDuty.toFixed(0), unit: "%",
                          color: engineData.injectorDuty > 85 ? "#F44336" : "#FF9800", warn: engineData.injectorDuty > 85 },
                        { label: "BATT", value: engineData.batteryVoltage.toFixed(1), unit: "V",
                          color: engineData.batteryVoltage < 12 ? "#F44336" : "#00E676", warn: engineData.batteryVoltage < 12 }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: modelData.warn ? Qt.rgba(244/255, 67/255, 54/255, 0.15) : "#1a1a1a"
                        border.color: modelData.warn ? "#F44336" : "#252525"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: "#666666"
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 1
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
                                    text: modelData.unit
                                    color: "#555555"
                                    font.pixelSize: 11
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
    // MODE 1: STREET - Minimal, speed focused
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 1
        anchors.topMargin: 50

        // Giant speed display
        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -20
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: engineData.vehicleSpeed.toFixed(0)
                color: "#00B8D4"
                font.pixelSize: Math.min(parent.parent.width * 0.25, 160)
                font.bold: true
                font.family: "Roboto Mono, Consolas, monospace"

                // Subtle glow
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
            anchors.margins: 20
            height: 50
            radius: 12
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16

                Text {
                    text: "RPM"
                    color: "#666666"
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1
                }

                // RPM Bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 6
                    color: "#1a1a1a"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * Math.min(1, engineData.rpm / 8000)
                        radius: 6

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
                    font.pixelSize: 24
                    font.bold: true
                    font.family: "Roboto Mono, Consolas, monospace"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Bottom info strip
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 20
            height: 60
            radius: 12
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 32

                Repeater {
                    model: [
                        { label: "GEAR", value: engineData.gear === 0 ? "N" : engineData.gear.toString(),
                          color: engineData.gear === 0 ? "#FF9800" : "#00B8D4" },
                        { label: "CLT", value: engineData.coolantTemp.toFixed(0) + "°",
                          color: engineData.coolantTemp > 100 ? "#F44336" : "#00E676" },
                        { label: "AFR", value: afr.toFixed(1), color: "#AA00FF" },
                        { label: "BATT", value: engineData.batteryVoltage.toFixed(1) + "V",
                          color: engineData.batteryVoltage < 12 ? "#F44336" : "#00E676" }
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
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 24
                                font.bold: true
                                font.family: "Roboto Mono, Consolas, monospace"
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 2: TRACK - All critical data visible
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 2
        anchors.topMargin: 50

        GridLayout {
            anchors.fill: parent
            anchors.margins: 12
            columns: 4
            rows: 3
            columnSpacing: 8
            rowSpacing: 8

            // Row 1: Main gauges
            GaugeCard {
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

            GaugeCard {
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

            GaugeCard {
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
            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "CLT"
                value: engineData.coolantTemp.toFixed(0)
                unit: "°C"
                progress: engineData.coolantTemp / 120
                accentColor: engineData.coolantTemp > 100 ? "#F44336" : engineData.coolantTemp > 95 ? "#FF9800" : "#00E676"
                warning: engineData.coolantTemp > 95
                showBar: true
            }

            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IAT"
                value: engineData.intakeTemp.toFixed(0)
                unit: "°C"
                progress: (engineData.intakeTemp + 20) / 100
                accentColor: engineData.intakeTemp > 50 ? "#FF9800" : "#00B8D4"
                showBar: true
            }

            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "MAP"
                value: engineData.mapKpa.toFixed(0)
                unit: "kPa"
                progress: engineData.mapKpa / 250
                accentColor: "#FF9800"
                showBar: true
            }

            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: isBoost ? "BOOST" : "VAC"
                value: boostPsi.toFixed(1)
                unit: "psi"
                accentColor: isBoost ? "#FF9800" : "#00B8D4"
            }

            // Row 3: Fuel & ignition
            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "AFR"
                value: afr.toFixed(1)
                unit: ""
                accentColor: afr > 16 || afr < 11 ? "#F44336" : "#AA00FF"
                warning: afr > 16 || afr < 11
            }

            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "TPS"
                value: engineData.tps.toFixed(0)
                unit: "%"
                progress: engineData.tps / 100
                accentColor: "#00E676"
                showBar: true
            }

            GaugeCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IGN"
                value: engineData.ignitionAdvance.toFixed(1)
                unit: "°"
                accentColor: "#E91E63"
            }

            GaugeCard {
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
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE 3: DIAGNOSTIC - Full ECU data
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        anchors.fill: parent
        visible: displayMode === 3
        anchors.topMargin: 50

        GridLayout {
            anchors.fill: parent
            anchors.margins: 8
            columns: 5
            rows: 4
            columnSpacing: 6
            rowSpacing: 6

            Repeater {
                model: [
                    { label: "RPM", value: engineData.rpm.toFixed(0), unit: "", color: "#00E676" },
                    { label: "SPEED", value: engineData.vehicleSpeed.toFixed(0), unit: "km/h", color: "#00B8D4" },
                    { label: "GEAR", value: engineData.gear === 0 ? "N" : engineData.gear.toString(), unit: "", color: "#00B8D4" },
                    { label: "CLT", value: engineData.coolantTemp.toFixed(0), unit: "°C", color: engineData.coolantTemp > 95 ? "#F44336" : "#00E676" },
                    { label: "IAT", value: engineData.intakeTemp.toFixed(0), unit: "°C", color: "#00B8D4" },

                    { label: "MAP", value: engineData.mapKpa.toFixed(0), unit: "kPa", color: "#FF9800" },
                    { label: "TPS", value: engineData.tps.toFixed(0), unit: "%", color: "#00E676" },
                    { label: "AFR", value: afr.toFixed(1), unit: "", color: "#AA00FF" },
                    { label: "LAMBDA", value: engineData.lambda.toFixed(2), unit: "", color: "#AA00FF" },
                    { label: "AFR TGT", value: engineData.targetAfr.toFixed(1), unit: "", color: "#9C27B0" },

                    { label: "IGN ADV", value: engineData.ignitionAdvance.toFixed(1), unit: "°", color: "#E91E63" },
                    { label: "DWELL", value: engineData.sparkDwell.toFixed(1), unit: "ms", color: "#E91E63" },
                    { label: "INJ DC", value: engineData.injectorDuty.toFixed(0), unit: "%", color: "#FF9800" },
                    { label: "VE", value: engineData.veValue.toFixed(0), unit: "%", color: "#00BCD4" },
                    { label: "AFR COR", value: engineData.afrCorrection.toFixed(0), unit: "%", color: "#9C27B0" },

                    { label: "BATT", value: engineData.batteryVoltage.toFixed(1), unit: "V", color: engineData.batteryVoltage < 12 ? "#F44336" : "#00E676" },
                    { label: "OIL T", value: engineData.oilTemp.toFixed(0), unit: "°C", color: "#FFB300" },
                    { label: "OIL P", value: engineData.oilPressure.toFixed(0), unit: "psi", color: "#FFB300" },
                    { label: "LOOPS", value: engineData.loopsPerSec.toFixed(0), unit: "/s", color: "#4CAF50" },
                    { label: "RAM", value: engineData.freeRam.toFixed(0), unit: "", color: "#4CAF50" }
                ]

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#141414"
                    border.color: "#1e1e1e"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: "#555555"
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 0.5
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 2

                            Text {
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 20
                                font.bold: true
                                font.family: "Roboto Mono, Consolas, monospace"
                            }

                            Text {
                                visible: modelData.unit !== ""
                                text: modelData.unit
                                color: "#444444"
                                font.pixelSize: 10
                                anchors.bottom: parent.children[0].bottom
                                anchors.bottomMargin: 2
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // WARNING OVERLAY
    // ═══════════════════════════════════════════════════════════════════════

    Column {
        anchors.top: modeSelector.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        spacing: 8
        z: 1000

        Rectangle {
            visible: engineData.celOn
            width: celText.width + 32
            height: 36
            radius: 18
            color: "#FF9800"

            Row {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "⚠"
                    font.pixelSize: 16
                }

                Text {
                    id: celText
                    text: "CHECK ENGINE"
                    color: "#000000"
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.celOn
                loops: Animation.Infinite
                NumberAnimation { to: 0.6; duration: 400 }
                NumberAnimation { to: 1.0; duration: 400 }
            }
        }

        Rectangle {
            visible: engineData.overheat
            width: overheatText.width + 32
            height: 36
            radius: 18
            color: "#F44336"

            Row {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "🔥"
                    font.pixelSize: 14
                }

                Text {
                    id: overheatText
                    text: "OVERHEAT"
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            SequentialAnimation on opacity {
                running: engineData.overheat
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 200 }
                NumberAnimation { to: 1.0; duration: 200 }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // GAUGE CARD COMPONENT
    // ═══════════════════════════════════════════════════════════════════════

    component GaugeCard: Rectangle {
        id: gaugeCard

        property string label: ""
        property string value: ""
        property string unit: ""
        property real progress: 0
        property color accentColor: "#00E676"
        property bool showBar: false
        property bool warning: false
        property bool large: false
        property bool centered: false

        radius: 10
        color: warning ? Qt.rgba(244/255, 67/255, 54/255, 0.12) : "#141414"
        border.color: warning ? "#F44336" : "#1e1e1e"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: large ? 8 : 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: gaugeCard.label
                color: "#555555"
                font.pixelSize: large ? 11 : 9
                font.bold: true
                font.letterSpacing: 1
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3

                Text {
                    text: gaugeCard.value
                    color: gaugeCard.accentColor
                    font.pixelSize: large ? 36 : 24
                    font.bold: true
                    font.family: "Roboto Mono, Consolas, monospace"
                }

                Text {
                    visible: gaugeCard.unit !== ""
                    text: gaugeCard.unit
                    color: "#444444"
                    font.pixelSize: large ? 14 : 10
                    anchors.bottom: parent.children[0].bottom
                    anchors.bottomMargin: large ? 4 : 2
                }
            }

            // Progress bar
            Rectangle {
                visible: gaugeCard.showBar
                anchors.horizontalCenter: parent.horizontalCenter
                width: gaugeCard.width - 24
                height: 6
                radius: 3
                color: "#1a1a1a"

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * Math.min(1, gaugeCard.progress)
                    radius: 3
                    color: gaugeCard.accentColor

                    Behavior on width { NumberAnimation { duration: 100 } }
                }
            }
        }
    }
}
