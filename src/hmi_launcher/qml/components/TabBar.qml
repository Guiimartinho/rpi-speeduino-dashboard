import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * TabBar.qml
 * Bottom navigation bar for screen switching
 *
 * Features:
 * - 4 tabs: Home, Dash, Config, OpenAuto
 * - Persists across all screens (including OpenAuto)
 * - High z-index to stay above content
 * - Touch-optimized with 64dp height
 */
Rectangle {
    id: tabBar

    // Size
    width: parent.width
    height: Styles.Theme.tabBarHeight

    // Appearance
    color: Styles.Theme.tabBarBackground

    // Top border accent line
    Rectangle {
        id: topBorder
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Styles.Theme.backgroundTertiary
    }

    // Tab layout
    RowLayout {
        id: tabLayout
        anchors.fill: parent
        anchors.topMargin: 1
        spacing: 0

        // Home Tab
        TabButton {
            id: homeTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconSource: "\u2302"  // ⌂ House symbol
            label: "Home"
            isActive: State.AppState.currentScreen === State.AppState.screenHome
            onClicked: State.AppState.goHome()
        }

        // Dash Tab
        TabButton {
            id: dashTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconSource: "\u2299"  // ⊙ Gauge symbol
            label: "Dash"
            isActive: State.AppState.currentScreen === State.AppState.screenDash
            onClicked: State.AppState.goDash()
        }

        // Config Tab
        TabButton {
            id: configTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconSource: "\u2699"  // ⚙ Gear symbol
            label: "Config"
            isActive: State.AppState.currentScreen === State.AppState.screenConfig
            onClicked: State.AppState.goConfig()
        }

        // OpenAuto Tab
        TabButton {
            id: openAutoTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconSource: "\u25B6"  // ▶ Play symbol (Android Auto style)
            label: "Auto"
            activeColor: Styles.Theme.accentAndroidAuto
            isActive: State.AppState.currentScreen === State.AppState.screenOpenAuto
            onClicked: State.AppState.goOpenAuto()
        }
    }

    // Connection indicator dot (shows CAN status)
    Rectangle {
        id: connectionIndicator
        anchors.top: parent.top
        anchors.topMargin: Styles.Theme.spacingXs
        anchors.right: parent.right
        anchors.rightMargin: Styles.Theme.spacingSm
        width: Styles.Theme.spacingSm
        height: Styles.Theme.spacingSm
        radius: width / 2
        color: State.AppState.canConnected ? Styles.Theme.statusOk : Styles.Theme.statusCritical
        opacity: 0.8

        // Pulse animation when disconnected
        SequentialAnimation on opacity {
            running: !State.AppState.canConnected
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: 500 }
            NumberAnimation { to: 0.8; duration: 500 }
        }
    }

    // Hide during reverse camera overlay
    opacity: State.AppState.currentOverlay === State.AppState.overlayReverseCamera ? 0 : 1
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: Styles.Theme.animationFast
            easing.type: Styles.Theme.easingType
        }
    }
}
