# ZMQ Messages API Reference

This document describes the ZeroMQ IPC message protocol used for inter-service
communication in the Speeduino UI system.

## Table of Contents

- [Overview](#overview)
- [Endpoints](#endpoints)
- [Topics](#topics)
- [Message Structures](#message-structures)
  - [EngineData](#enginedata)
  - [ReverseEvent](#reverseevent)
  - [SteeringEvent](#steeringevent)
  - [CanCommand](#cancommand)
  - [CanCommandResponse](#cancommandresponse)
  - [SystemStatus](#systemstatus)
- [Serialization](#serialization)
- [Usage Examples](#usage-examples)

---

## Overview

The Speeduino UI system uses ZeroMQ with IPC (Inter-Process Communication)
transport for low-latency communication between services:

```
+---------------+     PUB/SUB      +----------------+
|  can_service  |----------------->|  hmi_launcher  |
+---------------+                  +----------------+
                                          |
+-------------------+    PUB/SUB          |
|  reverse_service  |------------------->-+
+-------------------+
```

**Communication Patterns:**
- **PUB/SUB**: One-to-many broadcast (engine data, reverse events, steering)
- **REQ/REP**: Request-response for CAN commands

**Serialization Format:** MessagePack (binary, compact, cross-platform)

---

## Endpoints

### Default IPC Endpoints

| Endpoint | Address | Pattern | Description |
|----------|---------|---------|-------------|
| `ENGINE_DATA` | `ipc:///tmp/speeduino_data.ipc` | PUB/SUB | Engine telemetry at 50Hz |
| `CAN_COMMAND` | `ipc:///tmp/speeduino_cmd.ipc` | REQ/REP | CAN frame transmission |
| `REVERSE_TRIGGER` | `ipc:///tmp/reverse_trigger.ipc` | PUB/SUB | Reverse gear events |
| `STEERING_EVENTS` | `ipc:///tmp/steering_events.ipc` | PUB/SUB | Steering wheel buttons |

### Configuring Endpoints

Endpoints can be configured at runtime for testing or custom deployments:

```cpp
#include "common/zmq_messages.hpp"

// Configure custom endpoints
speeduino::ZmqEndpointsConfig config;
config.engine_data = "ipc:///tmp/test_engine.ipc";
config.reverse_trigger = "ipc:///tmp/test_reverse.ipc";
speeduino::ZmqEndpoints::configure(config);

// Use configured endpoints
auto& ep = speeduino::ZmqEndpoints::instance();
socket.bind(ep.engineData());

// Reset to defaults
speeduino::ZmqEndpoints::reset();
```

### Legacy Access (Deprecated)

For backward compatibility, static constants are available:

```cpp
// DEPRECATED - use ZmqEndpoints::instance() instead
const char* addr = speeduino::endpoints::ENGINE_DATA;

// PREFERRED - use helper functions
const std::string& addr = speeduino::endpoints::getEngineData();
```

---

## Topics

ZMQ PUB/SUB uses topic filtering. Each message is prefixed with a topic string.

| Topic | Constant | Description |
|-------|----------|-------------|
| `ENGINE` | `topics::ENGINE` | Engine telemetry data |
| `REVERSE` | `topics::REVERSE` | Reverse gear state changes |
| `STEERING` | `topics::STEERING` | Steering wheel button events |
| `STATUS` | `topics::STATUS` | System status updates |

**Message Format:** `[topic][data]` (two-part ZMQ message)

---

## Message Structures

### EngineData

Real-time engine telemetry published at 50Hz from `can_service`.

```cpp
struct EngineData {
    uint32_t timestamp_ms;      // Monotonic timestamp (milliseconds)
    uint16_t rpm;               // Engine RPM (0-65535)
    int8_t   coolant_temp;      // Coolant temperature (Celsius, -128 to +127)
    int8_t   intake_temp;       // Intake air temperature (Celsius)
    uint8_t  tps;               // Throttle position (0-100%)
    uint16_t map_kpa;           // Manifold pressure (kPa x 10)
    uint16_t lambda;            // Lambda (x 1000, 1000 = stoichiometric)
    int16_t  ignition_advance;  // Ignition advance (degrees x 10)
    uint8_t  injector_duty;     // Injector duty cycle (0-100%)
    uint8_t  gear;              // Current gear (0=N, 1-6)
    uint16_t vehicle_speed;     // Speed (km/h x 10)
    uint16_t fuel_pressure;     // Fuel pressure (kPa)
    uint16_t oil_pressure;      // Oil pressure (kPa)
    int8_t   oil_temp;          // Oil temperature (Celsius)
    uint16_t battery_voltage;   // Battery voltage (millivolts)
    uint8_t  flags;             // Status flags (bitfield)
};
```

#### Status Flags

| Flag | Value | Description |
|------|-------|-------------|
| `FLAG_CEL_ON` | `0x01` | Check Engine Light active |
| `FLAG_OVERHEAT` | `0x02` | Coolant temperature critical (>105°C) |
| `FLAG_CAN_OK` | `0x04` | CAN bus communication healthy |
| `FLAG_ENGINE_RUN` | `0x08` | Engine is running (RPM > 0) |
| `FLAG_CLUTCH_IN` | `0x10` | Clutch pedal pressed |
| `FLAG_BRAKE_ON` | `0x20` | Brake pedal pressed |

#### Helper Methods

```cpp
bool isCelOn() const;        // Check if CEL is on
bool isOverheat() const;     // Check if overheating
bool isCanOk() const;        // Check CAN status
bool isEngineRunning() const; // Check if engine running
```

#### Scaling Factors

| Field | Scale | Example |
|-------|-------|---------|
| `map_kpa` | ÷10 | `1013` → 101.3 kPa |
| `lambda` | ÷1000 | `1000` → 1.000 λ |
| `ignition_advance` | ÷10 | `150` → 15.0° |
| `vehicle_speed` | ÷10 | `1205` → 120.5 km/h |
| `battery_voltage` | ÷1000 | `14200` → 14.2V |

---

### ReverseEvent

Published when reverse gear state changes.

```cpp
struct ReverseEvent {
    uint32_t timestamp_ms;  // Monotonic timestamp
    bool     engaged;       // true = reverse engaged, false = disengaged
    uint8_t  source;        // Detection source
};
```

#### Source Values

| Value | Constant | Description |
|-------|----------|-------------|
| `0` | - | CAN bus detection (Speeduino/ECU) |
| `1` | - | GPIO detection (hardware switch) |

---

### SteeringEvent

Published when a steering wheel button is pressed or released.

```cpp
struct SteeringEvent {
    uint32_t timestamp_ms;  // Monotonic timestamp
    uint8_t  button_id;     // Button identifier
    bool     pressed;       // true = pressed, false = released
};
```

#### Button IDs

| ID | Constant | Description |
|----|----------|-------------|
| `0x01` | `BTN_VOLUME_UP` | Volume up |
| `0x02` | `BTN_VOLUME_DOWN` | Volume down |
| `0x03` | `BTN_NEXT_TRACK` | Next track |
| `0x04` | `BTN_PREV_TRACK` | Previous track |
| `0x05` | `BTN_MUTE` | Mute toggle |
| `0x06` | `BTN_MODE` | Mode/source switch |
| `0x07` | `BTN_PHONE` | Phone answer/hangup |
| `0x08` | `BTN_VOICE` | Voice assistant |

---

### CanCommand

Request to transmit a CAN frame (sent via REQ socket).

```cpp
struct CanCommand {
    uint32_t can_id;              // CAN identifier (11-bit or 29-bit)
    std::array<uint8_t, 8> data;  // CAN data payload
    uint8_t  dlc;                 // Data length code (0-8)
};
```

**Note:** Only whitelisted CAN IDs can be transmitted for safety.

---

### CanCommandResponse

Response to a CAN command request (received via REP socket).

```cpp
struct CanCommandResponse {
    bool        success;        // Command execution result
    uint8_t     error_code;     // Error code (0 = success)
    std::string error_message;  // Human-readable error description
};
```

#### Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| `0x00` | `ERR_OK` | Success |
| `0x01` | `ERR_BLOCKED` | CAN ID not in whitelist |
| `0x02` | `ERR_RATE_LIMIT` | Rate limit exceeded |
| `0x03` | `ERR_CAN_WRITE` | CAN bus write failed |
| `0x04` | `ERR_INVALID` | Invalid command parameters |

---

### SystemStatus

Periodic system health status.

```cpp
struct SystemStatus {
    uint32_t timestamp_ms;      // Monotonic timestamp
    bool     can_connected;     // CAN bus connection status
    bool     camera_ready;      // Reverse camera available
    bool     openauto_running;  // OpenAuto process running
    uint32_t uptime_seconds;    // System uptime
    float    cpu_temp;          // CPU temperature (Celsius)
};
```

---

## Serialization

All messages use **MessagePack** for binary serialization.

### C++ (Publisher)

```cpp
#include <msgpack.hpp>
#include "common/zmq_messages.hpp"

speeduino::EngineData data;
data.rpm = 3500;
data.coolant_temp = 85;
// ... fill other fields

// Serialize
msgpack::sbuffer buffer;
msgpack::pack(buffer, data);

// Send via ZMQ
zmq::message_t topic(speeduino::topics::ENGINE, strlen(speeduino::topics::ENGINE));
zmq::message_t msg(buffer.data(), buffer.size());

socket.send(topic, zmq::send_flags::sndmore);
socket.send(msg, zmq::send_flags::none);
```

### C++ (Subscriber)

```cpp
zmq::message_t topic, data;
socket.recv(topic, zmq::recv_flags::none);
socket.recv(data, zmq::recv_flags::none);

// Deserialize
auto oh = msgpack::unpack(
    static_cast<const char*>(data.data()),
    data.size()
);

speeduino::EngineData engineData;
oh.get().convert(engineData);

qDebug() << "RPM:" << engineData.rpm;
```

### Python (Subscriber)

```python
import zmq
import msgpack

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("ipc:///tmp/speeduino_data.ipc")
socket.setsockopt_string(zmq.SUBSCRIBE, "ENGINE")

while True:
    topic = socket.recv_string()
    data = socket.recv()

    msg = msgpack.unpackb(data, raw=False)
    print(f"RPM: {msg[1]}, Coolant: {msg[2]}°C")
```

---

## Usage Examples

### Subscribing to Engine Data (Qt/C++)

```cpp
#include <zmq.hpp>
#include <msgpack.hpp>
#include "common/zmq_messages.hpp"

class EngineDataSubscriber : public QObject {
    Q_OBJECT
public:
    void start() {
        m_context = std::make_unique<zmq::context_t>(1);
        m_socket = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::sub);

        auto& ep = speeduino::ZmqEndpoints::instance();
        m_socket->connect(ep.engineData());
        m_socket->set(zmq::sockopt::subscribe, speeduino::topics::ENGINE);

        // Start polling in thread...
    }

signals:
    void dataReceived(const speeduino::EngineData& data);
};
```

### Sending CAN Commands

```cpp
#include <zmq.hpp>
#include "common/zmq_messages.hpp"

void sendCanCommand(uint32_t canId, const uint8_t* data, uint8_t len) {
    zmq::context_t ctx(1);
    zmq::socket_t socket(ctx, zmq::socket_type::req);

    auto& ep = speeduino::ZmqEndpoints::instance();
    socket.connect(ep.canCommand());

    speeduino::CanCommand cmd;
    cmd.can_id = canId;
    cmd.dlc = len;
    std::copy(data, data + len, cmd.data.begin());

    // Serialize and send
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, cmd);
    socket.send(zmq::buffer(buffer.data(), buffer.size()));

    // Receive response
    zmq::message_t reply;
    socket.recv(reply);

    auto oh = msgpack::unpack(
        static_cast<const char*>(reply.data()),
        reply.size()
    );
    speeduino::CanCommandResponse response;
    oh.get().convert(response);

    if (!response.success) {
        qWarning() << "CAN command failed:" << response.error_message.c_str();
    }
}
```

### Monitoring Reverse Gear

```cpp
void onReverseEvent(const speeduino::ReverseEvent& event) {
    if (event.engaged) {
        qInfo() << "Reverse engaged via"
                << (event.source == 0 ? "CAN" : "GPIO");
        activateRearCamera();
    } else {
        qInfo() << "Reverse disengaged";
        deactivateRearCamera();
    }
}
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.3.0 | 2024 | Added configurable endpoints via `ZmqEndpoints` |
| 0.2.0 | 2024 | Added `SystemStatus`, steering events |
| 0.1.0 | 2024 | Initial release with `EngineData`, `ReverseEvent` |

---

**Compatibility:** Speeduino UI v0.3.0+
**Protocol:** ZeroMQ 4.x with MessagePack serialization
