#!/bin/bash
# Install pre-commit and pre-push hooks for RPi Speeduino Dashboard
# Usage: ./.hooks/install-hooks.sh

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=== Installing Git Hooks ===${NC}"

# Get repository root
REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
if [ -z "$REPO_ROOT" ]; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

cd "$REPO_ROOT"

# =============================================================================
# 1. INSTALL PRE-COMMIT (Python package)
# =============================================================================
echo -e "\n${CYAN}[1/5] Checking pre-commit installation...${NC}"

if ! command -v pre-commit &> /dev/null; then
    echo -e "${YELLOW}pre-commit not found. Installing...${NC}"
    if command -v pip3 &> /dev/null; then
        pip3 install --user pre-commit
    elif command -v pip &> /dev/null; then
        pip install --user pre-commit
    else
        echo -e "${RED}Error: pip not found. Install Python and pip first.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}pre-commit: $(pre-commit --version)${NC}"

# =============================================================================
# 2. INSTALL PRE-COMMIT HOOKS
# =============================================================================
echo -e "\n${CYAN}[2/5] Installing pre-commit hooks...${NC}"

pre-commit install --install-hooks
echo -e "${GREEN}Pre-commit hooks installed${NC}"

# =============================================================================
# 3. INSTALL PRE-PUSH HOOK
# =============================================================================
echo -e "\n${CYAN}[3/5] Installing pre-push hook...${NC}"

cp .hooks/pre-push .git/hooks/pre-push
chmod +x .git/hooks/pre-push
echo -e "${GREEN}Pre-push hook installed${NC}"

# =============================================================================
# 4. MAKE HOOK SCRIPTS EXECUTABLE
# =============================================================================
echo -e "\n${CYAN}[4/5] Setting hook permissions...${NC}"

chmod +x .hooks/*.sh
chmod +x .hooks/*.py 2>/dev/null || true
echo -e "${GREEN}Hook scripts are executable${NC}"

# =============================================================================
# 5. CHECK OPTIONAL DEPENDENCIES
# =============================================================================
echo -e "\n${CYAN}[5/5] Checking optional dependencies...${NC}"

# cppcheck
if command -v cppcheck &> /dev/null; then
    echo -e "${GREEN}  cppcheck: $(cppcheck --version | head -1)${NC}"
else
    echo -e "${YELLOW}  cppcheck: not installed (sudo apt install cppcheck)${NC}"
fi

# clang-format
if command -v clang-format &> /dev/null; then
    echo -e "${GREEN}  clang-format: $(clang-format --version | head -1)${NC}"
else
    echo -e "${YELLOW}  clang-format: not installed (sudo apt install clang-format)${NC}"
fi

# clang-tidy
if command -v clang-tidy &> /dev/null; then
    echo -e "${GREEN}  clang-tidy: $(clang-tidy --version | head -1)${NC}"
else
    echo -e "${YELLOW}  clang-tidy: not installed (sudo apt install clang-tidy)${NC}"
fi

# shellcheck
if command -v shellcheck &> /dev/null; then
    echo -e "${GREEN}  shellcheck: $(shellcheck --version | grep version: | head -1)${NC}"
else
    echo -e "${YELLOW}  shellcheck: not installed (sudo apt install shellcheck)${NC}"
fi

# qmllint
if command -v qmllint &> /dev/null || [ -f "/usr/lib/qt6/bin/qmllint" ]; then
    echo -e "${GREEN}  qmllint: available${NC}"
else
    echo -e "${YELLOW}  qmllint: not installed (install Qt6 dev tools)${NC}"
fi

# pytest
if command -v pytest &> /dev/null; then
    echo -e "${GREEN}  pytest: $(pytest --version | head -1)${NC}"
else
    echo -e "${YELLOW}  pytest: not installed (pip install pytest)${NC}"
fi

# =============================================================================
# DONE
# =============================================================================
echo -e "\n${GREEN}=== Hook installation complete ===${NC}"
echo ""
echo -e "Hooks installed:"
echo -e "  ${CYAN}pre-commit${NC} - Runs on every commit (formatting, lint, etc.)"
echo -e "  ${CYAN}pre-push${NC}   - Runs before push (tests, build check)"
echo ""
echo -e "Commands:"
echo -e "  ${CYAN}pre-commit run --all-files${NC}  - Run all pre-commit hooks"
echo -e "  ${CYAN}pre-commit autoupdate${NC}       - Update hook versions"
echo -e "  ${CYAN}git commit --no-verify${NC}      - Skip pre-commit (emergency)"
echo -e "  ${CYAN}git push --no-verify${NC}        - Skip pre-push (emergency)"
echo ""
