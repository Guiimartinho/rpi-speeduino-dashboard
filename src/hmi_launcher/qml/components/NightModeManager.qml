pragma Singleton
import QtQuick

/**
 * NightModeManager.qml - Automatic night mode with ambient light support
 *
 * Features:
 * - Automatic mode based on time of day
 * - Manual override
 * - Smooth brightness transitions
 * - Headlight signal input support (from CAN)
 * - Theme color adjustments for night visibility
 */
QtObject {
    id: root

    // Night mode state
    property bool isNightMode: false
    property bool autoMode: true
    property bool headlightsOn: false  // External signal from CAN

    // Time-based settings
    property int sunsetHour: 18     // 6 PM
    property int sunriseHour: 6     // 6 AM

    // Brightness levels (0.0 - 1.0)
    property real dayBrightness: 1.0
    property real nightBrightness: 0.5
    property real currentBrightness: dayBrightness

    // Theme colors - automatically adjusted based on mode
    property color backgroundColor: isNightMode ? "#050505" : "#0a0a0a"
    property color cardBackground: isNightMode ? "#0f0f0f" : "#1a1a1a"
    property color borderColor: isNightMode ? "#1a1a1a" : "#333333"

    property color textPrimary: Qt.rgba(1, 1, 1, isNightMode ? 0.87 : 0.96)
    property color textSecondary: Qt.rgba(1, 1, 1, isNightMode ? 0.56 : 0.72)
    property color textTertiary: Qt.rgba(1, 1, 1, isNightMode ? 0.28 : 0.38)

    property color accentGreen: isNightMode ? "#00C853" : "#00E676"
    property color accentRed: isNightMode ? "#D32F2F" : "#FF5252"
    property color accentYellow: isNightMode ? "#F9A825" : "#FFC107"
    property color accentBlue: isNightMode ? "#1976D2" : "#2196F3"

    // Gauge colors adjusted for night
    property color gaugeArc: isNightMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.12)
    property color gaugeText: textPrimary

    // Brightness animation
    Behavior on currentBrightness {
        NumberAnimation {
            duration: 1000
            easing.type: Easing.InOutQuad
        }
    }

    // Check time and update mode
    function updateNightMode() {
        if (!autoMode) return

        var now = new Date()
        var hour = now.getHours()

        // Night mode if between sunset and sunrise, or if headlights are on
        var shouldBeNight = (hour >= sunsetHour || hour < sunriseHour) || headlightsOn

        if (shouldBeNight !== isNightMode) {
            isNightMode = shouldBeNight
            currentBrightness = shouldBeNight ? nightBrightness : dayBrightness
        }
    }

    // Manual toggle
    function toggleNightMode() {
        autoMode = false
        isNightMode = !isNightMode
        currentBrightness = isNightMode ? nightBrightness : dayBrightness
    }

    // Set manual mode
    function setNightMode(night) {
        autoMode = false
        isNightMode = night
        currentBrightness = night ? nightBrightness : dayBrightness
    }

    // Enable auto mode
    function enableAutoMode() {
        autoMode = true
        updateNightMode()
    }

    // Set brightness levels
    function setBrightnessLevels(day, night) {
        dayBrightness = Math.max(0.3, Math.min(1.0, day))
        nightBrightness = Math.max(0.1, Math.min(0.8, night))
        currentBrightness = isNightMode ? nightBrightness : dayBrightness
    }

    // Set time thresholds
    function setTimeThresholds(sunrise, sunset) {
        sunriseHour = Math.max(0, Math.min(12, sunrise))
        sunsetHour = Math.max(12, Math.min(23, sunset))
        updateNightMode()
    }

    // Handle headlight signal from CAN
    function setHeadlightsState(on) {
        headlightsOn = on
        if (autoMode) {
            updateNightMode()
        }
    }

    // Timer for automatic updates
    property Timer updateTimer: Timer {
        interval: 60000  // Check every minute
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: updateNightMode()
    }

    // Get current mode name
    function getModeName() {
        if (!autoMode) {
            return isNightMode ? "NIGHT (MANUAL)" : "DAY (MANUAL)"
        }
        return isNightMode ? "NIGHT (AUTO)" : "DAY (AUTO)"
    }

    // Get brightness percentage
    function getBrightnessPercent() {
        return Math.round(currentBrightness * 100)
    }

    Component.onCompleted: {
        updateNightMode()
    }
}
