import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

/**
 * TestDashScreen.qml
 * Direct test of the enhanced DashScreen with all Speeduino parameters
 */
Window {
    id: testWindow
    visible: true
    width: 1024
    height: 600
    title: "Speeduino Dashboard Test"
    color: "#0a0a0a"

    // Mock Data Provider
    MockDataProvider {
        id: mockData
    }

    // Main content area - loads DashScreen directly
    Loader {
        id: dashLoader
        anchors.fill: parent

        source: "../src/hmi_launcher/qml/screens/DashScreen.qml"

        onLoaded: {
            console.log("DashScreen loaded successfully!")
            if (item) {
                // Bind engineData
                item.engineData = Qt.binding(function() {
                    return {
                        rpm: mockData.rpm,
                        coolantTemp: mockData.coolantTemp,
                        intakeTemp: mockData.intakeTemp,
                        tps: mockData.tps,
                        mapKpa: mockData.mapKpa,
                        lambda: mockData.lambda,
                        ignitionAdvance: mockData.ignitionAdvance,
                        injectorDuty: mockData.injectorDuty,
                        vehicleSpeed: mockData.vehicleSpeed,
                        gear: mockData.gear,
                        canOk: mockData.canConnected,
                        celOn: mockData.celOn,
                        overheat: mockData.overheat,
                        batteryVoltage: mockData.batteryVoltage,
                        oilPressure: mockData.oilPressure,
                        oilTemp: mockData.oilTemp,
                        boostPsi: mockData.boostPsi,
                        veValue: mockData.veValue,
                        sparkDwell: mockData.sparkDwell,
                        loopsPerSec: mockData.loopsPerSec,
                        freeRam: mockData.freeRam,
                        targetAfr: mockData.targetAfr,
                        afrCorrection: mockData.afrCorrection
                    }
                })
            }
        }

        onStatusChanged: {
            if (status === Loader.Error) {
                console.error("Failed to load DashScreen!")
            }
        }
    }

    // Control overlay (smaller, bottom right)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10
        width: 220
        height: 180
        color: "#2a2a2a"
        radius: 8
        opacity: 0.95

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text {
                text: "Controls"
                color: "#00ff00"
                font.bold: true
                font.pixelSize: 14
            }

            Row {
                spacing: 6

                Button {
                    text: "Idle"
                    width: 60
                    onClicked: mockData.setIdle()
                }
                Button {
                    text: "Rev"
                    width: 60
                    onClicked: mockData.rev(6000)
                }
                Button {
                    text: "Drive"
                    width: 60
                    onClicked: mockData.drive(4, 60)
                }
            }

            Row {
                spacing: 6

                Button {
                    text: mockData.celOn ? "CEL ON" : "CEL"
                    width: 60
                    palette.button: mockData.celOn ? "#ff6600" : "#444"
                    onClicked: mockData.toggleCel()
                }
                Button {
                    text: "HOT"
                    width: 60
                    onClicked: mockData.testOverheat()
                }
                Button {
                    text: "Reset"
                    width: 60
                    onClicked: mockData.reset()
                }
            }

            // RPM and Speed display
            Row {
                spacing: 20
                Text {
                    text: "RPM: " + mockData.rpm.toFixed(0)
                    color: "#0f0"
                    font.pixelSize: 12
                    font.family: "monospace"
                }
                Text {
                    text: mockData.vehicleSpeed.toFixed(0) + " km/h"
                    color: "#0af"
                    font.pixelSize: 12
                    font.family: "monospace"
                }
            }

            // Gear selector
            Row {
                spacing: 4
                Repeater {
                    model: ["N", "1", "2", "3", "4", "5", "6"]
                    Button {
                        text: modelData
                        width: 26
                        height: 26
                        highlighted: mockData.gear === index
                        onClicked: mockData.gear = index
                    }
                }
            }
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Space"
        onActivated: mockData.rev(7000)
    }
    Shortcut {
        sequence: "Escape"
        onActivated: mockData.setIdle()
    }
    Shortcut {
        sequence: "1"
        onActivated: dashLoader.item.displayMode = 0
    }
    Shortcut {
        sequence: "2"
        onActivated: dashLoader.item.displayMode = 1
    }
    Shortcut {
        sequence: "3"
        onActivated: dashLoader.item.displayMode = 2
    }
    Shortcut {
        sequence: "4"
        onActivated: dashLoader.item.displayMode = 3
    }

    Component.onCompleted: {
        console.log("TestDashScreen started")
        console.log("Press 1-4 to change display modes")
        console.log("Press Space to rev, Escape to idle")
    }
}
