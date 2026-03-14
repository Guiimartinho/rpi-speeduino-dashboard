#!/bin/bash
# Quick cppcheck for pre-commit (staged files only)
# Runs fast checks suitable for commit-time validation

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Check if cppcheck is installed
if ! command -v cppcheck &> /dev/null; then
    echo -e "${YELLOW}Warning: cppcheck not installed, skipping...${NC}"
    echo "Install with: sudo apt install cppcheck"
    exit 0
fi

# Get files passed as arguments
FILES="$@"

if [ -z "$FILES" ]; then
    exit 0
fi

# Run cppcheck with quick settings
# --enable=warning,style: Basic checks (fast)
# --suppress=missingIncludeSystem: Don't check system includes
# --inline-suppr: Allow inline suppressions
# --quiet: Only show errors
# --error-exitcode=1: Exit with error if issues found

ERRORS=0

for file in $FILES; do
    if [ -f "$file" ]; then
        OUTPUT=$(cppcheck \
            --enable=warning,style,performance \
            --suppress=missingIncludeSystem \
            --suppress=unmatchedSuppression \
            --suppress=useStlAlgorithm \
            --inline-suppr \
            --quiet \
            --template='{file}:{line}: [{severity}] {message} ({id})' \
            "$file" 2>&1)

        if [ -n "$OUTPUT" ]; then
            echo -e "${RED}Issues in $file:${NC}"
            echo "$OUTPUT"
            ERRORS=1
        fi
    fi
done

if [ $ERRORS -eq 1 ]; then
    echo -e "${RED}cppcheck found issues. Please fix before committing.${NC}"
    exit 1
fi

echo -e "${GREEN}cppcheck: All checks passed${NC}"
exit 0
