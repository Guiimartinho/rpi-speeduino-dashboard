import QtQuick

/**
 * TripComputer.qml - Trip computer with distance, fuel consumption, time
 *
 * Features:
 * - Trip A and Trip B independent odometers
 * - Instantaneous and average fuel consumption
 * - Trip time and average speed tracking
 * - Estimated range calculation
 * - Total odometer with persistence support
 */
Item {
    id: root

    // Input values
    property real speed: 0              // km/h
    property real rpm: 0                // RPM
    property real tps: 0                // Throttle position %
    property real mapKpa: 0             // Manifold pressure kPa
    property real fuelTankCapacity: 50  // Liters
    property real fuelLevel: 50         // Liters remaining (estimated)
    property real injectorPulseWidth: 0 // ms (if available from ECU)

    // Trip A data
    property real tripADistance: 0      // km
    property real tripAFuelUsed: 0      // Liters
    property int tripAStartTime: 0      // ms timestamp

    // Trip B data
    property real tripBDistance: 0      // km
    property real tripBFuelUsed: 0      // Liters
    property int tripBStartTime: 0      // ms timestamp

    // Total odometer (should be persisted)
    property real totalOdometer: 0      // km

    // Calculated values
    property real instantFuelConsumption: 0     // L/100km
    property real avgFuelConsumptionA: 0        // L/100km
    property real avgFuelConsumptionB: 0        // L/100km
    property real avgSpeedA: 0                  // km/h
    property real avgSpeedB: 0                  // km/h
    property real estimatedRange: 0             // km

    // Active trip display (0 = A, 1 = B)
    property int activeTrip: 0

    // Unit preference
    property bool useMetric: true       // true = km/L, false = miles/gallon

    // Internal tracking
    property real lastUpdateTime: 0
    property real fuelFlowRate: 0       // L/h estimated

    // Signals for persistence
    signal odometerChanged(real value)
    signal tripDataChanged()

    // Update calculations
    Timer {
        interval: 100   // 10 Hz update
        running: true
        repeat: true
        onTriggered: updateCalculations()
    }

    function updateCalculations() {
        var now = Date.now()
        var deltaTime = (now - lastUpdateTime) / 1000 / 3600  // Hours

        if (lastUpdateTime > 0 && deltaTime > 0 && deltaTime < 0.01) {
            // Calculate distance traveled
            var distanceTraveled = speed * deltaTime  // km

            // Update odometers only when moving
            if (speed > 1) {
                tripADistance += distanceTraveled
                tripBDistance += distanceTraveled
                totalOdometer += distanceTraveled
            }

            // Estimate fuel consumption
            if (injectorPulseWidth > 0) {
                // Use real injector data if available
                fuelFlowRate = calculateFuelFromInjector(rpm, injectorPulseWidth)
            } else {
                // Estimate from engine parameters
                fuelFlowRate = estimateFuelFlow(rpm, mapKpa, tps)
            }

            var fuelUsed = fuelFlowRate * deltaTime  // Liters

            tripAFuelUsed += fuelUsed
            tripBFuelUsed += fuelUsed

            // Calculate instantaneous fuel consumption (L/100km)
            if (speed > 5) {
                instantFuelConsumption = (fuelFlowRate / speed) * 100
            } else if (speed > 0.5) {
                // At very low speeds, show L/h instead (scaled)
                instantFuelConsumption = fuelFlowRate * 10
            } else {
                instantFuelConsumption = 0
            }

            // Calculate trip A averages
            if (tripADistance > 0.1) {
                avgFuelConsumptionA = (tripAFuelUsed / tripADistance) * 100
                var tripATimeHours = (now - tripAStartTime) / 1000 / 3600
                if (tripATimeHours > 0) {
                    avgSpeedA = tripADistance / tripATimeHours
                }
            }

            // Calculate trip B averages
            if (tripBDistance > 0.1) {
                avgFuelConsumptionB = (tripBFuelUsed / tripBDistance) * 100
                var tripBTimeHours = (now - tripBStartTime) / 1000 / 3600
                if (tripBTimeHours > 0) {
                    avgSpeedB = tripBDistance / tripBTimeHours
                }
            }

            // Estimate range
            var currentAvgConsumption = activeTrip === 0 ? avgFuelConsumptionA : avgFuelConsumptionB
            if (currentAvgConsumption > 0) {
                estimatedRange = (fuelLevel / currentAvgConsumption) * 100
            } else if (instantFuelConsumption > 0) {
                estimatedRange = (fuelLevel / instantFuelConsumption) * 100
            }
        }

        lastUpdateTime = now
    }

    // Calculate fuel flow from injector pulse width (more accurate)
    function calculateFuelFromInjector(rpm, pulseWidth) {
        if (rpm < 100 || pulseWidth < 0.5) return 0

        // Assuming 4 cylinder, 440cc injectors
        var injectorFlowRate = 440 / 1000  // L/min at full duty
        var cylinderCount = 4
        var dutyCycle = (pulseWidth * rpm) / 120000  // Approximate duty cycle

        return injectorFlowRate * cylinderCount * dutyCycle * 60  // L/h
    }

    // Estimate fuel flow rate (L/h) based on engine parameters
    function estimateFuelFlow(rpm, map, tps) {
        if (rpm < 100) return 0

        // Base idle consumption (~0.8 L/h)
        var baseFlow = 0.8

        // Add consumption based on RPM
        var rpmFactor = rpm / 1000 * 0.5

        // Add consumption based on load (MAP/TPS)
        var loadFactor = (map / 100) * (tps / 100) * 8

        return baseFlow + rpmFactor + loadFactor
    }

    // Reset Trip A
    function resetTripA() {
        tripADistance = 0
        tripAFuelUsed = 0
        tripAStartTime = Date.now()
        avgFuelConsumptionA = 0
        avgSpeedA = 0
        tripDataChanged()
    }

    // Reset Trip B
    function resetTripB() {
        tripBDistance = 0
        tripBFuelUsed = 0
        tripBStartTime = Date.now()
        avgFuelConsumptionB = 0
        avgSpeedB = 0
        tripDataChanged()
    }

    // Format time duration
    function formatDuration(startTime) {
        var seconds = Math.floor((Date.now() - startTime) / 1000)
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = seconds % 60
        return (h > 0 ? h + ":" : "") +
               (m < 10 && h > 0 ? "0" : "") + m + ":" +
               (s < 10 ? "0" : "") + s
    }

    // Convert units
    function kmToMiles(km) {
        return km * 0.621371
    }

    function litersTo100ToMpg(l100) {
        if (l100 <= 0) return 0
        return 235.215 / l100
    }

    Component.onCompleted: {
        tripAStartTime = Date.now()
        tripBStartTime = Date.now()
        lastUpdateTime = Date.now()
    }

    // Visual display
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 12
        border.color: "#333333"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header with trip selector
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "TRIP COMPUTER"
                    color: "#2196F3"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                // Trip A/B selector
                Row {
                    spacing: 4
                    anchors.right: parent.right

                    Rectangle {
                        width: 40
                        height: 24
                        radius: 4
                        color: activeTrip === 0 ? "#2196F3" : "#333333"

                        Text {
                            anchors.centerIn: parent
                            text: "A"
                            color: activeTrip === 0 ? "#000000" : Qt.rgba(1, 1, 1, 0.72)
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: activeTrip = 0
                            onPressAndHold: resetTripA()
                        }
                    }

                    Rectangle {
                        width: 40
                        height: 24
                        radius: 4
                        color: activeTrip === 1 ? "#2196F3" : "#333333"

                        Text {
                            anchors.centerIn: parent
                            text: "B"
                            color: activeTrip === 1 ? "#000000" : Qt.rgba(1, 1, 1, 0.72)
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: activeTrip = 1
                            onPressAndHold: resetTripB()
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#333333"
            }

            // Main trip data
            Grid {
                width: parent.width
                columns: 2
                rowSpacing: 8
                columnSpacing: 16

                TripItem {
                    label: "DISTANCE"
                    value: (activeTrip === 0 ? tripADistance : tripBDistance).toFixed(1)
                    unit: useMetric ? "km" : "mi"
                    width: (parent.width - parent.columnSpacing) / 2
                }

                TripItem {
                    label: "TIME"
                    value: formatDuration(activeTrip === 0 ? tripAStartTime : tripBStartTime)
                    unit: ""
                    width: (parent.width - parent.columnSpacing) / 2
                }

                TripItem {
                    label: "AVG SPEED"
                    value: (activeTrip === 0 ? avgSpeedA : avgSpeedB).toFixed(0)
                    unit: useMetric ? "km/h" : "mph"
                    width: (parent.width - parent.columnSpacing) / 2
                }

                TripItem {
                    label: "AVG CONS"
                    value: (activeTrip === 0 ? avgFuelConsumptionA : avgFuelConsumptionB).toFixed(1)
                    unit: useMetric ? "L/100" : "mpg"
                    width: (parent.width - parent.columnSpacing) / 2
                }

                TripItem {
                    label: speed > 5 ? "INST CONS" : "FUEL RATE"
                    value: speed > 5 ? instantFuelConsumption.toFixed(1) : fuelFlowRate.toFixed(1)
                    unit: speed > 5 ? (useMetric ? "L/100" : "mpg") : "L/h"
                    highlight: true
                    width: (parent.width - parent.columnSpacing) / 2
                }

                TripItem {
                    label: "RANGE"
                    value: estimatedRange > 0 ? estimatedRange.toFixed(0) : "--"
                    unit: useMetric ? "km" : "mi"
                    highlight: estimatedRange < 50 && estimatedRange > 0
                    highlightColor: "#FFC107"
                    width: (parent.width - parent.columnSpacing) / 2
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#333333"
            }

            // Total odometer
            Row {
                width: parent.width

                Text {
                    text: "TOTAL"
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 10
                    font.family: "Roboto, sans-serif"
                }

                Item { width: 10; height: 1 }

                Text {
                    text: totalOdometer.toFixed(0) + (useMetric ? " km" : " mi")
                    color: Qt.rgba(1, 1, 1, 0.72)
                    font.pixelSize: 12
                    font.family: "Roboto Mono, monospace"
                    anchors.right: parent.right
                }
            }

            // Instructions
            Text {
                width: parent.width
                text: "Hold A/B to reset trip"
                color: Qt.rgba(1, 1, 1, 0.38)
                font.pixelSize: 9
                font.family: "Roboto, sans-serif"
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    // Trip item component
    component TripItem: Item {
        property string label: ""
        property string value: ""
        property string unit: ""
        property bool highlight: false
        property color highlightColor: "#2196F3"

        height: 36

        Column {
            anchors.fill: parent
            spacing: 2

            Text {
                text: label
                color: Qt.rgba(1, 1, 1, 0.56)
                font.pixelSize: 10
                font.family: "Roboto, sans-serif"
            }

            Row {
                spacing: 4

                Text {
                    text: value
                    color: highlight ? highlightColor : Qt.rgba(1, 1, 1, 0.96)
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: unit
                    color: Qt.rgba(1, 1, 1, 0.56)
                    font.pixelSize: 12
                    font.family: "Roboto, sans-serif"
                    visible: unit !== ""
                }
            }
        }
    }
}
