import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import "components"
import "screens"

/**
 * ImprovedPreview.qml - Complete dashboard preview with all new components
 *
 * Features:
 * - All new visual components (ArcGauge, CompactGauge, ShiftLightBar, etc.)
 * - Multiple display modes (Sport, Eco, Minimal, Full)
 * - Realistic engine simulation
 * - Interactive control panel
 * - Keyboard shortcuts
 *
 * Usage: qml ImprovedPreview.qml
 *
 * Keyboard shortcuts:
 *   1-4: Switch screens (Home, Dash, Settings, Auto)
 *   M: Cycle display modes (Sport, Eco, Minimal, Full)
 *   C: Toggle CEL warning
 *   T: Toggle overheat warning
 *   R: Toggle reverse gear
 *   Space: Rev engine
 *   Esc: Return to idle
 */
Window {
    id: window
    visible: true
    width: 800
    height: 480
    title: "Speeduino Dashboard - Improved Preview"
    color: "#0a0a0a"

    // ═══════════════════════════════════════════════════════════════════════
    // MOCK ENGINE DATA
    // ═══════════════════════════════════════════════════════════════════════

    QtObject {
        id: engineData

        // Engine parameters
        property real rpm: 850
        property real targetRpm: 850
        property real speed: 0
        property real targetSpeed: 0
        property real coolantTemp: 85
        property real mapKpa: 35
        property real tps: 0
        property real afr: 14.7
        property real boostPsi: -0.5
        property real oilTemp: 90
        property real oilPressure: 45
        property real fuelPressure: 43
        property real iat: 35
        property real batteryVoltage: 14.2
        property real ignitionAdvance: 15
        property int gear: 0

        // Status flags
        property bool canConnected: true
        property bool celOn: false
        property bool overheat: false
        property bool lowFuel: false
        property bool isRevving: false

        // Config
        property real redlineRpm: 7000
        property real shiftRpm: 6500

        // Display mode: 0=Sport, 1=Eco, 2=Minimal, 3=Full
        property int displayMode: 0

        property var modeNames: ["SPORT", "ECO", "MINIMAL", "FULL"]
    }

    // Engine simulation timer
    Timer {
        interval: 50
        running: true
        repeat: true
        onTriggered: {
            // Smooth RPM transition
            var rpmDiff = engineData.targetRpm - engineData.rpm
            engineData.rpm += rpmDiff * 0.15

            // Add realistic fluctuation at idle
            if (Math.abs(engineData.targetRpm - 850) < 50) {
                engineData.rpm += (Math.random() - 0.5) * 20
            }

            // Clamp RPM
            engineData.rpm = Math.max(0, Math.min(engineData.redlineRpm, engineData.rpm))

            // Speed follows RPM loosely (simulated gear ratio)
            if (engineData.gear > 0) {
                var gearRatios = [0, 12, 8, 6, 4.5, 3.5, 2.8]
                engineData.targetSpeed = engineData.rpm / gearRatios[engineData.gear]
            } else if (engineData.gear === 0) {
                engineData.targetSpeed = 0
            } else {
                // Reverse
                engineData.targetSpeed = engineData.rpm / 15
            }

            var speedDiff = engineData.targetSpeed - engineData.speed
            engineData.speed += speedDiff * 0.1
            engineData.speed = Math.max(0, engineData.speed)

            // Update related parameters based on RPM
            var rpmPercent = engineData.rpm / engineData.redlineRpm

            // TPS follows revving
            engineData.tps = engineData.isRevving ? 75 + Math.random() * 20 : 2 + Math.random() * 3

            // MAP increases with RPM and TPS
            engineData.mapKpa = 35 + (engineData.tps * 2) + (rpmPercent * 50)

            // AFR varies with load
            engineData.afr = 14.7 - (engineData.tps * 0.02) + (Math.random() - 0.5) * 0.2

            // Boost (negative is vacuum, positive is boost)
            engineData.boostPsi = (engineData.mapKpa - 101.3) / 6.895

            // Ignition advance varies with RPM and load
            engineData.ignitionAdvance = 10 + (rpmPercent * 25) - (engineData.tps * 0.1)

            // Small temperature variations
            engineData.coolantTemp += (Math.random() - 0.5) * 0.1
            engineData.oilTemp += (Math.random() - 0.5) * 0.1

            // Oil pressure varies with RPM
            engineData.oilPressure = 20 + (rpmPercent * 40) + (Math.random() - 0.5) * 2

            // Auto gear calculation based on speed (very simplified)
            if (engineData.gear > 0 && !engineData.isRevving) {
                if (engineData.speed > 180 && engineData.gear < 6) engineData.gear = 6
                else if (engineData.speed > 140 && engineData.gear < 5) engineData.gear = 5
                else if (engineData.speed > 100 && engineData.gear < 4) engineData.gear = 4
                else if (engineData.speed > 60 && engineData.gear < 3) engineData.gear = 3
                else if (engineData.speed > 30 && engineData.gear < 2) engineData.gear = 2
                else if (engineData.speed > 0 && engineData.gear < 1) engineData.gear = 1
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CURRENT SCREEN STATE
    // ═══════════════════════════════════════════════════════════════════════

    property int currentScreen: 0  // 0=Home, 1=Dash, 2=Settings, 3=Auto

    // ═══════════════════════════════════════════════════════════════════════
    // MAIN LAYOUT
    // ═══════════════════════════════════════════════════════════════════════

    // Status Bar (topo - fino)
    StatusBar {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        canConnected: engineData.canConnected
        celOn: engineData.celOn
        overheat: engineData.overheat
        lowFuel: engineData.lowFuel
        displayMode: engineData.modeNames[engineData.displayMode]
    }

    // Nav Bar (rodapé - fino)
    NavBar {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        currentIndex: window.currentScreen
        onTabClicked: function(index) {
            window.currentScreen = index
        }
    }

    // Content Area (centro - máximo possível)
    Item {
        id: contentArea
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: navBar.top

            // Home Screen
            HomeScreen {
                anchors.fill: parent
                visible: currentScreen === 0
                rpm: engineData.rpm
                speed: engineData.speed
                coolantTemp: engineData.coolantTemp
                canConnected: engineData.canConnected
                onNavigateTo: function(screen) {
                    currentScreen = screen
                    navBar.currentIndex = screen
                }
            }

            // Dash Screen
            DashScreen {
                anchors.fill: parent
                visible: currentScreen === 1
                rpm: engineData.rpm
                speed: engineData.speed
                coolantTemp: engineData.coolantTemp
                mapKpa: engineData.mapKpa
                tps: engineData.tps
                afr: engineData.afr
                boostPsi: engineData.boostPsi
                oilTemp: engineData.oilTemp
                oilPressure: engineData.oilPressure
                fuelPressure: engineData.fuelPressure
                iat: engineData.iat
                batteryVoltage: engineData.batteryVoltage
                ignitionAdvance: engineData.ignitionAdvance
                gear: engineData.gear
                redlineRpm: engineData.redlineRpm
                shiftRpm: engineData.shiftRpm
                displayMode: engineData.displayMode
            }

            // Settings Screen (placeholder)
            Rectangle {
                anchors.fill: parent
                visible: currentScreen === 2
                color: "#0a0a0a"

                Column {
                    anchors.centerIn: parent
                    spacing: 20

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Settings"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 32
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }

                    // Display mode selector
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8

                        Text {
                            text: "Display Mode:"
                            color: Qt.rgba(1, 1, 1, 0.72)
                            font.pixelSize: 14
                            font.family: "Roboto, sans-serif"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Repeater {
                            model: engineData.modeNames

                            Rectangle {
                                width: 80
                                height: 36
                                radius: 8
                                color: engineData.displayMode === index ? "#00E676" : "#1a1a1a"
                                border.color: engineData.displayMode === index ? "#00E676" : "#333333"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: engineData.displayMode === index ? "#000000" : Qt.rgba(1, 1, 1, 0.72)
                                    font.pixelSize: 12
                                    font.bold: true
                                    font.family: "Roboto, sans-serif"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: engineData.displayMode = index
                                }
                            }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Press 'M' to cycle modes"
                        color: Qt.rgba(1, 1, 1, 0.38)
                        font.pixelSize: 12
                        font.family: "Roboto, sans-serif"
                    }
                }
            }

            // Android Auto Screen (placeholder)
            Rectangle {
                anchors.fill: parent
                visible: currentScreen === 3
                color: "#0a0a0a"

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 80
                        height: 80
                        radius: 40
                        color: "#4285F4"
                        opacity: 0.2

                        Text {
                            anchors.centerIn: parent
                            text: "A"
                            color: "#4285F4"
                            font.pixelSize: 40
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Android Auto"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 28
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Connect phone via USB"
                        color: Qt.rgba(1, 1, 1, 0.56)
                        font.pixelSize: 14
                        font.family: "Roboto, sans-serif"
                    }
                }
            }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // KEYBOARD SHORTCUTS
    // ═══════════════════════════════════════════════════════════════════════

    // Screen navigation
    Shortcut { sequence: "1"; onActivated: { currentScreen = 0; navBar.currentIndex = 0 } }
    Shortcut { sequence: "2"; onActivated: { currentScreen = 1; navBar.currentIndex = 1 } }
    Shortcut { sequence: "3"; onActivated: { currentScreen = 2; navBar.currentIndex = 2 } }
    Shortcut { sequence: "4"; onActivated: { currentScreen = 3; navBar.currentIndex = 3 } }

    // Display mode
    Shortcut {
        sequence: "M"
        onActivated: {
            engineData.displayMode = (engineData.displayMode + 1) % 4
        }
    }

    // Warning toggles
    Shortcut { sequence: "C"; onActivated: engineData.celOn = !engineData.celOn }
    Shortcut { sequence: "T"; onActivated: engineData.overheat = !engineData.overheat }
    Shortcut { sequence: "F"; onActivated: engineData.lowFuel = !engineData.lowFuel }

    // Reverse gear toggle
    Shortcut {
        sequence: "R"
        onActivated: {
            if (engineData.gear === -1) {
                engineData.gear = 0
            } else {
                engineData.gear = -1
                engineData.targetRpm = 1500
            }
        }
    }

    // Rev engine
    Shortcut {
        sequence: "Space"
        onActivated: {
            engineData.isRevving = true
            if (engineData.gear === 0) engineData.gear = 1
            engineData.targetRpm = engineData.redlineRpm * 0.95
        }
    }

    // Return to idle
    Shortcut {
        sequence: "Escape"
        onActivated: {
            engineData.isRevving = false
            engineData.targetRpm = 850
            engineData.gear = 0
        }
    }

    // Gear up/down
    Shortcut {
        sequence: "Up"
        onActivated: {
            if (engineData.gear < 6 && engineData.gear >= 0) {
                engineData.gear++
            }
        }
    }

    Shortcut {
        sequence: "Down"
        onActivated: {
            if (engineData.gear > 0) {
                engineData.gear--
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // STARTUP INFO
    // ═══════════════════════════════════════════════════════════════════════

    Component.onCompleted: {
        console.log("╔════════════════════════════════════════════════════════════╗")
        console.log("║           Speeduino Dashboard - Improved Preview           ║")
        console.log("╠════════════════════════════════════════════════════════════╣")
        console.log("║  Keyboard Shortcuts:                                       ║")
        console.log("║    1-4      : Switch screens (Home/Dash/Settings/Auto)     ║")
        console.log("║    M        : Cycle display modes (Sport/Eco/Minimal/Full) ║")
        console.log("║    C        : Toggle CEL warning                           ║")
        console.log("║    T        : Toggle overheat warning                      ║")
        console.log("║    F        : Toggle low fuel warning                      ║")
        console.log("║    R        : Toggle reverse gear                          ║")
        console.log("║    Space    : Rev engine                                   ║")
        console.log("║    Up/Down  : Shift gear                                   ║")
        console.log("║    Escape   : Return to idle                               ║")
        console.log("╚════════════════════════════════════════════════════════════╝")
    }
}
