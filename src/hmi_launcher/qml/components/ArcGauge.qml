import QtQuick
import QtQuick.Shapes

/**
 * ArcGauge.qml - Main circular gauge (RPM, Speed)
 *
 * Features:
 * - 270° arc with smooth needle animation
 * - Spring physics for realistic needle movement
 * - Color zones (normal, warning, critical)
 * - Large, readable center value
 * - Subtle glow effects
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property real value: 0
    property real minValue: 0
    property real maxValue: 8000
    property real warningValue: 6500
    property real criticalValue: 7500

    property string label: "RPM"
    property string unit: ""
    property int decimals: 0

    property color accentColor: "#00E676"
    property color warningColor: "#FFB300"
    property color criticalColor: "#FF3D00"

    property bool showTicks: true
    property bool showLabels: true
    property bool showGlow: true
    property int majorTickCount: 8
    property int minorTicksPerMajor: 4

    property int gaugeSize: 200
    width: gaugeSize
    height: gaugeSize

    // ═══════════════════════════════════════════════════════════════════════
    // CALCULATED PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    readonly property real startAngle: 135
    readonly property real endAngle: 405
    readonly property real sweepAngle: 270

    readonly property real normalizedValue: Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
    readonly property real currentAngle: startAngle + (normalizedValue * sweepAngle)

    readonly property real centerX: width / 2
    readonly property real centerY: height / 2
    readonly property real outerRadius: (gaugeSize / 2) - 8
    readonly property real arcRadius: outerRadius - 8
    readonly property real needleLength: outerRadius - 30
    readonly property real tickRadius: outerRadius - 24
    readonly property real labelRadius: tickRadius - 18

    readonly property color currentColor: {
        if (value >= criticalValue) return criticalColor
        if (value >= warningValue) return warningColor
        return accentColor
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND CIRCLE
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#1a1a1a"

        // Inner circle with gradient
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.88
            height: parent.height * 0.88
            radius: width / 2

            gradient: RadialGradient {
                centerX: parent.width / 2
                centerY: parent.height / 2
                centerRadius: parent.width / 2
                focalX: centerX
                focalY: centerY
                GradientStop { position: 0.0; color: "#222222" }
                GradientStop { position: 0.6; color: "#181818" }
                GradientStop { position: 1.0; color: "#141414" }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ARC TRACK (background)
    // ═══════════════════════════════════════════════════════════════════════

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 8

        ShapePath {
            strokeColor: "#2a2a2a"
            strokeWidth: 14
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.centerX
                centerY: root.centerY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: root.startAngle - 90
                sweepAngle: root.sweepAngle
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // VALUE ARC (filled)
    // ═══════════════════════════════════════════════════════════════════════

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 8

        ShapePath {
            strokeColor: root.currentColor
            strokeWidth: 14
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.centerX
                centerY: root.centerY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: root.startAngle - 90
                sweepAngle: root.normalizedValue * root.sweepAngle

                Behavior on sweepAngle {
                    NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // WARNING & CRITICAL ZONE MARKERS
    // ═══════════════════════════════════════════════════════════════════════

    // Warning zone
    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 4
        opacity: 0.25

        ShapePath {
            strokeColor: root.warningColor
            strokeWidth: 14
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap

            PathAngleArc {
                centerX: root.centerX
                centerY: root.centerY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: root.startAngle - 90 + ((root.warningValue - root.minValue) / (root.maxValue - root.minValue)) * root.sweepAngle
                sweepAngle: ((root.criticalValue - root.warningValue) / (root.maxValue - root.minValue)) * root.sweepAngle
            }
        }
    }

    // Critical zone
    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 4
        opacity: 0.25

        ShapePath {
            strokeColor: root.criticalColor
            strokeWidth: 14
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap

            PathAngleArc {
                centerX: root.centerX
                centerY: root.centerY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: root.startAngle - 90 + ((root.criticalValue - root.minValue) / (root.maxValue - root.minValue)) * root.sweepAngle
                sweepAngle: ((root.maxValue - root.criticalValue) / (root.maxValue - root.minValue)) * root.sweepAngle
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MAJOR TICK MARKS
    // ═══════════════════════════════════════════════════════════════════════

    Repeater {
        model: showTicks ? (majorTickCount + 1) : 0

        Item {
            anchors.fill: parent

            property real tickAngle: root.startAngle + (index / majorTickCount) * root.sweepAngle
            property real tickRad: tickAngle * Math.PI / 180
            property real tickVal: root.minValue + (index / majorTickCount) * (root.maxValue - root.minValue)
            property color tickColor: tickVal >= root.criticalValue ? root.criticalColor :
                                      tickVal >= root.warningValue ? root.warningColor : "#666666"

            // Tick line
            Rectangle {
                x: root.centerX + root.tickRadius * Math.cos(parent.tickRad) - 1
                y: root.centerY + root.tickRadius * Math.sin(parent.tickRad) - 7
                width: 2
                height: 14
                color: parent.tickColor
                rotation: parent.tickAngle + 90
                transformOrigin: Item.Center
                antialiasing: true
            }

            // Tick label
            Text {
                visible: showLabels && gaugeSize >= 140
                x: root.centerX + root.labelRadius * Math.cos(parent.tickRad) - width/2
                y: root.centerY + root.labelRadius * Math.sin(parent.tickRad) - height/2
                text: root.maxValue >= 1000 ? (parent.tickVal / 1000).toFixed(0) : parent.tickVal.toFixed(0)
                color: parent.tickColor
                font.pixelSize: Math.max(9, gaugeSize * 0.055)
                font.family: "Roboto Mono, monospace"
                font.bold: true
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // NEEDLE WITH SPRING ANIMATION
    // ═══════════════════════════════════════════════════════════════════════

    Item {
        id: needleContainer
        anchors.fill: parent
        rotation: currentAngle

        Behavior on rotation {
            SpringAnimation {
                spring: 3.5
                damping: 0.35
                mass: 0.4
            }
        }

        // Needle shadow/glow
        Rectangle {
            visible: showGlow
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.centerY - needleLength - 4
            width: 10
            height: needleLength + 12
            radius: 5
            color: root.currentColor
            opacity: 0.25
            transformOrigin: Item.Bottom
        }

        // Main needle
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.centerY - needleLength
            width: 5
            height: needleLength + 8
            radius: 2.5
            transformOrigin: Item.Bottom

            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(root.currentColor, 1.4) }
                GradientStop { position: 0.3; color: root.currentColor }
                GradientStop { position: 1.0; color: Qt.darker(root.currentColor, 1.3) }
            }
        }

        // Center cap
        Rectangle {
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: 10
            color: "#2a2a2a"
            border.color: root.currentColor
            border.width: 3

            Behavior on border.color {
                ColorAnimation { duration: 150 }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CENTER VALUE DISPLAY
    // ═══════════════════════════════════════════════════════════════════════

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: gaugeSize * 0.18
        spacing: 2

        // Main value (large)
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value.toFixed(decimals)
            color: root.currentColor
            font.pixelSize: Math.max(24, gaugeSize * 0.24)
            font.bold: true
            font.family: "Roboto Mono, monospace"

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }

        // Label
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label + (root.unit ? " " + root.unit : "")
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: Math.max(10, gaugeSize * 0.07)
            font.family: "Roboto, sans-serif"
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CRITICAL BLINK
    // ═══════════════════════════════════════════════════════════════════════

    SequentialAnimation {
        running: value >= criticalValue
        loops: Animation.Infinite

        NumberAnimation { target: needleContainer; property: "opacity"; to: 0.4; duration: 120 }
        NumberAnimation { target: needleContainer; property: "opacity"; to: 1.0; duration: 120 }
    }
}
