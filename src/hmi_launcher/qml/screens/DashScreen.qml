import QtQuick
import QtQuick.Layouts
import "../styles" as Styles
import "../state" as State
import "../components" as Components

/**
 * DashScreen.qml
 * Full dashboard with all gauges
 *
 * Features:
 * - Responsive gauge layout adapts to screen size
 * - Primary gauges: RPM, Speed (large)
 * - Secondary gauges: CLT, IAT, MAP, TPS, AFR, IGN
 * - Warning indicators for critical values
 * - Optimized for 7" (800x480) and 10.1" (1280x800) displays
 */
Item {
    id: dashScreen

    // Engine data from DataProvider
    property var engineData: ({
        rpm: 0,
        coolantTemp: 0,
        intakeTemp: 0,
        tps: 0,
        mapKpa: 0,
        lambda: 1.0,
        ignitionAdvance: 0,
        injectorDuty: 0,
        vehicleSpeed: 0,
        gear: 0,
        canOk: false,
        celOn: false,
        overheat: false
    })

    // Background
    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.backgroundPrimary
    }

    // Responsive layout based on available space
    property bool compactMode: width < 900

    // Main gauge grid
    GridLayout {
        id: gaugeGrid
        anchors.fill: parent
        anchors.margins: Styles.Theme.spacingSm
        columns: compactMode ? 3 : 4
        rows: compactMode ? 3 : 2
        rowSpacing: Styles.Theme.spacingSm
        columnSpacing: Styles.Theme.spacingSm

        // ═══════════════════════════════════════════════════════════════
        // PRIMARY GAUGES (Large)
        // ═══════════════════════════════════════════════════════════════

        // RPM Gauge - Spans 2 columns in compact, 2 in wide
        Components.SafeGauge {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: parent.height * (compactMode ? 0.45 : 0.5)

            label: "RPM"
            unit: ""
            value: dashScreen.engineData.rpm
            minValue: 0
            maxValue: 8000
            warningThreshold: 6500
            criticalThreshold: 7500
            precision: 0
            gaugeColor: Styles.Theme.accentPrimary
            showArc: true
            arcStartAngle: -140
            arcEndAngle: 140
            isPrimary: true
        }

        // Speed Gauge - Spans 2 columns
        Components.SafeGauge {
            Layout.columnSpan: compactMode ? 1 : 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: parent.height * (compactMode ? 0.45 : 0.5)

            label: "SPEED"
            unit: "km/h"
            value: dashScreen.engineData.vehicleSpeed
            minValue: 0
            maxValue: 280
            precision: 0
            gaugeColor: Styles.Theme.accentSecondary
            showArc: true
            arcStartAngle: -140
            arcEndAngle: 140
            isPrimary: true

            // Gear indicator overlay
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: Styles.Theme.spacingSm
                width: Styles.Theme.dp(40)
                height: Styles.Theme.dp(40)
                radius: Styles.Theme.radiusMedium
                color: Styles.Theme.backgroundTertiary
                visible: dashScreen.engineData.gear > 0

                Text {
                    anchors.centerIn: parent
                    text: dashScreen.engineData.gear === 0 ? "N" : dashScreen.engineData.gear.toString()
                    font.pixelSize: Styles.Theme.fontXl
                    font.weight: Styles.Theme.fontWeightBold
                    color: Styles.Theme.textPrimary
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════
        // SECONDARY GAUGES (Smaller)
        // ═══════════════════════════════════════════════════════════════

        // CLT - Coolant Temperature
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "CLT"
            unit: "\u00B0C"
            value: dashScreen.engineData.coolantTemp
            minValue: 0
            maxValue: 120
            warningThreshold: 95
            criticalThreshold: 105
            precision: 0
            gaugeColor: getCoolantColor()
            showBar: true

            function getCoolantColor() {
                var temp = dashScreen.engineData.coolantTemp
                if (temp > 100) return Styles.Theme.statusCritical
                if (temp > 95) return Styles.Theme.statusWarning
                if (temp < 60) return Styles.Theme.accentSecondary
                return Styles.Theme.accentPrimary
            }
        }

        // IAT - Intake Air Temperature
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "IAT"
            unit: "\u00B0C"
            value: dashScreen.engineData.intakeTemp
            minValue: -20
            maxValue: 80
            warningThreshold: 50
            criticalThreshold: 60
            precision: 0
            gaugeColor: Styles.Theme.accentSecondary
            showBar: true
        }

        // MAP - Manifold Pressure
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "MAP"
            unit: "kPa"
            value: dashScreen.engineData.mapKpa
            minValue: 0
            maxValue: 250
            warningThreshold: 200
            precision: 0
            gaugeColor: Styles.Theme.accentOrange
            showBar: true
        }

        // TPS - Throttle Position
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "TPS"
            unit: "%"
            value: dashScreen.engineData.tps
            minValue: 0
            maxValue: 100
            precision: 0
            gaugeColor: Styles.Theme.accentPrimary
            showBar: true
        }

        // Lambda / AFR
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "AFR"
            unit: ""
            value: dashScreen.engineData.lambda * 14.7  // Convert to AFR
            minValue: 10
            maxValue: 20
            warningThreshold: 12
            criticalThreshold: 11
            warningThresholdHigh: 16
            criticalThresholdHigh: 17
            precision: 1
            gaugeColor: Styles.Theme.accentPurple
            showBar: true
        }

        // Ignition Advance
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: "IGN"
            unit: "\u00B0"
            value: dashScreen.engineData.ignitionAdvance
            minValue: -10
            maxValue: 50
            precision: 1
            gaugeColor: Styles.Theme.accentPink
            showBar: true
        }

        // Injector Duty (only in wide mode or as 7th gauge)
        Components.SafeGauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !compactMode || gaugeGrid.columns >= 4

            label: "INJ"
            unit: "%"
            value: dashScreen.engineData.injectorDuty
            minValue: 0
            maxValue: 100
            warningThreshold: 80
            criticalThreshold: 95
            precision: 0
            gaugeColor: Styles.Theme.accentOrange
            showBar: true
        }
    }

    // Warning overlay for critical conditions
    Rectangle {
        id: warningOverlay
        anchors.fill: parent
        color: "transparent"
        visible: dashScreen.engineData.celOn || dashScreen.engineData.overheat

        // CEL Warning Banner
        Rectangle {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: Styles.Theme.spacingMd
            width: celText.implicitWidth + Styles.Theme.spacingLg * 2
            height: Styles.Theme.touchTargetMin
            radius: Styles.Theme.radiusMedium
            color: Styles.Theme.statusWarning
            visible: dashScreen.engineData.celOn

            Text {
                id: celText
                anchors.centerIn: parent
                text: "CHECK ENGINE"
                font.pixelSize: Styles.Theme.fontLg
                font.weight: Styles.Theme.fontWeightBold
                color: Styles.Theme.backgroundPrimary
            }
        }

        // Overheat Warning Banner
        Rectangle {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: dashScreen.engineData.celOn
                              ? Styles.Theme.touchTargetMin + Styles.Theme.spacingMd * 2
                              : Styles.Theme.spacingMd
            width: overheatText.implicitWidth + Styles.Theme.spacingLg * 2
            height: Styles.Theme.touchTargetMin
            radius: Styles.Theme.radiusMedium
            color: Styles.Theme.statusCritical
            visible: dashScreen.engineData.overheat

            Text {
                id: overheatText
                anchors.centerIn: parent
                text: "OVERHEAT!"
                font.pixelSize: Styles.Theme.fontLg
                font.weight: Styles.Theme.fontWeightBold
                color: Styles.Theme.textPrimary
            }

            // Blink animation
            SequentialAnimation on opacity {
                running: dashScreen.engineData.overheat
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 200 }
                NumberAnimation { to: 1.0; duration: 200 }
            }
        }
    }
}
