import QtQuick

/**
 * GaugeCard.qml - Reusable card-style gauge component
 *
 * ISO 26262 ASIL-B: Extracted from inline component for:
 * - Better testability
 * - Reduced QML engine overhead
 * - Consistent rendering across screens
 *
 * Usage:
 *   GaugeCard {
 *       label: "RPM"
 *       value: "3500"
 *       unit: ""
 *       progress: rpm / 8000
 *       accentColor: "#00E676"
 *       showBar: true
 *       large: true
 *   }
 */
Rectangle {
    id: gaugeCard

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════════════════════

    /// Label displayed above the value (e.g., "RPM", "CLT", "AFR")
    property string label: ""

    /// The current value as string (e.g., "3500", "85.5", "N")
    property string value: ""

    /// Unit suffix (e.g., "°C", "km/h", "%")
    property string unit: ""

    /// Progress value 0.0-1.0 for the bar (if showBar is true)
    property real progress: 0

    /// Accent color for value text and progress bar
    property color accentColor: "#00E676"

    /// Show horizontal progress bar below the value
    property bool showBar: false

    /// Enable warning state (red border and background tint)
    property bool warning: false

    /// Use larger fonts for primary gauges
    property bool large: false

    /// Center the value vertically (for gear display)
    property bool centered: false

    // ═══════════════════════════════════════════════════════════════════════
    // CARD STYLING
    // ═══════════════════════════════════════════════════════════════════════

    radius: 10
    color: warning ? Qt.rgba(244/255, 67/255, 54/255, 0.12) : "#141414"
    border.color: warning ? "#F44336" : "#1e1e1e"
    border.width: 1

    // ═══════════════════════════════════════════════════════════════════════
    // CONTENT LAYOUT
    // ═══════════════════════════════════════════════════════════════════════

    Column {
        anchors.centerIn: parent
        spacing: large ? 8 : 4

        // Label
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: gaugeCard.label
            color: "#555555"
            font.pixelSize: large ? 11 : 9
            font.bold: true
            font.letterSpacing: 1
        }

        // Value + Unit
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 3

            Text {
                text: gaugeCard.value
                color: gaugeCard.accentColor
                font.pixelSize: large ? 36 : 24
                font.bold: true
                font.family: "Roboto Mono, Consolas, monospace"
            }

            Text {
                visible: gaugeCard.unit !== ""
                text: gaugeCard.unit
                color: "#444444"
                font.pixelSize: large ? 14 : 10
                anchors.bottom: parent.children[0].bottom
                anchors.bottomMargin: large ? 4 : 2
            }
        }

        // Progress bar (optional)
        Rectangle {
            visible: gaugeCard.showBar
            anchors.horizontalCenter: parent.horizontalCenter
            width: gaugeCard.width - 24
            height: 6
            radius: 3
            color: "#1a1a1a"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * Math.min(1, gaugeCard.progress)
                radius: 3
                color: gaugeCard.accentColor

                Behavior on width { NumberAnimation { duration: 100 } }
            }
        }
    }
}
