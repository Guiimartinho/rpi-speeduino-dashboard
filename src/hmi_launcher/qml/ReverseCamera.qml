import QtQuick
import QtQuick.Controls
import QtMultimedia

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    // Camera view using Qt Multimedia
    MediaPlayer {
        id: mediaPlayer
        source: "v4l2://" + cameraController.device
        videoOutput: videoOutput

        Component.onCompleted: {
            if (cameraController.active) {
                play()
            }
        }
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
    }

    // Fallback if camera not available
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        visible: !cameraController.active || cameraController.errorMessage !== ""

        Column {
            anchors.centerIn: parent
            spacing: 20

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "REVERSE CAMERA"
                font.pixelSize: 32
                font.bold: true
                color: "#ffffff"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: cameraController.errorMessage !== ""
                      ? cameraController.errorMessage
                      : "Waiting for camera..."
                font.pixelSize: 18
                color: cameraController.errorMessage !== "" ? "#ff6600" : "#888888"
            }

            // Loading indicator
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: cameraController.errorMessage === "" && !cameraController.active
                palette.dark: "#00ff00"
            }
        }
    }

    // Guidelines overlay
    Canvas {
        id: guidelines
        anchors.fill: parent
        visible: cameraController.active

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Draw parking guidelines
            ctx.strokeStyle = "#00ff00"
            ctx.lineWidth = 2

            var centerX = width / 2
            var bottomY = height - 20

            // Left guideline
            ctx.beginPath()
            ctx.moveTo(centerX - 150, bottomY)
            ctx.lineTo(centerX - 80, height * 0.4)
            ctx.stroke()

            // Right guideline
            ctx.beginPath()
            ctx.moveTo(centerX + 150, bottomY)
            ctx.lineTo(centerX + 80, height * 0.4)
            ctx.stroke()

            // Distance markers
            ctx.strokeStyle = "#ffff00"
            var distances = [0.7, 0.5, 0.3]
            distances.forEach(function(d) {
                var y = height * d
                var leftX = centerX - 150 + (150 - 80) * (1 - d / 0.6)
                var rightX = centerX + 150 - (150 - 80) * (1 - d / 0.6)

                ctx.beginPath()
                ctx.moveTo(leftX, y)
                ctx.lineTo(rightX, y)
                ctx.stroke()
            })
        }
    }

    // Reverse indicator
    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
        width: 120
        height: 40
        radius: 5
        color: "#ff0000"

        Text {
            anchors.centerIn: parent
            text: "REVERSE"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        SequentialAnimation on opacity {
            running: true
            loops: Animation.Infinite
            NumberAnimation { to: 0.5; duration: 500 }
            NumberAnimation { to: 1.0; duration: 500 }
        }
    }
}
