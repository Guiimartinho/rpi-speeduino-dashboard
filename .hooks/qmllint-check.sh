#!/bin/bash
# QML linting for Qt6 QML files
# Uses qmllint from Qt6 installation

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Try to find qmllint
QMLLINT=""

# Common Qt6 installation paths
QT_PATHS=(
    "/usr/lib/qt6/bin/qmllint"
    "/usr/bin/qmllint"
    "$HOME/Qt/6.*/gcc_64/bin/qmllint"
    "/opt/Qt/6.*/gcc_64/bin/qmllint"
)

for path in ${QT_PATHS[@]}; do
    # Handle glob patterns
    for expanded in $path; do
        if [ -x "$expanded" ]; then
            QMLLINT="$expanded"
            break 2
        fi
    done
done

# Also try which
if [ -z "$QMLLINT" ]; then
    QMLLINT=$(which qmllint 2>/dev/null || true)
fi

if [ -z "$QMLLINT" ]; then
    echo -e "${YELLOW}Warning: qmllint not found, skipping QML checks...${NC}"
    echo "Install Qt6 development tools to enable QML linting."
    exit 0
fi

FILES="$@"

if [ -z "$FILES" ]; then
    exit 0
fi

ERRORS=0

for file in $FILES; do
    if [ -f "$file" ]; then
        OUTPUT=$("$QMLLINT" "$file" 2>&1 || true)

        # Check for errors (qmllint returns warnings even on success)
        if echo "$OUTPUT" | grep -q "Error:"; then
            echo -e "${RED}QML errors in $file:${NC}"
            echo "$OUTPUT" | grep "Error:"
            ERRORS=1
        fi
    fi
done

if [ $ERRORS -eq 1 ]; then
    echo -e "${RED}qmllint found errors. Please fix before committing.${NC}"
    exit 1
fi

echo -e "${GREEN}qmllint: All QML files OK${NC}"
exit 0
