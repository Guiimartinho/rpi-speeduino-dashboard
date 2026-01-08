import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * StatusBar.qml
 * Top status bar showing system info and warnings
 *
 * Features:
 * - Clock display
 * - CAN connection status
 * - CEL/Warning indicators
 * - System mode display
 */
Rectangle {
    id: statusBar

    // Size
    width: parent.width
    height: Styles.Theme.statusBarHeight

    // Appearance
    color: Styles.Theme.backgroundPrimary

    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Styles.Theme.backgroundTertiary
    }

    // Content layout
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Styles.Theme.spacingMd
        anchors.rightMargin: Styles.Theme.spacingMd
        spacing: Styles.Theme.spacingMd

        // Left section: System status indicators
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: Styles.Theme.spacingSm

            // CAN Status
            StatusIndicatorSmall {
                iconText: "CAN"
                isActive: State.AppState.canConnected
                activeColor: Styles.Theme.statusOk
                inactiveColor: Styles.Theme.statusCritical
            }

            // Camera Available
            StatusIndicatorSmall {
                iconText: "CAM"
                isActive: State.AppState.cameraAvailable
                activeColor: Styles.Theme.statusOk
                inactiveColor: Styles.Theme.textTertiary
                visible: State.AppState.cameraAvailable || State.AppState.reverseEngaged
            }
        }

        // Center section: Warning indicators
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter
            spacing: Styles.Theme.spacingMd

            // CEL (Check Engine Light)
            WarningIndicator {
                visible: State.AppState.celOn
                iconText: "CEL"
                warningColor: Styles.Theme.statusWarning
            }

            // Overheat Warning
            WarningIndicator {
                visible: State.AppState.overheat
                iconText: "TEMP"
                warningColor: Styles.Theme.statusCritical
                blink: true
            }

            // System Mode Warning
            WarningIndicator {
                visible: State.AppState.systemMode !== State.AppState.SystemMode.Normal
                iconText: getModeText()
                warningColor: State.AppState.systemMode === State.AppState.SystemMode.SafeMode
                             ? Styles.Theme.statusCritical
                             : Styles.Theme.statusWarning

                function getModeText() {
                    switch (State.AppState.systemMode) {
                        case State.AppState.SystemMode.DegradedCAN: return "CAN!"
                        case State.AppState.SystemMode.DegradedCamera: return "CAM!"
                        case State.AppState.SystemMode.DegradedMultiple: return "SYS!"
                        case State.AppState.SystemMode.SafeMode: return "SAFE"
                        default: return ""
                    }
                }
            }
        }

        // Right section: Clock
        Text {
            id: clockText
            Layout.alignment: Qt.AlignVCenter
            text: Qt.formatTime(new Date(), "HH:mm")
            font.pixelSize: Styles.Theme.fontMd
            font.weight: Styles.Theme.fontWeightMedium
            font.family: "monospace"
            color: Styles.Theme.textPrimary

            // Update clock every minute
            Timer {
                interval: 1000
                running: true
                repeat: true
                onTriggered: clockText.text = Qt.formatTime(new Date(), "HH:mm")
            }
        }
    }

    // Hide during reverse camera
    opacity: State.AppState.currentOverlay === State.AppState.Overlay.ReverseCamera ? 0 : 1
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: Styles.Theme.animationFast
            easing.type: Styles.Theme.easingType
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // INTERNAL COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    // Small status indicator (CAN, CAM)
    component StatusIndicatorSmall: Rectangle {
        property string iconText: ""
        property bool isActive: false
        property color activeColor: Styles.Theme.statusOk
        property color inactiveColor: Styles.Theme.textTertiary

        width: indicatorText.implicitWidth + Styles.Theme.spacingSm * 2
        height: Styles.Theme.statusBarHeight - Styles.Theme.spacingSm
        radius: Styles.Theme.radiusSmall
        color: "transparent"
        border.width: 1
        border.color: isActive ? activeColor : inactiveColor

        Text {
            id: indicatorText
            anchors.centerIn: parent
            text: iconText
            font.pixelSize: Styles.Theme.fontXs
            font.weight: Styles.Theme.fontWeightBold
            font.family: "monospace"
            color: parent.isActive ? parent.activeColor : parent.inactiveColor
        }
    }

    // Warning indicator with optional blink
    component WarningIndicator: Rectangle {
        property string iconText: ""
        property color warningColor: Styles.Theme.statusWarning
        property bool blink: false

        width: warningText.implicitWidth + Styles.Theme.spacingSm * 2
        height: Styles.Theme.statusBarHeight - Styles.Theme.spacingSm
        radius: Styles.Theme.radiusSmall
        color: warningColor

        Text {
            id: warningText
            anchors.centerIn: parent
            text: iconText
            font.pixelSize: Styles.Theme.fontXs
            font.weight: Styles.Theme.fontWeightBold
            font.family: "monospace"
            color: Styles.Theme.backgroundPrimary
        }

        // Blink animation
        SequentialAnimation on opacity {
            running: blink
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: 300 }
            NumberAnimation { to: 1.0; duration: 300 }
        }
    }
}
