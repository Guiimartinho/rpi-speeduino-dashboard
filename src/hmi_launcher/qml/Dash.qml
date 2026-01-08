import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root

    signal backClicked()

    Rectangle {
        anchors.fill: parent
        color: "#0a0a0a"
    }

    // Back button
    Rectangle {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        width: 60
        height: 40
        radius: 5
        color: backButtonMouse.pressed ? "#444444" : "#333333"

        Text {
            anchors.centerIn: parent
            text: "< BACK"
            color: "#ffffff"
            font.pixelSize: 12
        }

        MouseArea {
            id: backButtonMouse
            anchors.fill: parent
            onClicked: root.backClicked()
        }
    }

    // Main gauges layout
    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        anchors.topMargin: 60
        spacing: 20

        // Left column - RPM and Speed
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.35
            spacing: 20

            // RPM Gauge
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "RPM"
                value: dataProvider.rpm
                minValue: 0
                maxValue: 8000
                warningValue: 6500
                criticalValue: 7500
                unit: ""
                decimals: 0
                arcColor: "#00ff00"
            }

            // Speed
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "SPEED"
                value: dataProvider.vehicleSpeed
                minValue: 0
                maxValue: 280
                unit: "km/h"
                decimals: 0
                arcColor: "#00aaff"
            }
        }

        // Center column - Big RPM display and indicators
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10

            // Big RPM number
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: dataProvider.rpm
                font.pixelSize: 80
                font.bold: true
                color: dataProvider.rpm > 6500 ? "#ff6600" : "#ffffff"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "RPM"
                font.pixelSize: 20
                color: "#888888"
            }

            // Gear indicator
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 80
                height: 80
                radius: 10
                color: "#222222"
                border.color: "#00ff00"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: dataProvider.gear > 0 ? dataProvider.gear : "N"
                    font.pixelSize: 48
                    font.bold: true
                    color: "#00ff00"
                }
            }

            // Warning indicators
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 20

                WarningIndicator {
                    label: "CEL"
                    active: dataProvider.celOn
                    warningColor: "#ff6600"
                }

                WarningIndicator {
                    label: "HEAT"
                    active: dataProvider.overheat
                    warningColor: "#ff0000"
                    blinking: true
                }
            }

            Item { Layout.fillHeight: true }
        }

        // Right column - Temps and pressures
        GridLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.35
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            // Coolant temp
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "CLT"
                value: dataProvider.coolantTemp
                minValue: 0
                maxValue: 120
                warningValue: 100
                criticalValue: 110
                unit: "C"
                decimals: 0
                arcColor: "#00aaff"
            }

            // Intake temp
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IAT"
                value: dataProvider.intakeTemp
                minValue: 0
                maxValue: 80
                warningValue: 60
                criticalValue: 70
                unit: "C"
                decimals: 0
                arcColor: "#00aaff"
            }

            // MAP
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "MAP"
                value: dataProvider.mapKpa
                minValue: 0
                maxValue: 250
                unit: "kPa"
                decimals: 0
                arcColor: "#ffaa00"
            }

            // TPS
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "TPS"
                value: dataProvider.tps
                minValue: 0
                maxValue: 100
                unit: "%"
                decimals: 0
                arcColor: "#ffaa00"
            }

            // Lambda
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "AFR"
                value: dataProvider.lambda * 14.7
                minValue: 10
                maxValue: 20
                warningValue: 15
                unit: ""
                decimals: 1
                arcColor: "#aa00ff"
            }

            // Ignition
            Gauge {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "IGN"
                value: dataProvider.ignitionAdvance
                minValue: -10
                maxValue: 50
                unit: "deg"
                decimals: 1
                arcColor: "#ff00aa"
            }
        }
    }
}
