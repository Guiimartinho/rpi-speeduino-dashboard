# CMake Toolchain File for Raspberry Pi 5 Cross-Compilation
# Target: BCM2712 (Cortex-A76)
#
# ISO 26262 ASIL-B: Optimized for automotive dashboard on RPi5
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rpi5.cmake \
#         -DCMAKE_BUILD_TYPE=Release ..
#
# Prerequisites:
#   - Cross-compiler: aarch64-linux-gnu-gcc
#   - Sysroot from RPi5 with all dependencies installed

# Include base ARM64 toolchain
include(${CMAKE_CURRENT_LIST_DIR}/aarch64-linux-gnu.cmake)

# RPi5 Cortex-A76 optimizations
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a76 -mtune=cortex-a76 -mfpu=neon-fp-armv8")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a76 -mtune=cortex-a76 -mfpu=neon-fp-armv8")

# Release optimizations for automotive
set(CMAKE_C_FLAGS_RELEASE_INIT "-O2 -DNDEBUG -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O2 -DNDEBUG -ffast-math")

# VideoCore VII GPU support (for QML rendering)
add_compile_definitions(
    RASPBERRY_PI=5
    BCM2712
)

message(STATUS "Target: Raspberry Pi 5 (Cortex-A76)")
