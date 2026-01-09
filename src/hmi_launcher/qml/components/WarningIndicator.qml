import QtQuick

/**
 * WarningIndicator.qml - Warning/status indicator with animations
 *
 * Features:
 * - Three states: inactive (outline), active (filled + glow + pulse), critical (filled + fast blink)
 * - Customizable icon text
 * - Glow effect when active
 * - Smooth state transitions
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    enum State {
        Inactive,   // Outline only
        Active,     // Filled + glow + pulse
        Critical    // Filled + fast blink
    }

    property int state: WarningIndicator.State.Inactive

    property string icon: "!"               // Icon text (can be emoji or text)
    property string label: "WARNING"        // Label below icon
    property bool showLabel: true

    property color inactiveColor: "#555555"
    property color activeColor: "#FFB300"   // Amber for warnings
    property color criticalColor: "#FF3D00" // Red for critical

    property bool showGlow: true
    property int size: 48

    width: size
    height: showLabel ? size + 16 : size

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    readonly property color currentColor: {
        switch(state) {
            case WarningIndicator.State.Critical: return criticalColor
            case WarningIndicator.State.Active: return activeColor
            default: return inactiveColor
        }
    }

    readonly property bool isActive: state !== WarningIndicator.State.Inactive
    readonly property bool isCritical: state === WarningIndicator.State.Critical

    property real pulseOpacity: 1.0

    // ═══════════════════════════════════════════════════════════════════════
    // ANIMATIONS
    // ═══════════════════════════════════════════════════════════════════════

    // Pulse animation for Active state
    SequentialAnimation {
        id: pulseAnimation
        running: state === WarningIndicator.State.Active
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "pulseOpacity"
            from: 1.0
            to: 0.6
            duration: 800
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: root
            property: "pulseOpacity"
            from: 0.6
            to: 1.0
            duration: 800
            easing.type: Easing.InOutSine
        }
    }

    // Blink animation for Critical state
    property bool blinkVisible: true

    Timer {
        id: blinkTimer
        interval: 300
        running: isCritical
        repeat: true
        onTriggered: blinkVisible = !blinkVisible
    }

    // Reset states when not in use
    onStateChanged: {
        if (state !== WarningIndicator.State.Critical) {
            blinkVisible = true
        }
        if (state !== WarningIndicator.State.Active) {
            pulseOpacity = 1.0
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // GLOW EFFECT
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: glowOuter
        visible: showGlow && isActive && (isCritical ? blinkVisible : true)
        anchors.centerIn: iconContainer
        width: size + 24
        height: size + 24
        radius: (size + 24) / 2
        color: currentColor
        opacity: 0.15 * pulseOpacity

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }

    Rectangle {
        id: glowInner
        visible: showGlow && isActive && (isCritical ? blinkVisible : true)
        anchors.centerIn: iconContainer
        width: size + 12
        height: size + 12
        radius: (size + 12) / 2
        color: currentColor
        opacity: 0.3 * pulseOpacity

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ICON CONTAINER
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: iconContainer
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: size
        height: size
        radius: size / 2

        // Background fill
        color: isActive ? currentColor : "transparent"
        opacity: (isCritical ? (blinkVisible ? 1 : 0.3) : 1) * pulseOpacity

        // Border
        border.color: currentColor
        border.width: isActive ? 0 : 2

        Behavior on color {
            ColorAnimation { duration: 200 }
        }

        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }

        // Icon
        Text {
            anchors.centerIn: parent
            text: root.icon
            color: isActive ? "#000000" : currentColor
            font.pixelSize: size * 0.5
            font.bold: true
            font.family: "Roboto, sans-serif"

            Behavior on color {
                ColorAnimation { duration: 200 }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // LABEL
    // ═══════════════════════════════════════════════════════════════════════

    Text {
        visible: showLabel
        anchors.top: iconContainer.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: isActive ? currentColor : inactiveColor
        font.pixelSize: 9
        font.bold: true
        font.family: "Roboto, sans-serif"
        opacity: (isCritical ? (blinkVisible ? 1 : 0.3) : 1) * pulseOpacity

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }
}
