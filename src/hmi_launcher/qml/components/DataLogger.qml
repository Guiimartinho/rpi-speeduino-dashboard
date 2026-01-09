import QtQuick

/**
 * DataLogger.qml - Data logging control and status display
 *
 * Features:
 * - Start/Stop logging control
 * - Log file management
 * - Recording status with elapsed time
 * - Sample rate display
 * - Storage space indicator
 * - Quick channel selection
 */
Item {
    id: root

    // Logging state
    property bool isLogging: false
    property int logStartTime: 0
    property int logDuration: 0          // ms
    property int sampleCount: 0
    property real sampleRate: 10         // Hz

    // Storage info
    property real storageUsedMB: 0
    property real storageTotalMB: 1024   // 1GB default
    property real storageAvailableMB: storageTotalMB - storageUsedMB

    // Log file info
    property string currentLogFile: ""
    property int logFileCount: 0
    property var recentLogs: []          // List of recent log files

    // Channel selection (what to log)
    property var availableChannels: [
        { name: "RPM", enabled: true, category: "engine" },
        { name: "MAP", enabled: true, category: "engine" },
        { name: "TPS", enabled: true, category: "engine" },
        { name: "CLT", enabled: true, category: "temp" },
        { name: "IAT", enabled: true, category: "temp" },
        { name: "AFR", enabled: true, category: "fuel" },
        { name: "Advance", enabled: true, category: "ignition" },
        { name: "InjDuty", enabled: true, category: "fuel" },
        { name: "Speed", enabled: true, category: "vehicle" },
        { name: "Gear", enabled: true, category: "vehicle" },
        { name: "Battery", enabled: false, category: "electrical" },
        { name: "OilPress", enabled: false, category: "engine" },
        { name: "OilTemp", enabled: false, category: "temp" },
        { name: "FuelPress", enabled: false, category: "fuel" },
        { name: "BoostPSI", enabled: false, category: "engine" },
        { name: "EGT", enabled: false, category: "temp" }
    ]

    property int enabledChannelCount: {
        var count = 0
        for (var i = 0; i < availableChannels.length; i++) {
            if (availableChannels[i].enabled) count++
        }
        return count
    }

    // Signals for external control
    signal startLogging()
    signal stopLogging()
    signal exportLog(string filename)

    function formatDuration(ms) {
        var totalSeconds = Math.floor(ms / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60
        return (hours > 0 ? hours + ":" : "") +
               (minutes < 10 && hours > 0 ? "0" : "") + minutes + ":" +
               (seconds < 10 ? "0" : "") + seconds
    }

    function formatFileSize(mb) {
        if (mb >= 1024) {
            return (mb / 1024).toFixed(2) + " GB"
        }
        return mb.toFixed(1) + " MB"
    }

    function generateLogFileName() {
        var now = new Date()
        var dateStr = now.getFullYear() + "-" +
                      String(now.getMonth() + 1).padStart(2, '0') + "-" +
                      String(now.getDate()).padStart(2, '0') + "_" +
                      String(now.getHours()).padStart(2, '0') + "-" +
                      String(now.getMinutes()).padStart(2, '0') + "-" +
                      String(now.getSeconds()).padStart(2, '0')
        return "speeduino_log_" + dateStr + ".csv"
    }

    function toggleLogging() {
        if (isLogging) {
            isLogging = false
            logDuration = Date.now() - logStartTime
            stopLogging()

            // Add to recent logs
            if (currentLogFile !== "") {
                recentLogs.unshift({
                    filename: currentLogFile,
                    duration: logDuration,
                    samples: sampleCount,
                    timestamp: Date.now()
                })
                if (recentLogs.length > 5) {
                    recentLogs.pop()
                }
                logFileCount++
            }
        } else {
            currentLogFile = generateLogFileName()
            logStartTime = Date.now()
            sampleCount = 0
            isLogging = true
            startLogging()
        }
    }

    function toggleChannel(index) {
        if (index >= 0 && index < availableChannels.length) {
            availableChannels[index].enabled = !availableChannels[index].enabled
            availableChannelsChanged()
        }
    }

    // Update timer for duration display
    Timer {
        interval: 100
        running: isLogging
        repeat: true
        onTriggered: {
            logDuration = Date.now() - logStartTime
            sampleCount = Math.floor(logDuration / 1000 * sampleRate)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 12
        border.color: isLogging ? "#FF5252" : "#333333"
        border.width: isLogging ? 2 : 1

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header
            Row {
                width: parent.width
                spacing: 8

                // Recording indicator
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: isLogging ? "#FF5252" : "#666666"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: isLogging
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1.0; duration: 500 }
                    }
                }

                Text {
                    text: "DATA LOGGER"
                    color: isLogging ? "#FF5252" : "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                Text {
                    text: isLogging ? "REC" : "IDLE"
                    color: isLogging ? "#FF5252" : Qt.rgba(1, 1, 1, 0.5)
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                    anchors.right: parent.right
                }
            }

            // Recording status
            Rectangle {
                width: parent.width
                height: 60
                radius: 8
                color: isLogging ? Qt.rgba(1, 0.3, 0.3, 0.2) : "#222222"

                Row {
                    anchors.fill: parent
                    anchors.margins: 12

                    // Duration
                    Column {
                        width: parent.width / 2
                        spacing: 4

                        Text {
                            text: "DURATION"
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 10
                        }

                        Text {
                            text: formatDuration(logDuration)
                            color: isLogging ? "#FF5252" : Qt.rgba(1, 1, 1, 0.87)
                            font.pixelSize: 24
                            font.bold: true
                            font.family: "Roboto Mono, monospace"
                        }
                    }

                    // Sample count
                    Column {
                        width: parent.width / 2
                        spacing: 4

                        Text {
                            text: "SAMPLES"
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 10
                        }

                        Text {
                            text: sampleCount.toLocaleString()
                            color: Qt.rgba(1, 1, 1, 0.87)
                            font.pixelSize: 16
                            font.family: "Roboto Mono, monospace"
                        }

                        Text {
                            text: sampleRate + " Hz"
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 10
                            font.family: "Roboto Mono, monospace"
                        }
                    }
                }
            }

            // Channel count and storage
            Row {
                width: parent.width
                spacing: 16

                Column {
                    width: (parent.width - 16) / 2
                    spacing: 2

                    Text {
                        text: "CHANNELS"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 9
                    }

                    Text {
                        text: enabledChannelCount + " / " + availableChannels.length
                        color: "#00E676"
                        font.pixelSize: 14
                        font.family: "Roboto Mono, monospace"
                    }
                }

                Column {
                    width: (parent.width - 16) / 2
                    spacing: 2

                    Text {
                        text: "STORAGE"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 9
                    }

                    Row {
                        spacing: 4

                        Text {
                            text: formatFileSize(storageAvailableMB)
                            color: storageAvailableMB < 100 ? "#FF5252" : "#00E676"
                            font.pixelSize: 14
                            font.family: "Roboto Mono, monospace"
                        }

                        Text {
                            text: "free"
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 10
                        }
                    }
                }
            }

            // Storage bar
            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: "#333333"

                Rectangle {
                    width: Math.min(1, storageUsedMB / storageTotalMB) * parent.width
                    height: parent.height
                    radius: 3
                    color: (storageUsedMB / storageTotalMB) > 0.9 ? "#FF5252" :
                           (storageUsedMB / storageTotalMB) > 0.7 ? "#FFC107" : "#00E676"
                }
            }

            // Current file
            Text {
                width: parent.width
                text: isLogging ? currentLogFile : (logFileCount + " logs recorded")
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 9
                font.family: "Roboto Mono, monospace"
                elide: Text.ElideMiddle
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#333333"
            }

            // Quick channel toggles (most important ones)
            Flow {
                width: parent.width
                spacing: 4

                Repeater {
                    model: availableChannels.slice(0, 8)  // First 8 channels

                    Rectangle {
                        width: 50
                        height: 24
                        radius: 4
                        color: modelData.enabled ? "#333333" : "#1a1a1a"
                        border.color: modelData.enabled ? "#00E676" : "#333333"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.name
                            color: modelData.enabled ? "#00E676" : Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 9
                            font.bold: modelData.enabled
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: !isLogging
                            onClicked: toggleChannel(index)
                        }
                    }
                }
            }

            // Control button
            Rectangle {
                width: parent.width
                height: 44
                radius: 8
                color: logButton.pressed ? (isLogging ? "#D32F2F" : "#00C853") :
                       (isLogging ? "#FF5252" : "#00E676")

                Row {
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 12
                        height: 12
                        radius: isLogging ? 2 : 6
                        color: isLogging ? "#FFFFFF" : "#000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: isLogging ? "STOP LOGGING" : "START LOGGING"
                        color: isLogging ? "#FFFFFF" : "#000000"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                MouseArea {
                    id: logButton
                    anchors.fill: parent
                    onClicked: toggleLogging()
                }
            }

            // Recent logs (if any)
            Column {
                width: parent.width
                spacing: 4
                visible: recentLogs.length > 0 && !isLogging

                Text {
                    text: "RECENT LOGS"
                    color: Qt.rgba(1, 1, 1, 0.5)
                    font.pixelSize: 9
                }

                Repeater {
                    model: recentLogs.slice(0, 3)

                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: modelData.filename
                            color: Qt.rgba(1, 1, 1, 0.72)
                            font.pixelSize: 10
                            font.family: "Roboto Mono, monospace"
                            width: parent.width - 80
                            elide: Text.ElideMiddle
                        }

                        Text {
                            text: formatDuration(modelData.duration)
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 10
                            font.family: "Roboto Mono, monospace"
                        }
                    }
                }
            }
        }
    }
}
