import QtQuick
import QtQuick.Layouts

/**
 * TabBar.qml - Bottom navigation tab bar
 *
 * Features:
 * - 4 navigation tabs (Home, Dash, Config, Auto)
 * - Active tab indicator with glow
 * - Smooth transitions
 * - Touch-friendly sizing
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    property int currentIndex: 0
    property color backgroundColor: "#0d0d0d"
    property color activeColor: "#00E676"
    property color inactiveColor: "#666666"

    signal tabClicked(int index)

    height: 56
    width: parent.width

    // Tab model
    property var tabs: [
        { icon: "H", label: "Home" },
        { icon: "D", label: "Dash" },
        { icon: "S", label: "Settings" },
        { icon: "A", label: "Auto" }
    ]

    // ═══════════════════════════════════════════════════════════════════════
    // BACKGROUND
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: backgroundColor

        // Top border
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1a1a1a"
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // TAB ITEMS
    // ═══════════════════════════════════════════════════════════════════════

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: tabs

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                property bool isActive: root.currentIndex === index
                property color tabColor: isActive ? activeColor : inactiveColor

                // Active indicator glow
                Rectangle {
                    visible: isActive
                    anchors.centerIn: parent
                    width: 60
                    height: 44
                    radius: 12
                    color: activeColor
                    opacity: 0.1
                }

                // Tab content
                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    // Icon circle
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 28
                        height: 28
                        radius: 14
                        color: isActive ? activeColor : "transparent"
                        border.color: tabColor
                        border.width: isActive ? 0 : 1.5

                        Text {
                            anchors.centerIn: parent
                            text: modelData.icon
                            color: isActive ? "#000000" : tabColor
                            font.pixelSize: 14
                            font.bold: true
                            font.family: "Roboto, sans-serif"
                        }

                        Behavior on color {
                            ColorAnimation { duration: 200 }
                        }

                        Behavior on border.color {
                            ColorAnimation { duration: 200 }
                        }
                    }

                    // Label
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: tabColor
                        font.pixelSize: 10
                        font.family: "Roboto, sans-serif"
                        font.weight: isActive ? Font.Bold : Font.Normal

                        Behavior on color {
                            ColorAnimation { duration: 200 }
                        }
                    }
                }

                // Touch area
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.currentIndex = index
                        root.tabClicked(index)
                    }

                    // Press feedback
                    onPressed: parent.scale = 0.95
                    onReleased: parent.scale = 1.0
                    onCanceled: parent.scale = 1.0
                }

                Behavior on scale {
                    NumberAnimation { duration: 100 }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ACTIVE INDICATOR LINE
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        id: activeIndicator
        anchors.top: parent.top
        height: 3
        width: parent.width / tabs.length - 40
        radius: 1.5
        color: activeColor
        x: (currentIndex * parent.width / tabs.length) + 20

        Behavior on x {
            NumberAnimation {
                duration: 250
                easing.type: Easing.OutCubic
            }
        }
    }
}
