import QtQuick
import QtQuick.Layouts
import QtMultimedia
import "../styles" as Styles
import "../state" as State

/**
 * OpenAutoScreen.qml
 * Container for OpenAuto/Android Auto display
 *
 * ARCHITECTURE (QML-native video):
 * - Uses native QML VideoOutput instead of QWidget overlay
 * - QMLVideoOutput (C++) provides video sink for H.264 decoded frames
 * - Touch events are captured and forwarded via sendTouch()
 * - No widget parenting/positioning needed - pure QML layout
 *
 * Features:
 * - Native QML video surface for OpenAuto (no EGLFS conflicts)
 * - Connection status overlay when not connected
 * - Touch passthrough to OpenAuto embedded library
 * - Tab bar remains accessible at bottom
 */
Item {
    id: openAutoScreen

    // Signals for OpenAuto control
    signal requestStart()
    signal requestStop()
    signal requestRestart()
    signal touchEvent(int x, int y, int type)

    // Reference to embedded controller (set by parent)
    property var embeddedController: null

    // Use embedded OpenAuto only if available AND process mode not forced
    // NOTE: useProcessOpenAuto is set in main.qml - when true, we use external autoapp process
    property bool useEmbedded: !useProcessOpenAuto && typeof openAutoEmbedded !== "undefined" && openAutoEmbedded !== null

    // ═══════════════════════════════════════════════════════════════
    // LIFECYCLE - Connect video sink and start OpenAuto
    // ═══════════════════════════════════════════════════════════════

    Component.onCompleted: {
        console.log("OpenAutoScreen: initialized, useEmbedded =", useEmbedded)
        console.log("OpenAutoScreen: size =", width, "x", height)

        if (useEmbedded && openAutoEmbedded) {
            // Connect video sink from QML VideoOutput to C++ QMLVideoOutput
            // This allows H.264 decoded frames to display in our native QML surface
            connectVideoSink()

            // Notify C++ that we're visible
            openAutoEmbedded.setVideoVisible(true)

            // Auto-start using Qt.callLater() to avoid blocking UI thread
            if (!openAutoEmbedded.running) {
                console.log("OpenAutoScreen: Scheduling embedded OpenAuto start (deferred)")
                Qt.callLater(function() {
                    if (openAutoEmbedded && !openAutoEmbedded.running) {
                        console.log("OpenAutoScreen: Starting embedded OpenAuto")
                        openAutoEmbedded.start()
                    }
                })
            }
        }
    }

    // HIGH FIX: Track video sink connection retries to prevent infinite loops
    property int _videoSinkRetryCount: 0
    readonly property int _maxVideoSinkRetries: 10  // Maximum retry attempts

    // Helper function to connect video sink with retry limit
    function connectVideoSink() {
        if (openAutoEmbedded && openAutoEmbedded.qmlVideoOutput && openAutoVideoOutput.videoSink) {
            console.log("OpenAutoScreen: Connecting video sink to QMLVideoOutput")
            openAutoEmbedded.qmlVideoOutput.setVideoSink(openAutoVideoOutput.videoSink)
            _videoSinkRetryCount = 0  // Reset on success
        } else {
            // HIGH FIX: Limit retry attempts to prevent infinite loop
            _videoSinkRetryCount++
            if (_videoSinkRetryCount >= _maxVideoSinkRetries) {
                console.error("OpenAutoScreen: Failed to connect video sink after",
                             _maxVideoSinkRetries, "attempts - giving up")
                _videoSinkRetryCount = 0
                return
            }
            console.log("OpenAutoScreen: Video sink not ready, retry", _videoSinkRetryCount, "of", _maxVideoSinkRetries)
            Qt.callLater(connectVideoSink)
        }
    }

    Component.onDestruction: {
        if (useEmbedded && openAutoEmbedded) {
            openAutoEmbedded.setVideoVisible(false)
            // MEDIUM FIX: Disconnect video sink on destruction to prevent dangling references
            if (openAutoEmbedded.qmlVideoOutput) {
                openAutoEmbedded.qmlVideoOutput.setVideoSink(null)
            }
        }
        // Reset retry counter on destruction
        _videoSinkRetryCount = 0
    }

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
            // HIGH FIX: Null check before accessing errorMessage
            if (openAutoEmbedded && openAutoEmbedded.errorMessage !== "") {
                console.log("OpenAutoScreen: Error -", openAutoEmbedded.errorMessage)
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // VIDEO SURFACE - Native QML VideoOutput
    // ═══════════════════════════════════════════════════════════════

    Rectangle {
        id: videoSurface
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary

        // Native QML VideoOutput - displays H.264 decoded frames from QMLVideoOutput
        // No widget overlay needed - this is pure QML rendering
        VideoOutput {
            id: openAutoVideoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit

            // Debug: show when video is actually playing
            visible: State.AppState.openAutoConnected
        }

        // Debug border when connected (shows video bounds)
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: State.AppState.openAutoConnected ? Styles.Theme.accentAndroidAuto : "transparent"
            border.width: State.AppState.openAutoConnected ? 2 : 0
            visible: State.AppState.openAutoConnected
            z: 1
        }

        // Touch passthrough area - forwards touch to embedded OpenAuto
        MouseArea {
            id: touchArea
            anchors.fill: parent
            enabled: State.AppState.openAutoRunning && State.AppState.openAutoConnected
            z: 2  // Above video output

            function validateAndSendTouch(mouseX, mouseY, action) {
                if (!isFinite(mouseX) || !isFinite(mouseY)) {
                    console.warn("OpenAutoScreen: Invalid touch coordinates (non-finite)")
                    return
                }

                var safeX = Math.max(0, Math.min(Math.round(mouseX), width))
                var safeY = Math.max(0, Math.min(Math.round(mouseY), height))

                if (useEmbedded && openAutoEmbedded) {
                    openAutoEmbedded.sendTouch(safeX, safeY, action)
                }
                openAutoScreen.touchEvent(safeX, safeY, action)
            }

            onPressed: function(mouse) {
                validateAndSendTouch(mouse.x, mouse.y, 0)  // 0 = press
            }
            onReleased: function(mouse) {
                validateAndSendTouch(mouse.x, mouse.y, 1)  // 1 = release
            }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    validateAndSendTouch(mouse.x, mouse.y, 2)  // 2 = move
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
                            // Use restart() which handles stop + delayed start safely
                            openAutoScreen.requestRestart()
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
