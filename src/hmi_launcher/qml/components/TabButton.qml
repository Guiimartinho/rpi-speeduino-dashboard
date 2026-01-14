import QtQuick
import QtQuick.Layouts
import "../styles" as Styles

/**
 * TabButton.qml
 * Individual tab button for bottom navigation
 *
 * Features:
 * - Icon + label layout
 * - Active/inactive states with animations
 * - Touch feedback with ripple effect
 * - Accessibility support
 */
Item {
    id: root

    // Required properties
    required property string iconSource
    required property string label
    property bool isActive: false
    property color activeColor: Styles.Theme.tabActive
    property color inactiveColor: Styles.Theme.tabInactive

    // Size (set by TabBar)
    implicitWidth: Styles.Theme.touchTargetLarge
    implicitHeight: Styles.Theme.tabBarHeight

    // Signals
    signal clicked()
    signal pressAndHold()

    // State for visual feedback
    property bool pressed: mouseArea.pressed

    // Accessibility
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: clicked()

    // Background with touch feedback
    Rectangle {
        id: background
        anchors.fill: parent
        color: "transparent"
        radius: Styles.Theme.radiusMedium

        // Touch feedback rectangle
        Rectangle {
            id: touchFeedback
            anchors.centerIn: parent
            width: parent.width * 0.8
            height: parent.height * 0.8
            radius: Styles.Theme.radiusMedium
            color: root.isActive ? Qt.rgba(root.activeColor.r, root.activeColor.g, root.activeColor.b, 0.1)
                                 : "transparent"
            opacity: root.pressed ? 0.3 : (root.isActive ? 1.0 : 0)

            Behavior on opacity {
                NumberAnimation {
                    duration: Styles.Theme.animationFast
                    easing.type: Styles.Theme.easingType
                }
            }
        }
    }

    // Content layout
    ColumnLayout {
        anchors.centerIn: parent
        spacing: Styles.Theme.spacingXs

        // Icon
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Styles.Theme.tabIconSize + Styles.Theme.spacingSm
            Layout.preferredHeight: Styles.Theme.tabIconSize + Styles.Theme.spacingSm

            // Icon text (using text as icon for simplicity)
            // In production, replace with Image or Icon component
            Text {
                id: iconText
                anchors.centerIn: parent
                text: root.iconSource
                font.pixelSize: Styles.Theme.tabIconSize
                font.family: "Segoe UI Symbol, Noto Sans Symbols, sans-serif"
                color: root.isActive ? root.activeColor : root.inactiveColor

                Behavior on color {
                    ColorAnimation {
                        duration: Styles.Theme.animationNormal
                        easing.type: Styles.Theme.easingType
                    }
                }

                // Scale animation on active
                transform: Scale {
                    origin.x: iconText.width / 2
                    origin.y: iconText.height / 2
                    xScale: root.isActive ? 1.1 : 1.0
                    yScale: root.isActive ? 1.1 : 1.0

                    Behavior on xScale {
                        NumberAnimation {
                            duration: Styles.Theme.animationNormal
                            easing.type: Easing.OutBack
                        }
                    }
                    Behavior on yScale {
                        NumberAnimation {
                            duration: Styles.Theme.animationNormal
                            easing.type: Easing.OutBack
                        }
                    }
                }
            }
        }

        // Label
        Text {
            id: labelText
            Layout.alignment: Qt.AlignHCenter
            text: root.label
            font.pixelSize: Styles.Theme.fontXs
            font.weight: root.isActive ? Styles.Theme.fontWeightBold : Styles.Theme.fontWeightNormal
            color: root.isActive ? root.activeColor : root.inactiveColor

            Behavior on color {
                ColorAnimation {
                    duration: Styles.Theme.animationNormal
                    easing.type: Styles.Theme.easingType
                }
            }
        }

        // Active indicator line
        Rectangle {
            id: activeIndicator
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.isActive ? Styles.Theme.touchTargetMin * 0.6 : 0
            Layout.preferredHeight: Styles.Theme.tabIndicatorHeight
            radius: Styles.Theme.tabIndicatorHeight / 2
            color: root.activeColor
            opacity: root.isActive ? 1.0 : 0

            Behavior on Layout.preferredWidth {
                NumberAnimation {
                    duration: Styles.Theme.animationNormal
                    easing.type: Easing.OutBack
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: Styles.Theme.animationFast
                    easing.type: Styles.Theme.easingType
                }
            }
        }
    }

    // Mouse/Touch area
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: root.clicked()
        onPressAndHold: root.pressAndHold()

        // Visual feedback on press
        onPressed: {
            pressAnimation.start()
        }
    }

    // Press animation (scale down slightly)
    SequentialAnimation {
        id: pressAnimation

        NumberAnimation {
            target: root
            property: "scale"
            to: 0.95
            duration: 50
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: root
            property: "scale"
            to: 1.0
            duration: 100
            easing.type: Easing.OutBack
        }
    }
}
