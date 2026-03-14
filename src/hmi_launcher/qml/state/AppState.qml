pragma Singleton
import QtQuick

QtObject {
    id: appState

    property int currentScreen: 0
    property int currentOverlay: 0
    property bool canConnected: false
    property bool canHealthy: true
    property bool reverseEngaged: false
    property bool openAutoRunning: false
    property bool openAutoConnected: false
    property string openAutoPhoneName: ""
    property string openAutoConnectionType: ""
    property bool cameraAvailable: true
    property bool cameraActive: false
    property bool celOn: false
    property bool overheat: false

    // Internal tracking for signal emission
    property int _previousScreen: 0
    property int _previousOverlay: 0

    readonly property int screenHome: 0
    readonly property int screenDash: 1
    readonly property int screenConfig: 2
    readonly property int screenOpenAuto: 3

    readonly property int overlayNone: 0
    readonly property int overlayReverseCamera: 1
    readonly property int overlaySplash: 2

    signal screenChanged(int newScreen, int oldScreen)
    signal overlayChanged(int newOverlay, int oldOverlay)

    // Emit signals when properties change
    onCurrentScreenChanged: {
        if (currentScreen !== _previousScreen) {
            var old = _previousScreen
            _previousScreen = currentScreen
            screenChanged(currentScreen, old)
        }
    }

    onCurrentOverlayChanged: {
        if (currentOverlay !== _previousOverlay) {
            var old = _previousOverlay
            _previousOverlay = currentOverlay
            overlayChanged(currentOverlay, old)
        }
    }

    function goHome() { currentScreen = screenHome }
    function goDash() { currentScreen = screenDash }
    function goConfig() { currentScreen = screenConfig }
    function goOpenAuto() { currentScreen = screenOpenAuto }
    function goBack() { currentScreen = screenHome }
    function canGoBack() { return currentScreen !== screenHome }

    function showReverseCamera() { currentOverlay = overlayReverseCamera }
    function hideReverseCamera() { currentOverlay = overlayNone }
    function hideOverlay() { currentOverlay = overlayNone }
    function showSplash() { currentOverlay = overlaySplash }
    function hideSplash() { currentOverlay = overlayNone }
    function setReverseEngaged(engaged) { reverseEngaged = engaged }
    function updateSystemMode() { }
    function setOpenAutoRunning(running) { openAutoRunning = running }
    function setOpenAutoConnected(connected) { openAutoConnected = connected }
    function setOpenAutoPhoneName(name) { openAutoPhoneName = name }
    function setOpenAutoConnectionType(type) { openAutoConnectionType = type }
    function setCanHealthy(healthy) { canHealthy = healthy }

    function screenName(screen) {
        switch (screen) {
            case screenHome: return "Home"
            case screenDash: return "Dashboard"
            case screenConfig: return "Config"
            case screenOpenAuto: return "OpenAuto"
            default: return "Unknown"
        }
    }
}
