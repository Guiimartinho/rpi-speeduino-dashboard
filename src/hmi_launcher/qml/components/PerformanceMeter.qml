import QtQuick

/**
 * PerformanceMeter.qml - Acceleration performance measurement
 *
 * Features:
 * - 0-60 mph / 0-100 km/h timing
 * - 1/8 mile and 1/4 mile timing with trap speed
 * - 60-foot time
 * - Best run tracking
 * - Automatic start detection
 */
Item {
    id: root

    // Input values
    property real speed: 0               // km/h
    property real rpm: 0                 // For launch detection

    // Configuration
    property bool useMetric: true        // true = km/h, false = mph
    property real launchThreshold: 3     // Speed to detect launch (km/h)
    property real rpmThreshold: 2000     // RPM threshold for ready state

    // State
    property int runState: 0             // 0=idle, 1=staging, 2=running, 3=completed
    property int runStartTime: 0
    property int runEndTime: 0

    // Distance tracking (estimated from speed integration)
    property real distanceTraveled: 0    // meters
    property real lastSpeed: 0
    property int lastUpdateTime: 0

    // Target distances in meters
    readonly property real sixtyFeetMeters: 18.288     // 60 feet
    readonly property real eighthMileMeters: 201.168   // 1/8 mile
    readonly property real quarterMileMeters: 402.336  // 1/4 mile
    readonly property real kmhTo100: 100               // 0-100 km/h
    readonly property real kmhTo60mph: 96.56           // 0-60 mph (~96.56 km/h)

    // Current run results
    property int time60ft: 0             // ms
    property int timeEighthMile: 0       // ms
    property int timeQuarterMile: 0      // ms
    property int time0to60: 0            // ms (0-60 mph)
    property int time0to100: 0           // ms (0-100 km/h)
    property real trapSpeed60ft: 0       // km/h
    property real trapSpeedEighth: 0     // km/h
    property real trapSpeedQuarter: 0    // km/h

    // Best runs
    property int bestTime60ft: 0
    property int bestTimeEighth: 0
    property int bestTimeQuarter: 0
    property int bestTime0to60: 0
    property int bestTime0to100: 0
    property real bestTrapSpeedQuarter: 0

    // Run history
    property var runHistory: []
    property int maxRunHistory: 10

    // Flags for milestone completion
    property bool hit60ft: false
    property bool hitEighth: false
    property bool hitQuarter: false
    property bool hit60mph: false
    property bool hit100kmh: false

    function formatTime(ms) {
        if (ms <= 0) return "--.---"
        return (ms / 1000).toFixed(3)
    }

    function formatSpeed(kmh) {
        if (kmh <= 0) return "---"
        if (useMetric) {
            return kmh.toFixed(1) + " km/h"
        } else {
            return (kmh * 0.621371).toFixed(1) + " mph"
        }
    }

    function getStateText() {
        switch (runState) {
            case 0: return "READY"
            case 1: return "STAGING..."
            case 2: return "RUNNING"
            case 3: return "COMPLETED"
            default: return "---"
        }
    }

    function getStateColor() {
        switch (runState) {
            case 0: return "#666666"
            case 1: return "#FFC107"
            case 2: return "#00E676"
            case 3: return "#2196F3"
            default: return "#666666"
        }
    }

    // Reset for new run
    function resetRun() {
        runState = 0
        distanceTraveled = 0
        time60ft = 0
        timeEighthMile = 0
        timeQuarterMile = 0
        time0to60 = 0
        time0to100 = 0
        trapSpeed60ft = 0
        trapSpeedEighth = 0
        trapSpeedQuarter = 0
        hit60ft = false
        hitEighth = false
        hitQuarter = false
        hit60mph = false
        hit100kmh = false
    }

    // Reset all including bests
    function resetAll() {
        resetRun()
        bestTime60ft = 0
        bestTimeEighth = 0
        bestTimeQuarter = 0
        bestTime0to60 = 0
        bestTime0to100 = 0
        bestTrapSpeedQuarter = 0
        runHistory = []
    }

    // Stage for launch (manual trigger)
    function stageRun() {
        if (runState === 0 && speed < launchThreshold) {
            runState = 1
            resetRun()
            runState = 1  // Keep staging state
        }
    }

    // Complete run and save to history
    function completeRun() {
        runState = 3
        runEndTime = Date.now()

        // Update best times
        if (time60ft > 0 && (bestTime60ft === 0 || time60ft < bestTime60ft)) {
            bestTime60ft = time60ft
        }
        if (timeEighthMile > 0 && (bestTimeEighth === 0 || timeEighthMile < bestTimeEighth)) {
            bestTimeEighth = timeEighthMile
        }
        if (timeQuarterMile > 0 && (bestTimeQuarter === 0 || timeQuarterMile < bestTimeQuarter)) {
            bestTimeQuarter = timeQuarterMile
            bestTrapSpeedQuarter = trapSpeedQuarter
        }
        if (time0to60 > 0 && (bestTime0to60 === 0 || time0to60 < bestTime0to60)) {
            bestTime0to60 = time0to60
        }
        if (time0to100 > 0 && (bestTime0to100 === 0 || time0to100 < bestTime0to100)) {
            bestTime0to100 = time0to100
        }

        // Save to history
        var runData = {
            timestamp: runEndTime,
            time60ft: time60ft,
            timeEighth: timeEighthMile,
            timeQuarter: timeQuarterMile,
            time0to60: time0to60,
            time0to100: time0to100,
            trapSpeed: trapSpeedQuarter
        }
        runHistory.unshift(runData)
        if (runHistory.length > maxRunHistory) {
            runHistory.pop()
        }
    }

    // Speed change handler - main logic
    onSpeedChanged: {
        var now = Date.now()

        // Auto-stage when stopped with high RPM
        if (runState === 0 && speed < launchThreshold && rpm > rpmThreshold) {
            runState = 1
        }

        // Detect launch
        if (runState === 1 && speed >= launchThreshold) {
            runState = 2
            runStartTime = now
            lastUpdateTime = now
            lastSpeed = 0
            distanceTraveled = 0
        }

        // Running state - track distance and times
        if (runState === 2) {
            var deltaTime = (now - lastUpdateTime) / 1000  // seconds

            if (deltaTime > 0 && deltaTime < 1) {  // Sanity check
                // Integrate distance (trapezoidal rule)
                var avgSpeed = (speed + lastSpeed) / 2  // km/h
                var distanceIncrement = (avgSpeed / 3.6) * deltaTime  // meters
                distanceTraveled += distanceIncrement

                var elapsedMs = now - runStartTime

                // Check milestones
                if (!hit60ft && distanceTraveled >= sixtyFeetMeters) {
                    time60ft = elapsedMs
                    trapSpeed60ft = speed
                    hit60ft = true
                }

                if (!hitEighth && distanceTraveled >= eighthMileMeters) {
                    timeEighthMile = elapsedMs
                    trapSpeedEighth = speed
                    hitEighth = true
                }

                if (!hitQuarter && distanceTraveled >= quarterMileMeters) {
                    timeQuarterMile = elapsedMs
                    trapSpeedQuarter = speed
                    hitQuarter = true
                    completeRun()
                }

                // Speed milestones
                if (!hit60mph && speed >= kmhTo60mph) {
                    time0to60 = elapsedMs
                    hit60mph = true
                }

                if (!hit100kmh && speed >= kmhTo100) {
                    time0to100 = elapsedMs
                    hit100kmh = true
                }
            }

            lastSpeed = speed
            lastUpdateTime = now

            // Abort if speed drops significantly (failed run)
            if (speed < 5 && distanceTraveled > 10) {
                runState = 0  // Reset to idle
            }
        }

        // After completed run, if stopped, ready for new run
        if (runState === 3 && speed < launchThreshold) {
            runState = 0
        }
    }

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

            // Header
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "PERFORMANCE"
                    color: "#FF5252"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                // Status indicator
                Rectangle {
                    width: 80
                    height: 20
                    radius: 4
                    color: getStateColor()
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: getStateText()
                        color: runState === 1 ? "#000000" : "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            // Main times display
            Grid {
                width: parent.width
                columns: 2
                rowSpacing: 8
                columnSpacing: 16

                // 0-60 mph / 0-100 km/h
                TimeDisplay {
                    label: useMetric ? "0-100 km/h" : "0-60 mph"
                    time: useMetric ? time0to100 : time0to60
                    bestTime: useMetric ? bestTime0to100 : bestTime0to60
                    width: (parent.width - 16) / 2
                }

                // 60-foot
                TimeDisplay {
                    label: "60 ft"
                    time: time60ft
                    bestTime: bestTime60ft
                    trapSpeed: trapSpeed60ft
                    width: (parent.width - 16) / 2
                }

                // 1/8 mile
                TimeDisplay {
                    label: "1/8 mile"
                    time: timeEighthMile
                    bestTime: bestTimeEighth
                    trapSpeed: trapSpeedEighth
                    width: (parent.width - 16) / 2
                }

                // 1/4 mile
                TimeDisplay {
                    label: "1/4 mile"
                    time: timeQuarterMile
                    bestTime: bestTimeQuarter
                    trapSpeed: trapSpeedQuarter
                    highlight: true
                    width: (parent.width - 16) / 2
                }
            }

            // Progress bar (distance)
            Rectangle {
                width: parent.width
                height: 8
                radius: 4
                color: "#333333"
                visible: runState === 2

                Rectangle {
                    width: Math.min(1, distanceTraveled / quarterMileMeters) * parent.width
                    height: parent.height
                    radius: 4
                    color: "#00E676"

                    Behavior on width {
                        NumberAnimation { duration: 50 }
                    }
                }

                // Distance markers
                Repeater {
                    model: [sixtyFeetMeters / quarterMileMeters, eighthMileMeters / quarterMileMeters, 1.0]

                    Rectangle {
                        x: modelData * parent.width - 1
                        width: 2
                        height: parent.height
                        color: "#666666"
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#333333"
            }

            // Control buttons
            Row {
                width: parent.width
                spacing: 8

                Rectangle {
                    width: (parent.width - 8) / 2
                    height: 36
                    radius: 6
                    color: stageMouse.pressed ? "#FFA000" : (runState === 1 ? "#FFC107" : "#333333")
                    visible: runState < 2

                    Text {
                        anchors.centerIn: parent
                        text: "STAGE"
                        color: runState === 1 ? "#000000" : Qt.rgba(1, 1, 1, 0.87)
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: stageMouse
                        anchors.fill: parent
                        onClicked: stageRun()
                    }
                }

                Rectangle {
                    width: runState < 2 ? (parent.width - 8) / 2 : parent.width
                    height: 36
                    radius: 6
                    color: resetMouse.pressed ? "#D32F2F" : "#333333"

                    Text {
                        anchors.centerIn: parent
                        text: runState === 3 ? "NEW RUN" : "RESET"
                        color: Qt.rgba(1, 1, 1, 0.87)
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: resetMouse
                        anchors.fill: parent
                        onClicked: resetRun()
                        onPressAndHold: resetAll()
                    }
                }
            }

            // Instructions
            Text {
                width: parent.width
                text: runState === 0 ? "Stage or rev above " + rpmThreshold + " RPM" :
                      runState === 1 ? "Launch when ready!" :
                      runState === 2 ? "Distance: " + distanceTraveled.toFixed(1) + " m" :
                      "Hold RESET to clear bests"
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 10
                font.family: "Roboto, sans-serif"
                horizontalAlignment: Text.AlignCenter
            }
        }
    }

    // Time display component
    component TimeDisplay: Item {
        property string label: ""
        property int time: 0
        property int bestTime: 0
        property real trapSpeed: 0
        property bool highlight: false

        height: 50

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: highlight ? Qt.rgba(0, 0.9, 0.46, 0.1) : "transparent"
            border.color: highlight ? "#00E676" : "transparent"
            border.width: 1
        }

        Column {
            anchors.fill: parent
            anchors.margins: 4
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
                    text: formatTime(time)
                    color: time > 0 && time === bestTime ? "#FFC107" : Qt.rgba(1, 1, 1, 0.96)
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: "s"
                    color: Qt.rgba(1, 1, 1, 0.5)
                    font.pixelSize: 12
                    font.family: "Roboto, sans-serif"
                    anchors.baseline: parent.children[0].baseline
                    visible: time > 0
                }
            }

            Text {
                text: trapSpeed > 0 ? formatSpeed(trapSpeed) : (bestTime > 0 ? "BEST: " + formatTime(bestTime) : "")
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 9
                font.family: "Roboto Mono, monospace"
            }
        }
    }
}
