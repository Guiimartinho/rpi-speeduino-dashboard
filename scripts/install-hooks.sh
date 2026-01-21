#!/bin/bash
#
# install-hooks.sh
# Installs git hooks for the speeduino-ui-openauto project
#
# Usage: ./scripts/install-hooks.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
HOOKS_SOURCE="$SCRIPT_DIR/git-hooks"
HOOKS_DEST="$PROJECT_ROOT/.git/hooks"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Installing git hooks...${NC}"
echo "Source: $HOOKS_SOURCE"
echo "Destination: $HOOKS_DEST"
echo ""

# Check if we're in a git repository
if [ ! -d "$PROJECT_ROOT/.git" ]; then
    echo -e "${RED}Error: Not a git repository${NC}"
    exit 1
fi

# Check if hooks source exists
if [ ! -d "$HOOKS_SOURCE" ]; then
    echo -e "${RED}Error: Hooks source directory not found: $HOOKS_SOURCE${NC}"
    exit 1
fi

# Install each hook
INSTALLED=0
for hook in "$HOOKS_SOURCE"/*; do
    if [ -f "$hook" ]; then
        hook_name=$(basename "$hook")
        dest="$HOOKS_DEST/$hook_name"

        # Backup existing hook if it exists and is different
        if [ -f "$dest" ]; then
            if ! diff -q "$hook" "$dest" > /dev/null 2>&1; then
                backup="$dest.backup.$(date +%Y%m%d%H%M%S)"
                echo -e "${YELLOW}  Backing up existing $hook_name to $backup${NC}"
                cp "$dest" "$backup"
            fi
        fi

        # Copy the hook
        cp "$hook" "$dest"
        chmod +x "$dest"
        echo -e "${GREEN}  Installed: $hook_name${NC}"
        INSTALLED=$((INSTALLED + 1))
    fi
done

echo ""
if [ $INSTALLED -gt 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Successfully installed $INSTALLED hook(s)!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Hooks installed:"
    ls -la "$HOOKS_DEST"/pre-* 2>/dev/null || true
    echo ""
    echo "To skip hooks temporarily, use:"
    echo "  git commit --no-verify"
    echo "  git push --no-verify"
else
    echo -e "${YELLOW}No hooks found to install${NC}"
fi
