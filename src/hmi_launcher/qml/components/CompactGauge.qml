import QtQuick
import QtQuick.Layouts

/**
 * CompactGauge.qml - Compact gauge with bar and numeric display
 *
 * Features:
 * - Horizontal progress bar with rounded ends
 * - Large numeric value display
 * - Label and unit
 * - Color zones (normal, warning, critical)
 * - Smooth animations
 * - Optional min/max labels
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property real value: 0
    property real minValue: 0
    property real maxValue: 100
    property real warningValue: 80
    property real criticalValue: 95

    property string label: "CLT"
    property string unit: "°C"
    property int decimals: 0

    property color accentColor: "#00E676"
    property color warningColor: "#FFB300"
    property color criticalColor: "#FF3D00"
    property color backgroundColor: "#242424"
    property color trackColor: "#333333"

    property bool showMinMax: false
    property bool showGlow: true
    property bool vertical: false

    // Size
    width: vertical ? 80 : 160
    height: vertical ? 160 : 70

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    readonly property real normalizedValue: Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))

    readonly property color currentColor: {
        if (value >= criticalValue) return criticalColor
        if (value >= warningValue) return warningColor
        return accentColor
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MAIN CONTAINER
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: backgroundColor

        // Subtle inner shadow effect
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 7
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
        }

        // Horizontal Layout
        ColumnLayout {
            visible: !vertical
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            // Top row: Label + Value + Unit
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // Label
                Text {
                    text: root.label
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 12
                    font.family: "Roboto, sans-serif"
                    font.weight: Font.Medium
                }

                Item { Layout.fillWidth: true }

                // Value
                Text {
                    id: valueText
                    text: root.value.toFixed(decimals)
                    color: root.currentColor
                    font.pixelSize: 28
                    font.bold: true
                    font.family: "Roboto Mono, monospace"

                    Behavior on text {
                        enabled: false
                    }

                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                }

                // Unit
                Text {
                    text: root.unit
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 14
                    font.family: "Roboto, sans-serif"
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 4
                }
            }

            // Progress bar
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 8

                // Track (background)
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: root.trackColor
                }

                // Fill (value)
                Rectangle {
                    id: fillBar
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * root.normalizedValue
                    radius: 4
                    color: root.currentColor

                    Behavior on width {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutQuad
                        }
                    }

                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }

                    // Glow effect
                    Rectangle {
                        visible: showGlow && root.normalizedValue > 0.05
                        anchors.fill: parent
                        radius: parent.radius
                        color: root.currentColor
                        opacity: 0.3

                        // Blur simulation
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: parent.radius + 4
                            color: root.currentColor
                            opacity: 0.15
                        }
                    }

                    // Gradient overlay for depth
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.15) }
                            GradientStop { position: 0.5; color: "transparent" }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.1) }
                        }
                    }
                }

                // Warning zone indicator
                Rectangle {
                    visible: root.warningValue < root.maxValue
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    x: parent.width * ((root.warningValue - root.minValue) / (root.maxValue - root.minValue))
                    width: 1
                    color: root.warningColor
                    opacity: 0.5
                }

                // Critical zone indicator
                Rectangle {
                    visible: root.criticalValue < root.maxValue
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    x: parent.width * ((root.criticalValue - root.minValue) / (root.maxValue - root.minValue))
                    width: 1
                    color: root.criticalColor
                    opacity: 0.5
                }
            }

            // Min/Max labels (optional)
            RowLayout {
                visible: showMinMax
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: root.minValue.toFixed(0)
                    color: Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 9
                    font.family: "Roboto Mono, monospace"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.maxValue.toFixed(0)
                    color: Qt.rgba(1, 1, 1, 0.38)
                    font.pixelSize: 9
                    font.family: "Roboto Mono, monospace"
                }
            }
        }

        // Vertical Layout
        ColumnLayout {
            visible: vertical
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            // Label
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: root.label
                color: Qt.rgba(1, 1, 1, 0.56)
                font.pixelSize: 11
                font.family: "Roboto, sans-serif"
                font.weight: Font.Medium
            }

            // Vertical progress bar
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Track (background)
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: parent.height
                    radius: 6
                    color: root.trackColor

                    // Fill (value)
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * root.normalizedValue
                        radius: 6
                        color: root.currentColor

                        Behavior on height {
                            NumberAnimation {
                                duration: 150
                                easing.type: Easing.OutQuad
                            }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 200 }
                        }

                        // Gradient overlay
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.15) }
                                GradientStop { position: 0.5; color: "transparent" }
                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.1) }
                            }
                        }
                    }
                }
            }

            // Value + Unit
            Column {
                Layout.alignment: Qt.AlignHCenter
                spacing: 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.value.toFixed(decimals)
                    color: root.currentColor
                    font.pixelSize: 20
                    font.bold: true
                    font.family: "Roboto Mono, monospace"

                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.unit
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 10
                    font.family: "Roboto, sans-serif"
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CRITICAL PULSE ANIMATION
    // ═══════════════════════════════════════════════════════════════════════

    SequentialAnimation {
        running: value >= criticalValue
        loops: Animation.Infinite

        NumberAnimation {
            target: valueText
            property: "opacity"
            to: 0.5
            duration: 200
        }
        NumberAnimation {
            target: valueText
            property: "opacity"
            to: 1.0
            duration: 200
        }
    }
}
