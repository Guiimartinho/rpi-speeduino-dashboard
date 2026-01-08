import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

/**
 * StandalonePreview.qml
 * Standalone preview that doesn't depend on main project files
 * Use this if PreviewMain.qml has import errors
 *
 * Usage: qml StandalonePreview.qml
 */
Window {
    id: window
    visible: true
    width: 800
    height: 480
    title: "Speeduino UI - Standalone Preview"
    color: "#0a0a0a"

    // ═══════════════════════════════════════════════════════════════
    // MOCK DATA
    // ═══════════════════════════════════════════════════════════════

    QtObject {
        id: mockData
        property real rpm: 850
        property real coolantTemp: 85
        property real tps: 0
        property real mapKpa: 35
        property real vehicleSpeed: 0
        property real lambda: 1.0
        property real ignitionAdvance: 15
        property int gear: 0
        property bool celOn: false
        property bool overheat: false
        property bool canConnected: true
        property int currentScreen: 0  // 0=Home, 1=Dash, 2=Config, 3=OpenAuto

        Timer {
            interval: 50
            running: true
            repeat: true
            onTriggered: {
                // Add some variation
                mockData.rpm += (Math.random() - 0.5) * 30
                mockData.rpm = Math.max(700, Math.min(900, mockData.rpm))
                mockData.coolantTemp += (Math.random() - 0.5) * 0.5
                mockData.mapKpa += (Math.random() - 0.5) * 2
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // THEME CONSTANTS
    // ═══════════════════════════════════════════════════════════════

    QtObject {
        id: theme
        property color bgPrimary: "#0a0a0a"
        property color bgSecondary: "#1a1a1a"
        property color bgTertiary: "#2a2a2a"
        property color textPrimary: "#ffffff"
        property color textSecondary: "#b0b0b0"
        property color accentGreen: "#00ff00"
        property color accentBlue: "#00aaff"
        property color accentOrange: "#ff6600"
        property color warning: "#ffaa00"
        property color critical: "#ff0000"
        property int spacing: 8
        property int radiusMedium: 8
    }

    // ═══════════════════════════════════════════════════════════════
    // STATUS BAR
    // ═══════════════════════════════════════════════════════════════

    Rectangle {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        color: theme.bgPrimary
        z: 100

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            // CAN Status
            Rectangle {
                width: 50
                height: 24
                radius: 4
                color: "transparent"
                border.color: mockData.canConnected ? theme.accentGreen : theme.critical
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "CAN"
                    color: mockData.canConnected ? theme.accentGreen : theme.critical
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            // CEL Warning
            Rectangle {
                visible: mockData.celOn
                width: 40
                height: 24
                radius: 4
                color: theme.warning

                Text {
                    anchors.centerIn: parent
                    text: "CEL"
                    color: theme.bgPrimary
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Overheat Warning
            Rectangle {
                visible: mockData.overheat
                width: 50
                height: 24
                radius: 4
                color: theme.critical

                Text {
                    anchors.centerIn: parent
                    text: "TEMP"
                    color: theme.textPrimary
                    font.pixelSize: 10
                    font.bold: true
                }

                SequentialAnimation on opacity {
                    running: mockData.overheat
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 300 }
                    NumberAnimation { to: 1.0; duration: 300 }
                }
            }

            Item { Layout.fillWidth: true }

            // Clock
            Text {
                text: Qt.formatTime(new Date(), "HH:mm")
                color: theme.textPrimary
                font.pixelSize: 14
                font.family: "monospace"

                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: parent.text = Qt.formatTime(new Date(), "HH:mm")
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // CONTENT AREA
    // ═══════════════════════════════════════════════════════════════

    Item {
        id: contentArea
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: tabBar.top

        // Screen Loader
        Loader {
            anchors.fill: parent
            sourceComponent: {
                switch(mockData.currentScreen) {
                    case 0: return homeScreen
                    case 1: return dashScreen
                    case 2: return configScreen
                    case 3: return openAutoScreen
                    default: return homeScreen
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // HOME SCREEN
    // ═══════════════════════════════════════════════════════════════

    Component {
        id: homeScreen

        Item {
            Rectangle {
                anchors.fill: parent
                color: theme.bgPrimary
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                // Summary card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: theme.bgSecondary
                    radius: theme.radiusMedium

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 32

                        Column {
                            Text { text: "RPM"; color: theme.textSecondary; font.pixelSize: 12 }
                            Text { text: mockData.rpm.toFixed(0); color: theme.accentGreen; font.pixelSize: 28; font.bold: true }
                        }

                        Rectangle { width: 1; Layout.fillHeight: true; color: theme.bgTertiary }

                        Column {
                            Text { text: "km/h"; color: theme.textSecondary; font.pixelSize: 12 }
                            Text { text: mockData.vehicleSpeed.toFixed(0); color: theme.accentBlue; font.pixelSize: 28; font.bold: true }
                        }

                        Rectangle { width: 1; Layout.fillHeight: true; color: theme.bgTertiary }

                        Column {
                            Text { text: "CLT"; color: theme.textSecondary; font.pixelSize: 12 }
                            Text { text: mockData.coolantTemp.toFixed(0) + "°"; color: theme.accentGreen; font.pixelSize: 28; font.bold: true }
                        }
                    }
                }

                // Navigation grid
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: 2
                    rowSpacing: 16
                    columnSpacing: 16

                    Repeater {
                        model: [
                            { icon: "⊙", label: "Dashboard", color: theme.accentGreen, screen: 1 },
                            { icon: "▶", label: "Android Auto", color: "#4285f4", screen: 3 },
                            { icon: "⚙", label: "Settings", color: theme.textSecondary, screen: 2 },
                            { icon: "▣", label: "Camera", color: theme.accentBlue, screen: -1 }
                        ]

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: mouseArea.pressed ? "#444" : theme.bgSecondary
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.icon
                                    font.pixelSize: 48
                                    color: modelData.color
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: theme.textPrimary
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                onClicked: if (modelData.screen >= 0) mockData.currentScreen = modelData.screen
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // DASH SCREEN
    // ═══════════════════════════════════════════════════════════════

    Component {
        id: dashScreen

        Item {
            Rectangle {
                anchors.fill: parent
                color: theme.bgPrimary
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 8
                columns: 4
                rows: 2
                rowSpacing: 8
                columnSpacing: 8

                // RPM Gauge (large)
                Rectangle {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.bgSecondary
                    radius: 8

                    Column {
                        anchors.centerIn: parent
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "RPM"; color: theme.textSecondary; font.pixelSize: 14 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: mockData.rpm.toFixed(0); color: theme.accentGreen; font.pixelSize: 48; font.bold: true; font.family: "monospace" }
                    }
                }

                // Speed Gauge (large)
                Rectangle {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.bgSecondary
                    radius: 8

                    Column {
                        anchors.centerIn: parent
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "km/h"; color: theme.textSecondary; font.pixelSize: 14 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: mockData.vehicleSpeed.toFixed(0); color: theme.accentBlue; font.pixelSize: 48; font.bold: true; font.family: "monospace" }
                    }
                }

                // Small gauges
                Repeater {
                    model: [
                        { label: "CLT", value: mockData.coolantTemp.toFixed(0), unit: "°C", color: theme.accentGreen },
                        { label: "MAP", value: mockData.mapKpa.toFixed(0), unit: "kPa", color: theme.accentOrange },
                        { label: "TPS", value: mockData.tps.toFixed(0), unit: "%", color: theme.accentGreen },
                        { label: "AFR", value: (mockData.lambda * 14.7).toFixed(1), unit: "", color: "#aa00ff" }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: theme.bgSecondary
                        radius: 8

                        Column {
                            anchors.centerIn: parent
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: theme.textSecondary; font.pixelSize: 12 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value + modelData.unit; color: modelData.color; font.pixelSize: 24; font.bold: true; font.family: "monospace" }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // CONFIG SCREEN
    // ═══════════════════════════════════════════════════════════════

    Component {
        id: configScreen

        Rectangle {
            color: theme.bgPrimary

            Text {
                anchors.centerIn: parent
                text: "Settings Screen\n(Placeholder)"
                color: theme.textSecondary
                font.pixelSize: 24
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // OPENAUTO SCREEN
    // ═══════════════════════════════════════════════════════════════

    Component {
        id: openAutoScreen

        Rectangle {
            color: theme.bgPrimary

            Column {
                anchors.centerIn: parent
                spacing: 16

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "▶"
                    font.pixelSize: 72
                    color: "#4285f4"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Android Auto"
                    font.pixelSize: 24
                    font.bold: true
                    color: theme.textPrimary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Connect phone via USB"
                    font.pixelSize: 14
                    color: theme.textSecondary
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // TAB BAR
    // ═══════════════════════════════════════════════════════════════

    Rectangle {
        id: tabBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64
        color: "#0d0d0d"
        z: 100

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Repeater {
                model: [
                    { icon: "⌂", label: "Home", screen: 0 },
                    { icon: "⊙", label: "Dash", screen: 1 },
                    { icon: "⚙", label: "Config", screen: 2 },
                    { icon: "▶", label: "Auto", screen: 3 }
                ]

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    property bool isActive: mockData.currentScreen === modelData.screen

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.icon
                            font.pixelSize: 24
                            color: isActive ? theme.accentGreen : "#666"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            font.pixelSize: 10
                            color: isActive ? theme.accentGreen : "#666"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: mockData.currentScreen = modelData.screen
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // KEYBOARD SHORTCUTS
    // ═══════════════════════════════════════════════════════════════

    Shortcut { sequence: "1"; onActivated: mockData.currentScreen = 0 }
    Shortcut { sequence: "2"; onActivated: mockData.currentScreen = 1 }
    Shortcut { sequence: "3"; onActivated: mockData.currentScreen = 2 }
    Shortcut { sequence: "4"; onActivated: mockData.currentScreen = 3 }
    Shortcut { sequence: "C"; onActivated: mockData.celOn = !mockData.celOn }
    Shortcut { sequence: "T"; onActivated: mockData.overheat = !mockData.overheat }
}
