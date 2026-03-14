import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal backClicked()

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
    }

    // Header
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#2a2a2a"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10

            Rectangle {
                width: 60
                height: 40
                radius: 5
                color: backMouse.pressed ? "#444444" : "#333333"

                Text {
                    anchors.centerIn: parent
                    text: "< BACK"
                    color: "#ffffff"
                    font.pixelSize: 12
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    onClicked: root.backClicked()
                }
            }

            Text {
                Layout.fillWidth: true
                text: "SETTINGS"
                font.pixelSize: 24
                font.bold: true
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 60 }  // Spacer for symmetry
        }
    }

    // Settings content
    ScrollView {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20

        ColumnLayout {
            width: parent.width
            spacing: 20

            // CAN Status section
            GroupBox {
                Layout.fillWidth: true
                title: "CAN Bus Status"

                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 5
                }

                label: Text {
                    text: "CAN Bus Status"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 20

                    Text { text: "Connection:"; color: "#888888" }
                    Text {
                        text: dataProvider.canConnected ? "Connected" : "Disconnected"
                        color: dataProvider.canConnected ? "#00ff00" : "#ff0000"
                        font.bold: true
                    }

                    Text { text: "Engine:"; color: "#888888" }
                    Text {
                        text: dataProvider.engineRunning ? "Running" : "Off"
                        color: dataProvider.engineRunning ? "#00ff00" : "#888888"
                    }

                    Text { text: "RPM:"; color: "#888888" }
                    Text { text: dataProvider.rpm; color: "#ffffff" }

                    Text { text: "Coolant:"; color: "#888888" }
                    Text { text: dataProvider.coolantTemp + " C"; color: "#ffffff" }
                }
            }

            // OpenAuto section
            GroupBox {
                Layout.fillWidth: true
                title: "Android Auto"

                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 5
                }

                label: Text {
                    text: "Android Auto"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }

                ColumnLayout {
                    spacing: 10

                    property bool aaRunning: typeof openAutoEmbedded !== "undefined" && openAutoEmbedded ? openAutoEmbedded.running : false
                    property string aaError: typeof openAutoEmbedded !== "undefined" && openAutoEmbedded ? openAutoEmbedded.errorMessage : ""

                    RowLayout {
                        Text { text: "Status:"; color: "#888888" }
                        Text {
                            text: parent.parent.aaRunning ? "Running" : "Stopped"
                            color: parent.parent.aaRunning ? "#00ff00" : "#888888"
                        }
                    }

                    Button {
                        text: parent.aaRunning ? "Stop Android Auto" : "Start Android Auto"
                        onClicked: {
                            if (typeof openAutoEmbedded !== "undefined" && openAutoEmbedded) {
                                openAutoEmbedded.toggle()
                            }
                        }

                        background: Rectangle {
                            color: parent.pressed ? "#444444" : "#333333"
                            radius: 5
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    Text {
                        visible: parent.aaError !== ""
                        text: parent.aaError
                        color: "#ff6600"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // Camera section
            GroupBox {
                Layout.fillWidth: true
                title: "Reverse Camera"

                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 5
                }

                label: Text {
                    text: "Reverse Camera"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }

                ColumnLayout {
                    spacing: 10

                    RowLayout {
                        Text { text: "Status:"; color: "#888888" }
                        Text {
                            text: cameraController.active ? "Active" : "Inactive"
                            color: cameraController.active ? "#00ff00" : "#888888"
                        }
                    }

                    RowLayout {
                        Text { text: "Reverse:"; color: "#888888" }
                        Text {
                            text: dataProvider.reverseEngaged ? "ENGAGED" : "Disengaged"
                            color: dataProvider.reverseEngaged ? "#ff0000" : "#888888"
                            font.bold: dataProvider.reverseEngaged
                        }
                    }

                    Button {
                        text: "Test Camera"
                        onClicked: {
                            if (cameraController.active) {
                                cameraController.stop()
                            } else {
                                cameraController.start()
                            }
                        }

                        background: Rectangle {
                            color: parent.pressed ? "#444444" : "#333333"
                            radius: 5
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Version info
            GroupBox {
                Layout.fillWidth: true
                title: "System Info"

                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 5
                }

                label: Text {
                    text: "System Info"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 20

                    Text { text: "Version:"; color: "#888888" }
                    Text { text: "1.0.0"; color: "#ffffff" }

                    Text { text: "Qt Version:"; color: "#888888" }
                    Text { text: "6.x"; color: "#ffffff" }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
