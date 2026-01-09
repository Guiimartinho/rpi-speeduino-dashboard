import QtQuick

/**
 * GForceMeter.qml - Real-time G-Force visualization
 *
 * Features:
 * - Circular G-force display with lateral and longitudinal axes
 * - Peak G tracking with fade trail
 * - Configurable range (typical: 1.5G to 2.5G for street, 3G+ for racing)
 * - Color-coded zones (green/yellow/red)
 */
Item {
    id: root

    // Current G-force values (-1.0 to +1.0 typical, can exceed for racing)
    property real lateralG: 0.0      // Left (-) / Right (+)
    property real longitudinalG: 0.0 // Braking (-) / Acceleration (+)

    // Configuration
    property real maxG: 1.5          // Maximum G for full scale
    property bool showPeaks: true
    property bool showTrail: true
    property bool showGrid: true
    property bool showValues: true

    // Peak tracking
    property real peakLateralLeft: 0
    property real peakLateralRight: 0
    property real peakLongBraking: 0
    property real peakLongAccel: 0
    property real combinedG: Math.sqrt(lateralG * lateralG + longitudinalG * longitudinalG)
    property real peakCombinedG: 0

    // Trail history (last N positions)
    property var trailHistory: []
    property int maxTrailLength: 30

    // Colors
    property color backgroundColor: "#0a0a0a"
    property color gridColor: Qt.rgba(1, 1, 1, 0.15)
    property color dotColor: "#00E676"
    property color trailColor: "#00E676"
    property color peakColor: "#FFC107"

    // Update peaks
    onLateralGChanged: {
        if (lateralG < peakLateralLeft) peakLateralLeft = lateralG
        if (lateralG > peakLateralRight) peakLateralRight = lateralG
        updateTrail()
    }

    onLongitudinalGChanged: {
        if (longitudinalG < peakLongBraking) peakLongBraking = longitudinalG
        if (longitudinalG > peakLongAccel) peakLongAccel = longitudinalG
        updateTrail()
    }

    onCombinedGChanged: {
        if (combinedG > peakCombinedG) peakCombinedG = combinedG
    }

    function updateTrail() {
        if (!showTrail) return
        var newPoint = {
            x: lateralG,
            y: longitudinalG,
            age: 0
        }
        trailHistory.push(newPoint)
        if (trailHistory.length > maxTrailLength) {
            trailHistory.shift()
        }
        // Age existing points
        for (var i = 0; i < trailHistory.length; i++) {
            trailHistory[i].age++
        }
        canvas.requestPaint()
    }

    function resetPeaks() {
        peakLateralLeft = 0
        peakLateralRight = 0
        peakLongBraking = 0
        peakLongAccel = 0
        peakCombinedG = 0
        trailHistory = []
        canvas.requestPaint()
    }

    // Get zone color based on G value
    function getZoneColor(g) {
        var absG = Math.abs(g)
        var ratio = absG / maxG
        if (ratio < 0.5) {
            return "#00E676"  // Green - normal
        } else if (ratio < 0.8) {
            return "#FFC107"  // Yellow - moderate
        } else {
            return "#FF5252"  // Red - high
        }
    }

    Rectangle {
        anchors.fill: parent
        color: backgroundColor
        radius: 12
        border.color: "#333333"
        border.width: 1

        Canvas {
            id: canvas
            anchors.fill: parent
            anchors.margins: 8
            renderStrategy: Canvas.Cooperative

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var centerX = width / 2
                var centerY = height / 2
                var radius = Math.min(width, height) / 2 - 20

                // Background circle
                ctx.beginPath()
                ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI)
                ctx.fillStyle = Qt.rgba(0.1, 0.1, 0.1, 0.8)
                ctx.fill()

                if (showGrid) {
                    // Draw grid circles
                    ctx.strokeStyle = gridColor
                    ctx.lineWidth = 1

                    for (var i = 1; i <= 4; i++) {
                        ctx.beginPath()
                        ctx.arc(centerX, centerY, radius * i / 4, 0, 2 * Math.PI)
                        ctx.stroke()
                    }

                    // Draw crosshairs
                    ctx.beginPath()
                    ctx.moveTo(centerX - radius, centerY)
                    ctx.lineTo(centerX + radius, centerY)
                    ctx.moveTo(centerX, centerY - radius)
                    ctx.lineTo(centerX, centerY + radius)
                    ctx.stroke()

                    // Draw G labels
                    ctx.fillStyle = Qt.rgba(1, 1, 1, 0.5)
                    ctx.font = "10px 'Roboto Mono', monospace"
                    ctx.textAlign = "center"

                    for (var j = 1; j <= 4; j++) {
                        var gValue = (maxG * j / 4).toFixed(1)
                        ctx.fillText(gValue, centerX + radius * j / 4, centerY - 4)
                    }
                }

                // Draw trail
                if (showTrail && trailHistory.length > 1) {
                    ctx.lineWidth = 2
                    for (var t = 0; t < trailHistory.length - 1; t++) {
                        var point = trailHistory[t]
                        var alpha = 1 - (point.age / maxTrailLength)
                        ctx.strokeStyle = Qt.rgba(0, 0.9, 0.46, alpha * 0.5)

                        var trailX = centerX + (point.x / maxG) * radius
                        var trailY = centerY - (point.y / maxG) * radius

                        ctx.beginPath()
                        ctx.arc(trailX, trailY, 3, 0, 2 * Math.PI)
                        ctx.stroke()
                    }
                }

                // Draw peak markers
                if (showPeaks) {
                    ctx.fillStyle = peakColor
                    ctx.globalAlpha = 0.6

                    // Peak lateral left
                    if (peakLateralLeft < -0.1) {
                        var plX = centerX + (peakLateralLeft / maxG) * radius
                        ctx.beginPath()
                        ctx.arc(plX, centerY, 4, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    // Peak lateral right
                    if (peakLateralRight > 0.1) {
                        var prX = centerX + (peakLateralRight / maxG) * radius
                        ctx.beginPath()
                        ctx.arc(prX, centerY, 4, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    // Peak braking
                    if (peakLongBraking < -0.1) {
                        var pbY = centerY - (peakLongBraking / maxG) * radius
                        ctx.beginPath()
                        ctx.arc(centerX, pbY, 4, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    // Peak acceleration
                    if (peakLongAccel > 0.1) {
                        var paY = centerY - (peakLongAccel / maxG) * radius
                        ctx.beginPath()
                        ctx.arc(centerX, paY, 4, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    ctx.globalAlpha = 1.0
                }

                // Draw current position dot
                var dotX = centerX + (lateralG / maxG) * radius
                var dotY = centerY - (longitudinalG / maxG) * radius

                // Clamp to circle
                var dist = Math.sqrt(Math.pow(dotX - centerX, 2) + Math.pow(dotY - centerY, 2))
                if (dist > radius) {
                    var angle = Math.atan2(dotY - centerY, dotX - centerX)
                    dotX = centerX + Math.cos(angle) * radius
                    dotY = centerY + Math.sin(angle) * radius
                }

                // Glow effect
                var glow = ctx.createRadialGradient(dotX, dotY, 0, dotX, dotY, 15)
                glow.addColorStop(0, getZoneColor(combinedG))
                glow.addColorStop(1, "transparent")
                ctx.fillStyle = glow
                ctx.beginPath()
                ctx.arc(dotX, dotY, 15, 0, 2 * Math.PI)
                ctx.fill()

                // Main dot
                ctx.fillStyle = getZoneColor(combinedG)
                ctx.beginPath()
                ctx.arc(dotX, dotY, 8, 0, 2 * Math.PI)
                ctx.fill()

                // Inner highlight
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.5)
                ctx.beginPath()
                ctx.arc(dotX - 2, dotY - 2, 3, 0, 2 * Math.PI)
                ctx.fill()
            }
        }

        // Axis labels
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 12
            text: "ACCEL"
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: 9
            font.family: "Roboto, sans-serif"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            text: "BRAKE"
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: 9
            font.family: "Roboto, sans-serif"
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            text: "L"
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: 9
            font.family: "Roboto, sans-serif"
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 12
            text: "R"
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: 9
            font.family: "Roboto, sans-serif"
        }

        // Values display
        Column {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 12
            spacing: 2
            visible: showValues

            Text {
                text: combinedG.toFixed(2) + " G"
                color: getZoneColor(combinedG)
                font.pixelSize: 14
                font.bold: true
                font.family: "Roboto Mono, monospace"
                horizontalAlignment: Text.AlignRight
            }

            Text {
                text: "PEAK: " + peakCombinedG.toFixed(2)
                color: peakColor
                font.pixelSize: 10
                font.family: "Roboto Mono, monospace"
                horizontalAlignment: Text.AlignRight
            }
        }

        // Reset button
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            width: 50
            height: 20
            radius: 4
            color: resetMouse.pressed ? "#FF5252" : "#333333"
            visible: showPeaks

            Text {
                anchors.centerIn: parent
                text: "RESET"
                color: Qt.rgba(1, 1, 1, 0.87)
                font.pixelSize: 9
                font.bold: true
            }

            MouseArea {
                id: resetMouse
                anchors.fill: parent
                onClicked: root.resetPeaks()
            }
        }
    }

    // Refresh timer
    Timer {
        interval: 50  // 20 Hz
        running: true
        repeat: true
        onTriggered: canvas.requestPaint()
    }
}
