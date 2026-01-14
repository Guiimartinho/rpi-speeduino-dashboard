# CMake Toolchain File for ARM64 Cross-Compilation
# Target: Raspberry Pi 5 / Jetson Nano / Orin
#
# ISO 26262 ASIL-B: Deterministic cross-compilation for embedded targets
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
#         -DCMAKE_BUILD_TYPE=Release ..
#
# Prerequisites (Ubuntu/Debian host):
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   sudo apt install qemu-user-static  # for running tests

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler paths
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Sysroot for target libraries (optional, set via environment)
if(DEFINED ENV{SYSROOT})
    set(CMAKE_SYSROOT $ENV{SYSROOT})
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
endif()

# Search paths configuration
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # Use host programs (cmake, ninja)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # Use target libraries
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # Use target headers
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)    # Use target packages

# ARM64-specific flags (Cortex-A76 for RPi5, Cortex-A57 for Jetson Nano)
# Default to generic ARMv8-A for compatibility
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")

# Platform-specific optimizations (can be overridden)
# RPi5:      -mcpu=cortex-a76 -mtune=cortex-a76
# Jetson:    -mcpu=cortex-a57 -mtune=cortex-a57
# Orin:      -mcpu=cortex-a78 -mtune=cortex-a78

# pkg-config for cross-compilation
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

# Qt6 cross-compilation support
# Requires Qt6 host tools and target libraries
if(DEFINED ENV{QT6_HOST_PATH})
    set(QT_HOST_PATH $ENV{QT6_HOST_PATH})
endif()

# Disable tests when cross-compiling (use QEMU or target for testing)
set(BUILD_TESTS OFF CACHE BOOL "Disable tests for cross-compilation")

message(STATUS "Cross-compiling for aarch64-linux-gnu")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  CXX compiler: ${CMAKE_CXX_COMPILER}")
if(CMAKE_SYSROOT)
    message(STATUS "  Sysroot: ${CMAKE_SYSROOT}")
endif()
