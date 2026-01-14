import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

/**
 * PreviewMain.qml
 * Standalone preview runner for UI development on Windows
 *
 * Usage:
 *   qml6 PreviewMain.qml
 *   or
 *   C:\Qt\6.x.x\mingw_64\bin\qml.exe PreviewMain.qml
 *
 * Features:
 * - Full UI preview with mock data
 * - Control panel for simulation
 * - Hot reload friendly
 */
Window {
    id: previewWindow
    visible: true
    width: 1000
    height: 600
    title: "Speeduino UI Preview"
    color: "#1a1a1a"

    // Load mock data provider
    MockDataProvider {
        id: mockDataProvider
    }

    // Load styles
    property var theme: Qt.createComponent("../src/hmi_launcher/qml/styles/Theme.qml").createObject(previewWindow)

    // Update theme dimensions
    Component.onCompleted: {
        if (theme) {
            theme.windowWidth = Qt.binding(function() { return mainContent.width })
            theme.windowHeight = Qt.binding(function() { return mainContent.height })
        }
        console.log("Preview started - Window:", width, "x", height)
    }

    // Main layout: Content + Control Panel
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ═══════════════════════════════════════════════════════════════
        // MAIN UI CONTENT (simulates actual app window)
        // ═══════════════════════════════════════════════════════════════
        Rectangle {
            id: mainContent
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 800
            color: "#0a0a0a"

            // Simulated screen size selector
            property int simWidth: 800
            property int simHeight: 480

            // Content area with simulated resolution
            Item {
                id: simulatedScreen
                anchors.centerIn: parent
                width: Math.min(parent.width, mainContent.simWidth)
                height: Math.min(parent.height, mainContent.simHeight)
                clip: true

                // Border to show screen bounds
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "#333"
                    border.width: 1
                }

                // Load MainContent.qml (Item, not Window - can be loaded via Loader)
                Loader {
                    id: mainLoader
                    anchors.fill: parent
                    source: "../src/hmi_launcher/qml/MainContent.qml"

                    // Inject mock providers
                    onLoaded: {
                        if (item) {
                            console.log("PreviewMain: MainContent loaded successfully")
                            item.dataProvider = mockDataProvider
                            console.log("PreviewMain: dataProvider injected, rpm =", mockDataProvider.rpm)
                        }
                    }

                    onStatusChanged: {
                        console.log("PreviewMain: Loader status =", status)
                        if (status === Loader.Error) {
                            console.error("PreviewMain: Failed to load MainContent.qml")
                            errorText.visible = true
                            errorText.text = "Failed to load MainContent.qml\nCheck console for errors"
                        } else if (status === Loader.Ready) {
                            console.log("PreviewMain: Loader ready")
                        }
                    }
                }

                // Error display
                Text {
                    id: errorText
                    anchors.centerIn: parent
                    visible: false
                    text: "Failed to load UI\nCheck console for errors"
                    color: "#ff0000"
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                }

                // Resolution indicator
                Text {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 5
                    text: simulatedScreen.width + "x" + simulatedScreen.height
                    color: "#666"
                    font.pixelSize: 10
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════
        // CONTROL PANEL
        // ═══════════════════════════════════════════════════════════════
        Rectangle {
            id: controlPanel
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#2a2a2a"

            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    // Header
                    Text {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        text: "Control Panel"
                        color: "#00ff00"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

                    // ═══════════════════════════════════════════════════════════════
                    // RESOLUTION SELECTOR
                    // ═══════════════════════════════════════════════════════════════
                    Text {
                        Layout.leftMargin: 10
                        text: "Screen Size"
                        color: "#aaa"
                        font.pixelSize: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        columns: 2
                        rowSpacing: 5
                        columnSpacing: 5

                        Repeater {
                            model: [
                                { label: "7\" 800x480", w: 800, h: 480 },
                                { label: "10\" 1280x800", w: 1280, h: 800 },
                                { label: "5\" 800x480", w: 800, h: 480 },
                                { label: "Custom", w: 1024, h: 600 }
                            ]

                            Button {
                                Layout.fillWidth: true
                                text: modelData.label
                                onClicked: {
                                    mainContent.simWidth = modelData.w
                                    mainContent.simHeight = modelData.h
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

                    // ═══════════════════════════════════════════════════════════════
                    // ENGINE SIMULATION
                    // ═══════════════════════════════════════════════════════════════
                    Text {
                        Layout.leftMargin: 10
                        text: "Engine Simulation"
                        color: "#aaa"
                        font.pixelSize: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        columns: 2
                        rowSpacing: 5
                        columnSpacing: 5

                        Button {
                            Layout.fillWidth: true
                            text: "Idle"
                            onClicked: mockDataProvider.setIdle()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "Rev 5K"
                            onClicked: mockDataProvider.rev(5000)
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "Rev 7K"
                            onClicked: mockDataProvider.rev(7000)
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "Drive"
                            onClicked: mockDataProvider.drive(3, 50)
                        }
                    }

                    // RPM Slider
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10

                        Text { text: "RPM"; color: "#aaa"; font.pixelSize: 11 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 8000
                            value: mockDataProvider.targetRpm
                            onValueChanged: mockDataProvider.targetRpm = value
                        }
                        Text {
                            text: mockDataProvider.rpm.toFixed(0)
                            color: "#0f0"
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // TPS Slider
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10

                        Text { text: "TPS"; color: "#aaa"; font.pixelSize: 11 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 100
                            value: mockDataProvider.targetTps
                            onValueChanged: mockDataProvider.targetTps = value
                        }
                        Text {
                            text: mockDataProvider.tps.toFixed(0) + "%"
                            color: "#0f0"
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // Gear selector
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        spacing: 5

                        Text { text: "Gear"; color: "#aaa"; font.pixelSize: 11 }
                        Repeater {
                            model: ["N", "1", "2", "3", "4", "5", "6"]
                            Button {
                                text: modelData
                                width: 25
                                highlighted: mockDataProvider.gear === index
                                onClicked: mockDataProvider.gear = index
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

                    // ═══════════════════════════════════════════════════════════════
                    // STATUS TOGGLES
                    // ═══════════════════════════════════════════════════════════════
                    Text {
                        Layout.leftMargin: 10
                        text: "Status Toggles"
                        color: "#aaa"
                        font.pixelSize: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        columns: 2
                        rowSpacing: 5
                        columnSpacing: 5

                        Button {
                            Layout.fillWidth: true
                            text: mockDataProvider.celOn ? "CEL ON" : "CEL Off"
                            palette.button: mockDataProvider.celOn ? "#ff6600" : "#444"
                            onClicked: mockDataProvider.toggleCel()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: mockDataProvider.overheat ? "HOT!" : "Temp OK"
                            palette.button: mockDataProvider.overheat ? "#ff0000" : "#444"
                            onClicked: mockDataProvider.testOverheat()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: mockDataProvider.canConnected ? "CAN OK" : "CAN ERR"
                            palette.button: mockDataProvider.canConnected ? "#00ff00" : "#ff0000"
                            onClicked: mockDataProvider.toggleCanConnection()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: mockDataProvider.reverseEngaged ? "REVERSE" : "Forward"
                            palette.button: mockDataProvider.reverseEngaged ? "#ff0000" : "#444"
                            onClicked: mockDataProvider.toggleReverse()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

                    // ═══════════════════════════════════════════════════════════════
                    // LIVE VALUES
                    // ═══════════════════════════════════════════════════════════════
                    Text {
                        Layout.leftMargin: 10
                        text: "Live Values"
                        color: "#aaa"
                        font.pixelSize: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        columns: 2
                        rowSpacing: 2
                        columnSpacing: 10

                        Text { text: "RPM:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: mockDataProvider.rpm.toFixed(0); color: "#0f0"; font.pixelSize: 10; font.family: "monospace" }

                        Text { text: "Speed:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: mockDataProvider.vehicleSpeed.toFixed(0) + " km/h"; color: "#0af"; font.pixelSize: 10; font.family: "monospace" }

                        Text { text: "CLT:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: mockDataProvider.coolantTemp.toFixed(1) + "°C"; color: mockDataProvider.coolantTemp > 100 ? "#f00" : "#0f0"; font.pixelSize: 10; font.family: "monospace" }

                        Text { text: "MAP:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: mockDataProvider.mapKpa.toFixed(0) + " kPa"; color: "#f60"; font.pixelSize: 10; font.family: "monospace" }

                        Text { text: "AFR:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: (mockDataProvider.lambda * 14.7).toFixed(1); color: "#a0f"; font.pixelSize: 10; font.family: "monospace" }

                        Text { text: "IGN:"; color: "#888"; font.pixelSize: 10 }
                        Text { text: mockDataProvider.ignitionAdvance.toFixed(1) + "°"; color: "#f0a"; font.pixelSize: 10; font.family: "monospace" }
                    }

                    // Reset button
                    Button {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        text: "Reset All"
                        onClicked: mockDataProvider.reset()
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "R"
        onActivated: mockDataProvider.toggleReverse()
    }
    Shortcut {
        sequence: "C"
        onActivated: mockDataProvider.toggleCel()
    }
    Shortcut {
        sequence: "Space"
        onActivated: mockDataProvider.rev(6000)
    }
    Shortcut {
        sequence: "Escape"
        onActivated: mockDataProvider.setIdle()
    }
}
