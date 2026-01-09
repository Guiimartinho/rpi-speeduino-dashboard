import QtQuick
import QtQuick.Controls

/**
 * LapTimer.qml - Lap timer with sector times and delta display
 *
 * Features:
 * - Manual or GPS-triggered lap detection
 * - Best lap tracking
 * - Sector times (up to 4 sectors)
 * - Live delta to best lap
 * - Lap history
 */
Item {
    id: root

    // Current lap state
    property bool isRunning: false
    property int currentLapNumber: 0
    property int currentLapTime: 0       // ms
    property int currentSector: 0        // 0-based
    property var currentSectorTimes: []  // Array of sector times in ms

    // Best lap data
    property int bestLapTime: 0          // ms
    property int bestLapNumber: 0
    property var bestSectorTimes: []

    // Predictive timing (delta to best)
    property int predictedDelta: 0       // ms, positive = slower
    property bool showDelta: true

    // Configuration
    property int numSectors: 3           // 1-4 sectors per lap
    property bool autoStartOnMovement: false
    property real movementThreshold: 5   // km/h to consider moving

    // External inputs
    property real speed: 0               // km/h for auto-start
    property bool sectorTrigger: false   // External sector trigger (GPS/beacon)
    property bool lapTrigger: false      // External lap trigger

    // Lap history (last N laps)
    property var lapHistory: []
    property int maxLapHistory: 20

    // Internal timing
    property int lapStartTime: 0
    property int sectorStartTime: 0

    // Format time as MM:SS.mmm
    function formatTime(ms) {
        if (ms <= 0) return "--:--.---"
        var totalSeconds = Math.floor(ms / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        var millis = ms % 1000
        return (minutes < 10 ? "0" : "") + minutes + ":" +
               (seconds < 10 ? "0" : "") + seconds + "." +
               (millis < 10 ? "00" : (millis < 100 ? "0" : "")) + millis
    }

    // Format delta as +/-X.XXX
    function formatDelta(ms) {
        var sign = ms >= 0 ? "+" : "-"
        var absMs = Math.abs(ms)
        var seconds = (absMs / 1000).toFixed(3)
        return sign + seconds
    }

    // Start new lap
    function startLap() {
        if (!isRunning) {
            isRunning = true
            currentLapNumber = 1
        }
        lapStartTime = Date.now()
        sectorStartTime = lapStartTime
        currentLapTime = 0
        currentSector = 0
        currentSectorTimes = []
        for (var i = 0; i < numSectors; i++) {
            currentSectorTimes.push(0)
        }
    }

    // Complete sector
    function completeSector() {
        if (!isRunning) return

        var now = Date.now()
        var sectorTime = now - sectorStartTime
        currentSectorTimes[currentSector] = sectorTime

        // Calculate delta vs best
        if (bestSectorTimes.length > currentSector && bestSectorTimes[currentSector] > 0) {
            predictedDelta += sectorTime - bestSectorTimes[currentSector]
        }

        currentSector++
        sectorStartTime = now

        // If all sectors complete, finish lap
        if (currentSector >= numSectors) {
            completeLap()
        }
    }

    // Complete lap
    function completeLap() {
        if (!isRunning) return

        var now = Date.now()
        var lapTime = now - lapStartTime

        // Record lap in history
        var lapData = {
            number: currentLapNumber,
            time: lapTime,
            sectors: currentSectorTimes.slice(),
            timestamp: now
        }
        lapHistory.unshift(lapData)
        if (lapHistory.length > maxLapHistory) {
            lapHistory.pop()
        }

        // Check for best lap
        if (bestLapTime === 0 || lapTime < bestLapTime) {
            bestLapTime = lapTime
            bestLapNumber = currentLapNumber
            bestSectorTimes = currentSectorTimes.slice()
        }

        // Start new lap
        currentLapNumber++
        predictedDelta = 0
        startLap()
    }

    // Stop timer
    function stopTimer() {
        isRunning = false
    }

    // Reset all data
    function resetAll() {
        isRunning = false
        currentLapNumber = 0
        currentLapTime = 0
        currentSector = 0
        currentSectorTimes = []
        bestLapTime = 0
        bestLapNumber = 0
        bestSectorTimes = []
        predictedDelta = 0
        lapHistory = []
    }

    // Handle external triggers
    onLapTriggerChanged: {
        if (lapTrigger && isRunning) {
            // Complete current lap if in last sector
            if (currentSector === numSectors - 1) {
                completeSector()
            } else {
                completeLap()
            }
        } else if (lapTrigger && !isRunning) {
            startLap()
        }
    }

    onSectorTriggerChanged: {
        if (sectorTrigger && isRunning) {
            completeSector()
        }
    }

    // Update timer
    Timer {
        interval: 10  // 100 Hz for smooth display
        running: isRunning
        repeat: true
        onTriggered: {
            currentLapTime = Date.now() - lapStartTime
        }
    }

    // Auto-start on movement
    onSpeedChanged: {
        if (autoStartOnMovement && !isRunning && speed > movementThreshold) {
            startLap()
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
                    text: "LAP TIMER"
                    color: "#FFC107"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                // Status indicator
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: isRunning ? "#00E676" : "#666666"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: isRunning
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1.0; duration: 500 }
                    }
                }

                Text {
                    text: isRunning ? "LAP " + currentLapNumber : "STOPPED"
                    color: Qt.rgba(1, 1, 1, 0.72)
                    font.pixelSize: 12
                    font.family: "Roboto Mono, monospace"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Current lap time (large)
            Text {
                width: parent.width
                text: formatTime(currentLapTime)
                color: "#FFFFFF"
                font.pixelSize: 32
                font.bold: true
                font.family: "Roboto Mono, monospace"
                horizontalAlignment: Text.AlignCenter
            }

            // Delta display
            Rectangle {
                width: parent.width
                height: 30
                radius: 6
                color: predictedDelta > 0 ? Qt.rgba(1, 0.3, 0.3, 0.3) :
                       predictedDelta < 0 ? Qt.rgba(0.3, 1, 0.3, 0.3) :
                       Qt.rgba(1, 1, 1, 0.1)
                visible: showDelta && bestLapTime > 0 && isRunning

                Text {
                    anchors.centerIn: parent
                    text: formatDelta(predictedDelta)
                    color: predictedDelta > 0 ? "#FF5252" :
                           predictedDelta < 0 ? "#00E676" : "#FFFFFF"
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }
            }

            // Sector times
            Row {
                width: parent.width
                spacing: 4
                visible: numSectors > 1

                Repeater {
                    model: numSectors

                    Rectangle {
                        width: (parent.width - (numSectors - 1) * 4) / numSectors
                        height: 40
                        radius: 4
                        color: index === currentSector ? "#333333" : "#222222"
                        border.color: index === currentSector ? "#00E676" : "transparent"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                text: "S" + (index + 1)
                                color: Qt.rgba(1, 1, 1, 0.5)
                                font.pixelSize: 9
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Text {
                                property int sectorTime: currentSectorTimes[index] || 0
                                text: sectorTime > 0 ? (sectorTime / 1000).toFixed(2) : "--"
                                color: {
                                    if (sectorTime <= 0) return Qt.rgba(1, 1, 1, 0.5)
                                    if (bestSectorTimes[index] && sectorTime < bestSectorTimes[index]) return "#00E676"
                                    if (bestSectorTimes[index] && sectorTime > bestSectorTimes[index]) return "#FF5252"
                                    return "#FFFFFF"
                                }
                                font.pixelSize: 12
                                font.bold: true
                                font.family: "Roboto Mono, monospace"
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
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

            // Best lap
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "BEST"
                    color: "#FFC107"
                    font.pixelSize: 10
                    font.family: "Roboto, sans-serif"
                }

                Text {
                    text: bestLapTime > 0 ? formatTime(bestLapTime) : "--:--.---"
                    color: "#FFC107"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: bestLapNumber > 0 ? "(Lap " + bestLapNumber + ")" : ""
                    color: Qt.rgba(1, 1, 1, 0.5)
                    font.pixelSize: 10
                    font.family: "Roboto, sans-serif"
                    visible: bestLapNumber > 0
                }
            }

            // Control buttons
            Row {
                width: parent.width
                spacing: 8

                Rectangle {
                    width: (parent.width - 8) / 2
                    height: 36
                    radius: 6
                    color: startMouse.pressed ? "#00C853" : (isRunning ? "#333333" : "#00E676")

                    Text {
                        anchors.centerIn: parent
                        text: isRunning ? "SECTOR" : "START"
                        color: isRunning ? Qt.rgba(1, 1, 1, 0.87) : "#000000"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: startMouse
                        anchors.fill: parent
                        onClicked: {
                            if (!isRunning) {
                                startLap()
                            } else {
                                completeSector()
                            }
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 8) / 2
                    height: 36
                    radius: 6
                    color: resetMouse.pressed ? "#D32F2F" : "#333333"

                    Text {
                        anchors.centerIn: parent
                        text: isRunning ? "STOP" : "RESET"
                        color: Qt.rgba(1, 1, 1, 0.87)
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: resetMouse
                        anchors.fill: parent
                        onClicked: {
                            if (isRunning) {
                                stopTimer()
                            } else {
                                resetAll()
                            }
                        }
                    }
                }
            }

            // Last laps (scrollable)
            ListView {
                width: parent.width
                height: Math.min(100, lapHistory.length * 25)
                clip: true
                model: lapHistory.slice(0, 4)
                visible: lapHistory.length > 0

                delegate: Row {
                    width: parent ? parent.width : 0
                    height: 24
                    spacing: 8

                    Text {
                        text: "L" + modelData.number
                        color: modelData.number === bestLapNumber ? "#FFC107" : Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 10
                        font.family: "Roboto Mono, monospace"
                        width: 30
                    }

                    Text {
                        text: formatTime(modelData.time)
                        color: modelData.number === bestLapNumber ? "#FFC107" : Qt.rgba(1, 1, 1, 0.72)
                        font.pixelSize: 11
                        font.family: "Roboto Mono, monospace"
                    }

                    Text {
                        property int delta: bestLapTime > 0 ? modelData.time - bestLapTime : 0
                        text: delta !== 0 ? formatDelta(delta) : ""
                        color: delta > 0 ? "#FF5252" : "#00E676"
                        font.pixelSize: 10
                        font.family: "Roboto Mono, monospace"
                        visible: delta !== 0
                    }
                }
            }
        }
    }
}
