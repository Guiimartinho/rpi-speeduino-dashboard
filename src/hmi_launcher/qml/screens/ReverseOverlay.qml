import QtQuick
import QtQuick.Layouts
import QtMultimedia
import "../styles" as Styles
import "../state" as State

/**
 * ReverseOverlay.qml
 * Full-screen reverse camera overlay with highest priority
 *
 * Features:
 * - Fullscreen camera feed via Qt6 VideoOutput
 * - Parking guide lines overlay (Canvas)
 * - Warning indicators
 * - Auto-dismiss when reverse disengaged
 * - Quick dismiss button for manual close
 * - Graceful handling when camera unavailable
 */
Item {
    id: reverseOverlay

    // Visibility controlled by AppState overlay
    visible: State.AppState.currentOverlay === State.AppState.overlayReverseCamera
    z: 1000  // Highest z-index to overlay everything

    // Camera status - use cameraController directly
    property bool cameraReady: typeof cameraController !== "undefined" && cameraController && cameraController.available
    property bool cameraActive: typeof cameraController !== "undefined" && cameraController && cameraController.active
    property bool cameraTestMode: typeof cameraController !== "undefined" && cameraController && cameraController.testMode

    // Connect video sink when overlay becomes visible
    onVisibleChanged: {
        if (visible) {
            connectVideoSink()
            if (cameraReady && typeof cameraController !== "undefined" && cameraController) {
                cameraController.start()
            }
        } else {
            if (typeof cameraController !== "undefined" && cameraController) {
                cameraController.stop()
            }
        }
    }

    // Retry connecting video sink
    function connectVideoSink() {
        if (typeof cameraController !== "undefined" && cameraController && cameraVideoOutput.videoSink) {
            console.log("ReverseOverlay: Connecting video sink")
            cameraController.setVideoSink(cameraVideoOutput.videoSink)
        } else {
            // Retry on next frame
            Qt.callLater(connectVideoSink)
        }
    }

    // Fade in/out animation
    opacity: visible ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation {
            duration: Styles.Theme.animationNormal
            easing.type: Styles.Theme.easingType
        }
    }

    // Background (fallback if camera fails)
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Camera video surface
    Rectangle {
        id: cameraView
        anchors.fill: parent
        color: "black"

        // Qt6 VideoOutput for camera feed
        VideoOutput {
            id: cameraVideoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectCrop
            visible: reverseOverlay.cameraActive

            // Mirror the video horizontally for rear-view effect (optional)
            // transform: Scale { xScale: -1; origin.x: cameraVideoOutput.width / 2 }
        }

        // Camera unavailable message
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingMd
            visible: !reverseOverlay.cameraReady

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\u25A3"  // ▣
                font.pixelSize: Styles.Theme.fontMega
                font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                color: Styles.Theme.statusCritical
                opacity: 0.5
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "CAMERA UNAVAILABLE"
                font.pixelSize: Styles.Theme.fontXxl
                font.weight: Styles.Theme.fontWeightBold
                color: Styles.Theme.statusCritical
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: typeof cameraController !== "undefined" && cameraController
                      ? cameraController.statusMessage
                      : "Check camera connection"
                font.pixelSize: Styles.Theme.fontMd
                color: Styles.Theme.textSecondary
            }
        }

        // Loading indicator when camera is starting
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingMd
            visible: reverseOverlay.cameraReady && !reverseOverlay.cameraActive

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: parent.visible
                palette.dark: Styles.Theme.accentPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Starting camera..."
                font.pixelSize: Styles.Theme.fontLg
                color: Styles.Theme.textSecondary
            }
        }
    }

    // Parking guide lines overlay
    Canvas {
        id: parkingGuides
        anchors.fill: parent
        visible: reverseOverlay.cameraActive

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var w = width
            var h = height

            // Guide line properties
            ctx.lineWidth = 3
            ctx.lineCap = "round"

            // Calculate guide positions (trapezoidal shape)
            var bottomY = h * 0.95
            var topY = h * 0.45
            var bottomWidth = w * 0.8
            var topWidth = w * 0.4

            var leftBottomX = (w - bottomWidth) / 2
            var rightBottomX = leftBottomX + bottomWidth
            var leftTopX = (w - topWidth) / 2
            var rightTopX = leftTopX + topWidth

            // Left guide line (green to red gradient simulated)
            drawGuideLine(ctx, leftBottomX, bottomY, leftTopX, topY, true)

            // Right guide line
            drawGuideLine(ctx, rightBottomX, bottomY, rightTopX, topY, false)

            // Horizontal distance markers
            drawDistanceMarker(ctx, leftBottomX, rightBottomX, h * 0.85, "0.5m", "#00ff00")
            drawDistanceMarker(ctx, leftBottomX + (leftTopX - leftBottomX) * 0.33,
                              rightBottomX + (rightTopX - rightBottomX) * 0.33,
                              h * 0.7, "1m", "#ffaa00")
            drawDistanceMarker(ctx, leftBottomX + (leftTopX - leftBottomX) * 0.66,
                              rightBottomX + (rightTopX - rightBottomX) * 0.66,
                              h * 0.55, "1.5m", "#ff0000")
        }

        function drawGuideLine(ctx, x1, y1, x2, y2, isLeft) {
            // Draw segmented line with color gradient
            var segments = 10
            var colors = ["#00ff00", "#00ff00", "#00ff00", "#55ff00", "#aaff00",
                         "#ffff00", "#ffaa00", "#ff5500", "#ff0000", "#ff0000"]

            for (var i = 0; i < segments; i++) {
                var t1 = i / segments
                var t2 = (i + 1) / segments

                var startX = x1 + (x2 - x1) * t1
                var startY = y1 + (y2 - y1) * t1
                var endX = x1 + (x2 - x1) * t2
                var endY = y1 + (y2 - y1) * t2

                ctx.strokeStyle = colors[i]
                ctx.beginPath()
                ctx.moveTo(startX, startY)
                ctx.lineTo(endX, endY)
                ctx.stroke()
            }
        }

        function drawDistanceMarker(ctx, x1, x2, y, label, color) {
            ctx.strokeStyle = color
            ctx.fillStyle = color
            ctx.lineWidth = 2

            // Horizontal line
            ctx.beginPath()
            ctx.moveTo(x1, y)
            ctx.lineTo(x2, y)
            ctx.stroke()

            // Label background
            var textWidth = 40
            var textHeight = 20
            var textX = (x1 + x2) / 2 - textWidth / 2
            var textY = y - textHeight / 2

            ctx.fillStyle = "rgba(0, 0, 0, 0.7)"
            ctx.fillRect(textX, textY, textWidth, textHeight)

            // Label text
            ctx.fillStyle = color
            ctx.font = "bold 12px monospace"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.fillText(label, (x1 + x2) / 2, y)
        }

        // Redraw on size change
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // Top status bar
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Styles.Theme.statusBarHeight
        color: Styles.Theme.overlayDark

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Styles.Theme.spacingMd
            anchors.rightMargin: Styles.Theme.spacingMd

            // REVERSE indicator
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width: reverseText.implicitWidth + Styles.Theme.spacingMd * 2
                height: Styles.Theme.dp(28)
                radius: Styles.Theme.radiusSmall
                color: Styles.Theme.statusCritical

                Text {
                    id: reverseText
                    anchors.centerIn: parent
                    text: "R  REVERSE"
                    font.pixelSize: Styles.Theme.fontMd
                    font.weight: Styles.Theme.fontWeightBold
                    color: Styles.Theme.textPrimary
                }

                // Blink animation
                SequentialAnimation on opacity {
                    running: reverseOverlay.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.6; duration: 500 }
                    NumberAnimation { to: 1.0; duration: 500 }
                }
            }

            Item { Layout.fillWidth: true }

            // Camera status
            Text {
                Layout.alignment: Qt.AlignVCenter
                text: reverseOverlay.cameraActive ? "CAM OK" : (reverseOverlay.cameraReady ? "STARTING" : "CAM ERR")
                font.pixelSize: Styles.Theme.fontSm
                font.weight: Styles.Theme.fontWeightBold
                color: reverseOverlay.cameraActive ? Styles.Theme.statusOk
                     : (reverseOverlay.cameraReady ? Styles.Theme.statusWarning : Styles.Theme.statusCritical)
            }
        }
    }

    // Close button (manual dismiss)
    Rectangle {
        id: closeButton
        anchors.top: topBar.bottom
        anchors.right: parent.right
        anchors.margins: Styles.Theme.spacingMd
        width: Styles.Theme.touchTargetStandard
        height: Styles.Theme.touchTargetStandard
        radius: Styles.Theme.radiusMedium
        color: closeButtonMouse.pressed ? Styles.Theme.surfacePressed : Styles.Theme.overlayDark

        Text {
            anchors.centerIn: parent
            text: "\u2715"  // ✕
            font.pixelSize: Styles.Theme.fontXl
            font.weight: Styles.Theme.fontWeightBold
            color: Styles.Theme.textPrimary
        }

        MouseArea {
            id: closeButtonMouse
            anchors.fill: parent
            onClicked: State.AppState.hideReverseCamera()
        }
    }

    // Test mode indicator badge
    Rectangle {
        id: testModeBadge
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.margins: Styles.Theme.spacingMd
        width: testModeLabel.width + Styles.Theme.spacingMd * 2
        height: Styles.Theme.dp(28)
        radius: Styles.Theme.radiusSmall
        color: "#ff9900"  // Orange for test mode
        visible: reverseOverlay.cameraTestMode

        Text {
            id: testModeLabel
            anchors.centerIn: parent
            text: "TEST MODE"
            font.pixelSize: Styles.Theme.fontSm
            font.weight: Styles.Theme.fontWeightBold
            color: "#000000"
        }

        // Subtle pulse animation
        SequentialAnimation on opacity {
            running: testModeBadge.visible
            loops: Animation.Infinite
            NumberAnimation { to: 0.7; duration: 800 }
            NumberAnimation { to: 1.0; duration: 800 }
        }
    }

    // Warning for gear still engaged when camera closes
    Rectangle {
        id: warningBanner
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Styles.Theme.touchTargetLarge
        color: Styles.Theme.statusWarning
        visible: !State.AppState.reverseEngaged && reverseOverlay.visible

        Text {
            anchors.centerIn: parent
            text: "Camera closing - Reverse disengaged"
            font.pixelSize: Styles.Theme.fontLg
            font.weight: Styles.Theme.fontWeightBold
            color: Styles.Theme.backgroundPrimary
        }
    }

    // Touch anywhere to dismiss hint (appears after 5 seconds)
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Styles.Theme.spacingXl
        text: "Tap anywhere or disengage reverse to close"
        font.pixelSize: Styles.Theme.fontSm
        color: Styles.Theme.textSecondary
        opacity: hintTimer.running ? 0 : 0.7
        visible: reverseOverlay.cameraActive

        Timer {
            id: hintTimer
            interval: 5000
            running: reverseOverlay.visible
        }

        Behavior on opacity {
            NumberAnimation { duration: Styles.Theme.animationSlow }
        }
    }

    // Full screen touch area to dismiss
    MouseArea {
        anchors.fill: parent
        z: -1  // Behind other interactive elements
        onClicked: {
            if (!State.AppState.reverseEngaged) {
                State.AppState.hideReverseCamera()
            }
        }
    }
}
