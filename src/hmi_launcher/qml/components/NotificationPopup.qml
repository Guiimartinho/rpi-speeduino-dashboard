import QtQuick
import QtQuick.Controls
import "../styles" as Styles

/**
 * NotificationPopup.qml - Toast-style notification overlay
 *
 * Features:
 * - Slide-in animation from top
 * - Auto-hide with configurable timeout
 * - Manual dismiss with tap or swipe
 * - Multiple severity levels (info, warning, critical)
 * - Queue system for multiple notifications
 *
 * Usage:
 *   NotificationPopup { id: notificationPopup }
 *   notificationPopup.show("Title", "Message")
 *   notificationPopup.show("Warning", "Engine hot!", NotificationPopup.Warning)
 */
Item {
    id: root
    anchors.fill: parent
    z: 999  // Below reverse overlay (1000) but above everything else

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC API
    // ═══════════════════════════════════════════════════════════════════════

    enum Severity {
        Info,
        Warning,
        Critical
    }

    property int defaultTimeout: 4000  // ms
    property bool isVisible: popup.y > -popup.height

    // Show notification with optional severity
    function show(title, message, severity) {
        if (severity === undefined) severity = NotificationPopup.Severity.Info

        // Add to queue
        notificationQueue.push({
            title: title,
            message: message,
            severity: severity
        })

        // Process queue if not already showing
        if (!showAnimation.running && !hideAnimation.running && !isVisible) {
            processQueue()
        }
    }

    // Manually hide current notification
    function hide() {
        hideAnimation.start()
    }

    // Clear all pending notifications
    function clearAll() {
        notificationQueue = []
        hide()
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PRIVATE
    // ═══════════════════════════════════════════════════════════════════════

    property var notificationQueue: []
    property var currentNotification: null

    function processQueue() {
        if (notificationQueue.length === 0) return

        currentNotification = notificationQueue.shift()
        showAnimation.start()
        autoHideTimer.restart()
    }

    function getSeverityColor(severity) {
        switch (severity) {
            case NotificationPopup.Severity.Warning:
                return Styles.Theme.statusWarning
            case NotificationPopup.Severity.Critical:
                return Styles.Theme.statusCritical
            default:
                return Styles.Theme.statusInfo
        }
    }

    function getSeverityIcon(severity) {
        switch (severity) {
            case NotificationPopup.Severity.Warning:
                return "\u26A0"  // Warning sign
            case NotificationPopup.Severity.Critical:
                return "\u2716"  // X mark
            default:
                return "\u2139"  // Info sign
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // AUTO-HIDE TIMER
    // ═══════════════════════════════════════════════════════════════════════

    Timer {
        id: autoHideTimer
        interval: root.defaultTimeout
        onTriggered: hideAnimation.start()
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ANIMATIONS
    // ═══════════════════════════════════════════════════════════════════════

    NumberAnimation {
        id: showAnimation
        target: popup
        property: "y"
        from: -popup.height
        to: Styles.Theme.spacingMd
        duration: Styles.Theme.animationNormal
        easing.type: Easing.OutBack
        easing.overshoot: 0.5
    }

    NumberAnimation {
        id: hideAnimation
        target: popup
        property: "y"
        from: popup.y
        to: -popup.height
        duration: Styles.Theme.animationFast
        easing.type: Easing.InQuad

        onFinished: {
            currentNotification = null
            // Process next in queue
            if (notificationQueue.length > 0) {
                processQueue()
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // POPUP CONTAINER
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: popup
        anchors.horizontalCenter: parent.horizontalCenter
        y: -height  // Start hidden above screen
        width: Math.min(parent.width - Styles.Theme.spacingLg * 2, Styles.Theme.dp(400))
        height: contentColumn.height + Styles.Theme.spacingMd * 2

        radius: Styles.Theme.radiusMedium
        color: Styles.Theme.backgroundElevated
        border.color: currentNotification ? getSeverityColor(currentNotification.severity) : Styles.Theme.statusInfo
        border.width: Styles.Theme.borderMedium

        // Shadow effect
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            z: -1
            radius: parent.radius + 2
            color: "transparent"
            border.color: Styles.Theme.shadowColor
            border.width: 4
            opacity: 0.5
        }

        // Glow effect based on severity
        Rectangle {
            id: glowEffect
            anchors.fill: parent
            anchors.margins: -Styles.Theme.spacingSm
            z: -2
            radius: parent.radius + Styles.Theme.spacingSm
            color: currentNotification ? getSeverityColor(currentNotification.severity) : "transparent"
            opacity: 0.15
            visible: currentNotification && currentNotification.severity !== NotificationPopup.Severity.Info

            // Pulse animation for critical
            SequentialAnimation on opacity {
                running: currentNotification && currentNotification.severity === NotificationPopup.Severity.Critical
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 400 }
                NumberAnimation { to: 0.1; duration: 400 }
            }
        }

        // Content
        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Styles.Theme.spacingMd
            spacing: Styles.Theme.spacingSm

            // Header row (icon + title + close button)
            Row {
                width: parent.width
                spacing: Styles.Theme.spacingSm

                // Severity icon
                Text {
                    id: severityIcon
                    text: currentNotification ? getSeverityIcon(currentNotification.severity) : ""
                    color: currentNotification ? getSeverityColor(currentNotification.severity) : Styles.Theme.textPrimary
                    font.pixelSize: Styles.Theme.fontXl
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    height: titleText.height
                }

                // Title
                Text {
                    id: titleText
                    text: currentNotification ? currentNotification.title : ""
                    color: Styles.Theme.textPrimary
                    font.pixelSize: Styles.Theme.fontLg
                    font.bold: true
                    font.family: "Roboto, sans-serif"
                    width: parent.width - severityIcon.width - closeButton.width - Styles.Theme.spacingSm * 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                // Close button
                Rectangle {
                    id: closeButton
                    width: Styles.Theme.touchTargetMin
                    height: Styles.Theme.touchTargetMin
                    radius: Styles.Theme.radiusSmall
                    color: closeMouseArea.pressed ? Styles.Theme.surfacePressed :
                           closeMouseArea.containsMouse ? Styles.Theme.surfaceHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "\u2715"  // X symbol
                        color: Styles.Theme.textSecondary
                        font.pixelSize: Styles.Theme.fontMd
                        font.bold: true
                    }

                    MouseArea {
                        id: closeMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: hideAnimation.start()
                    }
                }
            }

            // Message
            Text {
                id: messageText
                text: currentNotification ? currentNotification.message : ""
                color: Styles.Theme.textSecondary
                font.pixelSize: Styles.Theme.fontMd
                font.family: "Roboto, sans-serif"
                width: parent.width
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                visible: text.length > 0
            }

            // Progress bar (time remaining)
            Rectangle {
                width: parent.width
                height: 3
                radius: 1.5
                color: Styles.Theme.backgroundTertiary
                visible: autoHideTimer.running

                Rectangle {
                    id: progressBar
                    height: parent.height
                    radius: parent.radius
                    color: currentNotification ? getSeverityColor(currentNotification.severity) : Styles.Theme.statusInfo
                    opacity: 0.7
                    width: parent.width  // Initial width, updated by progressTimer

                    Behavior on width {
                        NumberAnimation { duration: 100 }
                    }
                }

                // Timer to update progress bar width
                Timer {
                    id: progressTimer
                    running: autoHideTimer.running
                    interval: 50
                    repeat: true
                    property real elapsed: 0
                    onTriggered: {
                        // Use qualified reference to avoid context ambiguity
                        progressTimer.elapsed += interval
                        if (progressTimer.elapsed > root.defaultTimeout) progressTimer.elapsed = root.defaultTimeout
                        progressBar.width = progressBar.parent.width * (1 - progressTimer.elapsed / root.defaultTimeout)
                    }
                    onRunningChanged: {
                        if (running) {
                            // MEDIUM FIX: Use qualified reference to property to avoid
                            // "Cannot assign to non-existent property" error
                            progressTimer.elapsed = 0
                            progressBar.width = progressBar.parent.width
                        }
                    }
                }
            }
        }

        // Tap to dismiss (anywhere on popup)
        MouseArea {
            anchors.fill: parent
            z: -1  // Below close button
            onClicked: {
                autoHideTimer.stop()
                hideAnimation.start()
            }

            // Swipe up to dismiss
            property real startY: 0
            onPressed: function(mouse) { startY = mouse.y }
            onPositionChanged: function(mouse) {
                if (mouse.y < startY - 30) {  // Swipe up threshold
                    autoHideTimer.stop()
                    hideAnimation.start()
                }
            }
        }
    }
}
