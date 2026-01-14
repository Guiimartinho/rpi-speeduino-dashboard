# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0] - 2026-01-12

### Added
- ISO 26262 ASIL-B safety compliance for automotive applications
- MISRA C++:2008 compliance across critical modules
- Comprehensive safety documentation (`docs/SAFETY.md`)
- Named constants replacing magic numbers (MISRA Rule 2-13-5)
- Safe integer clamping functions for type conversions
- Thread-safe error string handling with `strerror_r()`
- Timestamp wraparound handling for 49-day uptime

### Changed
- Replaced `system()` call with `ioctl()` for CAN interface recovery (security fix)
- Mutex deadlock prevention with copy-unlock-invoke-lock callback pattern
- Loop counters changed from signed to unsigned types (MISRA Rule 5-0-8)
- All destructors now marked `noexcept` (MISRA Rule 15-5-1)
- `Expected<void, E>` default initialization to error state (fail-safe)

### Fixed
- Command injection vulnerability in CAN error handler (CWE-78)
- TOCTOU race condition in sendTouch() widget access
- Buffer overflow prevention with DLC validation before memcpy
- Data race in ReverseDetector debounce state
- Integer overflow in CAN signal scale calculations
- Division by zero in ZMQ publish rate calculation
- Uninitialized `hasValue_` member in Expected<void, E>
- parseHexOrDec bounds check before string access
- setsockopt() return value now checked

### Security
- Removed shell command execution from recovery functions
- Strict input validation for interface names (alphanumeric only)
- Rate limiting and whitelist for CAN command writes

## [0.2.0] - 2026-01-11

### Added
- GPIO detection mode for classic cars (VW Gol Quadrado support)
- Wiring guide for VW Gol Quadrado (`docs/WIRING_GOL_QUADRADO.md`)
- Sample configuration for classic car reverse detection
- Detection mode selection: `can`, `gpio`, or `both`
- GPIO active-low configuration option

### Changed
- ReverseDetector now supports multiple detection modes
- Improved GPIO initialization with gpiod v2 API
- Enhanced logging for detection source

### Fixed
- GPIO initialization error handling
- Config loader include guard consistency

## [0.1.0] - 2026-01-10

### Added
- Initial project structure with CMake build system
- CAN service for Speeduino ECU communication
- Reverse detection service (CAN and GPIO)
- HMI launcher with Qt6/QML interface
- OpenAuto integration for Android Auto
- ZeroMQ IPC for inter-service communication
- Systemd service files
- Git hooks for code quality (clang-format, clang-tidy, cppcheck)
- CI/CD pipeline with GitHub Actions
- Multiple CAN protocol support (Haltech, BMW, VAG, OBD-II)

### Features
- Real-time engine data display (RPM, CLT, TPS, MAP, Lambda)
- Reverse camera with automatic switching
- Steering wheel controls via CAN
- CAN command whitelist with rate limiting
- Token bucket algorithm for write protection
- System health monitoring with graceful degradation

[Unreleased]: https://github.com/Guiimartinho/rpi-speeduino-dashboard/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/Guiimartinho/rpi-speeduino-dashboard/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/Guiimartinho/rpi-speeduino-dashboard/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/Guiimartinho/rpi-speeduino-dashboard/releases/tag/v0.1.0
