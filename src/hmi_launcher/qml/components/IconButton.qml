import QtQuick

Item {
    id: root

    property string iconText: "?"
    property string labelText: "Button"
    property color accentColor: "#00ff00"

    signal clicked()

    implicitWidth: 120
    implicitHeight: 140

    Rectangle {
        id: button
        anchors.horizontalCenter: parent.horizontalCenter
        width: 100
        height: 100
        radius: 15
        color: mouseArea.pressed ? Qt.darker(accentColor, 1.5) : "#2a2a2a"
        border.color: mouseArea.containsMouse ? accentColor : "#444444"
        border.width: 2

        Behavior on color {
            ColorAnimation { duration: 100 }
        }

        Behavior on border.color {
            ColorAnimation { duration: 100 }
        }

        // Icon
        Text {
            anchors.centerIn: parent
            text: iconText
            font.pixelSize: 48
            font.bold: true
            color: accentColor
        }

        // Glow effect when hovered
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: accentColor
            border.width: 2
            opacity: mouseArea.containsMouse ? 0.3 : 0
            scale: mouseArea.containsMouse ? 1.1 : 1.0

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
            Behavior on scale {
                NumberAnimation { duration: 150 }
            }
        }
    }

    // Label
    Text {
        anchors.top: button.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        text: labelText
        font.pixelSize: 14
        color: "#ffffff"
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
