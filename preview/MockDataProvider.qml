import QtQuick

/**
 * MockDataProvider.qml
 * Simulates the C++ DataProvider for UI preview on Windows
 *
 * Provides realistic animated engine data for testing the UI
 * without needing the actual CAN bus connection.
 */
QtObject {
    id: mockProvider

    // ═══════════════════════════════════════════════════════════════
    // ENGINE DATA PROPERTIES (mirrors C++ DataProvider)
    // ═══════════════════════════════════════════════════════════════

    property real rpm: 850
    property real coolantTemp: 85
    property real intakeTemp: 35
    property real tps: 0
    property real mapKpa: 35
    property real lambda: 1.0
    property real ignitionAdvance: 15
    property real injectorDuty: 5
    property real vehicleSpeed: 0
    property int gear: 0

    // Status flags
    property bool canConnected: true
    property bool celOn: false
    property bool overheat: false
    property bool reverseEngaged: false

    // CAN statistics
    property int canRxCount: 0
    property int canTxCount: 0
    property int canErrorCount: 0

    // ═══════════════════════════════════════════════════════════════
    // SIMULATION STATE
    // ═══════════════════════════════════════════════════════════════

    property bool simulationRunning: true
    property string simulationMode: "idle"  // idle, driving, revving, overheat_test

    // Internal state for simulation
    property real targetRpm: 850
    property real targetSpeed: 0
    property real targetTps: 0
    property real throttleResponse: 0.1
    property real engineInertia: 0.05

    // ═══════════════════════════════════════════════════════════════
    // SIMULATION TIMER
    // ═══════════════════════════════════════════════════════════════

    property Timer simulationTimer: Timer {
        interval: 50  // 20 Hz update rate
        running: mockProvider.simulationRunning
        repeat: true
        onTriggered: mockProvider.updateSimulation()
    }

    // ═══════════════════════════════════════════════════════════════
    // SIMULATION LOGIC
    // ═══════════════════════════════════════════════════════════════

    function updateSimulation() {
        // Increment CAN counters
        canRxCount += Math.floor(Math.random() * 10) + 5
        canTxCount += Math.floor(Math.random() * 3)

        // Smooth RPM changes
        var rpmDiff = targetRpm - rpm
        rpm += rpmDiff * engineInertia
        rpm += (Math.random() - 0.5) * 20  // Add some jitter

        // Clamp RPM
        rpm = Math.max(0, Math.min(8000, rpm))

        // TPS follows target with lag
        var tpsDiff = targetTps - tps
        tps += tpsDiff * throttleResponse
        tps = Math.max(0, Math.min(100, tps))

        // Speed based on RPM and gear
        if (gear > 0 && rpm > 1000) {
            var gearRatios = [0, 3.5, 2.1, 1.4, 1.0, 0.8, 0.65]
            var finalDrive = 4.1
            var tireCircumference = 2.0  // meters
            var speedMps = (rpm / 60) * tireCircumference / (gearRatios[gear] * finalDrive)
            targetSpeed = speedMps * 3.6  // Convert to km/h
        } else {
            targetSpeed = 0
        }

        var speedDiff = targetSpeed - vehicleSpeed
        vehicleSpeed += speedDiff * 0.1
        vehicleSpeed = Math.max(0, vehicleSpeed)

        // MAP based on TPS and RPM
        var baseMap = 30 + (tps * 0.7) + (rpm / 8000 * 20)
        mapKpa = baseMap + (Math.random() - 0.5) * 5

        // Lambda based on load
        var load = (tps / 100) * (rpm / 8000)
        if (load > 0.7) {
            lambda = 0.85 + Math.random() * 0.05  // Rich under load
        } else if (load < 0.2) {
            lambda = 1.0 + Math.random() * 0.05   // Stoich at idle
        } else {
            lambda = 0.95 + Math.random() * 0.1
        }

        // Ignition advance based on RPM and load
        var baseAdvance = 10 + (rpm / 8000) * 25 - (load * 10)
        ignitionAdvance = baseAdvance + (Math.random() - 0.5) * 2

        // Injector duty based on RPM and load
        injectorDuty = 5 + (rpm / 8000) * 30 + (load * 50)
        injectorDuty = Math.min(95, injectorDuty)

        // Temperature simulation
        if (simulationMode === "overheat_test") {
            coolantTemp += 0.5
            if (coolantTemp > 110) {
                overheat = true
            }
        } else {
            // Normal temperature - slowly approach 90°C
            var tempDiff = 90 - coolantTemp
            coolantTemp += tempDiff * 0.001
            coolantTemp += (Math.random() - 0.5) * 0.5
            overheat = coolantTemp > 105
        }

        // Intake temp varies with speed (air cooling)
        intakeTemp = 30 + (100 - vehicleSpeed) * 0.1 + (Math.random() - 0.5) * 2
    }

    // ═══════════════════════════════════════════════════════════════
    // SIMULATION CONTROL FUNCTIONS
    // ═══════════════════════════════════════════════════════════════

    function setIdle() {
        simulationMode = "idle"
        targetRpm = 850
        targetTps = 0
        gear = 0
    }

    function rev(targetRpmValue) {
        simulationMode = "revving"
        targetRpm = targetRpmValue || 5000
        targetTps = 80
        gear = 0
    }

    function drive(gearNum, throttle) {
        simulationMode = "driving"
        gear = gearNum || 3
        targetTps = throttle || 30
        targetRpm = 2000 + (throttle * 40)
    }

    function testOverheat() {
        simulationMode = "overheat_test"
        coolantTemp = 95
    }

    function toggleCel() {
        celOn = !celOn
    }

    function toggleReverse() {
        reverseEngaged = !reverseEngaged
    }

    function toggleCanConnection() {
        canConnected = !canConnected
        if (!canConnected) {
            canErrorCount += 10
        }
    }

    function reset() {
        simulationMode = "idle"
        rpm = 850
        coolantTemp = 85
        intakeTemp = 35
        tps = 0
        mapKpa = 35
        lambda = 1.0
        vehicleSpeed = 0
        gear = 0
        celOn = false
        overheat = false
        reverseEngaged = false
        canConnected = true
        targetRpm = 850
        targetTps = 0
        targetSpeed = 0
    }

    Component.onCompleted: {
        console.log("MockDataProvider: Simulation started")
    }
}
