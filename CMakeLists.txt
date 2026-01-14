cmake_minimum_required(VERSION 3.20)
project(speeduino_ui VERSION 0.3.0 LANGUAGES CXX)

# C++20 standard (required for std::source_location)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# BUILD OPTIONS
# ═══════════════════════════════════════════════════════════════════════════════

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

# Option for sanitizers in Debug mode
option(ENABLE_SANITIZERS "Enable ASAN/UBSAN sanitizers in Debug" ON)

# Option for code coverage
option(ENABLE_COVERAGE "Enable code coverage with gcov/lcov" OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# COMPILER FLAGS
# ═══════════════════════════════════════════════════════════════════════════════

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Platform-specific optimizations (native build only)
# Cross-compilation should use toolchain file settings
if(NOT CMAKE_CROSSCOMPILING)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        # Native ARM64 build - detect platform
        if(EXISTS "/proc/device-tree/model")
            file(READ "/proc/device-tree/model" DEVICE_MODEL)
            if(DEVICE_MODEL MATCHES "Raspberry Pi 5")
                set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=cortex-a76 -mtune=cortex-a76")
                message(STATUS "Detected: Raspberry Pi 5 (Cortex-A76)")
            elseif(DEVICE_MODEL MATCHES "Jetson")
                set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=cortex-a57 -mtune=cortex-a57")
                message(STATUS "Detected: NVIDIA Jetson (Cortex-A57)")
            else()
                message(STATUS "Detected: Generic ARM64")
            endif()
        endif()
    endif()
else()
    message(STATUS "Cross-compiling: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SANITIZERS (ASAN/UBSAN) - ISO 26262 ASIL-B: Runtime error detection
# Helps detect memory errors, undefined behavior, and data races
# ═══════════════════════════════════════════════════════════════════════════════

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
    message(STATUS "Enabling ASAN/UBSAN sanitizers for Debug build")

    # AddressSanitizer (memory errors: buffer overflows, use-after-free, etc.)
    # UndefinedBehaviorSanitizer (UB: integer overflow, null deref, etc.)
    set(SANITIZER_FLAGS
        "-fsanitize=address,undefined"
        "-fno-omit-frame-pointer"
        "-fno-optimize-sibling-calls"
    )

    add_compile_options(${SANITIZER_FLAGS})
    add_link_options(${SANITIZER_FLAGS})

    # ASAN options for better diagnostics
    set(ENV{ASAN_OPTIONS} "detect_leaks=1:detect_stack_use_after_return=1:check_initialization_order=1")
    set(ENV{UBSAN_OPTIONS} "print_stacktrace=1:halt_on_error=0")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# CODE COVERAGE (gcov/lcov)
# Usage: cmake -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ..
#        make && make test && lcov --capture --directory . --output-file coverage.info
#        genhtml coverage.info --output-directory coverage_report
# ═══════════════════════════════════════════════════════════════════════════════

if(ENABLE_COVERAGE)
    message(STATUS "Enabling code coverage with gcov")

    set(COVERAGE_FLAGS "--coverage" "-fprofile-arcs" "-ftest-coverage")
    add_compile_options(${COVERAGE_FLAGS})
    add_link_options(${COVERAGE_FLAGS})

    # Disable sanitizers when coverage is enabled (they conflict)
    if(ENABLE_SANITIZERS)
        message(WARNING "Disabling sanitizers because coverage is enabled")
    endif()
endif()

# Warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -Wno-unused-parameter
    -Wshadow
    -Wconversion
    -Wdouble-promotion
    -Wformat=2
    -Wformat-security
)

# Security hardening flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
    )
    add_link_options(
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
    )
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# ═══════════════════════════════════════════════════════════════════════════════
# DEPENDENCIES WITH VERSION CONSTRAINTS
# ISO 26262 ASIL-B: Pin minimum versions for reproducible builds
# ═══════════════════════════════════════════════════════════════════════════════

# ZeroMQ >= 4.3 (required for zmq_poller and modern API)
pkg_check_modules(ZMQ REQUIRED libzmq>=4.3)

# yaml-cpp >= 0.6 (required for YAML 1.2 support and C++11 API)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp>=0.6)

# libsystemd >= 246 (optional, for watchdog and sd_notify)
pkg_check_modules(SYSTEMD libsystemd>=246)
if(SYSTEMD_FOUND)
    add_compile_definitions(HAVE_SYSTEMD)
endif()

# MessagePack (try C++ header first, then C header)
find_path(MSGPACK_INCLUDE_DIR msgpack.hpp
    HINTS /usr/include /usr/local/include
)
if(NOT MSGPACK_INCLUDE_DIR)
    find_path(MSGPACK_INCLUDE_DIR msgpack.h
        HINTS /usr/include /usr/local/include
    )
endif()

# Qt6 >= 6.2 (for hmi_launcher, requires QML improvements)
find_package(Qt6 6.2 COMPONENTS Core Quick QuickControls2 Multimedia MultimediaWidgets Widgets Bluetooth QUIET)

# GTest >= 1.11 for unit tests (modern API)
find_package(GTest 1.11 QUIET)

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

# Subprojects
add_subdirectory(src/common)
add_subdirectory(src/can_service)
add_subdirectory(src/reverse_service)

if(Qt6_FOUND)
    add_subdirectory(src/hmi_launcher)
else()
    message(WARNING "Qt6 not found, skipping hmi_launcher")
endif()

# Tests
if(BUILD_TESTS AND GTest_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(DIRECTORY configs/ DESTINATION /etc/speeduino-ui)
install(DIRECTORY systemd/ DESTINATION /etc/systemd/system
    FILES_MATCHING PATTERN "*.service")
install(PROGRAMS scripts/setup_can.sh DESTINATION /usr/local/bin)

# ═══════════════════════════════════════════════════════════════════════════════
# CPACK - DEB PACKAGE CONFIGURATION
# ISO 26262 ASIL-B: Complete dependency declaration for reproducible deployments
# Usage: cmake --build . --target package
# ═══════════════════════════════════════════════════════════════════════════════

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "speeduino-ui")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Speeduino ECU Dashboard with OpenAuto integration")
set(CPACK_PACKAGE_DESCRIPTION "Professional automotive dashboard for Speeduino ECU
with Android Auto (OpenAuto) integration, CAN bus support, and reverse camera.
Designed for Raspberry Pi and Jetson platforms with ISO 26262 ASIL-B compliance.")

set(CPACK_PACKAGE_VENDOR "Speeduino UI Project")
set(CPACK_PACKAGE_CONTACT "maintainer@speeduino-ui.local")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/speeduino/speeduino-ui")

# DEB-specific settings
set(CPACK_DEBIAN_PACKAGE_SECTION "embedded")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")

# Runtime dependencies for DEB package
# CRITICAL: Include openauto and aasdk for Android Auto support
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6 (>= 6.2), \
    libqt6quick6 (>= 6.2), \
    libqt6qml6 (>= 6.2), \
    libqt6multimedia6 (>= 6.2), \
    libqt6widgets6 (>= 6.2), \
    qml6-module-qtquick (>= 6.2), \
    qml6-module-qtquick-controls (>= 6.2), \
    qml6-module-qtquick-layouts (>= 6.2), \
    qml6-module-qtmultimedia (>= 6.2), \
    libzmq5 (>= 4.3), \
    libyaml-cpp0.7, \
    libboost-system1.74.0, \
    libboost-log1.74.0, \
    libusb-1.0-0, \
    can-utils, \
    libgpiod2, \
    openauto (>= 2.0), \
    aasdk (>= 1.0)"
)

# Recommended packages (not required but useful)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
    "gstreamer1.0-plugins-good, \
    gstreamer1.0-plugins-bad, \
    v4l-utils"
)

# Suggested packages (for development)
set(CPACK_DEBIAN_PACKAGE_SUGGESTS
    "gdb, \
    can-utils, \
    i2c-tools"
)

# Post-install script for systemd service setup
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/packaging/postinst;${CMAKE_SOURCE_DIR}/packaging/prerm"
)

# Generate unique package filename
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}"
)

# Include CPack module
include(CPack)

# Summary
message(STATUS "")
message(STATUS "=== Speeduino UI Build Configuration ===")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:   ${CMAKE_CXX_COMPILER}")
message(STATUS "Processor:      ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Cross-compile:  ${CMAKE_CROSSCOMPILING}")
message(STATUS "ZeroMQ:         ${ZMQ_VERSION}")
message(STATUS "yaml-cpp:       ${YAML_CPP_VERSION}")
message(STATUS "libsystemd:     ${SYSTEMD_FOUND}")
message(STATUS "Qt6:            ${Qt6_FOUND}")
message(STATUS "GTest:          ${GTest_FOUND}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
message(STATUS "Sanitizers:     ${ENABLE_SANITIZERS}")
message(STATUS "Coverage:       ${ENABLE_COVERAGE}")
message(STATUS "=========================================")
message(STATUS "")
message(STATUS "Cross-compilation toolchains available:")
message(STATUS "  cmake/toolchains/aarch64-linux-gnu.cmake")
message(STATUS "  cmake/toolchains/rpi5.cmake")
message(STATUS "  cmake/toolchains/jetson.cmake")
message(STATUS "")
