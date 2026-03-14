# System Architecture

**Version:** 1.0
**Last Updated:** January 2026
**ISO 26262 ASIL-B Compliant**

This document describes the system architecture of the Speeduino UI project, covering both the macro (high-level) and detailed (component-level) views.

---

## Table of Contents

1. [Macro Architecture](#1-macro-architecture)
   - [System Overview](#11-system-overview)
   - [Service Layer](#12-service-layer)
   - [Data Flow](#13-data-flow)
   - [Hardware Integration](#14-hardware-integration)
2. [Detailed Architecture](#2-detailed-architecture)
   - [can_service](#21-can_service)
   - [reverse_service](#22-reverse_service)
   - [hmi_launcher](#23-hmi_launcher)
   - [common Library](#24-common-library)
3. [Communication Protocols](#3-communication-protocols)
   - [ZeroMQ IPC](#31-zeromq-ipc)
   - [CAN Bus Protocols](#32-can-bus-protocols)
4. [Threading Model](#4-threading-model)
5. [OpenAuto Integration](#5-openauto-integration)
6. [Deployment Architecture](#6-deployment-architecture)

---

## 1. Macro Architecture

### 1.1 System Overview

The Speeduino UI is a real-time automotive dashboard system designed for Raspberry Pi 4. It provides engine telemetry visualization, Android Auto integration via OpenAuto, and reverse camera functionality.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SPEEDUINO UI SYSTEM                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        APPLICATION LAYER                             │   │
│  │  ┌─────────────────────────────────────────────────────────────┐    │   │
│  │  │                    hmi_launcher (Qt6/QML)                    │    │   │
│  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐   │    │   │
│  │  │  │Dashboard │ │  Home    │ │ Config   │ │  OpenAuto    │   │    │   │
│  │  │  │ Screens  │ │  Screen  │ │ Screen   │ │  Screen      │   │    │   │
│  │  │  └──────────┘ └──────────┘ └──────────┘ └──────────────┘   │    │   │
│  │  └─────────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                              ZeroMQ (IPC)                                   │
│                                    │                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         SERVICE LAYER                                │   │
│  │  ┌──────────────────┐  ┌──────────────────┐                         │   │
│  │  │   can_service    │  │  reverse_service │                         │   │
│  │  │   (CAN Reader)   │  │  (Reverse Gear)  │                         │   │
│  │  └────────┬─────────┘  └────────┬─────────┘                         │   │
│  │           │                      │                                   │   │
│  │           └──────────┬───────────┘                                   │   │
│  └──────────────────────│───────────────────────────────────────────────┘   │
│                         │                                                   │
│  ┌──────────────────────│───────────────────────────────────────────────┐   │
│  │                 HARDWARE ABSTRACTION LAYER                           │   │
│  │                         │                                            │   │
│  │            ┌────────────┴────────────┐                               │   │
│  │            │    SocketCAN (can0)     │                               │   │
│  │            └────────────┬────────────┘                               │   │
│  │                         │                                            │   │
│  │            ┌────────────┴────────────┐                               │   │
│  │            │    MCP2515/MCP2518FD    │                               │   │
│  │            │        (SPI Bus)        │                               │   │
│  │            └────────────┬────────────┘                               │   │
│  └──────────────────────────│───────────────────────────────────────────┘   │
│                              │                                              │
└──────────────────────────────│──────────────────────────────────────────────┘
                               │
                        CAN Bus (500kbps)
                               │
                    ┌──────────┴──────────┐
                    │      Speeduino      │
                    │    (STM32F407VE)    │
                    └─────────────────────┘
```

### 1.2 Service Layer

The system follows a microservices architecture with three main services:

| Service | Binary | Purpose | IPC Endpoints |
|---------|--------|---------|---------------|
| `can_service` | `can_service` | Read CAN frames, parse engine data, publish to ZMQ | PUB: `/tmp/speeduino_data.ipc`<br>REP: `/tmp/speeduino_cmd.ipc` |
| `reverse_service` | `reverse_service` | Detect reverse gear via CAN/GPIO, notify HMI | PUB: `/tmp/reverse_trigger.ipc` |
| `hmi_launcher` | `hmi_launcher` | Qt6/QML UI, OpenAuto integration | SUB: all above endpoints |

### 1.3 Data Flow

```
┌──────────────┐    CAN      ┌──────────────┐    ZMQ PUB     ┌──────────────┐
│   Speeduino  │────────────►│ can_service  │───────────────►│ hmi_launcher │
│     ECU      │   500kbps   │   (Parser)   │   ENGINE.*     │   (Qt6/QML)  │
└──────────────┘             └──────────────┘                └──────────────┘
                                   │
                                   │ ZMQ REP
                                   ▼
                             ┌──────────────┐
                             │   Commands   │
                             │  (Whitelist) │
                             └──────────────┘

┌──────────────┐    CAN/GPIO ┌──────────────┐    ZMQ PUB     ┌──────────────┐
│ Reverse Gear │────────────►│reverse_service───────────────►│ hmi_launcher │
│   Signal     │             │  (Detector)  │   REVERSE.*    │(Camera View) │
└──────────────┘             └──────────────┘                └──────────────┘
```

**Message Topics:**

| Topic | Publisher | Content |
|-------|-----------|---------|
| `ENGINE.DATA` | can_service | Serialized `EngineData` struct (RPM, CLT, TPS, MAP, etc.) |
| `ENGINE.STATUS` | can_service | Connection status, error flags |
| `REVERSE.STATE` | reverse_service | Boolean engaged/disengaged |
| `STEERING.EVENT` | can_service | Steering wheel button events |

### 1.4 Hardware Integration

```
┌─────────────────────────────────────────────────────────────┐
│                    Raspberry Pi 4                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  GPIO Pinout:                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  SPI0 (CAN Module):                                  │   │
│  │    GPIO8  (CE0)  → MCP2515 CS                        │   │
│  │    GPIO9  (MISO) → MCP2515 SO                        │   │
│  │    GPIO10 (MOSI) → MCP2515 SI                        │   │
│  │    GPIO11 (SCLK) → MCP2515 SCK                       │   │
│  │    GPIO25       → MCP2515 INT                        │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  Optional GPIO:                                       │   │
│  │    GPIO17 → Reverse Gear (manual trigger)            │   │
│  │    GPIO27 → CEL Warning LED                          │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  Peripherals:                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  USB:                                                │   │
│  │    Port A → Android Phone (Android Auto)             │   │
│  │    Port B → UVC Reverse Camera                       │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  Display:                                            │   │
│  │    DSI/HDMI → 5"/7" Touchscreen (800x480)            │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Detailed Architecture

### 2.1 can_service

The CAN service reads raw CAN frames from SocketCAN, parses them according to configured protocols, and publishes engine data via ZeroMQ.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           can_service                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐   │
│  │   CanInterface  │────►│    CanParser    │────►│   ZmqPublisher  │   │
│  │  (SocketCAN)    │     │  (Protocol Cfg) │     │   (PUB Socket)  │   │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘   │
│           │                      │                        │             │
│           │              ┌───────┴───────┐                │             │
│           │              │  ConfigLoader │                │             │
│           │              │ (YAML Parser) │                │             │
│           │              └───────────────┘                │             │
│           │                                               │             │
│           │     ┌─────────────────┐     ┌─────────────────┐             │
│           └────►│ CanErrorHandler │     │   CanWriter     │◄───────────┘│
│                 │ (Bus Recovery)  │     │ (TX Commands)   │  CMD Socket │
│                 └─────────────────┘     └─────────────────┘             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Key Classes:**

| Class | File | Responsibility |
|-------|------|----------------|
| `CanInterface` | `can_interface.cpp` | Raw CAN frame read/write via SocketCAN |
| `CanParser` | `can_parser.cpp` | Protocol-specific frame decoding (Haltech, BMW, VAG, OBD-II) |
| `CanWriter` | `can_writer.cpp` | Transmit CAN frames with rate limiting |
| `CanErrorHandler` | `can_error_handler.cpp` | Bus-off recovery, error statistics |
| `ZmqPublisher` | `zmq_publisher.cpp` | Serialize and publish `EngineData` |
| `ConfigLoader` | `config_loader.cpp` | Parse YAML configuration files |

**Thread Model:**

```
┌─────────────────────────────────────────────┐
│              can_service Process            │
├─────────────────────────────────────────────┤
│  Main Thread:                               │
│    - Configuration loading                  │
│    - Service initialization                 │
│    - Signal handling (SIGTERM, SIGINT)      │
│                                             │
│  CAN Read Thread:                           │
│    - Blocking read from SocketCAN           │
│    - Frame parsing (signal extraction)      │
│    - Rate-limited ZMQ publish               │
│                                             │
│  Command Thread (optional):                 │
│    - ZMQ REP socket for commands            │
│    - Whitelist validation                   │
│    - Rate-limited CAN TX                    │
│                                             │
│  Health Monitor Thread:                     │
│    - Watchdog ping                          │
│    - Error rate monitoring                  │
│    - Bus recovery trigger                   │
└─────────────────────────────────────────────┘
```

### 2.2 reverse_service

The reverse service monitors for reverse gear engagement via CAN frame or GPIO input and notifies the HMI to switch to camera view.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        reverse_service                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐   │
│  │  CanInterface   │────►│ ReverseDetector │────►│   ZmqPublisher  │   │
│  │   (Optional)    │     │  (Debounce)     │     │   (PUB Socket)  │   │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘   │
│                                  ▲                                      │
│  ┌─────────────────┐             │                                      │
│  │   GpioMonitor   │─────────────┘                                      │
│  │   (Optional)    │                                                    │
│  └─────────────────┘                                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Detection Sources:**

| Source | Configuration | Signal |
|--------|---------------|--------|
| CAN | `can_id: 0x370, byte: 1, bit: 0x80` | BMW reverse gear frame |
| GPIO | `pin: 17, active_low: true` | Physical switch input |

### 2.3 hmi_launcher

The HMI launcher is a Qt6/QML application providing the graphical user interface, integrating with OpenAuto for Android Auto functionality.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          hmi_launcher                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      QML Layer                                   │   │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐ │   │
│  │  │  main.qml    │ │ AppState.qml │ │      Screens/            │ │   │
│  │  │ (Entry)      │ │ (Singleton)  │ │ DashScreen, HomeScreen,  │ │   │
│  │  │              │ │              │ │ ConfigScreen, OpenAuto   │ │   │
│  │  └──────────────┘ └──────────────┘ └──────────────────────────┘ │   │
│  │  ┌──────────────────────────────────────────────────────────────┐│   │
│  │  │                    Components/                               ││   │
│  │  │  ArcGauge, GaugeCard, ShiftLightBar, WarningIndicator, ...   ││   │
│  │  └──────────────────────────────────────────────────────────────┘│   │
│  └─────────────────────────────────────────────────────────────────────┘│
│                                   │                                     │
│                                   │ Q_PROPERTY bindings                 │
│                                   ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      C++ Layer                                   │   │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐ │   │
│  │  │ DataProvider │ │ OpenAuto     │ │    QMLVideoOutput        │ │   │
│  │  │ (ZMQ Sub)    │ │ Embedded     │ │  (GStreamer + QVideoSink)│ │   │
│  │  └──────────────┘ └──────────────┘ └──────────────────────────┘ │   │
│  │         │                │                      │                │   │
│  │         │                │                      │                │   │
│  │         ▼                ▼                      ▼                │   │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐ │   │
│  │  │ ZMQ Context  │ │ aasdk/       │ │    GStreamer Pipeline    │ │   │
│  │  │ (SUB sockets)│ │ openauto libs│ │ (H.264 decode)           │ │   │
│  │  └──────────────┘ └──────────────┘ └──────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘│
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Key C++ Classes:**

| Class | File | Responsibility |
|-------|------|----------------|
| `DataProvider` | `data_provider.cpp` | ZMQ subscriber, exposes data as Q_PROPERTYs |
| `OpenAutoEmbedded` | `openauto_embedded.cpp` | OpenAuto lifecycle, USB enumeration |
| `OpenAutoController` | `openauto_controller.cpp` | QML-exposed controller for OpenAuto |
| `QMLVideoOutput` | `qml_video_output.cpp` | GStreamer-based video rendering for AA |
| `InputDevice` | (via OpenAuto) | Touch event injection to Android device |

**QML State Management:**

```
                   ┌─────────────────┐
                   │  AppState.qml   │
                   │   (Singleton)   │
                   └────────┬────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ Engine Data   │  │   UI State    │  │ OpenAuto State│
│ rpm, clt, map │  │ currentScreen │  │ running       │
│ tps, lambda   │  │ displayMode   │  │ connected     │
│ gear, speed   │  │ reverseActive │  │               │
└───────────────┘  └───────────────┘  └───────────────┘
```

### 2.4 common Library

Shared utilities used across all services:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          libcommon                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐ │
│  │  ConfigLoader   │  │  SystemHealth   │  │       Logger            │ │
│  │  (YAML parsing) │  │  (Watchdog,     │  │ (spdlog wrapper)        │ │
│  │                 │  │   health check) │  │                         │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────────────┘ │
│                                                                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐ │
│  │    Expected     │  │  SignalHandler  │  │      Types              │ │
│  │  (Result type)  │  │ (POSIX signals) │  │ (EngineData, CanFrame)  │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────────────┘ │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Shared Data Types:**

```cpp
// EngineData - Primary telemetry structure
struct EngineData {
    uint16_t rpm;           // 0-65535 RPM
    int8_t   coolant_temp;  // -40 to +127°C
    int8_t   air_temp;      // -40 to +127°C
    uint8_t  tps;           // 0-100%
    uint16_t map_kpa;       // 0-655.35 kPa (x100)
    uint16_t lambda;        // 0.5-2.0 (x1000)
    uint8_t  battery;       // 0-25.5V (x10)
    uint16_t speed_kmh;     // 0-655 km/h
    uint8_t  gear;          // 0-7
    uint8_t  status_flags;  // CEL, Fan, Fuel pump, etc.
    uint32_t timestamp_ms;  // Monotonic timestamp
};
```

---

## 3. Communication Protocols

### 3.1 ZeroMQ IPC

All inter-process communication uses ZeroMQ with IPC transport for low-latency, zero-copy messaging.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ZeroMQ Topology                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  can_service                    reverse_service                         │
│  ┌──────────────┐               ┌──────────────┐                       │
│  │  PUB Socket  │               │  PUB Socket  │                       │
│  │  ENGINE.*    │               │  REVERSE.*   │                       │
│  └──────┬───────┘               └──────┬───────┘                       │
│         │                               │                               │
│         │ ipc:///tmp/                   │ ipc:///tmp/                   │
│         │ speeduino_data.ipc            │ reverse_trigger.ipc           │
│         │                               │                               │
│         └───────────┬───────────────────┘                               │
│                     │                                                   │
│                     ▼                                                   │
│               ┌──────────────┐                                          │
│               │  SUB Socket  │                                          │
│               │  (HMI)       │                                          │
│               │  ENGINE.*    │                                          │
│               │  REVERSE.*   │                                          │
│               └──────────────┘                                          │
│               hmi_launcher                                              │
│                                                                         │
│  ┌──────────────┐                                                       │
│  │  REP Socket  │◄───────────┐                                          │
│  │  (Commands)  │            │ REQ Socket                               │
│  └──────────────┘            │ (HMI commands)                           │
│  can_service                 │                                          │
│                              └──────────────────────────────────────────│
│                              ipc:///tmp/speeduino_cmd.ipc               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Message Format:**

```
┌─────────────────────────────────────────────────────────┐
│              ZMQ Multipart Message                      │
├─────────────────────────────────────────────────────────┤
│  Frame 0: Topic (string)                                │
│           e.g., "ENGINE.DATA", "REVERSE.STATE"          │
├─────────────────────────────────────────────────────────┤
│  Frame 1: Payload (binary)                              │
│           Serialized struct (memcpy)                    │
│           sizeof(EngineData) = 20 bytes                 │
└─────────────────────────────────────────────────────────┘
```

### 3.2 CAN Bus Protocols

Supported CAN protocols for Speeduino broadcast:

| Protocol | Frame IDs | Data Rate | Signals |
|----------|-----------|-----------|---------|
| Haltech IC-7 | 0x360-0x3E0 | 50 Hz | RPM, MAP, TPS, CLT, IAT, Lambda, Speed, Gear |
| BMW PT-CAN | 0x316, 0x329, 0x545 | 100 Hz | RPM, CLT, TPS, CEL status |
| VAG (VW/Audi) | 0x280, 0x288, 0x320 | 50 Hz | RPM, CLT, TPS |
| OBD-II | 0x7E8 (response) | On-request | Standard PIDs |

**Haltech IC-7 Frame Layout (Primary):**

```
Frame 0x360 (Engine RPM + MAP):
  Byte 0-1: RPM (Big Endian, raw)
  Byte 2-3: MAP kPa x10 (Big Endian)
  Byte 4-5: TPS x10 (Big Endian)
  Byte 6-7: Reserved

Frame 0x361 (Temperatures):
  Byte 0-1: Coolant Temp (°C + 40)
  Byte 2-3: Air Temp (°C + 40)
  Byte 4-7: Reserved

Frame 0x362 (Lambda + Fuel):
  Byte 0-1: Lambda x1000 (Big Endian)
  Byte 2-3: Fuel Pressure x10
  Byte 4-7: Reserved
```

---

## 4. Threading Model

Each service follows a consistent threading architecture:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       Service Threading Model                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Main Thread (Qt Event Loop for HMI / ASIO io_context for services)    │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  - Signal handling (graceful shutdown)                           │   │
│  │  - Configuration loading                                         │   │
│  │  - Service initialization                                        │   │
│  │  - Watchdog ping to systemd                                      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Worker Threads                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  CAN Read Thread:                                                │   │
│  │    - Blocking read from socket                                   │   │
│  │    - Minimal processing (frame copy)                             │   │
│  │    - Post to processing queue                                    │   │
│  │                                                                  │   │
│  │  ZMQ Publish Thread:                                             │   │
│  │    - Rate-limited publish (50 Hz default)                        │   │
│  │    - Atomic data access                                          │   │
│  │    - Non-blocking send                                           │   │
│  │                                                                  │   │
│  │  OpenAuto IO Thread (hmi_launcher only):                         │   │
│  │    - Boost.Asio io_context                                       │   │
│  │    - USB async operations                                        │   │
│  │    - Video frame processing                                      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Thread Synchronization:                                                │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  - std::atomic for simple flags and counters                     │   │
│  │  - std::mutex for complex state (config, callbacks)              │   │
│  │  - Lock-free queues for high-throughput paths                    │   │
│  │  - No I/O operations under locks (MISRA compliance)              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. OpenAuto Integration

The hmi_launcher integrates OpenAuto for Android Auto functionality using a custom video output implementation.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     OpenAuto Integration                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Android Phone (USB)                                                    │
│       │                                                                 │
│       │ USB AOA Protocol                                                │
│       ▼                                                                 │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                         aasdk                                    │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │   │
│  │  │  USBHub     │  │ Messenger   │  │  Service Channels       │  │   │
│  │  │ (libusb)    │  │ (Proto/SSL) │  │ (Video, Audio, Input)   │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│       │                                                                 │
│       │ Callbacks                                                       │
│       ▼                                                                 │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        openauto                                  │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │   │
│  │  │  App        │  │ServiceFactory│ │   VideoService          │  │   │
│  │  │ (Lifecycle) │  │ (DI)        │  │ (Frame callback)        │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│       │                                                                 │
│       │ Custom Video Output                                             │
│       ▼                                                                 │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    QMLVideoOutput                                │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │              GStreamer Pipeline                          │    │   │
│  │  │  appsrc → h264parse → avdec_h264 → videoconvert → appsink│    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │                          │                                       │   │
│  │                          ▼                                       │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │                   QVideoFrame                            │    │   │
│  │  │            (Rendered to QML VideoOutput)                 │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│       │                                                                 │
│       │ Touch Events                                                    │
│       ▼                                                                 │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     InputDevice                                  │   │
│  │  QML MouseArea → sendTouch() → QMouseEvent → eventFilter        │   │
│  │                                → aasdk InputService              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**OpenAuto State Flow:**

```
┌────────┐    USB     ┌──────────┐   Query   ┌──────────┐
│  IDLE  │──────────►│CONNECTING│─────────►│ RUNNING  │
└────────┘  Connect   └──────────┘  Success  └──────────┘
    ▲                      │                      │
    │                      │ Timeout              │ Disconnect
    │                      ▼                      │
    │               ┌──────────┐                  │
    └───────────────│  ERROR   │◄─────────────────┘
                    └──────────┘
```

---

## 6. Deployment Architecture

### Systemd Service Dependencies

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Systemd Service Graph                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Boot Sequence:                                                         │
│                                                                         │
│  ┌───────────────┐                                                      │
│  │ multi-user    │                                                      │
│  │   .target     │                                                      │
│  └───────┬───────┘                                                      │
│          │                                                              │
│          ▼                                                              │
│  ┌───────────────┐     ┌───────────────┐                               │
│  │ speeduino-can │────►│  can_service  │                               │
│  │   .service    │     │   .service    │                               │
│  │ (CAN ifup)    │     │ (CAN Parser)  │                               │
│  └───────────────┘     └───────┬───────┘                               │
│                                │                                        │
│                                ▼                                        │
│                        ┌───────────────┐                               │
│                        │reverse_service│                               │
│                        │   .service    │                               │
│                        └───────┬───────┘                               │
│                                │                                        │
│  ┌───────────────┐             │                                        │
│  │   graphical   │             │                                        │
│  │    .target    │─────────────┤                                        │
│  └───────────────┘             │                                        │
│                                ▼                                        │
│                        ┌───────────────┐                               │
│                        │ hmi_launcher  │                               │
│                        │   .service    │                               │
│                        │  (Qt6 UI)     │                               │
│                        └───────────────┘                               │
│                                                                         │
│  Legend:                                                                │
│    ───► = Requires/After                                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Service Configuration

| Service | Type | Watchdog | Restart | Security |
|---------|------|----------|---------|----------|
| `speeduino-can` | oneshot | No | No | Root (CAP_NET_ADMIN) |
| `can_service` | notify | 10s | always | User (CAP_NET_RAW) |
| `reverse_service` | notify | 10s | always | User |
| `hmi_launcher` | notify | 15s | always | User (video,input,audio,bluetooth) |

### Resource Limits

| Service | Memory Max | CPU Quota | Nice | CPU Affinity |
|---------|------------|-----------|------|--------------|
| `can_service` | 64M | 20% | -5 | Core 2-3 |
| `reverse_service` | 32M | 10% | 0 | Core 2-3 |
| `hmi_launcher` | 512M | 80% | 0 | Core 0-1 |

---

## Appendix A: Build System

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CMake Project Structure                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  CMakeLists.txt (root)                                                  │
│  ├── cmake/                                                             │
│  │   ├── FindZeroMQ.cmake                                               │
│  │   └── CompilerFlags.cmake                                            │
│  ├── src/                                                               │
│  │   ├── common/           → libcommon.a (static)                       │
│  │   ├── can_service/      → can_service (executable)                   │
│  │   ├── reverse_service/  → reverse_service (executable)               │
│  │   └── hmi_launcher/     → hmi_launcher (executable)                  │
│  │       └── qml/          → Qt6 QML resources                          │
│  └── tests/                                                             │
│      ├── unit/             → Unit tests (GTest)                         │
│      └── integration/      → Integration tests                          │
│                                                                         │
│  Build Targets:                                                         │
│    ninja                   → Build all                                  │
│    ninja install           → Install to /usr/local/bin                  │
│    ninja test              → Run tests                                  │
│    ninja package           → Create .deb package                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Appendix B: Configuration Files

| File | Format | Purpose |
|------|--------|---------|
| `configs/system.yaml` | YAML | Global system configuration |
| `configs/can_signals.yaml` | YAML | CAN signal definitions per protocol |
| `configs/steering_wheel.yaml` | YAML | Steering wheel button mappings |

**system.yaml Example:**

```yaml
can:
  interface: "can0"
  bitrate: 500000
  protocol: "haltech"

zmq:
  data_endpoint: "ipc:///tmp/speeduino_data.ipc"
  cmd_endpoint: "ipc:///tmp/speeduino_cmd.ipc"
  publish_rate_hz: 50

reverse:
  source: "can"
  can_id: 0x370
  byte_index: 1
  bit_mask: 0x80
  debounce_ms: 50

display:
  resolution: "800x480"
  fullscreen: true
  fps_limit: 60

openauto:
  resolution: "800x480"
  fps: 30
  dpi: 140
  touchscreen: true
```

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01 | Auto-generated | Initial architecture documentation |
