#!/bin/bash
# Check for TODO/FIXME/HACK markers in code
# Warns but doesn't fail - just informational

set -e

# Colors
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

FILES="$@"

if [ -z "$FILES" ]; then
    exit 0
fi

FOUND=0

for file in $FILES; do
    if [ -f "$file" ]; then
        # Search for TODO, FIXME, HACK, XXX markers
        MATCHES=$(grep -n -E '(TODO|FIXME|HACK|XXX):?' "$file" 2>/dev/null || true)

        if [ -n "$MATCHES" ]; then
            if [ $FOUND -eq 0 ]; then
                echo -e "${CYAN}=== TODO/FIXME markers found ===${NC}"
                FOUND=1
            fi
            echo -e "${YELLOW}$file:${NC}"
            echo "$MATCHES" | while read line; do
                echo "  $line"
            done
        fi
    fi
done

if [ $FOUND -eq 1 ]; then
    echo -e "${CYAN}Note: TODO markers found. Consider addressing them.${NC}"
fi

# Always pass - this is informational only
exit 0
