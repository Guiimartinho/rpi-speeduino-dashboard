import QtQuick

/**
 * ChartGauge.qml - Real-time scrolling line chart
 *
 * Features:
 * - Multiple data channels with different colors
 * - Auto-scaling Y axis
 * - Configurable time window
 * - Grid overlay with time markers
 * - Min/Max/Avg display
 */
Item {
    id: root

    // Data channels (array of objects: {name, value, color, minRange, maxRange})
    property var channels: []
    property real primaryValue: 0
    property string primaryLabel: "VALUE"
    property color primaryColor: "#00E676"
    property real primaryMin: 0
    property real primaryMax: 100

    // Secondary channel (optional)
    property real secondaryValue: -1
    property string secondaryLabel: ""
    property color secondaryColor: "#2196F3"
    property real secondaryMin: 0
    property real secondaryMax: 100

    // Chart configuration
    property int historySeconds: 30      // Time window in seconds
    property int samplesPerSecond: 10    // Sample rate
    property bool autoScale: false       // Auto-scale Y axis
    property bool showGrid: true
    property bool showStats: true
    property bool showLegend: true

    // Internal data storage
    property var primaryHistory: []
    property var secondaryHistory: []
    property int maxSamples: historySeconds * samplesPerSecond

    // Statistics
    property real primaryMin_: 999999
    property real primaryMax_: -999999
    property real primaryAvg: 0
    property real secondaryMin_: 999999
    property real secondaryMax_: -999999
    property real secondaryAvg: 0

    // Colors
    property color backgroundColor: "#0a0a0a"
    property color gridColor: Qt.rgba(1, 1, 1, 0.1)
    property color textColor: Qt.rgba(1, 1, 1, 0.72)

    // Update data
    function addSample() {
        // Primary channel
        primaryHistory.push(primaryValue)
        if (primaryHistory.length > maxSamples) {
            primaryHistory.shift()
        }

        // Update stats
        if (primaryValue < primaryMin_) primaryMin_ = primaryValue
        if (primaryValue > primaryMax_) primaryMax_ = primaryValue
        var sum = 0
        for (var i = 0; i < primaryHistory.length; i++) {
            sum += primaryHistory[i]
        }
        primaryAvg = sum / primaryHistory.length

        // Secondary channel
        if (secondaryValue >= 0) {
            secondaryHistory.push(secondaryValue)
            if (secondaryHistory.length > maxSamples) {
                secondaryHistory.shift()
            }

            if (secondaryValue < secondaryMin_) secondaryMin_ = secondaryValue
            if (secondaryValue > secondaryMax_) secondaryMax_ = secondaryValue
            var sum2 = 0
            for (var j = 0; j < secondaryHistory.length; j++) {
                sum2 += secondaryHistory[j]
            }
            secondaryAvg = sum2 / secondaryHistory.length
        }

        canvas.requestPaint()
    }

    function resetStats() {
        primaryMin_ = primaryValue
        primaryMax_ = primaryValue
        secondaryMin_ = secondaryValue
        secondaryMax_ = secondaryValue
        primaryHistory = []
        secondaryHistory = []
    }

    // Calculate Y axis range
    function getYRange(history, minRange, maxRange) {
        if (!autoScale || history.length < 2) {
            return { min: minRange, max: maxRange }
        }

        var min = minRange
        var max = maxRange

        for (var i = 0; i < history.length; i++) {
            if (history[i] < min) min = history[i]
            if (history[i] > max) max = history[i]
        }

        // Add 10% padding
        var padding = (max - min) * 0.1
        return { min: min - padding, max: max + padding }
    }

    Rectangle {
        anchors.fill: parent
        color: backgroundColor
        radius: 12
        border.color: "#333333"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            // Header with label and current value
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: primaryLabel
                    color: primaryColor
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: primaryValue.toFixed(1)
                    color: primaryColor
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                // Secondary label if present
                Text {
                    visible: secondaryValue >= 0 && secondaryLabel !== ""
                    text: secondaryLabel + ": " + secondaryValue.toFixed(1)
                    color: secondaryColor
                    font.pixelSize: 12
                    font.family: "Roboto Mono, monospace"
                }
            }

            // Chart canvas
            Canvas {
                id: canvas
                width: parent.width
                height: parent.height - (showStats ? 60 : 30)
                renderStrategy: Canvas.Cooperative

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    var chartX = 40
                    var chartY = 5
                    var chartW = width - 50
                    var chartH = height - 10

                    // Background
                    ctx.fillStyle = Qt.rgba(0.05, 0.05, 0.05, 0.8)
                    ctx.fillRect(chartX, chartY, chartW, chartH)

                    // Get Y range
                    var range = getYRange(primaryHistory, primaryMin, primaryMax)
                    var yMin = range.min
                    var yMax = range.max
                    var yRange = yMax - yMin

                    if (showGrid) {
                        // Horizontal grid lines
                        ctx.strokeStyle = gridColor
                        ctx.lineWidth = 1
                        ctx.setLineDash([2, 2])

                        var gridLines = 5
                        for (var g = 0; g <= gridLines; g++) {
                            var gridY = chartY + (chartH * g / gridLines)
                            ctx.beginPath()
                            ctx.moveTo(chartX, gridY)
                            ctx.lineTo(chartX + chartW, gridY)
                            ctx.stroke()

                            // Y axis labels
                            var labelValue = yMax - (yRange * g / gridLines)
                            ctx.fillStyle = textColor
                            ctx.font = "9px 'Roboto Mono', monospace"
                            ctx.textAlign = "right"
                            ctx.fillText(labelValue.toFixed(0), chartX - 4, gridY + 3)
                        }

                        // Vertical grid lines (time)
                        var timeLines = 6
                        for (var t = 0; t <= timeLines; t++) {
                            var gridX = chartX + (chartW * t / timeLines)
                            ctx.beginPath()
                            ctx.moveTo(gridX, chartY)
                            ctx.lineTo(gridX, chartY + chartH)
                            ctx.stroke()

                            // Time labels
                            if (t < timeLines) {
                                var timeLabel = -historySeconds + (historySeconds * t / timeLines)
                                ctx.textAlign = "center"
                                ctx.fillText(timeLabel.toFixed(0) + "s", gridX, chartY + chartH + 12)
                            }
                        }

                        ctx.setLineDash([])
                    }

                    // Draw secondary line first (behind primary)
                    if (secondaryHistory.length > 1) {
                        var range2 = getYRange(secondaryHistory, secondaryMin, secondaryMax)
                        var yMin2 = range2.min
                        var yMax2 = range2.max
                        var yRange2 = yMax2 - yMin2

                        ctx.strokeStyle = secondaryColor
                        ctx.lineWidth = 2
                        ctx.globalAlpha = 0.7
                        ctx.beginPath()

                        for (var s = 0; s < secondaryHistory.length; s++) {
                            var sx = chartX + (s / maxSamples) * chartW
                            var sy = chartY + chartH - ((secondaryHistory[s] - yMin2) / yRange2) * chartH

                            if (s === 0) {
                                ctx.moveTo(sx, sy)
                            } else {
                                ctx.lineTo(sx, sy)
                            }
                        }
                        ctx.stroke()
                        ctx.globalAlpha = 1.0
                    }

                    // Draw primary line
                    if (primaryHistory.length > 1) {
                        ctx.strokeStyle = primaryColor
                        ctx.lineWidth = 2
                        ctx.beginPath()

                        for (var p = 0; p < primaryHistory.length; p++) {
                            var px = chartX + (p / maxSamples) * chartW
                            var py = chartY + chartH - ((primaryHistory[p] - yMin) / yRange) * chartH

                            if (p === 0) {
                                ctx.moveTo(px, py)
                            } else {
                                ctx.lineTo(px, py)
                            }
                        }
                        ctx.stroke()

                        // Current value dot
                        if (primaryHistory.length > 0) {
                            var lastX = chartX + ((primaryHistory.length - 1) / maxSamples) * chartW
                            var lastY = chartY + chartH - ((primaryHistory[primaryHistory.length - 1] - yMin) / yRange) * chartH

                            ctx.fillStyle = primaryColor
                            ctx.beginPath()
                            ctx.arc(lastX, lastY, 4, 0, 2 * Math.PI)
                            ctx.fill()
                        }
                    }

                    // Border
                    ctx.strokeStyle = "#333333"
                    ctx.lineWidth = 1
                    ctx.strokeRect(chartX, chartY, chartW, chartH)
                }
            }

            // Statistics row
            Row {
                width: parent.width
                spacing: 16
                visible: showStats

                Column {
                    spacing: 2

                    Text {
                        text: "MIN"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 9
                    }
                    Text {
                        text: primaryMin_ < 999999 ? primaryMin_.toFixed(1) : "--"
                        color: primaryColor
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Roboto Mono, monospace"
                    }
                }

                Column {
                    spacing: 2

                    Text {
                        text: "MAX"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 9
                    }
                    Text {
                        text: primaryMax_ > -999999 ? primaryMax_.toFixed(1) : "--"
                        color: primaryColor
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Roboto Mono, monospace"
                    }
                }

                Column {
                    spacing: 2

                    Text {
                        text: "AVG"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 9
                    }
                    Text {
                        text: primaryHistory.length > 0 ? primaryAvg.toFixed(1) : "--"
                        color: primaryColor
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Roboto Mono, monospace"
                    }
                }

                Item { width: 10; height: 1 }

                // Reset button
                Rectangle {
                    width: 50
                    height: 24
                    radius: 4
                    color: resetMouse.pressed ? "#FF5252" : "#333333"
                    anchors.verticalCenter: parent.verticalCenter

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
                        onClicked: root.resetStats()
                    }
                }
            }
        }
    }

    // Sample timer
    Timer {
        interval: 1000 / samplesPerSecond
        running: true
        repeat: true
        onTriggered: addSample()
    }
}
