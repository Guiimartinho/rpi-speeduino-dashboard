# RPi Speeduino Dashboard

[![CI Lint](https://github.com/Guiimartinho/rpi-speeduino-dashboard/actions/workflows/ci-lint.yml/badge.svg)](https://github.com/Guiimartinho/rpi-speeduino-dashboard/actions/workflows/ci-lint.yml)
[![CI Build](https://github.com/Guiimartinho/rpi-speeduino-dashboard/actions/workflows/ci-build.yml/badge.svg)](https://github.com/Guiimartinho/rpi-speeduino-dashboard/actions/workflows/ci-build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=c%2B%2B)
![Qt 6.6+](https://img.shields.io/badge/Qt-6.6%2B-41CD52.svg?logo=qt)
![Raspberry Pi 4](https://img.shields.io/badge/Raspberry%20Pi-4-C51A4A.svg?logo=raspberrypi)
![OpenAuto](https://img.shields.io/badge/OpenAuto-Android%20Auto-3DDC84.svg?logo=android)
![ZeroMQ](https://img.shields.io/badge/ZeroMQ-IPC-DF0000.svg)
![ISO 26262](https://img.shields.io/badge/ISO%2026262-ASIL--B-orange.svg)
![MISRA C++](https://img.shields.io/badge/MISRA-C%2B%2B%3A2008-blue.svg)

Dashboard/multimedia system for Speeduino ECU running on Raspberry Pi 4. Inspired by FuelTech FT600 interface.

## Features

- **Real-time engine data display** via CAN bus (RPM, CLT, TPS, MAP, Lambda, etc.)
- **Android Auto integration** via OpenAuto (USB wired)
- **Reverse camera** with automatic switching (CAN or GPIO trigger)
- **Steering wheel controls** via CAN bus
- **Qt6/QML touchscreen UI** optimized for 5"/7" displays
- **Multiple CAN protocols**: Haltech IC-7, BMW PT-CAN, VAG, OBD-II

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Raspberry Pi 4                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │can_service │  │reverse_svc  │  │hmi_launcher │  [OpenAuto]  │
│  │  (C++17)   │  │  (C++17)    │  │ (Qt6/QML)   │              │
│  └─────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
│        └────────────────┴────────────────┘                      │
│                         │                                       │
│                   ZeroMQ (IPC)                                  │
├─────────────────────────────────────────────────────────────────┤
│                    SocketCAN (can0)                             │
├─────────────────────────────────────────────────────────────────┤
│                 MCP2515/MCP2518FD (SPI)                         │
└─────────────────────────────────────────────────────────────────┘
                           │
                     CAN 500kbps
                           │
                   ┌───────┴───────┐
                   │   Speeduino   │
                   │  STM32F407VE  │
                   └───────────────┘
```

## Hardware Requirements

- Raspberry Pi 4 (4GB)
- 5" or 7" touchscreen (HDMI or DSI)
- MCP2515 or MCP2518FD CAN module (SPI)
- USB cable for Android Auto
- USB UVC camera for reverse (optional)

### MCP2515 Wiring

| MCP2515 | RPi4 Pin | GPIO |
|---------|----------|------|
| VCC     | Pin 1    | 3.3V |
| GND     | Pin 6    | GND  |
| CS      | Pin 24   | GPIO8 (SPI0 CE0) |
| SO      | Pin 21   | GPIO9 (SPI0 MISO) |
| SI      | Pin 19   | GPIO10 (SPI0 MOSI) |
| SCK     | Pin 23   | GPIO11 (SPI0 SCLK) |
| INT     | Pin 22   | GPIO25 |

## Building

### Dependencies

```bash
# Run on Raspberry Pi OS 64-bit (Bookworm)
sudo ./scripts/install_deps.sh
```

### Compile

```bash
mkdir build && cd build
cmake -GNinja ..
ninja
sudo ninja install
```

## Configuration

### Speeduino Setup

1. In TunerStudio: `Settings` → `CAN/Second Serial`
2. Enable `CAN Broadcasting`
3. Select protocol: `Haltech IC-7` (recommended)
4. Bitrate: `500kbps`

### System Configuration

Edit `configs/system.yaml`:

```yaml
can:
  interface: "can0"
  bitrate: 500000
  protocol: "haltech"  # haltech, bmw, vag, obd2

reverse:
  source: "can"        # can or gpio
  can_id: 0x370
  byte_index: 1
  bit_mask: 0x80
```

## Running

### Manual Start

```bash
# Setup CAN interface
sudo ./scripts/setup_can.sh can0 500000

# Start services
./build/src/can_service/can_service -i can0 -c ./configs
./build/src/reverse_service/reverse_service -i can0 -c ./configs
./build/src/hmi_launcher/hmi_launcher -c ./configs -f
```

### Systemd Services

```bash
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable speeduino-can can_service reverse_service hmi_launcher
sudo systemctl start speeduino-can can_service reverse_service hmi_launcher
```

## Project Structure

```
rpi-speeduino-dashboard/
├── CMakeLists.txt
├── configs/
│   ├── system.yaml
│   ├── can_signals.yaml
│   └── steering_wheel.yaml
├── scripts/
│   ├── setup_can.sh
│   ├── install_deps.sh
│   └── flash_overlay.sh
├── systemd/
│   ├── can_service.service
│   ├── reverse_service.service
│   ├── hmi_launcher.service
│   └── speeduino-can.service
├── src/
│   ├── common/           # Shared library
│   ├── can_service/      # CAN bus reader/writer
│   ├── reverse_service/  # Reverse gear detection
│   └── hmi_launcher/     # Qt6/QML UI
├── docs/
│   └── bring_up.md       # Installation guide
└── tests/
    └── integration/
```

## Supported CAN Protocols

| Protocol | Frame IDs | Data |
|----------|-----------|------|
| Haltech IC-7 | 0x360-0x3E0 | RPM, MAP, TPS, CLT, Lambda, VSS |
| BMW PT-CAN | 0x316, 0x329, 0x545 | RPM, CLT, TPS, CEL |
| VAG | 0x280, 0x288, 0x320 | RPM, CLT, TPS |
| OBD-II | 0x7E8 | Standard PIDs |

## Safety Features

### ISO 26262 ASIL-B Compliance

This project implements automotive safety patterns compliant with **ISO 26262 ASIL-B** and **MISRA C++:2008** standards:

| Category | Implementation |
|----------|----------------|
| Buffer Safety | DLC validation before all memcpy operations |
| Thread Safety | Mutex + atomic with acquire/release semantics |
| Type Safety | Clamping functions prevent integer overflow |
| Input Validation | Strict whitelist for CAN commands and interface names |
| Fail-Safe Defaults | Error states as default for uninitialized values |

### Runtime Protection

- CAN command whitelist with rate limiting
- Token bucket algorithm for write protection
- CAN bus timeout detection (500ms)
- Overheat/CEL warning indicators
- Automatic camera failover
- Graceful degradation when subsystems fail
- Command injection prevention (no shell execution)

### Validation Results

| Agent | Result |
|-------|--------|
| MISRA C++:2008 | COMPLIANT |
| Static Analysis | LOW RISK |
| Memory Safety | 9.7/10 |

See [docs/SAFETY.md](docs/SAFETY.md) for detailed safety documentation.

## Development

### Setup Git Hooks

```bash
# Install pre-commit and hooks
./.hooks/install-hooks.sh
```

### Code Quality Tools

| Tool | Purpose | Config |
|------|---------|--------|
| clang-format | C++ formatting | `.clang-format` |
| clang-tidy | C++ static analysis | `.clang-tidy` |
| cppcheck | C++ linting | `.hooks/cppcheck-quick.sh` |
| MISRA check | Nesting depth (max 3) | `.hooks/check-nesting.py` |
| shellcheck | Shell script linting | Built-in |
| codespell | Spelling check | `.codespellrc` |

### Pre-Commit Hooks

```bash
# Run all hooks manually
pre-commit run --all-files

# Run specific hook
pre-commit run clang-format --all-files
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for full development guidelines.

## CI/CD

The project uses GitHub Actions for continuous integration:

| Workflow | Trigger | Runner |
|----------|---------|--------|
| CI Lint | All branches, PRs | GitHub-hosted |
| CI Build | Push to `main` | Self-hosted (Pi4) |
| Release | Tags `v*.*.*` | Self-hosted (Pi4) |

See [docs/ci-cd.md](docs/ci-cd.md) for setup instructions.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) - System architecture (macro and detailed views)
- [QML Architecture](docs/QML_ARCHITECTURE.md) - Qt6/QML component design
- [Bring-up Guide](docs/bring_up.md) - Hardware setup and installation (PT-BR)
- [Safety Compliance](docs/SAFETY.md) - ISO 26262 and MISRA C++ documentation
- [API ZMQ Messages](docs/API_ZMQ_MESSAGES.md) - ZeroMQ IPC protocol reference
- [Wiring Guide (Gol Quadrado)](docs/WIRING_GOL_QUADRADO.md) - Classic car GPIO wiring
- [CI/CD Pipeline](docs/ci-cd.md) - GitHub Actions and self-hosted runner (PT-BR)
- [Contributing](CONTRIBUTING.md) - Development setup and code style
- [Changelog](CHANGELOG.md) - Version history and release notes

## License

MIT License

## Credits

- [Speeduino](https://speeduino.com/) - Open source ECU
- [OpenAuto](https://github.com/openDsh/openauto) - Android Auto head unit
- [aasdk](https://github.com/openDsh/aasdk) - Android Auto SDK
