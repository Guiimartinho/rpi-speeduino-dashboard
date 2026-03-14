#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# setup_vnc_dev.sh - Configure VNC for remote development access
#
# This script sets up X11 and VNC server for remote access to the Speeduino UI.
# Use this during development to view and interact with the UI via TigerVNC.
#
# Usage:
#   ./setup_vnc_dev.sh start   - Start X11 + VNC server + HMI in dev mode
#   ./setup_vnc_dev.sh stop    - Stop VNC and restore production mode
#   ./setup_vnc_dev.sh status  - Check current mode
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

VNC_PORT=5900
VNC_DISPLAY=":0"

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root (sudo)"
        exit 1
    fi
}

install_dependencies() {
    log_info "Checking dependencies..."

    # Check if x11vnc is installed
    if ! command -v x11vnc &> /dev/null; then
        log_info "Installing x11vnc..."
        apt-get update
        apt-get install -y x11vnc
    fi

    # Check if X server is available
    if ! command -v Xorg &> /dev/null && ! command -v X &> /dev/null; then
        log_warn "X server not found. Installing xserver-xorg..."
        apt-get install -y xserver-xorg xinit
    fi

    log_info "Dependencies OK"
}

start_vnc_mode() {
    check_root
    install_dependencies

    log_info "Starting VNC development mode..."

    # Stop production HMI if running
    if systemctl is-active --quiet hmi_launcher.service; then
        log_info "Stopping production hmi_launcher..."
        systemctl stop hmi_launcher.service
    fi

    # Check if X is running on :0
    if ! xdpyinfo -display :0 &> /dev/null 2>&1; then
        log_info "Starting X server on display :0..."
        # Start X in background
        startx -- :0 vt7 &
        sleep 3
    fi

    # Start x11vnc if not running
    if ! pgrep -x "x11vnc" > /dev/null; then
        log_info "Starting x11vnc on port ${VNC_PORT}..."
        x11vnc -display :0 -rfbport ${VNC_PORT} -forever -shared -nopw -bg -o /tmp/x11vnc.log
    fi

    # Copy dev service if not exists
    if [ ! -f /etc/systemd/system/hmi_launcher_dev.service ]; then
        log_info "Installing development service file..."
        cp /opt/speeduino-ui/systemd/hmi_launcher_dev.service /etc/systemd/system/
        systemctl daemon-reload
    fi

    # Start HMI in dev mode
    log_info "Starting HMI in development mode (X11)..."
    systemctl start hmi_launcher_dev.service

    # Get IP address
    IP_ADDR=$(hostname -I | awk '{print $1}')

    echo ""
    log_info "═══════════════════════════════════════════════════════════════"
    log_info "VNC Development Mode Active!"
    log_info "═══════════════════════════════════════════════════════════════"
    log_info "Connect with VNC client to: ${IP_ADDR}:${VNC_PORT}"
    log_info "Or use: vncviewer ${IP_ADDR}:${VNC_PORT}"
    log_info ""
    log_info "To stop: sudo $0 stop"
    log_info "═══════════════════════════════════════════════════════════════"
}

stop_vnc_mode() {
    check_root

    log_info "Stopping VNC development mode..."

    # Stop dev HMI
    if systemctl is-active --quiet hmi_launcher_dev.service; then
        log_info "Stopping development hmi_launcher..."
        systemctl stop hmi_launcher_dev.service
    fi

    # Stop x11vnc
    if pgrep -x "x11vnc" > /dev/null; then
        log_info "Stopping x11vnc..."
        pkill x11vnc || true
    fi

    # Restore production mode (EGLFS)
    log_info "Restoring production mode (EGLFS)..."
    systemctl start hmi_launcher.service

    log_info "Production mode restored"
}

show_status() {
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "Speeduino HMI Display Mode Status"
    echo "═══════════════════════════════════════════════════════════════"

    # Check production service
    if systemctl is-active --quiet hmi_launcher.service; then
        echo -e "Production (EGLFS): ${GREEN}RUNNING${NC}"
    else
        echo -e "Production (EGLFS): ${RED}STOPPED${NC}"
    fi

    # Check dev service
    if systemctl is-active --quiet hmi_launcher_dev.service; then
        echo -e "Development (X11):  ${GREEN}RUNNING${NC}"
    else
        echo -e "Development (X11):  ${RED}STOPPED${NC}"
    fi

    # Check VNC
    if pgrep -x "x11vnc" > /dev/null; then
        IP_ADDR=$(hostname -I | awk '{print $1}')
        echo -e "VNC Server:         ${GREEN}RUNNING${NC} on ${IP_ADDR}:${VNC_PORT}"
    else
        echo -e "VNC Server:         ${RED}NOT RUNNING${NC}"
    fi

    # Check X server
    if xdpyinfo -display :0 &> /dev/null 2>&1; then
        echo -e "X Server:           ${GREEN}RUNNING${NC} on :0"
    else
        echo -e "X Server:           ${RED}NOT RUNNING${NC}"
    fi

    echo "═══════════════════════════════════════════════════════════════"
    echo ""
}

# Main
case "${1:-}" in
    start)
        start_vnc_mode
        ;;
    stop)
        stop_vnc_mode
        ;;
    status)
        show_status
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        echo ""
        echo "  start  - Start X11 + VNC server + HMI in dev mode"
        echo "  stop   - Stop VNC and restore production EGLFS mode"
        echo "  status - Show current mode status"
        exit 1
        ;;
esac
