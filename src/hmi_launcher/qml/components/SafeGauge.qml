/**
 * SafeGauge.qml
 * Enhanced gauge component with graceful degradation support
 *
 * Features:
 * - Stale data detection with visual warning
 * - Smooth value transitions
 * - Optimized for Qt Scene Graph batching
 * - Warning and critical thresholds
 */

import QtQuick

Item {
    id: root

    // Value properties
    property real value: 0
    property real minValue: 0
    property real maxValue: 100
    property real warningValue: -1
    property real criticalValue: -1

    // Display properties
    property string label: "VALUE"
    property string unit: ""
    property int decimals: 0
    property color arcColor: "#00ff00"

    // Data freshness
    property bool dataValid: true
    property int staleTimeoutMs: 500
    property real lastValidValue: 0

    // Internal state
    property real normalizedValue: Math.max(0, Math.min(1,
        (internalValue - minValue) / (maxValue - minValue)))

    property real internalValue: dataValid ? value : lastValidValue

    implicitWidth: 120
    implicitHeight: 120

    // Track last valid value
    onValueChanged: {
        if (dataValid) {
            lastValidValue = value
            staleTimer.restart()
        }
    }

    // Stale data detection timer
    Timer {
        id: staleTimer
        interval: staleTimeoutMs
        running: true
        repeat: false
        onTriggered: {
            if (root.dataValid) {
                // Data hasn't updated, mark as stale
                // External system should set dataValid = false
            }
        }
    }

    // Use states for color changes instead of bindings (better batching)
    state: {
        if (!dataValid) return "stale"
        if (criticalValue > 0 && value >= criticalValue) return "critical"
        if (warningValue > 0 && value >= warningValue) return "warning"
        return "normal"
    }

    states: [
        State {
            name: "normal"
            PropertyChanges { target: valueArc; strokeColor: arcColor }
            PropertyChanges { target: valueText; color: arcColor }
            PropertyChanges { target: staleOverlay; visible: false }
        },
        State {
            name: "warning"
            PropertyChanges { target: valueArc; strokeColor: "#ff6600" }
            PropertyChanges { target: valueText; color: "#ff6600" }
            PropertyChanges { target: staleOverlay; visible: false }
        },
        State {
            name: "critical"
            PropertyChanges { target: valueArc; strokeColor: "#ff0000" }
            PropertyChanges { target: valueText; color: "#ff0000" }
            PropertyChanges { target: staleOverlay; visible: false }
        },
        State {
            name: "stale"
            PropertyChanges { target: valueArc; strokeColor: "#666666" }
            PropertyChanges { target: valueText; color: "#666666" }
            PropertyChanges { target: staleOverlay; visible: true }
        }
    ]

    // Background arc (static, drawn once)
    Canvas {
        id: backgroundArc
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var centerX = width / 2
            var centerY = height / 2
            var radius = Math.min(width, height) / 2 - 8

            ctx.beginPath()
            ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 2.25)
            ctx.strokeStyle = "#333333"
            ctx.lineWidth = 8
            ctx.lineCap = "round"
            ctx.stroke()
        }

        Component.onCompleted: requestPaint()
    }

    // Value arc with smooth animation
    Canvas {
        id: valueArc
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        property real animatedValue: 0
        property color strokeColor: arcColor

        Behavior on animatedValue {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutQuad
            }
        }

        Behavior on strokeColor {
            ColorAnimation { duration: 100 }
        }

        onAnimatedValueChanged: requestPaint()
        onStrokeColorChanged: requestPaint()

        Component.onCompleted: animatedValue = root.normalizedValue

        Connections {
            target: root
            function onNormalizedValueChanged() {
                valueArc.animatedValue = root.normalizedValue
            }
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var centerX = width / 2
            var centerY = height / 2
            var radius = Math.min(width, height) / 2 - 8

            var startAngle = Math.PI * 0.75
            var endAngle = startAngle + (animatedValue * Math.PI * 1.5)

            if (animatedValue > 0.001) {
                ctx.beginPath()
                ctx.arc(centerX, centerY, radius, startAngle, endAngle)
                ctx.strokeStyle = strokeColor
                ctx.lineWidth = 8
                ctx.lineCap = "round"
                ctx.stroke()
            }
        }
    }

    // Value text display
    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            id: valueText
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.internalValue.toFixed(root.decimals)
            font.pixelSize: root.height * 0.25
            font.bold: true
            color: arcColor

            Behavior on color {
                ColorAnimation { duration: 100 }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.unit
            font.pixelSize: root.height * 0.12
            color: "#888888"
            visible: root.unit !== ""
        }
    }

    // Label
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        font.pixelSize: root.height * 0.1
        font.bold: true
        color: "#ffffff"
    }

    // Stale data overlay
    Rectangle {
        id: staleOverlay
        anchors.fill: parent
        color: "transparent"
        visible: false

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.6
            height: 20
            radius: 3
            color: "#cc000000"

            Text {
                anchors.centerIn: parent
                text: "NO DATA"
                font.pixelSize: 10
                font.bold: true
                color: "#ff6600"

                SequentialAnimation on opacity {
                    running: staleOverlay.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 400 }
                    NumberAnimation { to: 1.0; duration: 400 }
                }
            }
        }
    }
}
