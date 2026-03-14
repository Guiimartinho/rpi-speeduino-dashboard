#!/usr/bin/env python3
"""
Check for TODO/FIXME/HACK markers in code.
Warns but doesn't fail - informational only.

Cross-platform Python replacement for check-todos.sh
"""

import re
import sys
from pathlib import Path

# Colors for terminal output (works on most terminals)
YELLOW = '\033[0;33m'
CYAN = '\033[0;36m'
NC = '\033[0m'

# Patterns to search for
TODO_PATTERN = re.compile(r'(TODO|FIXME|HACK|XXX):?', re.IGNORECASE)


def check_file(filepath: Path) -> list:
    """
    Check a file for TODO markers.
    Returns list of (line_number, line_content) tuples.
    """
    matches = []

    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, 1):
                if TODO_PATTERN.search(line):
                    matches.append((line_num, line.rstrip()))
    except Exception:
        # Silently skip files we can't read
        pass

    return matches


def main() -> int:
    """Main entry point."""
    if len(sys.argv) < 2:
        return 0

    files = sys.argv[1:]
    found_any = False

    for filepath in files:
        path = Path(filepath)
        if not path.exists() or not path.is_file():
            continue

        matches = check_file(path)

        if matches:
            if not found_any:
                print(f"{CYAN}=== TODO/FIXME markers found ==={NC}")
                found_any = True

            print(f"{YELLOW}{filepath}:{NC}")
            for line_num, content in matches:
                print(f"  {line_num}: {content}")

    if found_any:
        print(f"{CYAN}Note: TODO markers found. Consider addressing them.{NC}")

    # Always pass - this is informational only
    return 0


if __name__ == '__main__':
    sys.exit(main())
