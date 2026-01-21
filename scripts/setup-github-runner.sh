#!/bin/bash
# =============================================================================
# GitHub Actions Self-Hosted Runner Setup for Raspberry Pi 5
# =============================================================================
# This script sets up a self-hosted GitHub Actions runner on Raspberry Pi 5
#
# Usage:
#   ./setup-github-runner.sh <GITHUB_REPO_URL> <RUNNER_TOKEN>
#
# Example:
#   ./setup-github-runner.sh https://github.com/Guiimartinho/rpi-speeduino-dashboard ABC123TOKEN
#
# Get runner token from:
#   GitHub Repo → Settings → Actions → Runners → New self-hosted runner
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
RUNNER_VERSION="2.311.0"
RUNNER_DIR="/opt/github-runner"
RUNNER_USER="runner"

echo -e "${CYAN}"
echo "=============================================="
echo "  GitHub Actions Runner Setup - Raspberry Pi 5"
echo "=============================================="
echo -e "${NC}"

# =============================================================================
# Check arguments
# =============================================================================
if [ "$#" -lt 2 ]; then
    echo -e "${RED}Error: Missing arguments${NC}"
    echo ""
    echo "Usage: $0 <GITHUB_REPO_URL> <RUNNER_TOKEN>"
    echo ""
    echo "Example:"
    echo "  $0 https://github.com/Guiimartinho/rpi-speeduino-dashboard ABC123TOKEN"
    echo ""
    echo "Get runner token from:"
    echo "  GitHub Repo → Settings → Actions → Runners → New self-hosted runner"
    exit 1
fi

GITHUB_REPO_URL="$1"
RUNNER_TOKEN="$2"
RUNNER_NAME="${3:-rpi5-$(hostname)}"

# Extract owner/repo from URL
REPO_PATH=$(echo "$GITHUB_REPO_URL" | sed 's|https://github.com/||' | sed 's|\.git$||')

echo -e "${CYAN}[1/8] Checking system...${NC}"
echo "  Architecture: $(uname -m)"
echo "  OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "  Repository: $REPO_PATH"
echo "  Runner name: $RUNNER_NAME"

# Check architecture
if [ "$(uname -m)" != "aarch64" ]; then
    echo -e "${YELLOW}Warning: This script is designed for ARM64 (aarch64)${NC}"
fi

# =============================================================================
# Install dependencies
# =============================================================================
echo -e "\n${CYAN}[2/8] Installing dependencies...${NC}"

sudo apt-get update
sudo apt-get install -y \
    curl \
    jq \
    libicu-dev \
    libssl-dev \
    libkrb5-dev \
    zlib1g-dev

# =============================================================================
# Install build dependencies for the project
# =============================================================================
echo -e "\n${CYAN}[3/8] Installing project build dependencies...${NC}"

sudo apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    git \
    pkg-config \
    libzmq3-dev \
    libyaml-cpp-dev \
    libgtest-dev \
    can-utils \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtmultimedia \
    libgpiod-dev

# =============================================================================
# Create runner user
# =============================================================================
echo -e "\n${CYAN}[4/8] Creating runner user...${NC}"

if ! id "$RUNNER_USER" &>/dev/null; then
    sudo useradd -m -s /bin/bash "$RUNNER_USER"
    sudo usermod -aG sudo "$RUNNER_USER"
    sudo usermod -aG dialout "$RUNNER_USER"  # For CAN access
    sudo usermod -aG gpio "$RUNNER_USER"     # For GPIO access
    echo -e "${GREEN}Created user: $RUNNER_USER${NC}"
else
    echo -e "${YELLOW}User $RUNNER_USER already exists${NC}"
fi

# =============================================================================
# Download and extract runner
# =============================================================================
echo -e "\n${CYAN}[5/8] Downloading GitHub Actions runner...${NC}"

sudo mkdir -p "$RUNNER_DIR"
sudo chown "$RUNNER_USER:$RUNNER_USER" "$RUNNER_DIR"

cd "$RUNNER_DIR"

# Download runner for ARM64
RUNNER_ARCHIVE="actions-runner-linux-arm64-${RUNNER_VERSION}.tar.gz"
RUNNER_URL="https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/${RUNNER_ARCHIVE}"

if [ ! -f "$RUNNER_ARCHIVE" ]; then
    echo "Downloading from: $RUNNER_URL"
    sudo -u "$RUNNER_USER" curl -o "$RUNNER_ARCHIVE" -L "$RUNNER_URL"
fi

echo "Extracting runner..."
sudo -u "$RUNNER_USER" tar xzf "$RUNNER_ARCHIVE"

# =============================================================================
# Configure runner
# =============================================================================
echo -e "\n${CYAN}[6/8] Configuring runner...${NC}"

sudo -u "$RUNNER_USER" ./config.sh \
    --url "$GITHUB_REPO_URL" \
    --token "$RUNNER_TOKEN" \
    --name "$RUNNER_NAME" \
    --labels "self-hosted,linux,arm64,rpi5" \
    --work "_work" \
    --replace \
    --unattended

# =============================================================================
# Install as systemd service
# =============================================================================
echo -e "\n${CYAN}[7/8] Installing systemd service...${NC}"

sudo ./svc.sh install "$RUNNER_USER"
sudo ./svc.sh start

# Check status
sleep 2
sudo ./svc.sh status

# =============================================================================
# Configure system for CI
# =============================================================================
echo -e "\n${CYAN}[8/8] Configuring system for CI...${NC}"

# Enable vcan module on boot
if ! grep -q "vcan" /etc/modules-load.d/can.conf 2>/dev/null; then
    echo "vcan" | sudo tee /etc/modules-load.d/can.conf
    echo -e "${GREEN}Enabled vcan module on boot${NC}"
fi

# Load vcan now
sudo modprobe vcan 2>/dev/null || true

# Create sudoers rule for runner (for vcan setup)
SUDOERS_FILE="/etc/sudoers.d/github-runner"
if [ ! -f "$SUDOERS_FILE" ]; then
    cat << 'EOF' | sudo tee "$SUDOERS_FILE"
# Allow github runner to manage virtual CAN interfaces
runner ALL=(ALL) NOPASSWD: /sbin/ip link add dev vcan* type vcan
runner ALL=(ALL) NOPASSWD: /sbin/ip link set vcan* up
runner ALL=(ALL) NOPASSWD: /sbin/ip link set vcan* down
runner ALL=(ALL) NOPASSWD: /sbin/ip link del vcan*
runner ALL=(ALL) NOPASSWD: /sbin/modprobe vcan
runner ALL=(ALL) NOPASSWD: /sbin/modprobe can
runner ALL=(ALL) NOPASSWD: /sbin/modprobe can_raw
EOF
    sudo chmod 440 "$SUDOERS_FILE"
    echo -e "${GREEN}Created sudoers rules for runner${NC}"
fi

# =============================================================================
# Summary
# =============================================================================
echo -e "\n${GREEN}"
echo "=============================================="
echo "  Runner Setup Complete!"
echo "=============================================="
echo -e "${NC}"
echo ""
echo "Runner Information:"
echo "  Name:       $RUNNER_NAME"
echo "  Labels:     self-hosted, linux, arm64, rpi5"
echo "  Directory:  $RUNNER_DIR"
echo "  User:       $RUNNER_USER"
echo ""
echo "Service Commands:"
echo "  Status:  sudo $RUNNER_DIR/svc.sh status"
echo "  Start:   sudo $RUNNER_DIR/svc.sh start"
echo "  Stop:    sudo $RUNNER_DIR/svc.sh stop"
echo "  Restart: sudo $RUNNER_DIR/svc.sh stop && sudo $RUNNER_DIR/svc.sh start"
echo ""
echo "Logs:"
echo "  sudo journalctl -u actions.runner.${REPO_PATH/\//-}.${RUNNER_NAME}.service -f"
echo ""
echo "Next Steps:"
echo "  1. Check runner status on GitHub:"
echo "     $GITHUB_REPO_URL/settings/actions/runners"
echo "  2. Push to main branch to trigger build"
echo ""
