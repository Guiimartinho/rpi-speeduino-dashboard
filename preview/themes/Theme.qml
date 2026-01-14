pragma Singleton
import QtQuick

/**
 * Theme.qml - Singleton theme system for Speeduino Dashboard
 *
 * Supports multiple display modes: Sport, Eco, Minimal, Full
 * Based on Google Android Automotive "Build from Black" guidelines
 * and Porsche Taycan HMI inspiration
 */
QtObject {
    id: theme

    // ═══════════════════════════════════════════════════════════════════════
    // THEME MODE
    // ═══════════════════════════════════════════════════════════════════════

    enum Mode {
        Sport,      // RPM grande, shift lights proeminentes, cores vibrantes
        Eco,        // Velocidade central, consumo, range
        Minimal,    // Apenas velocidade + warnings críticos
        Full        // Todos os dados visíveis (diagnóstico)
    }

    property int currentMode: Theme.Mode.Sport

    // ═══════════════════════════════════════════════════════════════════════
    // BASE COLORS (Build from Black - Android Automotive Guidelines)
    // ═══════════════════════════════════════════════════════════════════════

    // Backgrounds
    readonly property color bgPrimary: "#0a0a0a"        // Main background (almost black)
    readonly property color bgSecondary: "#141414"      // Cards, panels
    readonly property color bgTertiary: "#1e1e1e"       // Elevated elements
    readonly property color bgSurface: "#242424"        // Gauge backgrounds
    readonly property color bgElevated: "#2a2a2a"       // Highest elevation

    // Borders and dividers
    readonly property color borderSubtle: "#333333"
    readonly property color borderMedium: "#444444"
    readonly property color divider: "#2a2a2a"

    // ═══════════════════════════════════════════════════════════════════════
    // TEXT COLORS (White Opacity System - 4.5:1 contrast minimum)
    // ═══════════════════════════════════════════════════════════════════════

    readonly property color textPrimary: Qt.rgba(1, 1, 1, 0.96)      // 96% - main values
    readonly property color textSecondary: Qt.rgba(1, 1, 1, 0.72)    // 72% - labels
    readonly property color textTertiary: Qt.rgba(1, 1, 1, 0.56)     // 56% - hints
    readonly property color textDisabled: Qt.rgba(1, 1, 1, 0.38)     // 38% - disabled

    // ═══════════════════════════════════════════════════════════════════════
    // ACCENT COLORS (Minimal use - purposeful)
    // ═══════════════════════════════════════════════════════════════════════

    // Primary accent - used for main gauge values, positive status
    readonly property color accentPrimary: currentMode === Theme.Mode.Eco ? "#00B8D4" : "#00E676"

    // Secondary accent - used for secondary values
    readonly property color accentSecondary: "#00B8D4"  // Cyan

    // Status colors
    readonly property color statusOk: "#00E676"         // Green - all good
    readonly property color statusInfo: "#00B8D4"       // Cyan - informational
    readonly property color statusWarning: "#FFB300"    // Amber - attention needed
    readonly property color statusCritical: "#FF3D00"   // Red - critical/danger
    readonly property color statusPurple: "#AA00FF"     // Purple - AFR/Lambda

    // Gauge-specific colors
    readonly property color gaugeRpm: currentMode === Theme.Mode.Eco ? "#00B8D4" : "#00E676"
    readonly property color gaugeSpeed: "#00B8D4"
    readonly property color gaugeCoolant: "#00E676"
    readonly property color gaugeMap: "#FF6D00"
    readonly property color gaugeTps: "#00E676"
    readonly property color gaugeAfr: "#AA00FF"
    readonly property color gaugeBoost: "#FF6D00"
    readonly property color gaugeOil: "#FFB300"

    // ═══════════════════════════════════════════════════════════════════════
    // SHIFT LIGHT COLORS (Haltech-inspired)
    // ═══════════════════════════════════════════════════════════════════════

    readonly property color shiftGreen: "#00E676"
    readonly property color shiftYellow: "#FFEA00"
    readonly property color shiftOrange: "#FF9100"
    readonly property color shiftRed: "#FF1744"

    // Shift light thresholds (percentage of redline)
    readonly property real shiftThreshold1: 0.70    // Green starts
    readonly property real shiftThreshold2: 0.80    // Yellow
    readonly property real shiftThreshold3: 0.90    // Orange
    readonly property real shiftThreshold4: 0.95    // Red
    readonly property real shiftThreshold5: 0.98    // Blink

    // ═══════════════════════════════════════════════════════════════════════
    // TYPOGRAPHY
    // ═══════════════════════════════════════════════════════════════════════

    // Font families (fallback chain)
    readonly property string fontPrimary: "Roboto Mono, JetBrains Mono, Consolas, monospace"
    readonly property string fontSecondary: "Roboto, Inter, Segoe UI, sans-serif"
    readonly property string fontMono: "Roboto Mono, JetBrains Mono, Consolas, monospace"

    // Font sizes based on display mode
    readonly property int fontSizeHuge: currentMode === Theme.Mode.Minimal ? 72 : 64      // Main value (RPM/Speed)
    readonly property int fontSizeLarge: 48         // Secondary main value
    readonly property int fontSizeMedium: 32        // Compact gauge values
    readonly property int fontSizeNormal: 24        // Regular values
    readonly property int fontSizeSmall: 14         // Labels
    readonly property int fontSizeXSmall: 12        // Units, hints
    readonly property int fontSizeTiny: 10          // Status indicators

    // Font weights
    readonly property int fontWeightLight: Font.Light
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightBold: Font.Bold

    // ═══════════════════════════════════════════════════════════════════════
    // SPACING & DIMENSIONS
    // ═══════════════════════════════════════════════════════════════════════

    readonly property int spacingXSmall: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24
    readonly property int spacingXXLarge: 32

    // Border radius (consistent across UI)
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16
    readonly property int radiusRound: 9999         // Pill shape

    // Component heights
    readonly property int statusBarHeight: 36
    readonly property int tabBarHeight: 56
    readonly property int buttonHeight: 48
    readonly property int compactGaugeHeight: 80

    // ═══════════════════════════════════════════════════════════════════════
    // GAUGE DIMENSIONS
    // ═══════════════════════════════════════════════════════════════════════

    // Arc gauge (main RPM/Speed)
    readonly property int arcGaugeSizeLarge: 200
    readonly property int arcGaugeSizeMedium: 160
    readonly property int arcGaugeSizeSmall: 120
    readonly property int arcGaugeStrokeWidth: 12
    readonly property int arcGaugeNeedleWidth: 4

    // Compact gauge
    readonly property int compactGaugeBarHeight: 6
    readonly property int compactGaugeBarRadius: 3

    // Shift light bar
    readonly property int shiftLightCount: 14
    readonly property int shiftLightSize: 16
    readonly property int shiftLightSpacing: 4

    // ═══════════════════════════════════════════════════════════════════════
    // ANIMATIONS
    // ═══════════════════════════════════════════════════════════════════════

    // Needle animation (spring physics)
    readonly property real needleSpring: 3.0
    readonly property real needleDamping: 0.4
    readonly property real needleMass: 0.5

    // Value smoothing
    readonly property int valueAnimationDuration: 150
    readonly property int valueAnimationVelocity: 200

    // Transitions
    readonly property int transitionFast: 150
    readonly property int transitionNormal: 250
    readonly property int transitionSlow: 400

    // Warning blink
    readonly property int blinkFast: 300
    readonly property int blinkNormal: 500
    readonly property int blinkSlow: 800

    // Pulse animation
    readonly property int pulseInterval: 1500

    // ═══════════════════════════════════════════════════════════════════════
    // GLOW & EFFECTS
    // ═══════════════════════════════════════════════════════════════════════

    // Glow opacity (subtle, not overwhelming)
    readonly property real glowOpacity: 0.3
    readonly property real glowRadius: 20
    readonly property real glowSpread: 0.3

    // Shadow/elevation
    readonly property real shadowOpacity: 0.4
    readonly property int shadowRadius: 16
    readonly property int shadowOffsetY: 4

    // Gradient for gauge backgrounds
    function gaugeGradient(baseColor) {
        return Qt.lighter(baseColor, 1.15)
    }

    // ═══════════════════════════════════════════════════════════════════════
    // MODE-SPECIFIC VISIBILITY
    // ═══════════════════════════════════════════════════════════════════════

    readonly property bool showShiftLights: currentMode === Theme.Mode.Sport || currentMode === Theme.Mode.Full
    readonly property bool showSecondaryGauges: currentMode !== Theme.Mode.Minimal
    readonly property bool showAllGauges: currentMode === Theme.Mode.Full
    readonly property bool showLargeRpm: currentMode === Theme.Mode.Sport
    readonly property bool showLargeSpeed: currentMode === Theme.Mode.Eco || currentMode === Theme.Mode.Minimal

    // ═══════════════════════════════════════════════════════════════════════
    // HELPER FUNCTIONS
    // ═══════════════════════════════════════════════════════════════════════

    // Get color based on value percentage and thresholds
    function getValueColor(percentage, normalColor) {
        if (percentage >= 0.95) return statusCritical
        if (percentage >= 0.85) return statusWarning
        return normalColor
    }

    // Get temperature color (blue -> green -> yellow -> red)
    function getTempColor(temp, minTemp, maxTemp) {
        var normalizedTemp = (temp - minTemp) / (maxTemp - minTemp)
        if (normalizedTemp < 0.3) return "#00B8D4"      // Cold - cyan
        if (normalizedTemp < 0.7) return statusOk       // Normal - green
        if (normalizedTemp < 0.9) return statusWarning  // Warm - amber
        return statusCritical                            // Hot - red
    }

    // Get AFR color (lean -> stoich -> rich)
    function getAfrColor(afr) {
        if (afr < 13.5) return "#FF6D00"       // Rich - orange
        if (afr < 14.2) return statusOk        // Slightly rich - green
        if (afr <= 15.0) return statusOk       // Stoich range - green
        if (afr <= 16.0) return statusWarning  // Slightly lean - amber
        return statusCritical                   // Lean - red
    }

    // Create glow color from accent
    function glowColor(accentColor) {
        return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, glowOpacity)
    }

    // Mode name for display
    function modeName(mode) {
        switch(mode) {
            case Theme.Mode.Sport: return "SPORT"
            case Theme.Mode.Eco: return "ECO"
            case Theme.Mode.Minimal: return "MINIMAL"
            case Theme.Mode.Full: return "FULL"
            default: return "SPORT"
        }
    }
}
