#!/usr/bin/env python3
"""
Quick cppcheck for pre-commit (staged files only).
Runs fast checks suitable for commit-time validation.

Cross-platform Python replacement for cppcheck-quick.sh
"""

import shutil
import subprocess
import sys
from pathlib import Path

# Colors for terminal output
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[0;33m'
NC = '\033[0m'


def find_cppcheck() -> str | None:
    """Find cppcheck executable."""
    return shutil.which('cppcheck')


def run_cppcheck(cppcheck_path: str, filepath: str) -> tuple[bool, str]:
    """
    Run cppcheck on a file.
    Returns (has_issues, output).
    """
    args = [
        cppcheck_path,
        '--enable=warning,style,performance',
        '--suppress=missingIncludeSystem',
        '--suppress=unmatchedSuppression',
        '--suppress=useStlAlgorithm',
        '--inline-suppr',
        '--quiet',
        '--template={file}:{line}: [{severity}] {message} ({id})',
        filepath
    ]

    try:
        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=60
        )
        # cppcheck outputs to stderr
        output = result.stderr.strip()
        has_issues = bool(output)
        return has_issues, output
    except subprocess.TimeoutExpired:
        return True, f"Timeout running cppcheck on {filepath}"
    except Exception as e:
        return False, f"Failed to run cppcheck: {e}"


def main() -> int:
    """Main entry point."""
    if len(sys.argv) < 2:
        return 0

    # Find cppcheck
    cppcheck_path = find_cppcheck()

    if not cppcheck_path:
        print(f"{YELLOW}Warning: cppcheck not installed, skipping...{NC}")
        print("Install with: sudo apt install cppcheck (Linux) or choco install cppcheck (Windows)")
        return 0

    files = sys.argv[1:]
    errors_found = False

    for filepath in files:
        path = Path(filepath)
        if not path.exists() or not path.is_file():
            continue

        has_issues, output = run_cppcheck(cppcheck_path, filepath)

        if has_issues:
            print(f"{RED}Issues in {filepath}:{NC}")
            print(output)
            errors_found = True

    if errors_found:
        print(f"{RED}cppcheck found issues. Please fix before committing.{NC}")
        return 1

    print(f"{GREEN}cppcheck: All checks passed{NC}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
