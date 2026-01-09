import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Module imports
import "styles" as Styles
import "state" as State
import "components" as Components
import "screens" as Screens

/**
 * MainContent.qml
 * Main UI content (used by both main.qml and preview)
 * This is an Item, not a Window, so it can be loaded via Loader
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════
    // C++ CONTEXT PROPERTIES (injected by parent)
    // ═══════════════════════════════════════════════════════════════
    property var dataProvider: null
    property var systemMonitor: null
    property var cameraController: null
    property var openAutoController: null
    property var canService: null

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    Component.onCompleted: {
        Styles.Theme.windowWidth = Qt.binding(function() { return root.width })
        Styles.Theme.windowHeight = Qt.binding(function() { return root.height })
        console.log("MainContent: Initialized", root.width, "x", root.height)
    }

    // ═══════════════════════════════════════════════════════════════
    // DATA PROVIDER CONNECTIONS
    // ═══════════════════════════════════════════════════════════════

    property alias engineData: engineDataObj
    QtObject {
        id: engineDataObj
        // Primary engine data
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
        // Additional Speeduino parameters
        property real batteryVoltage: dataProvider ? dataProvider.batteryVoltage : 12.0
        property real oilPressure: dataProvider ? dataProvider.oilPressure : 0
        property real oilTemp: dataProvider ? dataProvider.oilTemp : 0
        property real boostPsi: dataProvider ? dataProvider.boostPsi : 0
        property real veValue: dataProvider ? dataProvider.veValue : 0
        property real sparkDwell: dataProvider ? dataProvider.sparkDwell : 0
        property int loopsPerSec: dataProvider ? dataProvider.loopsPerSec : 0
        property int freeRam: dataProvider ? dataProvider.freeRam : 0
        property real targetAfr: dataProvider ? dataProvider.targetAfr : 14.7
        property real afrCorrection: dataProvider ? dataProvider.afrCorrection : 0
    }

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
        enabled: dataProvider !== null

        function onCanConnectedChanged() {
            State.AppState.canConnected = dataProvider.canConnected
            State.AppState.updateSystemMode()
        }

        function onReverseEngagedChanged() {
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

    // ═══════════════════════════════════════════════════════════════
    // SCREEN CHANGE HANDLER
    // ═══════════════════════════════════════════════════════════════

    Connections {
        target: State.AppState

        function onScreenChanged(newScreen, oldScreen) {
            console.log("MainContent: Screen changed from", State.AppState.screenName(oldScreen),
                       "to", State.AppState.screenName(newScreen))

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
            console.log("MainContent: Overlay changed to", newOverlay)

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

            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: homeScreenComponent
                asynchronous: false

                onLoaded: {
                    if (item && item.hasOwnProperty("engineData")) {
                        item.engineData = Qt.binding(function() { return root.engineData })
                    }
                    if (item && item.hasOwnProperty("systemInfo")) {
                        item.systemInfo = Qt.binding(function() { return root.systemInfo })
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

        // Reverse Camera Overlay
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
            engineData: root.engineData
        }
    }

    Component {
        id: dashScreenComponent
        Screens.DashScreen {
            engineData: root.engineData
        }
    }

    Component {
        id: configScreenComponent
        Screens.ConfigScreen {
            systemInfo: root.systemInfo

            onRestartCanService: {
                if (canService) canService.restart()
            }

            onRestartOpenAuto: {
                if (openAutoController) {
                    openAutoController.stop()
                    openAutoController.start()
                }
            }

            onTestCamera: {
                State.AppState.showReverseCamera()
            }

            onCalibrateDisplay: {
                console.log("MainContent: Display calibration requested")
            }
        }
    }

    Component {
        id: openAutoScreenComponent
        Screens.OpenAutoScreen {
            onRequestStart: {
                if (openAutoController) openAutoController.start()
            }

            onRequestStop: {
                if (openAutoController) openAutoController.stop()
            }

            onTouchEvent: function(x, y, type) {
                if (openAutoController) {
                    openAutoController.sendTouch(x, y, type)
                }
            }
        }
    }
}
