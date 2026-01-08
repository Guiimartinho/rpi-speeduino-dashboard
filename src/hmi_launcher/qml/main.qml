import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 480
    title: "Speeduino UI"
    color: "#1a1a1a"

    visibility: isFullscreen ? Window.FullScreen : Window.Windowed

    // State machine for screen management
    property string currentScreen: "home"
    property string previousScreen: "home"

    // Handle reverse gear - priority override
    Connections {
        target: dataProvider
        function onReverseChanged() {
            if (dataProvider.reverseEngaged) {
                previousScreen = currentScreen
                currentScreen = "camera"
                cameraController.start()
            } else {
                cameraController.stop()
                currentScreen = previousScreen
            }
        }
    }

    // Screen stack
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: homeScreen

        // Disable transitions for faster switching
        pushEnter: null
        pushExit: null
        popEnter: null
        popExit: null
    }

    // Home screen
    Component {
        id: homeScreen
        Home {
            onDashClicked: {
                currentScreen = "dash"
                stackView.replace(dashScreen)
            }
            onAndroidAutoClicked: {
                currentScreen = "android_auto"
                openAutoController.start()
            }
            onSettingsClicked: {
                currentScreen = "settings"
                stackView.replace(settingsScreen)
            }
        }
    }

    // Dash screen
    Component {
        id: dashScreen
        Dash {
            onBackClicked: {
                currentScreen = "home"
                stackView.replace(homeScreen)
            }
        }
    }

    // Reverse camera screen
    Component {
        id: cameraScreen
        ReverseCamera {}
    }

    // Settings screen
    Component {
        id: settingsScreen
        Settings {
            onBackClicked: {
                currentScreen = "home"
                stackView.replace(homeScreen)
            }
        }
    }

    // Overlay for reverse camera (highest priority)
    Loader {
        anchors.fill: parent
        active: currentScreen === "camera"
        sourceComponent: cameraScreen
        z: 1000
    }

    // Status bar (always visible except during camera)
    Rectangle {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: "#2a2a2a"
        visible: currentScreen !== "camera"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 5

            // CAN status
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: dataProvider.canConnected ? "#00ff00" : "#ff0000"
            }

            Text {
                text: dataProvider.canConnected ? "CAN OK" : "CAN OFFLINE"
                color: dataProvider.canConnected ? "#00ff00" : "#ff0000"
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            // Warning indicators
            Text {
                visible: dataProvider.celOn
                text: "CEL"
                color: "#ff6600"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                visible: dataProvider.overheat
                text: "OVERHEAT"
                color: "#ff0000"
                font.pixelSize: 14
                font.bold: true

                SequentialAnimation on opacity {
                    running: dataProvider.overheat
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 300 }
                    NumberAnimation { to: 1.0; duration: 300 }
                }
            }

            Item { Layout.fillWidth: true }

            // Clock / info
            Text {
                text: Qt.formatTime(new Date(), "hh:mm")
                color: "#ffffff"
                font.pixelSize: 14
            }
        }
    }
}
