#!/bin/bash
# CAN Bus Health Monitor
# Monitors CAN interface health and provides diagnostics
#
# Features:
# - Real-time error counter monitoring
# - Bus state detection
# - Automatic alerts on degraded conditions
# - JSON output for integration with monitoring systems

set -euo pipefail

INTERFACE="${1:-can0}"
INTERVAL="${2:-1}"
JSON_OUTPUT="${3:-0}"
LOG_FILE="${4:-}"

# Error counter thresholds
WARNING_THRESHOLD=64
PASSIVE_THRESHOLD=128
BUSOFF_THRESHOLD=255

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_msg() {
    local level="$1"
    local msg="$2"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    if [[ -n "$LOG_FILE" ]]; then
        echo "[$timestamp] [$level] $msg" >> "$LOG_FILE"
    fi

    case "$level" in
        INFO)  echo -e "${BLUE}[$timestamp]${NC} $msg" ;;
        OK)    echo -e "${GREEN}[$timestamp]${NC} $msg" ;;
        WARN)  echo -e "${YELLOW}[$timestamp]${NC} $msg" ;;
        ERROR) echo -e "${RED}[$timestamp]${NC} $msg" ;;
    esac
}

get_can_state() {
    local state
    state=$(ip -details link show "$INTERFACE" 2>/dev/null | grep -oP 'state \K\w+' || echo "UNKNOWN")
    echo "$state"
}

get_error_counters() {
    # Try to get error counters from sysfs
    local tec=0
    local rec=0

    if [[ -f "/sys/class/net/$INTERFACE/can_bittiming/tec" ]]; then
        tec=$(cat "/sys/class/net/$INTERFACE/can_bittiming/tec" 2>/dev/null || echo 0)
    fi

    if [[ -f "/sys/class/net/$INTERFACE/can_bittiming/rec" ]]; then
        rec=$(cat "/sys/class/net/$INTERFACE/can_bittiming/rec" 2>/dev/null || echo 0)
    fi

    # Fallback: parse from ip output
    if [[ "$tec" == "0" ]] && [[ "$rec" == "0" ]]; then
        local ip_output
        ip_output=$(ip -details -statistics link show "$INTERFACE" 2>/dev/null || echo "")

        tec=$(echo "$ip_output" | grep -oP 'tx_errors \K\d+' | head -1 || echo 0)
        rec=$(echo "$ip_output" | grep -oP 'rx_errors \K\d+' | head -1 || echo 0)
    fi

    echo "$tec $rec"
}

get_frame_stats() {
    local rx_packets=0
    local tx_packets=0
    local rx_errors=0
    local tx_errors=0

    if [[ -d "/sys/class/net/$INTERFACE/statistics" ]]; then
        rx_packets=$(cat "/sys/class/net/$INTERFACE/statistics/rx_packets" 2>/dev/null || echo 0)
        tx_packets=$(cat "/sys/class/net/$INTERFACE/statistics/tx_packets" 2>/dev/null || echo 0)
        rx_errors=$(cat "/sys/class/net/$INTERFACE/statistics/rx_errors" 2>/dev/null || echo 0)
        tx_errors=$(cat "/sys/class/net/$INTERFACE/statistics/tx_errors" 2>/dev/null || echo 0)
    fi

    echo "$rx_packets $tx_packets $rx_errors $tx_errors"
}

get_bus_state_name() {
    local tec=$1
    local rec=$2
    local state=$3

    if [[ "$state" == "BUS-OFF" ]] || [[ "$tec" -ge "$BUSOFF_THRESHOLD" ]]; then
        echo "BUS-OFF"
    elif [[ "$tec" -ge "$PASSIVE_THRESHOLD" ]] || [[ "$rec" -ge "$PASSIVE_THRESHOLD" ]]; then
        echo "ERROR-PASSIVE"
    elif [[ "$tec" -ge "$WARNING_THRESHOLD" ]] || [[ "$rec" -ge "$WARNING_THRESHOLD" ]]; then
        echo "ERROR-WARNING"
    else
        echo "ERROR-ACTIVE"
    fi
}

output_json() {
    local state=$1
    local bus_state=$2
    local tec=$3
    local rec=$4
    local rx_packets=$5
    local tx_packets=$6
    local rx_errors=$7
    local tx_errors=$8
    local timestamp
    timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

    cat <<EOF
{
  "timestamp": "$timestamp",
  "interface": "$INTERFACE",
  "link_state": "$state",
  "bus_state": "$bus_state",
  "error_counters": {
    "tec": $tec,
    "rec": $rec
  },
  "statistics": {
    "rx_packets": $rx_packets,
    "tx_packets": $tx_packets,
    "rx_errors": $rx_errors,
    "tx_errors": $tx_errors
  },
  "healthy": $([ "$bus_state" == "ERROR-ACTIVE" ] && echo "true" || echo "false")
}
EOF
}

output_text() {
    local state=$1
    local bus_state=$2
    local tec=$3
    local rec=$4
    local rx_packets=$5
    local tx_packets=$6
    local rx_errors=$7
    local tx_errors=$8

    local state_color="$GREEN"
    case "$bus_state" in
        "ERROR-WARNING") state_color="$YELLOW" ;;
        "ERROR-PASSIVE") state_color="$YELLOW" ;;
        "BUS-OFF")       state_color="$RED" ;;
    esac

    printf "\r%s | State: ${state_color}%-14s${NC} | TEC: %3d | REC: %3d | RX: %8d | TX: %8d | Errors: %d/%d" \
        "$INTERFACE" "$bus_state" "$tec" "$rec" "$rx_packets" "$tx_packets" "$rx_errors" "$tx_errors"
}

monitor_once() {
    local state
    state=$(get_can_state)

    read -r tec rec <<< "$(get_error_counters)"
    read -r rx_packets tx_packets rx_errors tx_errors <<< "$(get_frame_stats)"

    local bus_state
    bus_state=$(get_bus_state_name "$tec" "$rec" "$state")

    if [[ "$JSON_OUTPUT" == "1" ]]; then
        output_json "$state" "$bus_state" "$tec" "$rec" "$rx_packets" "$tx_packets" "$rx_errors" "$tx_errors"
    else
        output_text "$state" "$bus_state" "$tec" "$rec" "$rx_packets" "$tx_packets" "$rx_errors" "$tx_errors"
    fi

    # Log state changes
    if [[ "$bus_state" != "${LAST_BUS_STATE:-ERROR-ACTIVE}" ]]; then
        case "$bus_state" in
            "ERROR-ACTIVE")
                log_msg "OK" "CAN bus recovered to ERROR-ACTIVE state"
                ;;
            "ERROR-WARNING")
                log_msg "WARN" "CAN bus entered ERROR-WARNING state (TEC=$tec, REC=$rec)"
                ;;
            "ERROR-PASSIVE")
                log_msg "WARN" "CAN bus entered ERROR-PASSIVE state (TEC=$tec, REC=$rec)"
                ;;
            "BUS-OFF")
                log_msg "ERROR" "CAN BUS-OFF detected! Automatic recovery should trigger."
                ;;
        esac
        LAST_BUS_STATE="$bus_state"
        export LAST_BUS_STATE
    fi

    echo "$bus_state"
}

show_help() {
    echo "Usage: $0 [INTERFACE] [INTERVAL] [JSON] [LOG_FILE]"
    echo ""
    echo "Arguments:"
    echo "  INTERFACE   CAN interface to monitor (default: can0)"
    echo "  INTERVAL    Update interval in seconds (default: 1)"
    echo "  JSON        Output JSON format (0=text, 1=json, default: 0)"
    echo "  LOG_FILE    Optional log file path"
    echo ""
    echo "Examples:"
    echo "  $0                        # Monitor can0 every second"
    echo "  $0 can0 0.5               # Monitor can0 every 500ms"
    echo "  $0 can0 1 1               # JSON output"
    echo "  $0 can0 1 0 /var/log/can.log  # With logging"
    echo ""
    echo "CAN Bus States:"
    echo "  ERROR-ACTIVE   Normal operation (TEC/REC < 96)"
    echo "  ERROR-WARNING  Warning threshold (TEC/REC >= 96)"
    echo "  ERROR-PASSIVE  Passive mode (TEC/REC >= 128)"
    echo "  BUS-OFF        Controller disconnected (TEC >= 256)"
}

main() {
    if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
        show_help
        exit 0
    fi

    # Check if interface exists
    if ! ip link show "$INTERFACE" &>/dev/null; then
        log_msg "ERROR" "Interface $INTERFACE not found"
        exit 1
    fi

    log_msg "INFO" "Starting CAN health monitor on $INTERFACE (interval: ${INTERVAL}s)"

    if [[ "$JSON_OUTPUT" != "1" ]]; then
        echo ""
        echo "Press Ctrl+C to stop"
        echo ""
    fi

    LAST_BUS_STATE="ERROR-ACTIVE"
    export LAST_BUS_STATE

    while true; do
        monitor_once > /dev/null
        sleep "$INTERVAL"
    done
}

# Handle single-shot mode
if [[ "${SINGLE_SHOT:-0}" == "1" ]]; then
    monitor_once
    echo ""
    exit 0
fi

main "$@"
