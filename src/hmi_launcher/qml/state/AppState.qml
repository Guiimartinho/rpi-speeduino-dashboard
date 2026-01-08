pragma Singleton
import QtQuick

/**
 * AppState.qml
 * Robust state machine for automotive HMI
 *
 * Manages:
 * - Screen navigation (Home, Dash, Config, OpenAuto)
 * - Priority overrides (Reverse Camera)
 * - System health states
 * - History for back navigation
 */
QtObject {
    id: appState

    // ═══════════════════════════════════════════════════════════════
    // SCREEN ENUMERATION
    // ═══════════════════════════════════════════════════════════════

    enum Screen {
        Home = 0,
        Dash = 1,
        Config = 2,
        OpenAuto = 3
    }

    enum Overlay {
        None = 0,
        ReverseCamera = 1,
        Alert = 2
    }

    enum SystemMode {
        Normal = 0,
        DegradedCAN = 1,
        DegradedCamera = 2,
        DegradedMultiple = 3,
        SafeMode = 4
    }

    // ═══════════════════════════════════════════════════════════════
    // STATE PROPERTIES
    // ═══════════════════════════════════════════════════════════════

    // Current active screen
    property int currentScreen: AppState.Screen.Home

    // Previous screen (for back navigation)
    property int previousScreen: AppState.Screen.Home

    // Screen before overlay was shown
    property int screenBeforeOverlay: AppState.Screen.Home

    // Current overlay (highest priority)
    property int currentOverlay: AppState.Overlay.None

    // System health mode
    property int systemMode: AppState.SystemMode.Normal

    // Navigation history stack (max 10 entries)
    property var navigationHistory: []
    readonly property int maxHistorySize: 10

    // ═══════════════════════════════════════════════════════════════
    // SUBSYSTEM STATUS
    // ═══════════════════════════════════════════════════════════════

    // CAN bus status
    property bool canConnected: false
    property bool canHealthy: true

    // Camera status
    property bool cameraAvailable: false
    property bool cameraActive: false

    // OpenAuto status
    property bool openAutoRunning: false
    property bool openAutoConnected: false

    // Reverse gear
    property bool reverseEngaged: false

    // Engine status
    property bool engineRunning: false

    // Alerts
    property bool celOn: false
    property bool overheat: false

    // ═══════════════════════════════════════════════════════════════
    // SIGNALS
    // ═══════════════════════════════════════════════════════════════

    signal screenChanged(int newScreen, int oldScreen)
    signal overlayChanged(int newOverlay, int oldOverlay)
    signal systemModeChanged(int newMode, int oldMode)
    signal navigationRequested(int screen)
    signal backRequested()
    signal reverseStateChanged(bool engaged)
    signal alertTriggered(string message, string severity)

    // ═══════════════════════════════════════════════════════════════
    // NAVIGATION METHODS
    // ═══════════════════════════════════════════════════════════════

    /**
     * Navigate to a specific screen
     * @param screen - Target screen (AppState.Screen enum)
     * @param addToHistory - Whether to add current screen to history
     */
    function navigateTo(screen, addToHistory) {
        if (addToHistory === undefined) addToHistory = true

        // Don't navigate if already on this screen
        if (screen === currentScreen && currentOverlay === AppState.Overlay.None) {
            return
        }

        // Clear any overlay first
        if (currentOverlay !== AppState.Overlay.None && currentOverlay !== AppState.Overlay.ReverseCamera) {
            hideOverlay()
        }

        // Add to history if requested
        if (addToHistory && currentScreen !== screen) {
            pushToHistory(currentScreen)
        }

        var oldScreen = currentScreen
        previousScreen = currentScreen
        currentScreen = screen

        console.log("AppState: Navigated from", screenName(oldScreen), "to", screenName(screen))
        screenChanged(screen, oldScreen)
        navigationRequested(screen)
    }

    /**
     * Navigate to Home screen
     */
    function goHome() {
        navigateTo(AppState.Screen.Home, true)
    }

    /**
     * Navigate to Dash screen
     */
    function goDash() {
        navigateTo(AppState.Screen.Dash, true)
    }

    /**
     * Navigate to Config screen
     */
    function goConfig() {
        navigateTo(AppState.Screen.Config, true)
    }

    /**
     * Navigate to OpenAuto screen
     */
    function goOpenAuto() {
        navigateTo(AppState.Screen.OpenAuto, true)
    }

    /**
     * Go back to previous screen
     */
    function goBack() {
        if (navigationHistory.length > 0) {
            var previousScreen = popFromHistory()
            navigateTo(previousScreen, false)
        } else {
            navigateTo(AppState.Screen.Home, false)
        }
        backRequested()
    }

    /**
     * Check if we can go back
     */
    function canGoBack() {
        return navigationHistory.length > 0
    }

    // ═══════════════════════════════════════════════════════════════
    // OVERLAY METHODS
    // ═══════════════════════════════════════════════════════════════

    /**
     * Show reverse camera overlay (highest priority)
     */
    function showReverseCamera() {
        if (currentOverlay !== AppState.Overlay.ReverseCamera) {
            screenBeforeOverlay = currentScreen
            var oldOverlay = currentOverlay
            currentOverlay = AppState.Overlay.ReverseCamera
            overlayChanged(currentOverlay, oldOverlay)
            console.log("AppState: Reverse camera overlay shown")
        }
    }

    /**
     * Hide reverse camera overlay
     */
    function hideReverseCamera() {
        if (currentOverlay === AppState.Overlay.ReverseCamera) {
            var oldOverlay = currentOverlay
            currentOverlay = AppState.Overlay.None
            overlayChanged(currentOverlay, oldOverlay)
            console.log("AppState: Reverse camera overlay hidden")
        }
    }

    /**
     * Show alert overlay
     */
    function showAlert(message, severity) {
        if (currentOverlay !== AppState.Overlay.ReverseCamera) {
            screenBeforeOverlay = currentScreen
            var oldOverlay = currentOverlay
            currentOverlay = AppState.Overlay.Alert
            overlayChanged(currentOverlay, oldOverlay)
            alertTriggered(message, severity)
        }
    }

    /**
     * Hide any non-priority overlay
     */
    function hideOverlay() {
        if (currentOverlay !== AppState.Overlay.ReverseCamera) {
            var oldOverlay = currentOverlay
            currentOverlay = AppState.Overlay.None
            overlayChanged(currentOverlay, oldOverlay)
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // REVERSE GEAR HANDLING
    // ═══════════════════════════════════════════════════════════════

    /**
     * Handle reverse gear state change
     */
    function setReverseEngaged(engaged) {
        if (reverseEngaged !== engaged) {
            reverseEngaged = engaged

            if (engaged) {
                showReverseCamera()
            } else {
                hideReverseCamera()
            }

            reverseStateChanged(engaged)
            console.log("AppState: Reverse gear", engaged ? "ENGAGED" : "disengaged")
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // SYSTEM MODE MANAGEMENT
    // ═══════════════════════════════════════════════════════════════

    /**
     * Update system mode based on subsystem health
     */
    function updateSystemMode() {
        var oldMode = systemMode
        var newMode = AppState.SystemMode.Normal

        var degradedCount = 0

        if (!canConnected || !canHealthy) {
            degradedCount++
            newMode = AppState.SystemMode.DegradedCAN
        }

        if (!cameraAvailable && reverseEngaged) {
            degradedCount++
            if (newMode === AppState.SystemMode.Normal) {
                newMode = AppState.SystemMode.DegradedCamera
            }
        }

        if (degradedCount > 1) {
            newMode = AppState.SystemMode.DegradedMultiple
        }

        if (degradedCount > 2 || (overheat && !canConnected)) {
            newMode = AppState.SystemMode.SafeMode
        }

        if (newMode !== oldMode) {
            systemMode = newMode
            systemModeChanged(newMode, oldMode)
            console.log("AppState: System mode changed from", modeName(oldMode), "to", modeName(newMode))
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // OPENUTO MANAGEMENT
    // ═══════════════════════════════════════════════════════════════

    /**
     * Set OpenAuto running state
     */
    function setOpenAutoRunning(running) {
        if (openAutoRunning !== running) {
            openAutoRunning = running
            console.log("AppState: OpenAuto", running ? "started" : "stopped")

            if (running && currentScreen !== AppState.Screen.OpenAuto) {
                navigateTo(AppState.Screen.OpenAuto, true)
            }
        }
    }

    /**
     * Set OpenAuto connected state (phone connected)
     */
    function setOpenAutoConnected(connected) {
        if (openAutoConnected !== connected) {
            openAutoConnected = connected
            console.log("AppState: OpenAuto", connected ? "connected" : "disconnected")
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // HISTORY MANAGEMENT
    // ═══════════════════════════════════════════════════════════════

    function pushToHistory(screen) {
        // Don't add duplicates
        if (navigationHistory.length > 0 &&
            navigationHistory[navigationHistory.length - 1] === screen) {
            return
        }

        navigationHistory.push(screen)

        // Limit history size
        while (navigationHistory.length > maxHistorySize) {
            navigationHistory.shift()
        }
    }

    function popFromHistory() {
        if (navigationHistory.length > 0) {
            return navigationHistory.pop()
        }
        return AppState.Screen.Home
    }

    function clearHistory() {
        navigationHistory = []
    }

    // ═══════════════════════════════════════════════════════════════
    // UTILITY METHODS
    // ═══════════════════════════════════════════════════════════════

    /**
     * Get screen name for logging
     */
    function screenName(screen) {
        switch (screen) {
            case AppState.Screen.Home: return "Home"
            case AppState.Screen.Dash: return "Dash"
            case AppState.Screen.Config: return "Config"
            case AppState.Screen.OpenAuto: return "OpenAuto"
            default: return "Unknown"
        }
    }

    /**
     * Get mode name for logging
     */
    function modeName(mode) {
        switch (mode) {
            case AppState.SystemMode.Normal: return "Normal"
            case AppState.SystemMode.DegradedCAN: return "DegradedCAN"
            case AppState.SystemMode.DegradedCamera: return "DegradedCamera"
            case AppState.SystemMode.DegradedMultiple: return "DegradedMultiple"
            case AppState.SystemMode.SafeMode: return "SafeMode"
            default: return "Unknown"
        }
    }

    /**
     * Check if current screen is the given screen
     */
    function isCurrentScreen(screen) {
        return currentScreen === screen && currentOverlay === AppState.Overlay.None
    }

    /**
     * Check if any overlay is active
     */
    function hasActiveOverlay() {
        return currentOverlay !== AppState.Overlay.None
    }

    /**
     * Check if reverse camera is showing
     */
    function isReverseCameraShowing() {
        return currentOverlay === AppState.Overlay.ReverseCamera
    }

    /**
     * Get the effective visible screen (considering overlays)
     */
    function getVisibleScreen() {
        if (currentOverlay === AppState.Overlay.ReverseCamera) {
            return -1  // Special case: camera overlay
        }
        return currentScreen
    }

    // ═══════════════════════════════════════════════════════════════
    // STATE PERSISTENCE (for crash recovery)
    // ═══════════════════════════════════════════════════════════════

    /**
     * Serialize state to JSON string
     */
    function serializeState() {
        return JSON.stringify({
            currentScreen: currentScreen,
            previousScreen: previousScreen,
            navigationHistory: navigationHistory
        })
    }

    /**
     * Restore state from JSON string
     */
    function restoreState(json) {
        try {
            var state = JSON.parse(json)
            if (state.currentScreen !== undefined) {
                currentScreen = state.currentScreen
            }
            if (state.previousScreen !== undefined) {
                previousScreen = state.previousScreen
            }
            if (state.navigationHistory !== undefined) {
                navigationHistory = state.navigationHistory
            }
            console.log("AppState: State restored")
            return true
        } catch (e) {
            console.warn("AppState: Failed to restore state:", e)
            return false
        }
    }

    /**
     * Reset to initial state
     */
    function reset() {
        currentScreen = AppState.Screen.Home
        previousScreen = AppState.Screen.Home
        currentOverlay = AppState.Overlay.None
        navigationHistory = []
        console.log("AppState: Reset to initial state")
    }

    // ═══════════════════════════════════════════════════════════════
    // INITIALIZATION
    // ═══════════════════════════════════════════════════════════════

    Component.onCompleted: {
        console.log("AppState: Initialized")
    }
}
