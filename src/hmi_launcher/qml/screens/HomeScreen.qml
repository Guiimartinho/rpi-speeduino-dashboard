import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * HomeScreen.qml
 * Main landing screen with quick access buttons
 *
 * Features:
 * - Large touch targets for quick navigation
 * - Engine status summary at top
 * - Quick action buttons
 * - Visual feedback for active states
 */
Item {
    id: homeScreen

    // Engine data from DataProvider (set by main.qml)
    property var engineData: ({
        rpm: 0,
        coolantTemp: 0,
        vehicleSpeed: 0,
        canOk: false
    })

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Main content
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Styles.Theme.spacingMd
        spacing: Styles.Theme.spacingMd

        // Top: Engine status summary card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Styles.Theme.dp(100)
            color: Styles.Theme.surfaceCard
            radius: Styles.Theme.radiusMedium

            RowLayout {
                anchors.fill: parent
                anchors.margins: Styles.Theme.spacingMd
                spacing: Styles.Theme.spacingLg

                // RPM Display
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "RPM"
                        font.pixelSize: Styles.Theme.fontSm
                        font.weight: Styles.Theme.fontWeightMedium
                        color: Styles.Theme.textSecondary
                    }

                    Text {
                        text: homeScreen.engineData.rpm.toFixed(0)
                        font.pixelSize: Styles.Theme.fontDisplay
                        font.weight: Styles.Theme.fontWeightBold
                        font.family: "monospace"
                        color: Styles.Theme.accentPrimary
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: Styles.Theme.backgroundTertiary
                }

                // Speed Display
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "km/h"
                        font.pixelSize: Styles.Theme.fontSm
                        font.weight: Styles.Theme.fontWeightMedium
                        color: Styles.Theme.textSecondary
                    }

                    Text {
                        text: homeScreen.engineData.vehicleSpeed.toFixed(0)
                        font.pixelSize: Styles.Theme.fontDisplay
                        font.weight: Styles.Theme.fontWeightBold
                        font.family: "monospace"
                        color: Styles.Theme.accentSecondary
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: Styles.Theme.backgroundTertiary
                }

                // Coolant Temp Display
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "CLT"
                        font.pixelSize: Styles.Theme.fontSm
                        font.weight: Styles.Theme.fontWeightMedium
                        color: Styles.Theme.textSecondary
                    }

                    Text {
                        text: homeScreen.engineData.coolantTemp.toFixed(0) + "\u00B0"
                        font.pixelSize: Styles.Theme.fontDisplay
                        font.weight: Styles.Theme.fontWeightBold
                        font.family: "monospace"
                        color: getCoolantColor()

                        function getCoolantColor() {
                            var temp = homeScreen.engineData.coolantTemp
                            if (temp > 100) return Styles.Theme.statusCritical
                            if (temp > 95) return Styles.Theme.statusWarning
                            if (temp < 60) return Styles.Theme.accentSecondary
                            return Styles.Theme.accentPrimary
                        }
                    }
                }

                // CAN Status indicator
                Rectangle {
                    Layout.preferredWidth: Styles.Theme.dp(60)
                    Layout.preferredHeight: Styles.Theme.dp(60)
                    radius: Styles.Theme.radiusMedium
                    color: homeScreen.engineData.canOk ? Styles.Theme.statusOk : Styles.Theme.statusCritical
                    opacity: 0.2

                    Text {
                        anchors.centerIn: parent
                        text: homeScreen.engineData.canOk ? "CAN\nOK" : "CAN\nERR"
                        font.pixelSize: Styles.Theme.fontSm
                        font.weight: Styles.Theme.fontWeightBold
                        horizontalAlignment: Text.AlignHCenter
                        color: homeScreen.engineData.canOk ? Styles.Theme.statusOk : Styles.Theme.statusCritical
                    }
                }
            }
        }

        // Center: Quick navigation grid
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            rowSpacing: Styles.Theme.spacingMd
            columnSpacing: Styles.Theme.spacingMd

            // Dash Button
            QuickNavButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconText: "\u2299"  // ⊙
                label: "Dashboard"
                description: "Full gauge display"
                accentColor: Styles.Theme.accentPrimary
                onClicked: State.AppState.goDash()
            }

            // Android Auto Button
            QuickNavButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconText: "\u25B6"  // ▶
                label: "Android Auto"
                description: State.AppState.openAutoConnected ? "Phone connected" : "Tap to connect"
                accentColor: Styles.Theme.accentAndroidAuto
                showBadge: State.AppState.openAutoConnected
                onClicked: State.AppState.goOpenAuto()
            }

            // Config Button
            QuickNavButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconText: "\u2699"  // ⚙
                label: "Settings"
                description: "System configuration"
                accentColor: Styles.Theme.textSecondary
                onClicked: State.AppState.goConfig()
            }

            // Camera Test Button
            QuickNavButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconText: "\u25A3"  // ▣
                label: "Camera"
                description: State.AppState.cameraAvailable ? "Test reverse view" : "Camera unavailable"
                accentColor: State.AppState.cameraAvailable ? Styles.Theme.accentSecondary : Styles.Theme.textDisabled
                enabled: State.AppState.cameraAvailable
                onClicked: State.AppState.showReverseCamera()
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // INTERNAL COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    component QuickNavButton: Rectangle {
        id: navButton
        property string iconText: ""
        property string label: ""
        property string description: ""
        property color accentColor: Styles.Theme.accentPrimary
        property bool showBadge: false
        property bool enabled: true

        signal clicked()

        color: mouseArea.pressed ? Styles.Theme.surfacePressed
             : mouseArea.containsMouse ? Styles.Theme.surfaceHover
             : Styles.Theme.surfaceCard
        radius: Styles.Theme.radiusLarge
        opacity: enabled ? 1.0 : 0.5

        Behavior on color {
            ColorAnimation { duration: Styles.Theme.animationFast }
        }

        // Content
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingSm

            // Icon
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: navButton.iconText
                font.pixelSize: Styles.Theme.fontHero
                font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                color: navButton.accentColor

                // Scale on press
                scale: mouseArea.pressed ? 0.9 : 1.0
                Behavior on scale {
                    NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                }
            }

            // Label
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: navButton.label
                font.pixelSize: Styles.Theme.fontLg
                font.weight: Styles.Theme.fontWeightBold
                color: Styles.Theme.textPrimary
            }

            // Description
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: navButton.description
                font.pixelSize: Styles.Theme.fontSm
                color: Styles.Theme.textSecondary
            }
        }

        // Badge indicator
        Rectangle {
            visible: navButton.showBadge
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Styles.Theme.spacingSm
            width: Styles.Theme.spacingMd
            height: Styles.Theme.spacingMd
            radius: width / 2
            color: Styles.Theme.statusOk

            SequentialAnimation on scale {
                running: navButton.showBadge
                loops: Animation.Infinite
                NumberAnimation { to: 1.2; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }

        // Touch area
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            enabled: navButton.enabled
            hoverEnabled: true
            onClicked: navButton.clicked()
        }
    }
}
