/**
 * StatusIndicator.qml
 * System status indicator component for graceful degradation
 *
 * Shows the current system mode and subsystem health status.
 * Uses color coding to indicate severity.
 */

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    // System mode from C++ SystemHealth
    property int systemMode: 0  // 0=Normal, 1=DegradedCAN, etc.
    property bool canConnected: true
    property bool cameraReady: true
    property bool openAutoRunning: false

    implicitWidth: 200
    implicitHeight: 30
    radius: 4

    // State-based color (optimized for batching)
    state: {
        switch (systemMode) {
            case 0: return "normal"
            case 1: return "degradedCAN"
            case 2: return "degradedCamera"
            case 3:
            case 4: return "degradedMultiple"
            case 5:
            case 6: return "safeMode"
            default: return "normal"
        }
    }

    states: [
        State {
            name: "normal"
            PropertyChanges { target: root; color: "#1a3d1a" }
            PropertyChanges { target: modeText; text: "SYSTEM OK"; color: "#00ff00" }
            PropertyChanges { target: statusIcon; color: "#00ff00" }
        },
        State {
            name: "degradedCAN"
            PropertyChanges { target: root; color: "#3d3d1a" }
            PropertyChanges { target: modeText; text: "CAN OFFLINE"; color: "#ffaa00" }
            PropertyChanges { target: statusIcon; color: "#ffaa00" }
        },
        State {
            name: "degradedCamera"
            PropertyChanges { target: root; color: "#3d3d1a" }
            PropertyChanges { target: modeText; text: "CAMERA ERROR"; color: "#ffaa00" }
            PropertyChanges { target: statusIcon; color: "#ffaa00" }
        },
        State {
            name: "degradedMultiple"
            PropertyChanges { target: root; color: "#3d2a1a" }
            PropertyChanges { target: modeText; text: "DEGRADED MODE"; color: "#ff6600" }
            PropertyChanges { target: statusIcon; color: "#ff6600" }
        },
        State {
            name: "safeMode"
            PropertyChanges { target: root; color: "#3d1a1a" }
            PropertyChanges { target: modeText; text: "SAFE MODE"; color: "#ff0000" }
            PropertyChanges { target: statusIcon; color: "#ff0000" }
        }
    ]

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 8

        // Status icon (circle)
        Rectangle {
            id: statusIcon
            width: 12
            height: 12
            radius: 6
            color: "#00ff00"

            // Blink animation for degraded states
            SequentialAnimation on opacity {
                running: root.systemMode > 0
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }

        // Mode text
        Text {
            id: modeText
            Layout.fillWidth: true
            text: "SYSTEM OK"
            font.pixelSize: 11
            font.bold: true
            color: "#00ff00"
        }

        // Subsystem indicators
        Row {
            spacing: 4

            // CAN indicator
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.canConnected ? "#00ff00" : "#ff0000"
                opacity: root.canConnected ? 1.0 : 0.8

                ToolTip {
                    visible: canMouseArea.containsMouse
                    text: root.canConnected ? "CAN Connected" : "CAN Disconnected"
                }

                MouseArea {
                    id: canMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            // Camera indicator
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.cameraReady ? "#00ff00" : "#666666"
                opacity: root.cameraReady ? 1.0 : 0.5

                ToolTip {
                    visible: camMouseArea.containsMouse
                    text: root.cameraReady ? "Camera Ready" : "Camera Not Available"
                }

                MouseArea {
                    id: camMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            // OpenAuto indicator
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.openAutoRunning ? "#00aaff" : "#666666"
                opacity: root.openAutoRunning ? 1.0 : 0.5

                ToolTip {
                    visible: aaMouseArea.containsMouse
                    text: root.openAutoRunning ? "Android Auto Active" : "Android Auto Inactive"
                }

                MouseArea {
                    id: aaMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }

    // Simple tooltip component
    component ToolTip: Rectangle {
        visible: false
        width: tooltipText.implicitWidth + 10
        height: tooltipText.implicitHeight + 6
        color: "#222222"
        border.color: "#444444"
        radius: 3
        z: 1000

        anchors.bottom: parent.top
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter

        property alias text: tooltipText.text

        Text {
            id: tooltipText
            anchors.centerIn: parent
            font.pixelSize: 10
            color: "#ffffff"
        }
    }
}
