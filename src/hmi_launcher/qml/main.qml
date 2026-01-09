import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Module imports
import "styles" as Styles
import "state" as State
import "components" as Components
import "screens" as Screens

/**
 * main.qml
 * Root application window with tab-based navigation
 *
 * Architecture:
 * - StatusBar (top) - system status and warnings
 * - Content Area (center) - screens loaded via StackView
 * - TabBar (bottom) - persistent navigation
 * - ReverseOverlay (z:1000) - priority camera overlay
 *
 * State Management:
 * - AppState singleton manages all navigation
 * - Theme singleton provides responsive sizing
 * - DataProvider (C++) provides engine data
 */
ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 480
    title: "Speeduino UI"
    color: Styles.Theme.backgroundPrimary

    // ═══════════════════════════════════════════════════════════════
    // C++ CONTEXT PROPERTIES (injected by main.cpp, null in preview)
    // ═══════════════════════════════════════════════════════════════
    property var dataProvider: null
    property var systemMonitor: null
    property var cameraController: null
    property var openAutoController: null
    property var canService: null

    // Fullscreen property
    property bool isFullscreen: false

    // Fullscreen mode (can be toggled)
    visibility: isFullscreen ? Window.FullScreen : Window.Windowed

    // Update Theme with window dimensions for responsive scaling
    Component.onCompleted: {
        Styles.Theme.windowWidth = Qt.binding(function() { return window.width })
        Styles.Theme.windowHeight = Qt.binding(function() { return window.height })
        console.log("Main: Window initialized", window.width, "x", window.height)
    }

    // ═══════════════════════════════════════════════════════════════
    // DATA PROVIDER CONNECTIONS
    // ═══════════════════════════════════════════════════════════════

    // Engine data object with reactive properties
    property alias engineData: engineDataObj
    QtObject {
        id: engineDataObj
        property real rpm: dataProvider ? dataProvider.rpm : 0
        property real coolantTemp: dataProvider ? dataProvider.coolantTemp : 0
        property real intakeTemp: dataProvider ? dataProvider.intakeTemp : 0
        property real tps: dataProvider ? dataProvider.tps : 0
        property real mapKpa: dataProvider ? dataProvider.mapKpa : 0
        property real lambda: dataProvider ? dataProvider.lambda : 1.0
        property real ignitionAdvance: dataProvider ? dataProvider.ignitionAdvance : 0
        property real injectorDuty: dataProvider ? dataProvider.injectorDuty : 0
        property real vehicleSpeed: dataProvider ? dataProvider.vehicleSpeed : 0
        property int gear: dataProvider ? dataProvider.gear : 0
        property bool canOk: dataProvider ? dataProvider.canConnected : false
        property bool celOn: dataProvider ? dataProvider.celOn : false
        property bool overheat: dataProvider ? dataProvider.overheat : false
    }

    // System info object with reactive properties
    property alias systemInfo: systemInfoObj
    QtObject {
        id: systemInfoObj
        property string canInterface: "can0"
        property string canBitrate: "500000"
        property int canRxCount: dataProvider ? dataProvider.canRxCount : 0
        property int canTxCount: dataProvider ? dataProvider.canTxCount : 0
        property int canErrorCount: dataProvider ? dataProvider.canErrorCount : 0
        property real cpuTemp: systemMonitor ? systemMonitor.cpuTemp : 0
        property real cpuUsage: systemMonitor ? systemMonitor.cpuUsage : 0
        property real memUsage: systemMonitor ? systemMonitor.memUsage : 0
        property string uptime: systemMonitor ? systemMonitor.uptime : "0:00:00"
    }

    // Sync AppState with DataProvider
    Connections {
        target: dataProvider
        enabled: dataProvider !== undefined

        function onCanConnectedChanged() {
            State.AppState.canConnected = dataProvider.canConnected
            State.AppState.updateSystemMode()
        }

        function onReverseChanged() {
            State.AppState.setReverseEngaged(dataProvider.reverseEngaged)
        }

        function onCelOnChanged() {
            State.AppState.celOn = dataProvider.celOn
        }

        function onOverheatChanged() {
            State.AppState.overheat = dataProvider.overheat
            State.AppState.updateSystemMode()
        }
    }

    // Sync with CameraController
    Connections {
        target: cameraController
        enabled: cameraController !== undefined

        function onCameraAvailableChanged() {
            State.AppState.cameraAvailable = cameraController.cameraAvailable
            State.AppState.updateSystemMode()
        }

        function onCameraActiveChanged() {
            State.AppState.cameraActive = cameraController.cameraActive
        }
    }

    // Sync with OpenAutoController
    Connections {
        target: openAutoController
        enabled: openAutoController !== undefined

        function onRunningChanged() {
            State.AppState.setOpenAutoRunning(openAutoController.running)
        }

        function onConnectedChanged() {
            State.AppState.setOpenAutoConnected(openAutoController.connected)
        }

        function onPhoneNameChanged() {
            State.AppState.setOpenAutoPhoneName(openAutoController.phoneName)
        }

        function onConnectionTypeChanged() {
            State.AppState.setOpenAutoConnectionType(openAutoController.connectionType)
        }

        function onPhoneConnected(deviceName) {
            console.log("Main: Phone connected -", deviceName)
            State.AppState.setOpenAutoPhoneName(deviceName)
            State.AppState.setOpenAutoConnected(true)
        }

        function onPhoneDisconnected() {
            console.log("Main: Phone disconnected")
            State.AppState.setOpenAutoPhoneName("")
            State.AppState.setOpenAutoConnected(false)
        }

        function onShowNotification(title, message) {
            console.log("Main: OpenAuto notification -", title, ":", message)
            // TODO: Show visual notification in UI
            // notificationPopup.show(title, message)
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // SCREEN CHANGE HANDLER
    // ═══════════════════════════════════════════════════════════════

    Connections {
        target: State.AppState

        function onScreenChanged(newScreen, oldScreen) {
            console.log("Main: Screen changed from", State.AppState.screenName(oldScreen),
                       "to", State.AppState.screenName(newScreen))

            // Load the appropriate screen
            switch (newScreen) {
                case State.AppState.screenHome:
                    contentLoader.sourceComponent = homeScreenComponent
                    break
                case State.AppState.screenDash:
                    contentLoader.sourceComponent = dashScreenComponent
                    break
                case State.AppState.screenConfig:
                    contentLoader.sourceComponent = configScreenComponent
                    break
                case State.AppState.screenOpenAuto:
                    contentLoader.sourceComponent = openAutoScreenComponent
                    break
            }
        }

        function onOverlayChanged(newOverlay, oldOverlay) {
            console.log("Main: Overlay changed to", newOverlay)

            // Handle camera overlay
            if (newOverlay === State.AppState.overlayReverseCamera) {
                if (cameraController) cameraController.start()
            } else if (oldOverlay === State.AppState.overlayReverseCamera) {
                if (cameraController) cameraController.stop()
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // LAYOUT
    // ═══════════════════════════════════════════════════════════════

    // Main layout container
    Item {
        id: mainContainer
        anchors.fill: parent

        // Status Bar (Top)
        Components.StatusBar {
            id: statusBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            z: 100
        }

        // Content Area (Center)
        Item {
            id: contentArea
            anchors.top: statusBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: tabBar.top

            // Screen loader
            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: homeScreenComponent
                asynchronous: false

                // Pass engine data to loaded screens
                onLoaded: {
                    if (item && item.hasOwnProperty("engineData")) {
                        item.engineData = Qt.binding(function() { return window.engineData })
                    }
                    if (item && item.hasOwnProperty("systemInfo")) {
                        item.systemInfo = Qt.binding(function() { return window.systemInfo })
                    }
                }
            }
        }

        // Tab Bar (Bottom)
        Components.TabBar {
            id: tabBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            z: 100
        }

        // Reverse Camera Overlay (Highest Priority)
        Screens.ReverseOverlay {
            id: reverseOverlay
            anchors.fill: parent
            z: 1000
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // SCREEN COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    Component {
        id: homeScreenComponent
        Screens.HomeScreen {
            engineData: window.engineData
        }
    }

    Component {
        id: dashScreenComponent
        Screens.DashScreen {
            engineData: window.engineData
        }
    }

    Component {
        id: configScreenComponent
        Screens.ConfigScreen {
            systemInfo: window.systemInfo

            onRestartCanService: {
                console.log("Main: Restart CAN service requested")
                if (canService) canService.restart()
            }

            onRestartOpenAuto: {
                console.log("Main: Restart OpenAuto requested")
                if (openAutoController) {
                    openAutoController.stop()
                    openAutoController.start()
                }
            }

            onTestCamera: {
                console.log("Main: Camera test requested")
                State.AppState.showReverseCamera()
            }

            onCalibrateDisplay: {
                console.log("Main: Display calibration requested")
                // Would launch touch calibration utility
            }
        }
    }

    Component {
        id: openAutoScreenComponent
        Screens.OpenAutoScreen {
            onRequestStart: {
                console.log("Main: OpenAuto start requested")
                if (openAutoController) openAutoController.start()
            }

            onRequestStop: {
                console.log("Main: OpenAuto stop requested")
                if (openAutoController) openAutoController.stop()
            }

            onTouchEvent: function(x, y, type) {
                // Forward touch events to OpenAuto
                if (openAutoController) {
                    openAutoController.sendTouch(x, y, type)
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // KEYBOARD SHORTCUTS (Development/Debug)
    // ═══════════════════════════════════════════════════════════════

    Shortcut {
        sequence: "F11"
        onActivated: {
            window.visibility = (window.visibility === Window.FullScreen)
                              ? Window.Windowed
                              : Window.FullScreen
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (State.AppState.currentOverlay !== State.AppState.overlayNone) {
                if (State.AppState.currentOverlay !== State.AppState.overlayReverseCamera ||
                    !State.AppState.reverseEngaged) {
                    State.AppState.hideOverlay()
                }
            } else if (State.AppState.canGoBack()) {
                State.AppState.goBack()
            }
        }
    }

    // Debug shortcuts (remove in production)
    Shortcut {
        sequence: "F1"
        onActivated: State.AppState.goHome()
    }
    Shortcut {
        sequence: "F2"
        onActivated: State.AppState.goDash()
    }
    Shortcut {
        sequence: "F3"
        onActivated: State.AppState.goConfig()
    }
    Shortcut {
        sequence: "F4"
        onActivated: State.AppState.goOpenAuto()
    }
    Shortcut {
        sequence: "F5"
        onActivated: State.AppState.showReverseCamera()
    }
    Shortcut {
        sequence: "F6"
        onActivated: State.AppState.hideReverseCamera()
    }
}
