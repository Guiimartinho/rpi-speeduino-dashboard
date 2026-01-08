#!/bin/bash
# Configure Raspberry Pi for MCP2515 CAN controller
# Run with sudo

set -e

echo "=== MCP2515 CAN Controller Setup ==="
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Error: This script must be run as root (sudo)"
    exit 1
fi

CONFIG_FILE="/boot/firmware/config.txt"
if [[ ! -f "$CONFIG_FILE" ]]; then
    CONFIG_FILE="/boot/config.txt"
fi

echo "Config file: $CONFIG_FILE"
echo ""

# Backup config
cp "$CONFIG_FILE" "${CONFIG_FILE}.backup.$(date +%Y%m%d_%H%M%S)"

# Check for existing MCP2515 config
if grep -q "mcp2515" "$CONFIG_FILE"; then
    echo "MCP2515 overlay already configured in config.txt"
    echo "Current configuration:"
    grep "mcp2515\|spi" "$CONFIG_FILE"
    exit 0
fi

# Add MCP2515 overlay
echo "Adding MCP2515 overlay configuration..."

cat >> "$CONFIG_FILE" << 'EOF'

# ========================================
# Speeduino CAN Interface (MCP2515)
# ========================================

# Enable SPI
dtparam=spi=on

# MCP2515 CAN controller on SPI0 CE0
# Oscillator: 16MHz (adjust if different)
# Interrupt: GPIO25 (physical pin 22)
dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25

# Optional: Second CAN interface on SPI0 CE1
# dtoverlay=mcp2515-can1,oscillator=16000000,interrupt=24

# Increase SPI buffer size for better CAN performance
dtparam=spidev_bufsiz=65536
EOF

echo ""
echo "Configuration added to $CONFIG_FILE"
echo ""
echo "IMPORTANT: Reboot required for changes to take effect"
echo ""
echo "Hardware connections (MCP2515 to RPi5):"
echo "  VCC  -> 3.3V (pin 1)"
echo "  GND  -> GND (pin 6)"
echo "  CS   -> SPI0 CE0 (pin 24, GPIO8)"
echo "  SO   -> SPI0 MISO (pin 21, GPIO9)"
echo "  SI   -> SPI0 MOSI (pin 19, GPIO10)"
echo "  SCK  -> SPI0 SCLK (pin 23, GPIO11)"
echo "  INT  -> GPIO25 (pin 22)"
echo ""
echo "After reboot, run: sudo ./scripts/setup_can.sh"
echo ""

read -p "Reboot now? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    reboot
fi
