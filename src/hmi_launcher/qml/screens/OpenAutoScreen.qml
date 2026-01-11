import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * OpenAutoScreen.qml
 * Container for OpenAuto/Android Auto display
 *
 * ARCHITECTURE:
 * - This screen provides the container Item where Android Auto video will be displayed
 * - On Component.onCompleted, we register our container with C++ via setVideoContainer()
 * - On visibility changes, we notify C++ via setVideoVisible()
 * - Touch events are captured and forwarded via sendTouch()
 * - The actual video rendering is done by a C++ QWidget that overlays on our container
 *
 * Features:
 * - Video surface for OpenAuto (embedded in content area, NOT fullscreen)
 * - Connection status overlay when not connected
 * - Touch passthrough to OpenAuto embedded library
 * - Tab bar remains accessible at bottom
 */
Item {
    id: openAutoScreen

    // Signals for OpenAuto control
    signal requestStart()
    signal requestStop()
    signal touchEvent(int x, int y, int type)

    // Reference to embedded controller (set by parent)
    property var embeddedController: null

    // Use embedded OpenAuto if available (default mode)
    property bool useEmbedded: typeof openAutoEmbedded !== "undefined" && openAutoEmbedded !== null

    // ═══════════════════════════════════════════════════════════════
    // LIFECYCLE - Register container with C++
    // ═══════════════════════════════════════════════════════════════

    Component.onCompleted: {
        console.log("OpenAutoScreen: initialized, useEmbedded =", useEmbedded)
        console.log("OpenAutoScreen: size =", width, "x", height)

        // CRITICAL: Register our video container with C++
        // This allows C++ to parent its video widget to our window and position it correctly
        if (useEmbedded && openAutoEmbedded) {
            console.log("OpenAutoScreen: Registering video container with C++")
            openAutoEmbedded.setVideoContainer(openAutoVideoPlaceholder)

            // Also notify C++ that we're visible (screen just loaded)
            openAutoEmbedded.setVideoVisible(true)

            // Auto-start if not already running
            if (!openAutoEmbedded.running) {
                console.log("OpenAutoScreen: Auto-starting embedded OpenAuto")
                openAutoEmbedded.start()
            }
        }
    }

    Component.onDestruction: {
        // Notify C++ that screen is being destroyed
        if (useEmbedded && openAutoEmbedded) {
            openAutoEmbedded.setVideoVisible(false)
        }
    }

    // CRITICAL: Track visibility changes to show/hide video widget
    onVisibleChanged: {
        console.log("OpenAutoScreen: visibility changed to", visible)
        if (useEmbedded && openAutoEmbedded) {
            openAutoEmbedded.setVideoVisible(visible)
        }
    }

    // Connections for embedded OpenAuto
    Connections {
        target: useEmbedded ? openAutoEmbedded : null
        enabled: useEmbedded

        function onStarted() {
            console.log("OpenAutoScreen: Embedded OpenAuto started")
        }

        function onStopped() {
            console.log("OpenAutoScreen: Embedded OpenAuto stopped")
        }

        function onProjectionStarted() {
            console.log("OpenAutoScreen: Projection started - phone connected")
        }

        function onProjectionStopped() {
            console.log("OpenAutoScreen: Projection stopped")
        }

        function onErrorChanged() {
            if (openAutoEmbedded.errorMessage !== "") {
                console.log("OpenAutoScreen: Error -", openAutoEmbedded.errorMessage)
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // VIDEO SURFACE CONTAINER
    // ═══════════════════════════════════════════════════════════════

    // OpenAuto video surface (C++ widget renders as overlay on this Item)
    Rectangle {
        id: videoSurface
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary

        // CRITICAL: This Item is the video container
        // C++ will parent its QWidget to our window and position it over this Item
        Item {
            id: openAutoVideoPlaceholder
            anchors.fill: parent
            objectName: "openAutoVideoSurface"  // For debugging/identification

            // Notify C++ when geometry changes (debounced to avoid excessive calls)
            onWidthChanged: geometryUpdateTimer.restart()
            onHeightChanged: geometryUpdateTimer.restart()
            onXChanged: geometryUpdateTimer.restart()
            onYChanged: geometryUpdateTimer.restart()

            // Debounce timer to batch rapid geometry changes (e.g., during animations)
            Timer {
                id: geometryUpdateTimer
                interval: 16  // ~60fps, batches rapid changes
                onTriggered: openAutoVideoPlaceholder.updateGeometryNow()
            }

            function updateGeometryNow() {
                if (useEmbedded && openAutoEmbedded && visible) {
                    var scenePos = mapToItem(null, 0, 0)
                    openAutoEmbedded.updateVideoGeometry(
                        Math.round(scenePos.x),
                        Math.round(scenePos.y),
                        Math.round(width),
                        Math.round(height)
                    )
                }
            }

            // Debug border when connected (shows container bounds)
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: State.AppState.openAutoConnected ? Styles.Theme.accentAndroidAuto : "transparent"
                border.width: State.AppState.openAutoConnected ? 2 : 0
                visible: State.AppState.openAutoConnected
            }
        }

        // Touch passthrough area - forwards touch DIRECTLY to embedded OpenAuto
        // CRITICAL: Call openAutoEmbedded.sendTouch() directly, not through signal
        MouseArea {
            anchors.fill: parent
            enabled: State.AppState.openAutoRunning && State.AppState.openAutoConnected

            onPressed: function(mouse) {
                if (useEmbedded && openAutoEmbedded) {
                    openAutoEmbedded.sendTouch(Math.round(mouse.x), Math.round(mouse.y), 0)  // 0 = press
                }
                openAutoScreen.touchEvent(mouse.x, mouse.y, 0)  // Also emit for compatibility
            }
            onReleased: function(mouse) {
                if (useEmbedded && openAutoEmbedded) {
                    openAutoEmbedded.sendTouch(Math.round(mouse.x), Math.round(mouse.y), 1)  // 1 = release
                }
                openAutoScreen.touchEvent(mouse.x, mouse.y, 1)
            }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    if (useEmbedded && openAutoEmbedded) {
                        openAutoEmbedded.sendTouch(Math.round(mouse.x), Math.round(mouse.y), 2)  // 2 = move
                    }
                    openAutoScreen.touchEvent(mouse.x, mouse.y, 2)
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // CONNECTION OVERLAY (shown when not connected)
    // ═══════════════════════════════════════════════════════════════

    Rectangle {
        id: connectionOverlay
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
        // FIX: Use opacity-based visibility to allow fade animation
        // visible binding would hide element before animation plays
        visible: opacity > 0
        opacity: State.AppState.openAutoConnected ? 0.0 : 1.0

        Behavior on opacity {
            NumberAnimation { duration: Styles.Theme.animationNormal }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingLg

            // Android Auto Icon
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\u25B6"  // Play icon
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
                        return "OpenAuto is not running\nPress Start to begin"
                    }
                    if (State.AppState.openAutoPhoneName) {
                        return "Device detected: " + State.AppState.openAutoPhoneName + "\nConnecting..."
                    }
                    return "Waiting for phone connection...\nConnect your Android phone via USB"
                }
            }

            // Error message (if any)
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: useEmbedded && openAutoEmbedded ? openAutoEmbedded.errorMessage : ""
                font.pixelSize: Styles.Theme.fontSm
                color: Styles.Theme.statusCritical
                horizontalAlignment: Text.AlignHCenter
                visible: text !== ""
                wrapMode: Text.WordWrap
                Layout.maximumWidth: parent.width * 0.8
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
                            restartTimer.start()
                        } else {
                            openAutoScreen.requestStart()
                        }
                    }
                }
            }

            // Restart delay timer
            Timer {
                id: restartTimer
                interval: 500
                onTriggered: openAutoScreen.requestStart()
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
                    text: "\u25A1"  // Square (phone placeholder)
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
                    text: "\u25A0"  // Filled square (car placeholder)
                    font.pixelSize: Styles.Theme.fontXl
                    font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                    color: Styles.Theme.accentAndroidAuto
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // MINI DASHBOARD OVERLAY (optional feature)
    // ═══════════════════════════════════════════════════════════════

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

    // ═══════════════════════════════════════════════════════════════
    // STATUS INDICATORS
    // ═══════════════════════════════════════════════════════════════

    // OpenAuto process status indicator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: Styles.Theme.spacingSm
        anchors.bottomMargin: Styles.Theme.spacingSm
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

    // Connected phone indicator (shown briefly when connected)
    Rectangle {
        id: connectedIndicator
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Styles.Theme.spacingSm
        width: connectedRow.implicitWidth + Styles.Theme.spacingMd * 2
        height: Styles.Theme.dp(32)
        radius: Styles.Theme.radiusSmall
        color: Styles.Theme.overlayDark
        visible: State.AppState.openAutoConnected
        opacity: connectedIndicatorTimer.running ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Styles.Theme.animationNormal }
        }

        RowLayout {
            id: connectedRow
            anchors.centerIn: parent
            spacing: Styles.Theme.spacingSm

            Text {
                text: "\u2713"  // Checkmark
                font.pixelSize: Styles.Theme.fontMd
                color: Styles.Theme.statusOk
            }

            Text {
                text: State.AppState.openAutoPhoneName || "Phone Connected"
                font.pixelSize: Styles.Theme.fontSm
                font.weight: Styles.Theme.fontWeightMedium
                color: Styles.Theme.textPrimary
            }

            Text {
                text: "(USB)"
                font.pixelSize: Styles.Theme.fontXs
                color: Styles.Theme.textSecondary
            }
        }

        // Auto-hide timer
        Timer {
            id: connectedIndicatorTimer
            interval: 5000
            running: false
        }

        // Show indicator when connected
        Connections {
            target: State.AppState
            function onOpenAutoConnectedChanged() {
                if (State.AppState.openAutoConnected) {
                    connectedIndicatorTimer.restart()
                }
            }
        }

        // Click to keep visible
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (connectedIndicatorTimer.running) {
                    connectedIndicatorTimer.stop()
                } else {
                    connectedIndicatorTimer.restart()
                }
            }
        }
    }
}
