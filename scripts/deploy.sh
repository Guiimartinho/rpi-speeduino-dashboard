#!/bin/bash
# deploy.sh - Deploy Speeduino UI to Raspberry Pi / Jetson
# Usage: ./scripts/deploy.sh [--build] [--test] [--run] [--qml-only] [--target rpi|jetson]
#
# ISO 26262 ASIL-B: Automated deployment for embedded targets

set -e

# Configuration - defaults can be overridden via environment or CLI
REMOTE_HOST="${REMOTE_HOST:-raspui}"
REMOTE_USER="${REMOTE_USER:-raspui}"
REMOTE_PATH="${REMOTE_PATH:-/home/${REMOTE_USER}/speeduino-ui}"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_PLATFORM="rpi"  # rpi or jetson

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Parse arguments
BUILD=false
TEST=false
RUN=false
QML_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --build|-b) BUILD=true; shift ;;
        --test|-t) TEST=true; BUILD=true; shift ;;
        --run|-r) RUN=true; shift ;;
        --qml-only|-q) QML_ONLY=true; shift ;;
        --host|-h) REMOTE_HOST="$2"; shift 2 ;;
        --target)
            TARGET_PLATFORM="$2"
            if [[ "$TARGET_PLATFORM" == "jetson" ]]; then
                REMOTE_HOST="${REMOTE_HOST:-jetson}"
                REMOTE_USER="${REMOTE_USER:-nvidia}"
                REMOTE_PATH="/home/${REMOTE_USER}/speeduino-ui"
            fi
            shift 2
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo -e "${CYAN}========================================"
echo "  Speeduino UI - Deploy to ${TARGET_PLATFORM^^}"
echo -e "========================================${NC}"
echo ""

# Determine step count
TOTAL_STEPS=3
$BUILD && TOTAL_STEPS=$((TOTAL_STEPS + 1))
$TEST && TOTAL_STEPS=$((TOTAL_STEPS + 1))
$RUN && TOTAL_STEPS=$((TOTAL_STEPS + 1))
CURRENT_STEP=0

# Test SSH connection
CURRENT_STEP=$((CURRENT_STEP + 1))
echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Testing SSH connection...${NC}"
if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "$REMOTE_HOST" "echo ok" &>/dev/null; then
    echo -e "${RED}ERROR: Cannot connect to $REMOTE_HOST${NC}"
    exit 1
fi
echo -e "${GREEN}  Connected to $REMOTE_HOST${NC}"

# Create remote directory
CURRENT_STEP=$((CURRENT_STEP + 1))
echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Creating remote directory...${NC}"
ssh "$REMOTE_HOST" "mkdir -p $REMOTE_PATH"

# Sync files
CURRENT_STEP=$((CURRENT_STEP + 1))
echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Syncing files...${NC}"

if $QML_ONLY; then
    echo -e "${CYAN}  Syncing QML files only (fast mode)...${NC}"
    rsync -avz --progress \
        --include='*.qml' \
        --include='*/' \
        --exclude='*' \
        "$PROJECT_ROOT/src/hmi_launcher/qml/" \
        "${REMOTE_HOST}:${REMOTE_PATH}/src/hmi_launcher/qml/"
else
    echo -e "${CYAN}  Full project sync...${NC}"
    rsync -avz --progress \
        --exclude='.git' \
        --exclude='build' \
        --exclude='cmake-build-*' \
        --exclude='.claude' \
        --exclude='preview' \
        --exclude='*.exe' \
        --exclude='*.dll' \
        --exclude='nul' \
        "$PROJECT_ROOT/" \
        "${REMOTE_HOST}:${REMOTE_PATH}/"
fi

echo -e "${GREEN}  Sync complete!${NC}"

# Build if requested
if $BUILD; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Building on ${TARGET_PLATFORM^^}...${NC}"

    # Set build options based on target
    BUILD_TYPE="Release"
    BUILD_JOBS=4
    if [[ "$TARGET_PLATFORM" == "jetson" ]]; then
        BUILD_JOBS=6  # Jetson Nano has 4 cores, Orin has 8+
    fi

    ssh "$REMOTE_HOST" "cd $REMOTE_PATH && \
        mkdir -p build && \
        cd build && \
        cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DBUILD_TESTS=ON .. && \
        ninja -j${BUILD_JOBS}"

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  Build successful!${NC}"
    else
        echo -e "${RED}  Build failed!${NC}"
        exit 1
    fi
fi

# Run tests if requested
if $TEST; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Running tests on ${TARGET_PLATFORM^^}...${NC}"

    ssh "$REMOTE_HOST" "cd $REMOTE_PATH/build && ctest --output-on-failure -j4"

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  All tests passed!${NC}"
    else
        echo -e "${RED}  Some tests failed!${NC}"
        # Don't exit on test failure - user may want to inspect
    fi
fi

# Run if requested
if $RUN; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    echo -e "${YELLOW}[$CURRENT_STEP/$TOTAL_STEPS] Launching application...${NC}"
    ssh "$REMOTE_HOST" "$REMOTE_PATH/build/bin/hmi_launcher"
fi

echo ""
echo -e "${GREEN}========================================"
echo "  Deploy complete!"
echo -e "========================================${NC}"
echo ""
echo -e "${CYAN}Quick commands:${NC}"
echo "  ssh $REMOTE_HOST                     - Connect to target"
echo "  ssh $REMOTE_HOST 'cd ~/speeduino-ui && ls' - Check files"
echo ""
echo -e "${CYAN}Deploy options:${NC}"
echo "  ./scripts/deploy.sh                         - Sync only"
echo "  ./scripts/deploy.sh --build                 - Sync + build"
echo "  ./scripts/deploy.sh --test                  - Sync + build + test"
echo "  ./scripts/deploy.sh --build --run           - Sync + build + run"
echo "  ./scripts/deploy.sh --qml-only              - Fast QML sync"
echo "  ./scripts/deploy.sh --target jetson --test  - Deploy to Jetson + test"
echo ""
