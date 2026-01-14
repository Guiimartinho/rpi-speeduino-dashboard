import QtQuick

/**
 * GearIndicator.qml - Large gear position display
 *
 * Features:
 * - Large, prominent gear number display
 * - Special displays for Neutral (N), Reverse (R), Park (P)
 * - Color coding per gear
 * - Smooth animations on gear change
 * - Optional gear label
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property int gear: 0                    // 0=N, -1=R, 1-6=gears, 7=P (park)
    property bool showLabel: true
    property bool showBackground: true
    property bool showGlow: true

    property int size: 80

    // Colors
    property color neutralColor: "#00B8D4"      // Cyan for neutral
    property color reverseColor: "#FF9100"      // Orange for reverse
    property color gear1Color: "#00E676"        // Green
    property color gear2Color: "#00E676"
    property color gear3Color: "#00E676"
    property color gear4Color: "#00B8D4"        // Cyan for higher gears
    property color gear5Color: "#00B8D4"
    property color gear6Color: "#00B8D4"
    property color backgroundColor: "#1a1a1a"

    width: size
    height: showLabel ? size + 20 : size

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    readonly property string gearText: {
        switch(gear) {
            case -1: return "R"
            case 0: return "N"
            case 7: return "P"
            default: return gear.toString()
        }
    }

    readonly property color currentColor: {
        switch(gear) {
            case -1: return reverseColor
            case 0: return neutralColor
            case 1: return gear1Color
            case 2: return gear2Color
            case 3: return gear3Color
            case 4: return gear4Color
            case 5: return gear5Color
            case 6: return gear6Color
            case 7: return neutralColor
            default: return neutralColor
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // GLOW EFFECT
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        visible: showGlow
        anchors.centerIn: gearContainer
        width: size + 20
        height: size + 20
        radius: 16
        color: currentColor
        opacity: 0.15

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    Rectangle {
        visible: showGlow
        anchors.centerIn: gearContainer
        width: size + 10
        height: size + 10
        radius: 12
        color: currentColor
        opacity: 0.25

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // GEAR CONTAINER
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: gearContainer
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: size
        height: size
        radius: 12
        color: showBackground ? backgroundColor : "transparent"
        border.color: currentColor
        border.width: 2

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        // Gear number
        Text {
            id: gearText
            anchors.centerIn: parent
            text: root.gearText
            color: currentColor
            font.pixelSize: size * 0.6
            font.bold: true
            font.family: "Roboto Mono, monospace"

            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            // Scale animation on gear change
            Behavior on text {
                SequentialAnimation {
                    NumberAnimation {
                        target: gearText
                        property: "scale"
                        to: 1.2
                        duration: 100
                        easing.type: Easing.OutQuad
                    }
                    NumberAnimation {
                        target: gearText
                        property: "scale"
                        to: 1.0
                        duration: 150
                        easing.type: Easing.OutBounce
                    }
                }
            }
        }

        // Inner highlight
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 10
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // LABEL
    // ═══════════════════════════════════════════════════════════════════════

    Text {
        visible: showLabel
        anchors.top: gearContainer.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: "GEAR"
        color: Qt.rgba(1, 1, 1, 0.56)
        font.pixelSize: 10
        font.family: "Roboto, sans-serif"
        font.weight: Font.Medium
    }

    // ═══════════════════════════════════════════════════════════════════════
    // REVERSE BLINK ANIMATION
    // ═══════════════════════════════════════════════════════════════════════

    SequentialAnimation {
        running: gear === -1
        loops: Animation.Infinite

        NumberAnimation {
            target: gearContainer
            property: "opacity"
            to: 0.6
            duration: 400
        }
        NumberAnimation {
            target: gearContainer
            property: "opacity"
            to: 1.0
            duration: 400
        }
    }

    // Reset opacity when not in reverse
    onGearChanged: {
        if (gear !== -1) {
            gearContainer.opacity = 1.0
        }
    }
}
