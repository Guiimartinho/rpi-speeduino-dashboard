# CMake Toolchain File for NVIDIA Jetson Cross-Compilation
# Target: Jetson Nano (Cortex-A57) / Jetson Orin (Cortex-A78AE)
#
# ISO 26262 ASIL-B: Optimized for automotive dashboard on Jetson
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/jetson.cmake \
#         -DJETSON_PLATFORM=nano \
#         -DCMAKE_BUILD_TYPE=Release ..
#
# JETSON_PLATFORM options: nano, orin
#
# Prerequisites:
#   - NVIDIA JetPack SDK
#   - Cross-compiler from JetPack or aarch64-linux-gnu-gcc

# Include base ARM64 toolchain
include(${CMAKE_CURRENT_LIST_DIR}/aarch64-linux-gnu.cmake)

# Platform selection (default: nano for wider compatibility)
if(NOT DEFINED JETSON_PLATFORM)
    set(JETSON_PLATFORM "nano" CACHE STRING "Jetson platform (nano/orin)")
endif()

if(JETSON_PLATFORM STREQUAL "orin")
    # Jetson Orin: Cortex-A78AE with NVIDIA Ampere GPU
    set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a78 -mtune=cortex-a78")
    set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a78 -mtune=cortex-a78")
    add_compile_definitions(
        NVIDIA_JETSON
        JETSON_ORIN
        TEGRA234  # Orin SoC
    )
    message(STATUS "Target: NVIDIA Jetson Orin (Cortex-A78AE)")
else()
    # Jetson Nano: Cortex-A57 with NVIDIA Maxwell GPU
    set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a57 -mtune=cortex-a57")
    set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a57 -mtune=cortex-a57")
    add_compile_definitions(
        NVIDIA_JETSON
        JETSON_NANO
        TEGRA210  # Nano SoC
    )
    message(STATUS "Target: NVIDIA Jetson Nano (Cortex-A57)")
endif()

# CUDA support (optional - for GPU-accelerated features)
# Set CUDA_TOOLKIT_ROOT_DIR if using CUDA
if(DEFINED ENV{CUDA_TOOLKIT_ROOT_DIR})
    set(CUDA_TOOLKIT_ROOT_DIR $ENV{CUDA_TOOLKIT_ROOT_DIR})
    message(STATUS "CUDA toolkit: ${CUDA_TOOLKIT_ROOT_DIR}")
endif()

# Release optimizations for automotive
set(CMAKE_C_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")

# Jetson-specific paths for JetPack libraries
if(DEFINED ENV{JETPACK_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{JETPACK_ROOT}/lib/aarch64-linux-gnu")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{JETPACK_ROOT}/include")
endif()
