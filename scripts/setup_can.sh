#!/bin/bash
# Setup SocketCAN interface for Speeduino UI
# Run with sudo

set -e

INTERFACE="${1:-can0}"
BITRATE="${2:-500000}"

echo "=== Speeduino CAN Setup ==="
echo "Interface: $INTERFACE"
echo "Bitrate: $BITRATE"
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Error: This script must be run as root (sudo)"
    exit 1
fi

# Load CAN modules
echo "[1/5] Loading kernel modules..."
modprobe can
modprobe can_raw
modprobe can_dev

# Check if using virtual CAN (for testing)
if [[ "$INTERFACE" == vcan* ]]; then
    echo "[2/5] Setting up virtual CAN interface..."
    modprobe vcan
    ip link add dev "$INTERFACE" type vcan 2>/dev/null || true
    ip link set "$INTERFACE" up
    echo "Virtual CAN interface $INTERFACE is up"
    exit 0
fi

# Check for MCP2515 overlay (SPI CAN controller)
if ! ip link show "$INTERFACE" &>/dev/null; then
    echo "[2/5] CAN interface not found, checking for MCP2515..."

    # Check if overlay is loaded
    if ! dtoverlay -l | grep -q mcp2515; then
        echo "Loading MCP2515 overlay..."
        # Default: MCP2515 on SPI0 CE0, 16MHz oscillator, GPIO25 interrupt
        dtoverlay mcp2515-can0 oscillator=16000000 interrupt=25
        sleep 1
    fi
fi

# Configure CAN interface
echo "[3/5] Configuring CAN interface..."

# Bring down if already up
ip link set "$INTERFACE" down 2>/dev/null || true

# Set bitrate and other parameters
ip link set "$INTERFACE" type can bitrate "$BITRATE"

# Optional: Set sample point (default 87.5%)
ip link set "$INTERFACE" type can sample-point 0.875 2>/dev/null || true

# Optional: Enable CAN FD (if supported)
# ip link set "$INTERFACE" type can dbitrate 2000000 fd on

# Optional: Set restart-ms for automatic recovery
ip link set "$INTERFACE" type can restart-ms 100

# Bring interface up
echo "[4/5] Bringing interface up..."
ip link set "$INTERFACE" up

# Verify
echo "[5/5] Verifying..."
ip -details link show "$INTERFACE"

echo ""
echo "=== CAN interface $INTERFACE is ready ==="
echo ""
echo "Test commands:"
echo "  candump $INTERFACE        # Receive frames"
echo "  cansend $INTERFACE 360#0BB80000  # Send test frame"
echo ""
