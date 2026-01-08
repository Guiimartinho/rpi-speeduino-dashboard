import QtQuick

Rectangle {
    id: root

    property string label: "WARN"
    property bool active: false
    property color warningColor: "#ff0000"
    property bool blinking: false

    implicitWidth: 60
    implicitHeight: 30
    radius: 5

    color: active ? warningColor : "#333333"
    border.color: active ? Qt.lighter(warningColor, 1.3) : "#555555"
    border.width: 1

    opacity: 1.0

    SequentialAnimation on opacity {
        running: active && blinking
        loops: Animation.Infinite
        NumberAnimation { to: 0.3; duration: 300 }
        NumberAnimation { to: 1.0; duration: 300 }
    }

    Text {
        anchors.centerIn: parent
        text: label
        font.pixelSize: 12
        font.bold: true
        color: active ? "#ffffff" : "#666666"
    }
}
