import QtQuick
import QtQuick.Window

Window {
    visible: true
    width: 400
    height: 300
    title: "Test"
    color: "#1a1a1a"

    Text {
        anchors.centerIn: parent
        text: "Qt is working!"
        color: "#00ff00"
        font.pixelSize: 32
    }
}
