import QtQuick

Item {
    id: root

    property string label: "VALUE"
    property real value: 0
    property real minValue: 0
    property real maxValue: 100
    property real warningValue: -1
    property real criticalValue: -1
    property string unit: ""
    property int decimals: 0
    property color arcColor: "#00ff00"

    implicitWidth: 120
    implicitHeight: 120

    // Calculate normalized value (0-1)
    readonly property real normalizedValue: Math.max(0, Math.min(1,
        (value - minValue) / (maxValue - minValue)))

    // Determine color based on warning/critical thresholds
    readonly property color displayColor: {
        if (criticalValue > 0 && value >= criticalValue) return "#ff0000"
        if (warningValue > 0 && value >= warningValue) return "#ff6600"
        return arcColor
    }

    // Background circle
    Canvas {
        id: backgroundArc
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var centerX = width / 2
            var centerY = height / 2
            var radius = Math.min(width, height) / 2 - 8

            // Draw background arc
            ctx.beginPath()
            ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 2.25)
            ctx.strokeStyle = "#333333"
            ctx.lineWidth = 8
            ctx.lineCap = "round"
            ctx.stroke()
        }
    }

    // Value arc
    Canvas {
        id: valueArc
        anchors.fill: parent

        property real animatedValue: 0

        Behavior on animatedValue {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutQuad
            }
        }

        onAnimatedValueChanged: requestPaint()

        Component.onCompleted: animatedValue = normalizedValue
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

            // Draw value arc
            var startAngle = Math.PI * 0.75
            var endAngle = startAngle + (animatedValue * Math.PI * 1.5)

            if (animatedValue > 0) {
                ctx.beginPath()
                ctx.arc(centerX, centerY, radius, startAngle, endAngle)
                ctx.strokeStyle = displayColor
                ctx.lineWidth = 8
                ctx.lineCap = "round"
                ctx.stroke()
            }
        }
    }

    // Value text
    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: value.toFixed(decimals)
            font.pixelSize: root.height * 0.25
            font.bold: true
            color: displayColor
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: unit
            font.pixelSize: root.height * 0.12
            color: "#888888"
            visible: unit !== ""
        }
    }

    // Label
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        text: label
        font.pixelSize: root.height * 0.1
        font.bold: true
        color: "#ffffff"
    }
}
