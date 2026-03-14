pragma Singleton
import QtQuick

/**
 * Theme.qml
 * Centralized theming and responsive sizing for automotive UI
 *
 * Design principles:
 * - Touch targets minimum 48dp for automotive use
 * - High contrast for daylight visibility
 * - Scalable based on screen DPI and size
 * - Consistent color palette across all screens
 */
QtObject {
    id: theme

    // Reference design: 800x480 (7" display)
    // All sizes scale proportionally from this base
    readonly property real referenceWidth: 800
    readonly property real referenceHeight: 480

    // Current window dimensions (set by main.qml)
    property real windowWidth: 800
    property real windowHeight: 480

    // Scale factors
    readonly property real scaleX: windowWidth / referenceWidth
    readonly property real scaleY: windowHeight / referenceHeight
    readonly property real scale: Math.min(scaleX, scaleY)

    // Responsive size helper function
    function dp(value) {
        return Math.round(value * scale)
    }

    function sp(value) {
        // Font scaling with minimum readable size
        return Math.max(Math.round(value * scale), 10)
    }

    // ═══════════════════════════════════════════════════════════════
    // COLOR PALETTE
    // ═══════════════════════════════════════════════════════════════

    // Background colors
    readonly property color backgroundPrimary: "#0a0a0a"
    readonly property color backgroundSecondary: "#1a1a1a"
    readonly property color backgroundTertiary: "#2a2a2a"
    readonly property color backgroundElevated: "#333333"

    // Surface colors
    readonly property color surfaceCard: "#1e1e1e"
    readonly property color surfaceAlt: "#252525"
    readonly property color surfacePressed: "#444444"
    readonly property color surfaceHover: "#3a3a3a"

    // Text colors
    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#b0b0b0"
    readonly property color textTertiary: "#888888"
    readonly property color textDisabled: "#555555"

    // Status colors
    readonly property color statusOk: "#00ff00"
    readonly property color statusWarning: "#ffaa00"
    readonly property color statusCritical: "#ff0000"
    readonly property color statusInfo: "#00aaff"

    // Accent colors
    readonly property color accentPrimary: "#00ff00"      // Green - primary gauge color
    readonly property color accentSecondary: "#00aaff"    // Blue - info, speed
    readonly property color accentAndroidAuto: "#4285f4"  // Google blue
    readonly property color accentOrange: "#ff6600"       // Warning orange
    readonly property color accentPurple: "#aa00ff"       // Lambda/AFR
    readonly property color accentPink: "#ff00aa"         // Ignition

    // Tab bar colors
    readonly property color tabBarBackground: "#0d0d0d"
    readonly property color tabActive: "#00ff00"
    readonly property color tabInactive: "#666666"

    // Overlay colors
    readonly property color overlayDark: "#cc000000"
    readonly property color overlayLight: "#80000000"

    // ═══════════════════════════════════════════════════════════════
    // SPACING & SIZING
    // ═══════════════════════════════════════════════════════════════

    // Base spacing unit (8dp grid system)
    readonly property int spacingUnit: dp(8)

    // Standard spacing
    readonly property int spacingXs: dp(4)
    readonly property int spacingSm: dp(8)
    readonly property int spacingMd: dp(16)
    readonly property int spacingLg: dp(24)
    readonly property int spacingXl: dp(32)
    readonly property int spacingXxl: dp(48)

    // Touch targets (minimum 48dp for automotive)
    readonly property int touchTargetMin: dp(48)
    readonly property int touchTargetStandard: dp(56)
    readonly property int touchTargetLarge: dp(64)

    // Border radius
    readonly property int radiusSmall: dp(4)
    readonly property int radiusMedium: dp(8)
    readonly property int radiusLarge: dp(12)
    readonly property int radiusRound: dp(999)

    // Border width
    readonly property int borderThin: dp(1)
    readonly property int borderMedium: dp(2)
    readonly property int borderThick: dp(3)

    // ═══════════════════════════════════════════════════════════════
    // COMPONENT SIZES
    // ═══════════════════════════════════════════════════════════════

    // Status bar
    readonly property int statusBarHeight: dp(36)

    // Tab bar
    readonly property int tabBarHeight: dp(64)
    readonly property int tabIconSize: dp(24)
    readonly property int tabIndicatorHeight: dp(3)

    // Gauges
    readonly property int gaugeSmall: dp(80)
    readonly property int gaugeMedium: dp(100)
    readonly property int gaugeLarge: dp(140)
    readonly property int gaugeXLarge: dp(180)

    // Buttons
    readonly property int buttonHeightSmall: dp(36)
    readonly property int buttonHeightMedium: dp(44)
    readonly property int buttonHeightLarge: dp(52)

    // Icons
    readonly property int iconSmall: dp(16)
    readonly property int iconMedium: dp(24)
    readonly property int iconLarge: dp(32)
    readonly property int iconXLarge: dp(48)

    // ═══════════════════════════════════════════════════════════════
    // TYPOGRAPHY
    // ═══════════════════════════════════════════════════════════════

    // Font sizes
    readonly property int fontXs: sp(10)
    readonly property int fontSm: sp(12)
    readonly property int fontMd: sp(14)
    readonly property int fontLg: sp(16)
    readonly property int fontXl: sp(20)
    readonly property int fontXxl: sp(24)
    readonly property int fontDisplay: sp(32)
    readonly property int fontHero: sp(48)
    readonly property int fontMega: sp(72)

    // Font weights (using font.weight)
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightBold: Font.Bold

    // ═══════════════════════════════════════════════════════════════
    // ANIMATIONS
    // ═══════════════════════════════════════════════════════════════

    // Duration (fast for automotive - no lag feeling)
    readonly property int animationFast: 100
    readonly property int animationNormal: 150
    readonly property int animationSlow: 250

    // Easing
    readonly property int easingType: Easing.OutQuad

    // ═══════════════════════════════════════════════════════════════
    // SHADOWS & EFFECTS
    // ═══════════════════════════════════════════════════════════════

    readonly property int shadowRadius: dp(8)
    readonly property int shadowOffset: dp(2)
    readonly property color shadowColor: "#40000000"

    // ═══════════════════════════════════════════════════════════════
    // HELPER FUNCTIONS
    // ═══════════════════════════════════════════════════════════════

    // Get gauge size based on available space
    function getGaugeSize(availableWidth, availableHeight, columns, rows) {
        var maxWidth = (availableWidth - (columns + 1) * spacingMd) / columns
        var maxHeight = (availableHeight - (rows + 1) * spacingMd) / rows
        return Math.min(maxWidth, maxHeight, gaugeLarge)
    }

    // Get responsive font size for value displays
    function getValueFontSize(containerHeight) {
        return Math.min(Math.max(containerHeight * 0.25, fontLg), fontHero)
    }

    // Check if we're in landscape mode
    function isLandscape() {
        return windowWidth > windowHeight
    }

    // Get safe area (accounting for status bar and tab bar)
    function getSafeAreaHeight() {
        return windowHeight - statusBarHeight - tabBarHeight
    }
}
