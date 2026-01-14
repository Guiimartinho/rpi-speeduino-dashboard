import QtQuick
import QtQuick.Layouts

/**
 * StatusBar.qml - Top status bar with CAN status, warnings, and clock
 *
 * Features:
 * - CAN connection status indicator
 * - CEL (Check Engine Light) warning
 * - Overheat warning with blinking
 * - Low fuel warning
 * - Current time display
 * - Display mode indicator
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property bool canConnected: true
    property bool celOn: false
    property bool overheat: false
    property bool lowFuel: false
    property bool lowOilPressure: false

    property string displayMode: "SPORT"

    property color backgroundColor: "#0a0a0a"
    property color connectedColor: "#00E676"
    property color disconnectedColor: "#FF3D00"
    property color warningColor: "#FFB300"
    property color criticalColor: "#FF3D00"

    height: 36
    width: parent.width

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property string currentTime: Qt.formatTime(new Date(), "HH:mm")

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: currentTime = Qt.formatTime(new Date(), "HH:mm")
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: backgroundColor

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1a1a1a"
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CONTENT
    // ═══════════════════════════════════════════════════════════════════════

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // ─────────────────────────────────────────────────────────────────
        // LEFT: CAN Status
        // ─────────────────────────────────────────────────────────────────

        Rectangle {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 24
            radius: 4
            color: "transparent"
            border.color: canConnected ? connectedColor : disconnectedColor
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 4

                // Status dot
                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: canConnected ? connectedColor : disconnectedColor
                    anchors.verticalCenter: parent.verticalCenter

                    // Pulse when connected
                    SequentialAnimation on opacity {
                        running: canConnected
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.4; duration: 1000 }
                        NumberAnimation { to: 1.0; duration: 1000 }
                    }
                }

                Text {
                    text: "CAN"
                    color: canConnected ? connectedColor : disconnectedColor
                    font.pixelSize: 11
                    font.bold: true
                    font.family: "Roboto, sans-serif"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Blink when disconnected
            SequentialAnimation on opacity {
                running: !canConnected
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 300 }
                NumberAnimation { to: 1.0; duration: 300 }
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // LEFT-CENTER: Display Mode
        // ─────────────────────────────────────────────────────────────────

        Rectangle {
            Layout.preferredWidth: 60
            Layout.preferredHeight: 20
            radius: 10
            color: "#1a1a1a"
            border.color: "#333333"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: displayMode
                color: Qt.rgba(1, 1, 1, 0.72)
                font.pixelSize: 9
                font.bold: true
                font.family: "Roboto, sans-serif"
                font.letterSpacing: 1
            }
        }

        Item { Layout.fillWidth: true }

        // ─────────────────────────────────────────────────────────────────
        // CENTER: Warning Indicators
        // ─────────────────────────────────────────────────────────────────

        Row {
            spacing: 8
            Layout.alignment: Qt.AlignHCenter

            // CEL Warning
            Rectangle {
                visible: celOn
                width: 44
                height: 24
                radius: 4
                color: warningColor

                Text {
                    anchors.centerIn: parent
                    text: "CEL"
                    color: "#000000"
                    font.pixelSize: 11
                    font.bold: true
                    font.family: "Roboto, sans-serif"
                }

                // Pulse animation
                SequentialAnimation on opacity {
                    running: celOn
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.7; duration: 800 }
                    NumberAnimation { to: 1.0; duration: 800 }
                }
            }

            // Overheat Warning
            Rectangle {
                visible: overheat
                width: 52
                height: 24
                radius: 4
                color: criticalColor

                Row {
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        text: "TEMP"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Fast blink for critical
                SequentialAnimation on opacity {
                    running: overheat
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 200 }
                    NumberAnimation { to: 1.0; duration: 200 }
                }
            }

            // Low Fuel Warning
            Rectangle {
                visible: lowFuel
                width: 44
                height: 24
                radius: 4
                color: warningColor

                Text {
                    anchors.centerIn: parent
                    text: "FUEL"
                    color: "#000000"
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Roboto, sans-serif"
                }
            }

            // Low Oil Pressure Warning
            Rectangle {
                visible: lowOilPressure
                width: 40
                height: 24
                radius: 4
                color: criticalColor

                Text {
                    anchors.centerIn: parent
                    text: "OIL"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Roboto, sans-serif"
                }

                // Fast blink for critical
                SequentialAnimation on opacity {
                    running: lowOilPressure
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 200 }
                    NumberAnimation { to: 1.0; duration: 200 }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // ─────────────────────────────────────────────────────────────────
        // RIGHT: Clock
        // ─────────────────────────────────────────────────────────────────

        Text {
            text: currentTime
            color: Qt.rgba(1, 1, 1, 0.96)
            font.pixelSize: 16
            font.family: "Roboto Mono, monospace"
            font.weight: Font.Medium
        }
    }
}
