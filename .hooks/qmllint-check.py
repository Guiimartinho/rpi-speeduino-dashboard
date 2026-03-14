#!/usr/bin/env python3
"""
QML linting for Qt6 QML files using qmllint.

Cross-platform Python replacement for qmllint-check.sh
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Colors for terminal output
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[0;33m'
NC = '\033[0m'


def find_qmllint() -> str | None:
    """Find qmllint executable."""
    # First try PATH
    qmllint = shutil.which('qmllint')
    if qmllint:
        return qmllint

    # Common Qt6 installation paths (Unix)
    unix_paths = [
        '/usr/lib/qt6/bin/qmllint',
        '/usr/bin/qmllint',
    ]

    # Home directory paths (Unix and Windows)
    home = Path.home()
    home_patterns = [
        home / 'Qt' / '6.*' / 'gcc_64' / 'bin' / 'qmllint',
        home / 'Qt' / '6.*' / 'mingw*' / 'bin' / 'qmllint.exe',
        home / 'Qt' / '6.*' / 'msvc*' / 'bin' / 'qmllint.exe',
    ]

    # Windows specific paths
    windows_paths = [
        Path('C:/Qt/6.*/mingw*/bin/qmllint.exe'),
        Path('C:/Qt/6.*/msvc*/bin/qmllint.exe'),
    ]

    all_patterns = unix_paths + [str(p) for p in home_patterns]
    if sys.platform == 'win32':
        all_patterns.extend([str(p) for p in windows_paths])

    # Check glob patterns
    for pattern in all_patterns:
        import glob
        matches = glob.glob(pattern)
        if matches:
            # Return first valid match
            for match in sorted(matches, reverse=True):  # Prefer newer versions
                if os.path.isfile(match) and os.access(match, os.X_OK):
                    return match
                # On Windows, .exe files are always executable
                if sys.platform == 'win32' and os.path.isfile(match):
                    return match

    return None


def run_qmllint(qmllint_path: str, filepath: str) -> tuple[bool, str]:
    """
    Run qmllint on a file.
    Returns (has_errors, output).
    """
    try:
        result = subprocess.run(
            [qmllint_path, filepath],
            capture_output=True,
            text=True,
            timeout=30
        )
        output = result.stdout + result.stderr

        # Check for actual errors (qmllint returns warnings even on success)
        has_errors = 'Error:' in output

        return has_errors, output
    except subprocess.TimeoutExpired:
        return True, f"Timeout running qmllint on {filepath}"
    except Exception as e:
        return False, f"Failed to run qmllint: {e}"


def main() -> int:
    """Main entry point."""
    if len(sys.argv) < 2:
        return 0

    # Find qmllint
    qmllint_path = find_qmllint()

    if not qmllint_path:
        print(f"{YELLOW}Warning: qmllint not found, skipping QML checks...{NC}")
        print("Install Qt6 development tools to enable QML linting.")
        return 0

    files = sys.argv[1:]
    errors_found = False

    for filepath in files:
        path = Path(filepath)
        if not path.exists() or not path.is_file():
            continue

        has_errors, output = run_qmllint(qmllint_path, filepath)

        if has_errors:
            print(f"{RED}QML errors in {filepath}:{NC}")
            # Extract error lines
            for line in output.splitlines():
                if 'Error:' in line:
                    print(f"  {line}")
            errors_found = True

    if errors_found:
        print(f"{RED}qmllint found errors. Please fix before committing.{NC}")
        return 1

    print(f"{GREEN}qmllint: All QML files OK{NC}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
