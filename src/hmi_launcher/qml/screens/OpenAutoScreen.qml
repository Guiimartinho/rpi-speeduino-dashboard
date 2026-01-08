import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * OpenAutoScreen.qml
 * Container for OpenAuto/Android Auto display
 *
 * Features:
 * - Fullscreen video surface for OpenAuto
 * - Connection status overlay when not connected
 * - Touch passthrough to OpenAuto process
 * - Tab bar remains accessible at bottom
 */
Item {
    id: openAutoScreen

    // Signals for OpenAuto control
    signal requestStart()
    signal requestStop()
    signal touchEvent(int x, int y, int type)

    // OpenAuto video surface (filled by C++ VideoSurface)
    Rectangle {
        id: videoSurface
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary

        // Placeholder for actual video surface
        // In production, this is replaced by a QQuickVideoOutput or custom surface
        Item {
            id: openAutoVideoPlaceholder
            anchors.fill: parent
            objectName: "openAutoVideoSurface"  // C++ looks for this
        }

        // Touch passthrough area
        MouseArea {
            anchors.fill: parent
            enabled: State.AppState.openAutoRunning && State.AppState.openAutoConnected

            onPressed: function(mouse) {
                openAutoScreen.touchEvent(mouse.x, mouse.y, 0)  // 0 = press
            }
            onReleased: function(mouse) {
                openAutoScreen.touchEvent(mouse.x, mouse.y, 1)  // 1 = release
            }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    openAutoScreen.touchEvent(mouse.x, mouse.y, 2)  // 2 = move
                }
            }
        }
    }

    // Connection overlay (shown when not connected)
    Rectangle {
        id: connectionOverlay
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
        visible: !State.AppState.openAutoConnected
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Styles.Theme.animationNormal }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingLg

            // Android Auto Icon
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\u25B6"  // ▶
                font.pixelSize: Styles.Theme.fontMega
                font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                color: Styles.Theme.accentAndroidAuto
            }

            // Title
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Android Auto"
                font.pixelSize: Styles.Theme.fontXxl
                font.weight: Styles.Theme.fontWeightBold
                color: Styles.Theme.textPrimary
            }

            // Status message
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: getStatusMessage()
                font.pixelSize: Styles.Theme.fontMd
                color: Styles.Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter

                function getStatusMessage() {
                    if (!State.AppState.openAutoRunning) {
                        return "OpenAuto is not running"
                    }
                    return "Waiting for phone connection...\nConnect your Android phone via USB"
                }
            }

            // Action button
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Styles.Theme.spacingMd
                width: buttonText.implicitWidth + Styles.Theme.spacingXl * 2
                height: Styles.Theme.touchTargetStandard
                radius: Styles.Theme.radiusMedium
                color: buttonMouse.pressed ? Styles.Theme.surfacePressed
                     : buttonMouse.containsMouse ? Styles.Theme.surfaceHover
                     : Styles.Theme.accentAndroidAuto

                Text {
                    id: buttonText
                    anchors.centerIn: parent
                    text: State.AppState.openAutoRunning ? "Restart OpenAuto" : "Start OpenAuto"
                    font.pixelSize: Styles.Theme.fontLg
                    font.weight: Styles.Theme.fontWeightBold
                    color: Styles.Theme.textPrimary
                }

                MouseArea {
                    id: buttonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (State.AppState.openAutoRunning) {
                            openAutoScreen.requestStop()
                            // Small delay then restart
                            Qt.callLater(function() {
                                openAutoScreen.requestStart()
                            })
                        } else {
                            openAutoScreen.requestStart()
                        }
                    }
                }
            }

            // Connection animation
            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Styles.Theme.spacingLg
                width: Styles.Theme.dp(100)
                height: Styles.Theme.dp(40)
                visible: State.AppState.openAutoRunning

                // Phone icon
                Text {
                    id: phoneIcon
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u25A1"  // □
                    font.pixelSize: Styles.Theme.fontXl
                    font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                    color: Styles.Theme.textTertiary
                }

                // Animated dots
                Row {
                    anchors.centerIn: parent
                    spacing: Styles.Theme.spacingXs

                    Repeater {
                        model: 3

                        Rectangle {
                            width: Styles.Theme.spacingSm
                            height: Styles.Theme.spacingSm
                            radius: width / 2
                            color: Styles.Theme.accentAndroidAuto

                            SequentialAnimation on opacity {
                                running: State.AppState.openAutoRunning && !State.AppState.openAutoConnected
                                loops: Animation.Infinite
                                PauseAnimation { duration: index * 200 }
                                NumberAnimation { to: 1.0; duration: 300 }
                                NumberAnimation { to: 0.3; duration: 300 }
                                PauseAnimation { duration: (2 - index) * 200 }
                            }
                        }
                    }
                }

                // Car icon
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u25A0"  // ■
                    font.pixelSize: Styles.Theme.fontXl
                    font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                    color: Styles.Theme.accentAndroidAuto
                }
            }
        }
    }

    // Mini dashboard overlay (optional, can be enabled)
    Rectangle {
        id: miniDashOverlay
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Styles.Theme.spacingSm
        width: miniDashRow.implicitWidth + Styles.Theme.spacingMd * 2
        height: Styles.Theme.dp(36)
        radius: Styles.Theme.radiusSmall
        color: Styles.Theme.overlayDark
        visible: State.AppState.openAutoConnected && showMiniDash
        opacity: 0.9

        property bool showMiniDash: false  // Can be toggled via settings

        RowLayout {
            id: miniDashRow
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingMd

            // RPM
            Text {
                text: "RPM: --"  // Would be bound to actual data
                font.pixelSize: Styles.Theme.fontSm
                font.family: "monospace"
                color: Styles.Theme.accentPrimary
            }

            Rectangle {
                width: 1
                height: parent.height * 0.6
                color: Styles.Theme.textTertiary
            }

            // Speed
            Text {
                text: "-- km/h"  // Would be bound to actual data
                font.pixelSize: Styles.Theme.fontSm
                font.family: "monospace"
                color: Styles.Theme.accentSecondary
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: miniDashOverlay.showMiniDash = false
        }
    }

    // OpenAuto process status indicator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: Styles.Theme.spacingSm
        anchors.bottomMargin: Styles.Theme.tabBarHeight + Styles.Theme.spacingSm
        width: processStatusText.implicitWidth + Styles.Theme.spacingSm * 2
        height: Styles.Theme.dp(24)
        radius: Styles.Theme.radiusSmall
        color: State.AppState.openAutoRunning ? Styles.Theme.statusOk : Styles.Theme.statusCritical
        opacity: 0.8
        visible: !State.AppState.openAutoConnected

        Text {
            id: processStatusText
            anchors.centerIn: parent
            text: State.AppState.openAutoRunning ? "OA Running" : "OA Stopped"
            font.pixelSize: Styles.Theme.fontXs
            font.weight: Styles.Theme.fontWeightBold
            color: Styles.Theme.backgroundPrimary
        }
    }
}
