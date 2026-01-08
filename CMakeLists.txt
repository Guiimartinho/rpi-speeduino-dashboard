cmake_minimum_required(VERSION 3.20)
project(speeduino_ui VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -Wno-unused-parameter
)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)

# yaml-cpp
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

# MessagePack (header-only, check if available)
find_path(MSGPACK_INCLUDE_DIR msgpack.hpp
    HINTS /usr/include /usr/local/include
)

# Qt6 (for hmi_launcher)
find_package(Qt6 COMPONENTS Core Quick Multimedia QUIET)

# GTest for unit tests
find_package(GTest QUIET)

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

# Summary
message(STATUS "")
message(STATUS "=== Speeduino UI Build Configuration ===")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:   ${CMAKE_CXX_COMPILER}")
message(STATUS "ZeroMQ:         ${ZMQ_VERSION}")
message(STATUS "yaml-cpp:       ${YAML_CPP_VERSION}")
message(STATUS "Qt6:            ${Qt6_FOUND}")
message(STATUS "GTest:          ${GTest_FOUND}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
message(STATUS "=========================================")
message(STATUS "")
