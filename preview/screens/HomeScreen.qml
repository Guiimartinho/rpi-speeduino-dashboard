import QtQuick
import QtQuick.Layouts

/**
 * HomeScreen.qml - Home/launcher screen with navigation grid
 *
 * Features:
 * - Quick summary of key metrics (RPM, Speed, CLT)
 * - Navigation grid to other screens
 * - Modern card-based layout
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // DATA PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property real rpm: 850
    property real speed: 0
    property real coolantTemp: 85
    property bool canConnected: true

    signal navigateTo(int screen)

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: "#0a0a0a"
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MAIN LAYOUT
    // ═══════════════════════════════════════════════════════════════════════

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ─────────────────────────────────────────────────────────────────
        // TOP: Quick Summary Card
        // ─────────────────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            radius: 12
            color: "#141414"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 0

                // RPM
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "RPM"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 11
                            font.family: "Roboto, sans-serif"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Math.round(root.rpm)
                            color: "#00E676"
                            font.pixelSize: 32
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                        }
                    }
                }

                // Divider
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    color: "#2a2a2a"
                }

                // Speed
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "km/h"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 11
                            font.family: "Roboto, sans-serif"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Math.round(root.speed)
                            color: "#00B8D4"
                            font.pixelSize: 32
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                        }
                    }
                }

                // Divider
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    color: "#2a2a2a"
                }

                // CLT
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "CLT"
                            color: Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 11
                            font.family: "Roboto, sans-serif"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Math.round(root.coolantTemp) + "°"
                            color: root.coolantTemp > 100 ? "#FF3D00" :
                                   root.coolantTemp > 90 ? "#FFB300" : "#00E676"
                            font.pixelSize: 32
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                        }
                    }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // BOTTOM: Navigation Grid
        // ─────────────────────────────────────────────────────────────────

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            rowSpacing: 16
            columnSpacing: 16

            // Dashboard
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: mouseAreaDash.pressed ? "#2a2a2a" : "#1a1a1a"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        radius: 28
                        color: "#00E676"
                        opacity: 0.15

                        Text {
                            anchors.centerIn: parent
                            text: "D"
                            color: "#00E676"
                            font.pixelSize: 28
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Dashboard"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 16
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }
                }

                MouseArea {
                    id: mouseAreaDash
                    anchors.fill: parent
                    onClicked: root.navigateTo(1)
                }
            }

            // Android Auto
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: mouseAreaAuto.pressed ? "#2a2a2a" : "#1a1a1a"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        radius: 28
                        color: "#4285F4"
                        opacity: 0.15

                        Text {
                            anchors.centerIn: parent
                            text: "A"
                            color: "#4285F4"
                            font.pixelSize: 28
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Android Auto"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 16
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }
                }

                MouseArea {
                    id: mouseAreaAuto
                    anchors.fill: parent
                    onClicked: root.navigateTo(3)
                }
            }

            // Settings
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: mouseAreaSettings.pressed ? "#2a2a2a" : "#1a1a1a"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        radius: 28
                        color: "#888888"
                        opacity: 0.15

                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            color: "#888888"
                            font.pixelSize: 28
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Settings"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 16
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }
                }

                MouseArea {
                    id: mouseAreaSettings
                    anchors.fill: parent
                    onClicked: root.navigateTo(2)
                }
            }

            // Camera
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: mouseAreaCamera.pressed ? "#2a2a2a" : "#1a1a1a"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        radius: 28
                        color: "#00B8D4"
                        opacity: 0.15

                        Text {
                            anchors.centerIn: parent
                            text: "C"
                            color: "#00B8D4"
                            font.pixelSize: 28
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Camera"
                        color: Qt.rgba(1, 1, 1, 0.96)
                        font.pixelSize: 16
                        font.bold: true
                        font.family: "Roboto, sans-serif"
                    }
                }

                MouseArea {
                    id: mouseAreaCamera
                    anchors.fill: parent
                    // Camera action - could open reverse camera preview
                }
            }
        }
    }
}
