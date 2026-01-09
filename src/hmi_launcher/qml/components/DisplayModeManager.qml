pragma Singleton
import QtQuick

/**
 * DisplayModeManager.qml - Display mode controller (Porsche-inspired)
 *
 * Modes:
 * - Sport:   RPM large, shift lights prominent, vibrant colors (Track/Performance)
 * - Eco:     Speed central, consumption/range focus (Daily use)
 * - Minimal: Only speed + critical warnings (Night/Highway)
 * - Full:    All data visible (Diagnostics/Tuning)
 *
 * Features:
 * - Smooth transitions between modes
 * - Auto-mode switching based on conditions
 * - Per-component visibility control
 * - Layout presets for each mode
 */
QtObject {
    id: displayModeManager

    // ═══════════════════════════════════════════════════════════════
    // MODE ENUM
    // ═══════════════════════════════════════════════════════════════

    enum Mode {
        Sport,
        Eco,
        Minimal,
        Full
    }

    // Current display mode
    property int currentMode: DisplayModeManager.Mode.Sport
    property int previousMode: DisplayModeManager.Mode.Sport

    // Auto-mode settings
    property bool autoModeEnabled: false
    property real speedThresholdHighway: 100  // km/h for minimal mode
    property real rpmThresholdSport: 4000     // RPM to auto-switch to sport

    // Transition state
    property bool transitioning: false
    property real transitionProgress: 1.0

    // Mode change signal
    signal modeChanged(int newMode, int oldMode)
    signal transitionStarted(int toMode)
    signal transitionCompleted(int mode)

    // ═══════════════════════════════════════════════════════════════
    // MODE DESCRIPTIONS
    // ═══════════════════════════════════════════════════════════════

    readonly property var modeInfo: ({
        [DisplayModeManager.Mode.Sport]: {
            name: "SPORT",
            description: "Track & Performance",
            icon: "\u26A1", // Lightning
            accentColor: "#FF5722"
        },
        [DisplayModeManager.Mode.Eco]: {
            name: "ECO",
            description: "Daily Driving",
            icon: "\u2618", // Leaf
            accentColor: "#4CAF50"
        },
        [DisplayModeManager.Mode.Minimal]: {
            name: "MINIMAL",
            description: "Night & Highway",
            icon: "\u263D", // Moon
            accentColor: "#FFFFFF"
        },
        [DisplayModeManager.Mode.Full]: {
            name: "FULL",
            description: "Diagnostics",
            icon: "\u2699", // Gear
            accentColor: "#2196F3"
        }
    })

    // ═══════════════════════════════════════════════════════════════
    // VISIBILITY CONFIGURATION PER MODE
    // ═══════════════════════════════════════════════════════════════

    // What's visible in each mode
    readonly property var modeVisibility: ({
        [DisplayModeManager.Mode.Sport]: {
            rpmGauge: true,
            speedGauge: true,
            shiftLights: true,
            gearIndicator: true,
            coolantTemp: true,
            oilTemp: false,
            oilPressure: false,
            map: false,
            tps: false,
            afr: true,
            iat: false,
            boostGauge: true,
            batteryVoltage: false,
            fuelPressure: false,
            egt: false,
            gForce: true,
            lapTimer: true,
            peakRecall: true,
            tripComputer: false,
            veMap: false,
            dataLogger: false,
            statusBar: true,
            navBar: true,
            warningIndicators: true
        },
        [DisplayModeManager.Mode.Eco]: {
            rpmGauge: false,
            speedGauge: true,
            shiftLights: false,
            gearIndicator: true,
            coolantTemp: true,
            oilTemp: false,
            oilPressure: false,
            map: false,
            tps: false,
            afr: false,
            iat: false,
            boostGauge: false,
            batteryVoltage: true,
            fuelPressure: false,
            egt: false,
            gForce: false,
            lapTimer: false,
            peakRecall: false,
            tripComputer: true,
            veMap: false,
            dataLogger: false,
            statusBar: true,
            navBar: true,
            warningIndicators: true
        },
        [DisplayModeManager.Mode.Minimal]: {
            rpmGauge: false,
            speedGauge: true,
            shiftLights: false,
            gearIndicator: true,
            coolantTemp: false,
            oilTemp: false,
            oilPressure: false,
            map: false,
            tps: false,
            afr: false,
            iat: false,
            boostGauge: false,
            batteryVoltage: false,
            fuelPressure: false,
            egt: false,
            gForce: false,
            lapTimer: false,
            peakRecall: false,
            tripComputer: false,
            veMap: false,
            dataLogger: false,
            statusBar: false,
            navBar: false,
            warningIndicators: true  // Always show critical warnings
        },
        [DisplayModeManager.Mode.Full]: {
            rpmGauge: true,
            speedGauge: true,
            shiftLights: true,
            gearIndicator: true,
            coolantTemp: true,
            oilTemp: true,
            oilPressure: true,
            map: true,
            tps: true,
            afr: true,
            iat: true,
            boostGauge: true,
            batteryVoltage: true,
            fuelPressure: true,
            egt: true,
            gForce: true,
            lapTimer: true,
            peakRecall: true,
            tripComputer: true,
            veMap: true,
            dataLogger: true,
            statusBar: true,
            navBar: true,
            warningIndicators: true
        }
    })

    // ═══════════════════════════════════════════════════════════════
    // LAYOUT CONFIGURATION PER MODE
    // ═══════════════════════════════════════════════════════════════

    readonly property var modeLayout: ({
        [DisplayModeManager.Mode.Sport]: {
            // Primary gauges dominate
            primaryGauge: "rpm",           // Main large gauge
            secondaryGauge: "speed",       // Secondary gauge
            primaryGaugeSize: "large",     // 300px
            secondaryGaugeSize: "medium",  // 200px
            compactGaugesPosition: "bottom",
            shiftLightsPosition: "top",
            gearPosition: "center",
            backgroundColor: "#0a0a0a",
            animationIntensity: "high"
        },
        [DisplayModeManager.Mode.Eco]: {
            primaryGauge: "speed",
            secondaryGauge: "none",
            primaryGaugeSize: "xlarge",    // 400px
            secondaryGaugeSize: "none",
            compactGaugesPosition: "sides",
            shiftLightsPosition: "hidden",
            gearPosition: "bottom-left",
            backgroundColor: "#0d1117",
            animationIntensity: "low"
        },
        [DisplayModeManager.Mode.Minimal]: {
            primaryGauge: "speed",
            secondaryGauge: "none",
            primaryGaugeSize: "fullscreen", // Digital, large
            secondaryGaugeSize: "none",
            compactGaugesPosition: "hidden",
            shiftLightsPosition: "hidden",
            gearPosition: "bottom-center",
            backgroundColor: "#000000",
            animationIntensity: "minimal"
        },
        [DisplayModeManager.Mode.Full]: {
            primaryGauge: "rpm",
            secondaryGauge: "speed",
            primaryGaugeSize: "medium",
            secondaryGaugeSize: "medium",
            compactGaugesPosition: "grid",  // All visible in grid
            shiftLightsPosition: "top",
            gearPosition: "center",
            backgroundColor: "#0a0a0a",
            animationIntensity: "medium"
        }
    })

    // ═══════════════════════════════════════════════════════════════
    // GAUGE SIZE PRESETS
    // ═══════════════════════════════════════════════════════════════

    readonly property var gaugeSizes: ({
        small: 120,
        medium: 200,
        large: 300,
        xlarge: 400,
        fullscreen: 500
    })

    // ═══════════════════════════════════════════════════════════════
    // MODE SWITCHING
    // ═══════════════════════════════════════════════════════════════

    function setMode(newMode) {
        if (newMode === currentMode) return

        previousMode = currentMode
        transitioning = true
        transitionProgress = 0

        transitionStarted(newMode)

        // Animate transition
        transitionAnimation.to = newMode
        transitionAnimation.start()
    }

    function nextMode() {
        var modes = [
            DisplayModeManager.Mode.Sport,
            DisplayModeManager.Mode.Eco,
            DisplayModeManager.Mode.Minimal,
            DisplayModeManager.Mode.Full
        ]
        var currentIndex = modes.indexOf(currentMode)
        var nextIndex = (currentIndex + 1) % modes.length
        setMode(modes[nextIndex])
    }

    function previousModeFunc() {
        var modes = [
            DisplayModeManager.Mode.Sport,
            DisplayModeManager.Mode.Eco,
            DisplayModeManager.Mode.Minimal,
            DisplayModeManager.Mode.Full
        ]
        var currentIndex = modes.indexOf(currentMode)
        var prevIndex = (currentIndex - 1 + modes.length) % modes.length
        setMode(modes[prevIndex])
    }

    // ═══════════════════════════════════════════════════════════════
    // VISIBILITY HELPERS
    // ═══════════════════════════════════════════════════════════════

    function isVisible(componentName) {
        var visibility = modeVisibility[currentMode]
        return visibility ? (visibility[componentName] || false) : false
    }

    function getLayout() {
        return modeLayout[currentMode] || modeLayout[DisplayModeManager.Mode.Sport]
    }

    function getGaugeSize(sizePreset) {
        return gaugeSizes[sizePreset] || gaugeSizes.medium
    }

    function getModeInfo(mode) {
        return modeInfo[mode] || modeInfo[DisplayModeManager.Mode.Sport]
    }

    function getCurrentModeInfo() {
        return getModeInfo(currentMode)
    }

    // ═══════════════════════════════════════════════════════════════
    // AUTO-MODE LOGIC
    // ═══════════════════════════════════════════════════════════════

    function evaluateAutoMode(speed, rpm, timeOfDay) {
        if (!autoModeEnabled) return

        // Night time (22:00 - 06:00) + highway speed = Minimal
        var hour = timeOfDay ? timeOfDay.getHours() : new Date().getHours()
        var isNight = hour >= 22 || hour < 6

        if (isNight && speed > speedThresholdHighway) {
            if (currentMode !== DisplayModeManager.Mode.Minimal) {
                setMode(DisplayModeManager.Mode.Minimal)
            }
            return
        }

        // High RPM = Sport mode
        if (rpm > rpmThresholdSport) {
            if (currentMode !== DisplayModeManager.Mode.Sport) {
                setMode(DisplayModeManager.Mode.Sport)
            }
            return
        }

        // Default to Eco during day with normal driving
        if (!isNight && speed < speedThresholdHighway && rpm < rpmThresholdSport) {
            if (currentMode !== DisplayModeManager.Mode.Eco) {
                setMode(DisplayModeManager.Mode.Eco)
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // TRANSITION ANIMATION
    // ═══════════════════════════════════════════════════════════════

    property NumberAnimation transitionAnimation: NumberAnimation {
        target: displayModeManager
        property: "transitionProgress"
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic

        onFinished: {
            currentMode = to
            transitioning = false
            modeChanged(currentMode, previousMode)
            transitionCompleted(currentMode)
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // OPACITY HELPERS (for smooth fade transitions)
    // ═══════════════════════════════════════════════════════════════

    function getComponentOpacity(componentName) {
        if (!transitioning) {
            return isVisible(componentName) ? 1.0 : 0.0
        }

        var currentVisible = modeVisibility[previousMode] ? modeVisibility[previousMode][componentName] : false
        var targetVisible = modeVisibility[currentMode] ? modeVisibility[currentMode][componentName] : false

        if (currentVisible && targetVisible) return 1.0
        if (!currentVisible && !targetVisible) return 0.0
        if (currentVisible && !targetVisible) return 1.0 - transitionProgress
        if (!currentVisible && targetVisible) return transitionProgress

        return 1.0
    }

    // Get interpolated gauge size during transition
    function getInterpolatedGaugeSize(gaugeName) {
        var currentLayout = modeLayout[previousMode]
        var targetLayout = modeLayout[currentMode]

        var currentSize = gaugeSizes[currentLayout.primaryGaugeSize] || gaugeSizes.medium
        var targetSize = gaugeSizes[targetLayout.primaryGaugeSize] || gaugeSizes.medium

        if (!transitioning) return targetSize

        return currentSize + (targetSize - currentSize) * transitionProgress
    }
}
