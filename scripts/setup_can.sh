#!/bin/bash
# Setup SocketCAN interface for Speeduino UI
# Run with sudo
#
# Features:
# - Automatic MCP2515/MCP2518FD detection
# - Bus-off automatic recovery (restart-ms)
# - Comprehensive diagnostics
# - Loopback testing support

set -euo pipefail

# Configuration
INTERFACE="${1:-can0}"
BITRATE="${2:-500000}"
RESTART_MS="${3:-100}"      # Bus-off auto-recovery delay
SAMPLE_POINT="${4:-0.875}"  # CAN sample point (87.5%)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (sudo)"
        exit 1
    fi
}

load_modules() {
    log_info "Loading CAN kernel modules..."

    local modules=("can" "can_raw" "can_dev" "can_bcm" "can_gw")

    for mod in "${modules[@]}"; do
        if ! lsmod | grep -q "^${mod}"; then
            modprobe "$mod" 2>/dev/null || true
        fi
    done

    log_success "Kernel modules loaded"
}

setup_vcan() {
    log_info "Setting up virtual CAN interface for testing..."

    modprobe vcan

    if ip link show "$INTERFACE" &>/dev/null; then
        ip link set "$INTERFACE" down 2>/dev/null || true
        ip link delete "$INTERFACE" 2>/dev/null || true
    fi

    ip link add dev "$INTERFACE" type vcan
    ip link set "$INTERFACE" up

    log_success "Virtual CAN interface $INTERFACE is up"
    echo ""
    echo "Test with:"
    echo "  Terminal 1: candump $INTERFACE"
    echo "  Terminal 2: cansend $INTERFACE 360#0BB80000"
}

detect_can_controller() {
    log_info "Detecting CAN controller..."

    # Check for existing interface
    if ip link show "$INTERFACE" &>/dev/null; then
        log_success "Interface $INTERFACE already exists"
        return 0
    fi

    # Check for MCP2515 SPI overlay
    if dtoverlay -l 2>/dev/null | grep -q "mcp2515\|mcp251xfd"; then
        log_success "MCP251x overlay already loaded"
        sleep 1
        return 0
    fi

    # Try to detect and load overlay
    log_info "CAN interface not found, attempting to load overlay..."

    # Check which SPI devices exist
    if [[ -d /sys/bus/spi/devices/spi0.0 ]]; then
        # Try MCP2518FD first (CAN FD capable)
        if dtoverlay mcp251xfd-can0 oscillator=40000000 interrupt=25 2>/dev/null; then
            log_success "Loaded MCP2518FD overlay (CAN FD capable)"
            sleep 2
            return 0
        fi

        # Fall back to MCP2515
        if dtoverlay mcp2515-can0 oscillator=16000000 interrupt=25 2>/dev/null; then
            log_success "Loaded MCP2515 overlay"
            sleep 2
            return 0
        fi
    fi

    log_error "Could not detect CAN controller"
    log_info "Ensure the CAN HAT/module is properly connected"
    log_info "For MCP2515: add 'dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25' to /boot/config.txt"
    return 1
}

configure_interface() {
    log_info "Configuring CAN interface..."

    # Bring down if up
    ip link set "$INTERFACE" down 2>/dev/null || true

    # Check if CAN FD capable
    local canfd_support=false
    if ip link set "$INTERFACE" type can fd on 2>/dev/null; then
        canfd_support=true
        ip link set "$INTERFACE" type can fd off
        log_info "CAN FD support detected"
    fi

    # Set bitrate
    log_info "Setting bitrate to $BITRATE bps..."
    ip link set "$INTERFACE" type can bitrate "$BITRATE"

    # Set sample point
    log_info "Setting sample point to $SAMPLE_POINT..."
    ip link set "$INTERFACE" type can sample-point "$SAMPLE_POINT" 2>/dev/null || true

    # Enable automatic bus-off recovery
    log_info "Setting automatic bus-off recovery (restart-ms=$RESTART_MS)..."
    ip link set "$INTERFACE" type can restart-ms "$RESTART_MS"

    # Enable error reporting for diagnostics
    log_info "Enabling listen-only mode check..."
    # Note: listen-only can be enabled with: ip link set can0 type can listen-only on

    # Set TX queue length for high-throughput scenarios
    ip link set "$INTERFACE" txqueuelen 1000

    log_success "Interface configured"
}

bring_up_interface() {
    log_info "Bringing interface up..."

    if ! ip link set "$INTERFACE" up; then
        log_error "Failed to bring interface up"
        show_diagnostics
        return 1
    fi

    # Wait for interface to stabilize
    sleep 0.5

    # Verify state
    local state
    state=$(ip -brief link show "$INTERFACE" 2>/dev/null | awk '{print $2}')

    if [[ "$state" == "UP" || "$state" == "UNKNOWN" ]]; then
        log_success "Interface $INTERFACE is up"
    else
        log_warn "Interface state: $state (expected UP)"
    fi
}

show_diagnostics() {
    echo ""
    echo "=== CAN Interface Diagnostics ==="
    echo ""

    # Interface details
    echo "Interface Details:"
    ip -details link show "$INTERFACE" 2>/dev/null || echo "  Interface not found"
    echo ""

    # Statistics
    echo "Statistics:"
    if [[ -f "/sys/class/net/$INTERFACE/statistics/rx_packets" ]]; then
        echo "  RX packets: $(cat /sys/class/net/$INTERFACE/statistics/rx_packets)"
        echo "  TX packets: $(cat /sys/class/net/$INTERFACE/statistics/tx_packets)"
        echo "  RX errors:  $(cat /sys/class/net/$INTERFACE/statistics/rx_errors)"
        echo "  TX errors:  $(cat /sys/class/net/$INTERFACE/statistics/tx_errors)"
    else
        echo "  Statistics not available"
    fi
    echo ""

    # CAN state (if available)
    echo "CAN State:"
    if command -v ip &>/dev/null; then
        ip -details -statistics link show "$INTERFACE" 2>/dev/null | grep -E "(state|bitrate|sample|restart|rx_error|tx_error|bus-off)" || true
    fi
    echo ""

    # Kernel messages
    echo "Recent kernel messages:"
    dmesg | grep -i "can\|mcp251\|spi" | tail -10 || true
    echo ""
}

run_loopback_test() {
    log_info "Running loopback test..."

    # Enable loopback mode
    ip link set "$INTERFACE" down
    ip link set "$INTERFACE" type can loopback on
    ip link set "$INTERFACE" up

    # Send test frame
    local test_id="7FF"
    local test_data="DEADBEEF"

    log_info "Sending test frame ${test_id}#${test_data}..."

    # Background candump
    timeout 2 candump "$INTERFACE" -n 1 > /tmp/can_loopback_test.txt 2>/dev/null &
    local dump_pid=$!
    sleep 0.5

    # Send frame
    cansend "$INTERFACE" "${test_id}#${test_data}" 2>/dev/null

    # Wait for candump
    wait $dump_pid 2>/dev/null || true

    # Check result
    if grep -q "$test_id" /tmp/can_loopback_test.txt 2>/dev/null; then
        log_success "Loopback test passed"
        rm -f /tmp/can_loopback_test.txt

        # Disable loopback for normal operation
        ip link set "$INTERFACE" down
        ip link set "$INTERFACE" type can loopback off
        ip link set "$INTERFACE" up
        return 0
    else
        log_error "Loopback test failed - no frame received"
        rm -f /tmp/can_loopback_test.txt
        return 1
    fi
}

show_help() {
    echo "Usage: $0 [INTERFACE] [BITRATE] [RESTART_MS] [SAMPLE_POINT]"
    echo ""
    echo "Arguments:"
    echo "  INTERFACE     CAN interface name (default: can0)"
    echo "  BITRATE       CAN bitrate in bps (default: 500000)"
    echo "  RESTART_MS    Bus-off recovery delay in ms (default: 100)"
    echo "  SAMPLE_POINT  CAN sample point 0.0-1.0 (default: 0.875)"
    echo ""
    echo "Examples:"
    echo "  $0                     # Setup can0 at 500kbps"
    echo "  $0 can0 250000         # Setup can0 at 250kbps"
    echo "  $0 vcan0               # Setup virtual CAN for testing"
    echo "  $0 can0 500000 100     # With 100ms bus-off recovery"
    echo ""
    echo "Common bitrates:"
    echo "  125000  - Low speed CAN"
    echo "  250000  - Standard speed"
    echo "  500000  - High speed (Speeduino default)"
    echo "  1000000 - 1 Mbps (CAN FD)"
    echo ""
    echo "Commands after setup:"
    echo "  candump can0           # Monitor all frames"
    echo "  candump can0,0:0       # Monitor with error frames"
    echo "  cansend can0 360#0BB80000  # Send test frame"
    echo "  ip -details link show can0  # Show interface status"
    echo "  ip link set can0 type can restart  # Manual bus-off recovery"
}

# Main
main() {
    echo "=== Speeduino CAN Setup ==="
    echo "Interface:    $INTERFACE"
    echo "Bitrate:      $BITRATE bps"
    echo "Restart-ms:   $RESTART_MS ms"
    echo "Sample point: $SAMPLE_POINT"
    echo ""

    # Handle help
    if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
        show_help
        exit 0
    fi

    check_root
    load_modules

    # Handle virtual CAN
    if [[ "$INTERFACE" == vcan* ]]; then
        setup_vcan
        exit 0
    fi

    # Setup real CAN interface
    detect_can_controller || exit 1
    configure_interface
    bring_up_interface || exit 1

    # Optional loopback test
    if [[ "${RUN_LOOPBACK_TEST:-0}" == "1" ]]; then
        run_loopback_test || log_warn "Loopback test failed, but interface may still work"
    fi

    show_diagnostics

    echo ""
    log_success "CAN interface $INTERFACE is ready"
    echo ""
    echo "Quick test commands:"
    echo "  candump $INTERFACE              # Monitor frames"
    echo "  candump $INTERFACE,0:0,#FFFFFFFF # Monitor with error frames"
    echo "  cansend $INTERFACE 360#0BB80000  # Send test frame"
    echo ""
    echo "Bus-off recovery:"
    echo "  Automatic recovery enabled (${RESTART_MS}ms delay)"
    echo "  Manual: ip link set $INTERFACE type can restart"
    echo ""
}

main "$@"
