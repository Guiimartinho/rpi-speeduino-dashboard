import QtQuick
import QtQuick.Layouts

/**
 * ShiftLightBar.qml - RGB LED shift light bar (Haltech iC-7 inspired)
 *
 * Features:
 * - 14 LEDs with progressive activation
 * - Color progression: green → yellow → orange → red → blinking red
 * - Configurable thresholds
 * - Glow effect on active LEDs
 * - Symmetric center-out or left-to-right activation
 * - Blinking at shift point
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property real rpm: 0
    property real redlineRpm: 7000
    property real activationRpm: 4500      // RPM where first LED lights

    property int ledCount: 14
    property int ledSize: 14
    property int ledSpacing: 4

    // Color thresholds (percentage of range from activation to redline)
    property real yellowThreshold: 0.5     // 50% = yellow starts
    property real orangeThreshold: 0.75    // 75% = orange starts
    property real redThreshold: 0.90       // 90% = red starts
    property real blinkThreshold: 0.98     // 98% = start blinking

    // Colors
    property color greenColor: "#00E676"
    property color yellowColor: "#FFEA00"
    property color orangeColor: "#FF9100"
    property color redColor: "#FF1744"
    property color offColor: "#2a2a2a"
    property color borderColor: "#444444"

    property bool showGlow: true
    property bool centerOut: true          // true = lights from center out, false = left to right
    property bool showBackground: true

    // Size
    width: (ledCount * ledSize) + ((ledCount - 1) * ledSpacing) + 20
    height: ledSize + 16

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    // Normalized RPM in the active range (0 to 1)
    readonly property real normalizedRpm: {
        if (rpm < activationRpm) return 0
        return Math.min(1, (rpm - activationRpm) / (redlineRpm - activationRpm))
    }

    // How many LEDs should be lit
    readonly property int activeLeds: Math.floor(normalizedRpm * ledCount)

    // Should we be blinking?
    readonly property bool shouldBlink: normalizedRpm >= blinkThreshold

    // Blink state
    property bool blinkState: true

    // ═══════════════════════════════════════════════════════════════════════
    // BLINK TIMER
    // ═══════════════════════════════════════════════════════════════════════

    Timer {
        id: blinkTimer
        interval: 100
        running: shouldBlink
        repeat: true
        onTriggered: blinkState = !blinkState
    }

    // Reset blink state when not blinking
    onShouldBlinkChanged: {
        if (!shouldBlink) blinkState = true
    }

    // ═══════════════════════════════════════════════════════════════════════
    // HELPER FUNCTIONS
    // ═══════════════════════════════════════════════════════════════════════

    function getLedColor(ledIndex) {
        // Calculate the threshold this LED represents
        var ledThreshold = (ledIndex + 1) / ledCount

        if (ledThreshold <= yellowThreshold) {
            return greenColor
        } else if (ledThreshold <= orangeThreshold) {
            return yellowColor
        } else if (ledThreshold <= redThreshold) {
            return orangeColor
        } else {
            return redColor
        }
    }

    function isLedActive(ledIndex) {
        if (centerOut) {
            // Center-out pattern: LEDs light from center towards edges
            var centerIndex = ledCount / 2
            var distanceFromCenter = Math.abs(ledIndex - centerIndex + 0.5)
            var maxDistance = ledCount / 2
            var normalizedDistance = distanceFromCenter / maxDistance
            return normalizedDistance <= normalizedRpm
        } else {
            // Left to right pattern
            return ledIndex < activeLeds
        }
    }

    function getLedVisualIndex(ledIndex) {
        if (centerOut) {
            // Reorder for center-out display
            var half = ledCount / 2
            if (ledIndex < half) {
                return half - 1 - ledIndex
            } else {
                return ledIndex
            }
        }
        return ledIndex
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        visible: showBackground
        anchors.fill: parent
        radius: 8
        color: "#0d0d0d"
        border.color: "#333333"
        border.width: 1
    }

    // ═══════════════════════════════════════════════════════════════════════
    // LED ROW
    // ═══════════════════════════════════════════════════════════════════════

    Row {
        anchors.centerIn: parent
        spacing: ledSpacing

        Repeater {
            model: ledCount

            Item {
                width: ledSize
                height: ledSize

                property bool isActive: isLedActive(index)
                property color ledColor: getLedColor(index)
                property bool showLed: isActive && (shouldBlink ? blinkState : true)

                // Glow effect (behind LED)
                Rectangle {
                    visible: showGlow && showLed
                    anchors.centerIn: parent
                    width: ledSize + 8
                    height: ledSize + 8
                    radius: (ledSize + 8) / 2
                    color: ledColor
                    opacity: 0.4

                    // Outer glow
                    Rectangle {
                        anchors.centerIn: parent
                        width: ledSize + 16
                        height: ledSize + 16
                        radius: (ledSize + 16) / 2
                        color: ledColor
                        opacity: 0.2
                    }
                }

                // LED body
                Rectangle {
                    anchors.fill: parent
                    radius: ledSize / 2
                    color: showLed ? ledColor : offColor
                    border.color: showLed ? Qt.lighter(ledColor, 1.2) : borderColor
                    border.width: 1

                    // Inner highlight for 3D effect
                    Rectangle {
                        visible: showLed
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 2
                        width: ledSize * 0.4
                        height: ledSize * 0.4
                        radius: width / 2
                        color: Qt.rgba(1, 1, 1, 0.3)
                    }

                    Behavior on color {
                        ColorAnimation { duration: 50 }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // RPM DISPLAY (optional, can be enabled)
    // ═══════════════════════════════════════════════════════════════════════

    property bool showRpmValue: false

    Text {
        visible: showRpmValue
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: rpm.toFixed(0)
        color: shouldBlink ? redColor : (normalizedRpm > 0 ? getLedColor(activeLeds - 1) : Qt.rgba(1, 1, 1, 0.38))
        font.pixelSize: 11
        font.bold: true
        font.family: "Roboto Mono, monospace"
    }
}
