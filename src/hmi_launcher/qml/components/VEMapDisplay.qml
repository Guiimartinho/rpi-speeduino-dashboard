import QtQuick

/**
 * VEMapDisplay.qml - Real-time VE/Advance/AFR map visualization
 *
 * Features:
 * - 2D table view with current cell highlighted
 * - Configurable map type (VE, Advance, AFR Target)
 * - Cell history tracking (visited cells)
 * - Color gradient based on values
 * - Auto-scrolling to follow current cell
 */
Item {
    id: root

    // Current operating point from ECU
    property real rpm: 0
    property real mapKpa: 0
    property real tps: 0

    // Map type: "VE", "ADVANCE", "AFR"
    property string mapType: "VE"

    // Map data (16x16 for Speeduino)
    property var mapData: generateDemoMap()
    property var rpmBins: [500, 700, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500]
    property var loadBins: [20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170]

    // Current cell indices
    property int currentRpmIdx: 0
    property int currentLoadIdx: 0

    // Cell history (visited cells with fade effect)
    property var cellHistory: ({})
    property int maxHistoryAge: 50

    // Display options
    property bool showValues: true
    property bool showHistory: true
    property int visibleRows: 8
    property int visibleCols: 8

    // Signal for map data updates from ECU
    signal mapDataReceived(var data)
    signal cellChanged(int rpmIdx, int loadIdx)

    // Calculate current cell based on RPM and load
    function updateCurrentCell() {
        var prevRpmIdx = currentRpmIdx
        var prevLoadIdx = currentLoadIdx

        // Find RPM bin
        currentRpmIdx = 0
        for (var i = 0; i < rpmBins.length - 1; i++) {
            if (rpm >= rpmBins[i]) currentRpmIdx = i
        }

        // Find load bin (using MAP for VE, or TPS for other maps)
        var load = mapType === "VE" ? mapKpa : tps
        currentLoadIdx = 0
        for (var j = 0; j < loadBins.length - 1; j++) {
            if (load >= loadBins[j]) currentLoadIdx = j
        }

        // Record in history
        var key = currentRpmIdx + "," + currentLoadIdx
        cellHistory[key] = maxHistoryAge

        // Age all history entries
        for (var k in cellHistory) {
            if (cellHistory.hasOwnProperty(k)) {
                cellHistory[k]--
                if (cellHistory[k] <= 0) {
                    delete cellHistory[k]
                }
            }
        }

        // Emit signal if cell changed
        if (prevRpmIdx !== currentRpmIdx || prevLoadIdx !== currentLoadIdx) {
            cellChanged(currentRpmIdx, currentLoadIdx)
        }
    }

    // Generate demo map data for preview
    function generateDemoMap() {
        var data = []
        for (var load = 0; load < 16; load++) {
            var row = []
            for (var rpmIdx = 0; rpmIdx < 16; rpmIdx++) {
                var value
                if (mapType === "VE") {
                    value = 30 + (load * 3) + (rpmIdx * 2) + Math.random() * 5
                    value = Math.min(100, Math.max(20, value))
                } else if (mapType === "ADVANCE") {
                    value = 10 + (rpmIdx * 2) - (load * 0.3) + Math.random() * 3
                    value = Math.min(45, Math.max(5, value))
                } else {
                    value = 14.7 - (load * 0.02) - (rpmIdx * 0.1) + Math.random() * 0.3
                    value = Math.min(16, Math.max(10, value))
                }
                row.push(value.toFixed(mapType === "AFR" ? 1 : 0))
            }
            data.push(row)
        }
        return data
    }

    // Update map data from external source
    function setMapData(data) {
        mapData = data
    }

    // Get color for cell value
    function getValueColor(value, row, col) {
        var key = col + "," + row
        var isCurrent = (col === currentRpmIdx && row === currentLoadIdx)
        var historyAge = cellHistory[key] || 0

        if (isCurrent) {
            return "#FFFFFF"
        }

        if (showHistory && historyAge > 0) {
            var alpha = historyAge / maxHistoryAge
            return Qt.rgba(0, 0.9, 0.47, alpha * 0.6)
        }

        // Color based on value
        var numValue = parseFloat(value)
        var normalized

        if (mapType === "VE") {
            normalized = (numValue - 20) / 80  // 20-100 range
        } else if (mapType === "ADVANCE") {
            normalized = (numValue - 5) / 40   // 5-45 range
        } else {
            normalized = (numValue - 10) / 6   // 10-16 range, inverted
            normalized = 1 - normalized
        }

        normalized = Math.max(0, Math.min(1, normalized))

        // Blue (low) -> Green (mid) -> Red (high)
        if (normalized < 0.5) {
            var t = normalized * 2
            return Qt.rgba(0, t * 0.8, 1 - t, 0.7)
        } else {
            var t2 = (normalized - 0.5) * 2
            return Qt.rgba(t2, 0.8 * (1 - t2), 0, 0.7)
        }
    }

    // Get text color for cell
    function getCellTextColor(row, col) {
        if (col === currentRpmIdx && row === currentLoadIdx) {
            return "#000000"
        }
        return Qt.rgba(1, 1, 1, 0.87)
    }

    // Get current cell value
    function getCurrentValue() {
        if (mapData[currentLoadIdx] && mapData[currentLoadIdx][currentRpmIdx]) {
            return mapData[currentLoadIdx][currentRpmIdx]
        }
        return "--"
    }

    // Update timer
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: updateCurrentCell()
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
            anchors.margins: 8
            spacing: 4

            // Header
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: mapType + " MAP"
                    color: mapType === "VE" ? "#00E676" : (mapType === "ADVANCE" ? "#FFC107" : "#2196F3")
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                }

                Item { width: 10; height: 1 }

                // Map type selector
                Row {
                    spacing: 4
                    anchors.right: parent.right

                    Repeater {
                        model: ["VE", "ADV", "AFR"]

                        Rectangle {
                            width: 36
                            height: 20
                            radius: 3
                            color: {
                                var type = modelData === "ADV" ? "ADVANCE" : modelData
                                return mapType === type ? "#333333" : "transparent"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: {
                                    var type = modelData === "ADV" ? "ADVANCE" : modelData
                                    return mapType === type ? Qt.rgba(1, 1, 1, 0.96) : Qt.rgba(1, 1, 1, 0.56)
                                }
                                font.pixelSize: 9
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    mapType = modelData === "ADV" ? "ADVANCE" : modelData
                                    mapData = generateDemoMap()
                                }
                            }
                        }
                    }
                }
            }

            // Current values display
            Row {
                width: parent.width
                spacing: 16

                Text {
                    text: "RPM: " + rpm.toFixed(0)
                    color: Qt.rgba(1, 1, 1, 0.72)
                    font.pixelSize: 10
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: (mapType === "VE" ? "MAP: " + mapKpa.toFixed(0) + " kPa" : "TPS: " + tps.toFixed(0) + "%")
                    color: Qt.rgba(1, 1, 1, 0.72)
                    font.pixelSize: 10
                    font.family: "Roboto Mono, monospace"
                }

                Text {
                    text: mapType + ": " + getCurrentValue()
                    color: "#00E676"
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Roboto Mono, monospace"
                    anchors.right: parent.right
                }
            }

            // Map grid container
            Item {
                id: mapContainer
                width: parent.width
                height: parent.height - 50

                // Calculate visible window centered on current cell
                property int startRow: Math.max(0, Math.min(16 - visibleRows, currentLoadIdx - Math.floor(visibleRows / 2)))
                property int startCol: Math.max(0, Math.min(16 - visibleCols, currentRpmIdx - Math.floor(visibleCols / 2)))

                // Load axis labels (left)
                Column {
                    id: loadLabels
                    anchors.left: parent.left
                    anchors.top: rpmLabels.bottom
                    anchors.topMargin: 2
                    width: 30

                    Repeater {
                        model: visibleRows

                        Text {
                            width: 30
                            height: (mapContainer.height - 16) / visibleRows
                            text: loadBins[mapContainer.startRow + index]
                            color: (mapContainer.startRow + index) === currentLoadIdx ? "#00E676" : Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 8
                            font.family: "Roboto Mono, monospace"
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                // RPM axis labels (top)
                Row {
                    id: rpmLabels
                    anchors.left: loadLabels.right
                    anchors.leftMargin: 4
                    anchors.top: parent.top
                    height: 14

                    Repeater {
                        model: visibleCols

                        Text {
                            width: (mapContainer.width - 34) / visibleCols
                            height: 14
                            text: (rpmBins[mapContainer.startCol + index] / 1000).toFixed(1)
                            color: (mapContainer.startCol + index) === currentRpmIdx ? "#00E676" : Qt.rgba(1, 1, 1, 0.56)
                            font.pixelSize: 8
                            font.family: "Roboto Mono, monospace"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                // Map cells
                Grid {
                    id: mapGrid
                    anchors.left: loadLabels.right
                    anchors.leftMargin: 4
                    anchors.top: rpmLabels.bottom
                    anchors.topMargin: 2
                    columns: visibleCols
                    rows: visibleRows

                    Repeater {
                        model: visibleRows * visibleCols

                        Rectangle {
                            property int row: mapContainer.startRow + Math.floor(index / visibleCols)
                            property int col: mapContainer.startCol + (index % visibleCols)
                            property string cellValue: mapData[row] ? mapData[row][col] : ""

                            width: (mapContainer.width - 34) / visibleCols
                            height: (mapContainer.height - 16) / visibleRows
                            color: getValueColor(cellValue, row, col)
                            border.color: (col === currentRpmIdx && row === currentLoadIdx) ? "#FFFFFF" : "#222222"
                            border.width: (col === currentRpmIdx && row === currentLoadIdx) ? 2 : 1

                            Text {
                                anchors.centerIn: parent
                                text: showValues ? cellValue : ""
                                color: getCellTextColor(row, col)
                                font.pixelSize: Math.min(parent.width / 3, 10)
                                font.bold: col === currentRpmIdx && row === currentLoadIdx
                                font.family: "Roboto Mono, monospace"
                            }
                        }
                    }
                }
            }
        }
    }
}
