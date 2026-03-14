import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State

/**
 * SplashScreen - Customizable boot splash screen
 *
 * Shows manufacturer logo and brand name on startup.
 * Configuration via /etc/speeduino-ui/system.yaml:
 *
 * branding:
 *   show_splash: true
 *   splash_duration_ms: 2500
 *   splash_logo: "/etc/speeduino-ui/assets/logo_vw.png"
 *   splash_logo_scale: 1.0
 *   brand_name: "Volkswagen"
 *   brand_color: "#00438f"
 */
Item {
    id: splashScreen
    anchors.fill: parent
    z: 1001  // Above ReverseOverlay (z: 1000)

    // Visibility controlled by AppState
    visible: State.AppState.currentOverlay === State.AppState.overlaySplash

    // Fade animation
    opacity: visible ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation {
            duration: Styles.Theme.animationNormal
            easing.type: Styles.Theme.easingType
        }
    }

    // Dark background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Main content
    ColumnLayout {
        anchors.centerIn: parent
        spacing: Styles.Theme.spacingLg

        // Logo image (if configured)
        Image {
            id: logoImage
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Styles.Theme.dp(200) * (brandingManager ? brandingManager.splashLogoScale : 1.0)
            Layout.preferredHeight: Styles.Theme.dp(200) * (brandingManager ? brandingManager.splashLogoScale : 1.0)
            source: brandingManager ? brandingManager.splashLogoUrl() : ""
            fillMode: Image.PreserveAspectFit
            visible: source.toString().length > 0
            asynchronous: false

            // Scale animation on load
            scale: 0.8
            SequentialAnimation on scale {
                running: splashScreen.visible && logoImage.visible
                NumberAnimation {
                    to: 1.0
                    duration: Styles.Theme.animationSlow
                    easing.type: Easing.OutBack
                }
            }
        }

        // Fallback icon (if no logo configured)
        Text {
            id: fallbackIcon
            Layout.alignment: Qt.AlignHCenter
            text: "S"  // Speeduino initial
            font.pixelSize: Styles.Theme.fontMega * 2
            font.weight: Font.Bold
            color: (typeof brandingManager !== "undefined" && brandingManager !== null
                   && typeof brandingManager.brandColor === "string" && brandingManager.brandColor !== "")
                   ? brandingManager.brandColor
                   : Styles.Theme.accentPrimary
            visible: !logoImage.visible

            // Pulse animation
            SequentialAnimation on opacity {
                running: splashScreen.visible && !logoImage.visible
                loops: Animation.Infinite
                NumberAnimation { to: 0.6; duration: 800 }
                NumberAnimation { to: 1.0; duration: 800 }
            }
        }

        // Brand name
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: brandingManager ? brandingManager.brandName : "Speeduino"
            font.pixelSize: Styles.Theme.fontXxl
            font.weight: Styles.Theme.fontWeightBold
            color: Styles.Theme.textPrimary
        }

        // Subtitle
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Dashboard"
            font.pixelSize: Styles.Theme.fontLg
            color: Styles.Theme.textSecondary
        }

        // Loading bar
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Styles.Theme.dp(200)
            Layout.preferredHeight: Styles.Theme.dp(4)
            Layout.topMargin: Styles.Theme.spacingMd
            radius: Styles.Theme.radiusSmall
            color: Styles.Theme.surfaceAlt

            Rectangle {
                id: progressBar
                height: parent.height
                radius: parent.radius
                color: (typeof brandingManager !== "undefined" && brandingManager !== null
                       && typeof brandingManager.brandColor === "string" && brandingManager.brandColor !== "")
                       ? brandingManager.brandColor
                       : Styles.Theme.accentPrimary
                width: 0

                SequentialAnimation on width {
                    running: splashScreen.visible
                    loops: 1

                    NumberAnimation {
                        to: splashScreen.width > 0 ? Styles.Theme.dp(200) : 200
                        duration: brandingManager ? brandingManager.splashDurationMs - 300 : 2200
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Status text
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Styles.Theme.spacingSm
            text: "Initializing..."
            font.pixelSize: Styles.Theme.fontMd
            color: Styles.Theme.textTertiary
        }
    }

    // Auto-hide timer
    Timer {
        id: splashTimer
        interval: brandingManager ? brandingManager.splashDurationMs : 2500
        running: splashScreen.visible && (brandingManager ? brandingManager.showSplash : true)
        onTriggered: State.AppState.hideSplash()
    }

    // Component completed - show splash if configured
    Component.onCompleted: {
        if (brandingManager && brandingManager.showSplash) {
            State.AppState.showSplash()
        }
    }
}
