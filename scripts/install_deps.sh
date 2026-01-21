#!/bin/bash
# Install dependencies for Speeduino UI on Raspberry Pi OS (64-bit)
# Run with sudo

set -e

echo "=== Speeduino UI Dependency Installation ==="
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Error: This script must be run as root (sudo)"
    exit 1
fi

# Update package lists
echo "[1/7] Updating package lists..."
apt update

# Build tools
echo "[2/7] Installing build tools..."
apt install -y \
    cmake \
    ninja-build \
    clang \
    clang-format \
    clang-tidy \
    cppcheck \
    git \
    pkg-config

# Qt 6
echo "[3/7] Installing Qt 6..."
apt install -y \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtmultimedia \
    libqt6svg6-dev

# SocketCAN
echo "[4/7] Installing CAN utilities..."
apt install -y \
    can-utils \
    libsocketcan-dev

# ZeroMQ
echo "[5/7] Installing ZeroMQ..."
apt install -y \
    libzmq3-dev \
    libczmq-dev

# Multimedia / GStreamer
echo "[6/7] Installing multimedia libraries..."
apt install -y \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev

# Other libraries
echo "[7/7] Installing other libraries..."
apt install -y \
    libyaml-cpp-dev \
    libmsgpack-dev \
    libgtest-dev \
    libgpiod-dev

echo ""
echo "=== Dependencies installed successfully ==="
echo ""
echo "Next steps:"
echo "  1. Run ./scripts/setup_can.sh to configure CAN interface"
echo "  2. Build the project:"
echo "     mkdir build && cd build"
echo "     cmake -GNinja .."
echo "     ninja"
echo ""
