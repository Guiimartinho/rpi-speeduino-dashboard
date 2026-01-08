import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root

    signal dashClicked()
    signal androidAutoClicked()
    signal settingsClicked()

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
    }

    // Title
    Text {
        id: title
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        text: "SPEEDUINO"
        font.pixelSize: 48
        font.bold: true
        color: "#ffffff"
    }

    // Subtitle with engine info
    Text {
        anchors.top: title.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        text: dataProvider.engineRunning
              ? dataProvider.rpm + " RPM | " + dataProvider.coolantTemp + " C"
              : "Engine Off"
        font.pixelSize: 20
        color: "#888888"
    }

    // Main menu buttons
    GridLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 30
        columns: 3
        rowSpacing: 30
        columnSpacing: 30

        IconButton {
            iconText: "D"
            labelText: "Dash"
            onClicked: root.dashClicked()
        }

        IconButton {
            iconText: "A"
            labelText: "Android Auto"
            accentColor: "#4285f4"
            onClicked: root.androidAutoClicked()
        }

        IconButton {
            iconText: "S"
            labelText: "Settings"
            accentColor: "#888888"
            onClicked: root.settingsClicked()
        }
    }

    // Quick info at bottom
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 50

        Column {
            Text {
                text: dataProvider.vehicleSpeed.toFixed(0) + " km/h"
                font.pixelSize: 24
                font.bold: true
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "Speed"
                font.pixelSize: 12
                color: "#888888"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Column {
            Text {
                text: "G" + (dataProvider.gear > 0 ? dataProvider.gear : "N")
                font.pixelSize: 24
                font.bold: true
                color: "#00ff00"
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "Gear"
                font.pixelSize: 12
                color: "#888888"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Column {
            Text {
                text: dataProvider.coolantTemp + " C"
                font.pixelSize: 24
                font.bold: true
                color: dataProvider.overheat ? "#ff0000" : "#00aaff"
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "Coolant"
                font.pixelSize: 12
                color: "#888888"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
