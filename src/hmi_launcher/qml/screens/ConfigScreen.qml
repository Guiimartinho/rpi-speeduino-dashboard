import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../styles" as Styles
import "../state" as State

/**
 * ConfigScreen.qml
 * System configuration and diagnostics screen
 *
 * Features:
 * - CAN bus status and diagnostics
 * - OpenAuto controls
 * - Camera settings
 * - System information
 * - Display settings
 */
Item {
    id: configScreen

    // Signals for actions
    signal restartCanService()
    signal restartOpenAuto()
    signal testCamera()
    signal calibrateDisplay()

    // System info (set by main.qml)
    property var systemInfo: ({
        canInterface: "can0",
        canBitrate: "500000",
        canRxCount: 0,
        canTxCount: 0,
        canErrorCount: 0,
        cpuTemp: 0,
        cpuUsage: 0,
        memUsage: 0,
        uptime: "0:00:00"
    })

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Scrollable content
    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: Styles.Theme.spacingSm
        contentHeight: contentColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: flickable.width
            spacing: Styles.Theme.spacingMd

            // ═══════════════════════════════════════════════════════════════
            // CAN BUS SECTION
            // ═══════════════════════════════════════════════════════════════
            ConfigSection {
                Layout.fillWidth: true
                title: "CAN Bus"
                iconText: "\u2301"  // ⌁

                ColumnLayout {
                    width: parent.width
                    spacing: Styles.Theme.spacingSm

                    // Connection status
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Styles.Theme.spacingMd

                        StatusBadge {
                            text: State.AppState.canConnected ? "Connected" : "Disconnected"
                            color: State.AppState.canConnected ? Styles.Theme.statusOk : Styles.Theme.statusCritical
                        }

                        StatusBadge {
                            text: State.AppState.canHealthy ? "Healthy" : "Errors"
                            color: State.AppState.canHealthy ? Styles.Theme.statusOk : Styles.Theme.statusWarning
                            visible: State.AppState.canConnected
                        }

                        Item { Layout.fillWidth: true }

                        ConfigButton {
                            text: "Restart"
                            onClicked: configScreen.restartCanService()
                        }
                    }

                    // CAN Statistics
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: Styles.Theme.spacingXs
                        columnSpacing: Styles.Theme.spacingLg

                        ConfigLabel { text: "Interface:" }
                        ConfigValue { text: configScreen.systemInfo.canInterface }

                        ConfigLabel { text: "Bitrate:" }
                        ConfigValue { text: configScreen.systemInfo.canBitrate + " bps" }

                        ConfigLabel { text: "RX Count:" }
                        ConfigValue { text: configScreen.systemInfo.canRxCount.toLocaleString() }

                        ConfigLabel { text: "TX Count:" }
                        ConfigValue { text: configScreen.systemInfo.canTxCount.toLocaleString() }

                        ConfigLabel { text: "Errors:" }
                        ConfigValue {
                            text: configScreen.systemInfo.canErrorCount.toLocaleString()
                            valueColor: configScreen.systemInfo.canErrorCount > 0
                                       ? Styles.Theme.statusWarning
                                       : Styles.Theme.textPrimary
                        }
                    }
                }
            }

            // ═══════════════════════════════════════════════════════════════
            // OPENAUTO SECTION
            // ═══════════════════════════════════════════════════════════════
            ConfigSection {
                Layout.fillWidth: true
                title: "Android Auto"
                iconText: "\u25B6"  // ▶

                ColumnLayout {
                    width: parent.width
                    spacing: Styles.Theme.spacingSm

                    // Status
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Styles.Theme.spacingMd

                        StatusBadge {
                            text: State.AppState.openAutoRunning ? "Running" : "Stopped"
                            color: State.AppState.openAutoRunning ? Styles.Theme.statusOk : Styles.Theme.textTertiary
                        }

                        StatusBadge {
                            text: State.AppState.openAutoConnected ? "Phone Connected" : "No Phone"
                            color: State.AppState.openAutoConnected ? Styles.Theme.accentAndroidAuto : Styles.Theme.textTertiary
                            visible: State.AppState.openAutoRunning
                        }

                        Item { Layout.fillWidth: true }

                        ConfigButton {
                            text: State.AppState.openAutoRunning ? "Restart" : "Start"
                            onClicked: configScreen.restartOpenAuto()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Connect your Android phone via USB to use Android Auto features."
                        font.pixelSize: Styles.Theme.fontSm
                        color: Styles.Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ═══════════════════════════════════════════════════════════════
            // CAMERA SECTION
            // ═══════════════════════════════════════════════════════════════
            ConfigSection {
                Layout.fillWidth: true
                title: "Reverse Camera"
                iconText: "\u25A3"  // ▣

                ColumnLayout {
                    width: parent.width
                    spacing: Styles.Theme.spacingSm

                    // Status
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Styles.Theme.spacingMd

                        StatusBadge {
                            text: State.AppState.cameraAvailable ? "Available" : "Not Found"
                            color: State.AppState.cameraAvailable ? Styles.Theme.statusOk : Styles.Theme.textTertiary
                        }

                        StatusBadge {
                            text: "Active"
                            color: Styles.Theme.accentSecondary
                            visible: State.AppState.cameraActive
                        }

                        Item { Layout.fillWidth: true }

                        ConfigButton {
                            text: "Test"
                            enabled: State.AppState.cameraAvailable
                            onClicked: configScreen.testCamera()
                        }
                    }

                    // Settings
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: Styles.Theme.spacingXs
                        columnSpacing: Styles.Theme.spacingLg
                        visible: State.AppState.cameraAvailable

                        ConfigLabel { text: "Trigger:" }
                        ConfigValue { text: "CAN + GPIO Fallback" }

                        ConfigLabel { text: "Resolution:" }
                        ConfigValue { text: "640x480 @ 30fps" }
                    }
                }
            }

            // ═══════════════════════════════════════════════════════════════
            // SYSTEM INFO SECTION
            // ═══════════════════════════════════════════════════════════════
            ConfigSection {
                Layout.fillWidth: true
                title: "System"
                iconText: "\u2139"  // ℹ

                GridLayout {
                    width: parent.width
                    columns: 2
                    rowSpacing: Styles.Theme.spacingXs
                    columnSpacing: Styles.Theme.spacingLg

                    ConfigLabel { text: "CPU Temp:" }
                    ConfigValue {
                        text: configScreen.systemInfo.cpuTemp.toFixed(1) + "\u00B0C"
                        valueColor: configScreen.systemInfo.cpuTemp > 70
                                   ? Styles.Theme.statusCritical
                                   : configScreen.systemInfo.cpuTemp > 60
                                     ? Styles.Theme.statusWarning
                                     : Styles.Theme.textPrimary
                    }

                    ConfigLabel { text: "CPU Usage:" }
                    ConfigValue {
                        text: configScreen.systemInfo.cpuUsage.toFixed(0) + "%"
                        valueColor: configScreen.systemInfo.cpuUsage > 80
                                   ? Styles.Theme.statusWarning
                                   : Styles.Theme.textPrimary
                    }

                    ConfigLabel { text: "Memory:" }
                    ConfigValue {
                        text: configScreen.systemInfo.memUsage.toFixed(0) + "%"
                        valueColor: configScreen.systemInfo.memUsage > 85
                                   ? Styles.Theme.statusWarning
                                   : Styles.Theme.textPrimary
                    }

                    ConfigLabel { text: "Uptime:" }
                    ConfigValue { text: configScreen.systemInfo.uptime }

                    ConfigLabel { text: "Display:" }
                    ConfigValue { text: Styles.Theme.windowWidth + "x" + Styles.Theme.windowHeight }

                    ConfigLabel { text: "Scale:" }
                    ConfigValue { text: (Styles.Theme.scale * 100).toFixed(0) + "%" }
                }
            }

            // ═══════════════════════════════════════════════════════════════
            // DISPLAY SECTION
            // ═══════════════════════════════════════════════════════════════
            ConfigSection {
                Layout.fillWidth: true
                title: "Display"
                iconText: "\u25A1"  // □

                ColumnLayout {
                    width: parent.width
                    spacing: Styles.Theme.spacingSm

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Brightness"
                            font.pixelSize: Styles.Theme.fontMd
                            color: Styles.Theme.textSecondary
                        }

                        Item { Layout.fillWidth: true }

                        Slider {
                            id: brightnessSlider
                            from: 10
                            to: 100
                            value: 100
                            stepSize: 5
                            implicitWidth: Styles.Theme.dp(200)

                            background: Rectangle {
                                x: brightnessSlider.leftPadding
                                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                                width: brightnessSlider.availableWidth
                                height: Styles.Theme.dp(4)
                                radius: 2
                                color: Styles.Theme.backgroundTertiary

                                Rectangle {
                                    width: brightnessSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: Styles.Theme.accentPrimary
                                    radius: 2
                                }
                            }

                            handle: Rectangle {
                                x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                                width: Styles.Theme.touchTargetMin
                                height: Styles.Theme.touchTargetMin
                                radius: width / 2
                                color: brightnessSlider.pressed ? Styles.Theme.surfacePressed : Styles.Theme.surfaceCard
                                border.color: Styles.Theme.accentPrimary
                                border.width: 2
                            }
                        }

                        Text {
                            text: brightnessSlider.value.toFixed(0) + "%"
                            font.pixelSize: Styles.Theme.fontMd
                            font.family: "monospace"
                            color: Styles.Theme.textPrimary
                            Layout.preferredWidth: Styles.Theme.dp(50)
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    ConfigButton {
                        text: "Calibrate Touch"
                        Layout.alignment: Qt.AlignRight
                        onClicked: configScreen.calibrateDisplay()
                    }
                }
            }

            // Bottom spacing
            Item { Layout.preferredHeight: Styles.Theme.spacingXl }
        }
    }

    // Scroll indicator
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 2
        width: 4
        radius: 2
        color: Styles.Theme.backgroundTertiary
        opacity: flickable.contentHeight > flickable.height ? 0.5 : 0

        Rectangle {
            anchors.right: parent.right
            width: parent.width
            height: Math.max(20, parent.height * (flickable.height / flickable.contentHeight))
            y: (parent.height - height) * (flickable.contentY / (flickable.contentHeight - flickable.height))
            radius: 2
            color: Styles.Theme.textTertiary
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // INTERNAL COMPONENTS
    // ═══════════════════════════════════════════════════════════════

    component ConfigSection: Rectangle {
        property string title: ""
        property string iconText: ""
        default property alias content: contentContainer.data

        implicitHeight: sectionColumn.implicitHeight + Styles.Theme.spacingMd * 2
        color: Styles.Theme.surfaceCard
        radius: Styles.Theme.radiusMedium

        ColumnLayout {
            id: sectionColumn
            anchors.fill: parent
            anchors.margins: Styles.Theme.spacingMd
            spacing: Styles.Theme.spacingSm

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Styles.Theme.spacingSm

                Text {
                    text: iconText
                    font.pixelSize: Styles.Theme.fontLg
                    font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                    color: Styles.Theme.accentPrimary
                }

                Text {
                    text: title
                    font.pixelSize: Styles.Theme.fontLg
                    font.weight: Styles.Theme.fontWeightBold
                    color: Styles.Theme.textPrimary
                }

                Item { Layout.fillWidth: true }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Styles.Theme.backgroundTertiary
            }

            // Content container
            Item {
                id: contentContainer
                Layout.fillWidth: true
                implicitHeight: childrenRect.height
            }
        }
    }

    component ConfigLabel: Text {
        font.pixelSize: Styles.Theme.fontSm
        color: Styles.Theme.textSecondary
    }

    component ConfigValue: Text {
        property color valueColor: Styles.Theme.textPrimary
        font.pixelSize: Styles.Theme.fontSm
        font.family: "monospace"
        color: valueColor
    }

    component StatusBadge: Rectangle {
        property alias text: badgeText.text
        property alias color: badgeRect.color

        id: badgeRect
        implicitWidth: badgeText.implicitWidth + Styles.Theme.spacingSm * 2
        implicitHeight: Styles.Theme.dp(24)
        radius: Styles.Theme.radiusSmall

        Text {
            id: badgeText
            anchors.centerIn: parent
            font.pixelSize: Styles.Theme.fontXs
            font.weight: Styles.Theme.fontWeightBold
            color: Styles.Theme.backgroundPrimary
        }
    }

    component ConfigButton: Rectangle {
        property alias text: buttonText.text
        property bool enabled: true
        signal clicked()

        implicitWidth: buttonText.implicitWidth + Styles.Theme.spacingMd * 2
        implicitHeight: Styles.Theme.buttonHeightSmall
        radius: Styles.Theme.radiusSmall
        color: enabled
               ? (buttonMouse.pressed ? Styles.Theme.surfacePressed : Styles.Theme.backgroundTertiary)
               : Styles.Theme.backgroundSecondary
        opacity: enabled ? 1.0 : 0.5

        Text {
            id: buttonText
            anchors.centerIn: parent
            font.pixelSize: Styles.Theme.fontSm
            font.weight: Styles.Theme.fontWeightMedium
            color: Styles.Theme.textPrimary
        }

        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            enabled: parent.enabled
            onClicked: parent.clicked()
        }
    }
}
