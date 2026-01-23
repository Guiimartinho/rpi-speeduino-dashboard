#!/usr/bin/env python3
"""
Pre-push hook for speeduino-ui-openauto.
Runs validation before allowing pushes to remote.

Cross-platform Python replacement for pre-push bash script.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

# Colors for terminal output
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'

PROTECTED_BRANCHES = ['main', 'master', 'production']

DEBUG_PATTERNS = [
    r'console\.log',
    r'print\s*\(',
    r'qDebug\s*\(',
    r'\bTODO\b',
    r'\bFIXME\b',
    r'\bHACK\b',
    r'\bXXX\b',
    r'debugger',
]

CONVENTIONAL_PATTERN = re.compile(
    r'^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\(.+\))?!?: .+'
)


def run_cmd(cmd: list, capture: bool = True) -> tuple[int, str]:
    """Run a command and return exit code and output."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=capture,
            text=True,
            timeout=60
        )
        return result.returncode, result.stdout + result.stderr
    except Exception as e:
        return 1, str(e)


def command_exists(cmd: str) -> bool:
    """Check if a command exists in PATH."""
    try:
        if sys.platform == "win32":
            result = subprocess.run(
                ["where", cmd],
                capture_output=True,
                creationflags=subprocess.CREATE_NO_WINDOW if hasattr(subprocess, 'CREATE_NO_WINDOW') else 0
            )
        else:
            result = subprocess.run(["which", cmd], capture_output=True)
        return result.returncode == 0
    except Exception:
        return False


def find_clang_format() -> str:
    """Find clang-format or clang-format-17 executable."""
    for cmd in ["clang-format-17", "clang-format"]:
        if command_exists(cmd):
            return cmd
    return ""


def get_current_branch() -> str:
    """Get the current git branch name."""
    code, output = run_cmd(['git', 'rev-parse', '--abbrev-ref', 'HEAD'])
    return output.strip() if code == 0 else 'unknown'


def get_changed_files(branch: str) -> list:
    """Get list of files changed between remote and local."""
    # Try to get changes from origin
    code, output = run_cmd(['git', 'diff', '--name-only', f'origin/{branch}..HEAD'])
    if code == 0 and output.strip():
        return output.strip().split('\n')

    # Fallback to last 10 commits
    code, output = run_cmd(['git', 'diff', '--name-only', 'HEAD~10..HEAD'])
    if code == 0 and output.strip():
        return output.strip().split('\n')

    return []


def get_commits(branch: str) -> list:
    """Get list of commits being pushed."""
    code, output = run_cmd(['git', 'log', f'origin/{branch}..HEAD', '--format=%s'])
    if code == 0 and output.strip():
        return output.strip().split('\n')

    code, output = run_cmd(['git', 'log', 'HEAD~5..HEAD', '--format=%s'])
    if code == 0 and output.strip():
        return output.strip().split('\n')

    return []


def check_protected_branch(branch: str) -> bool:
    """Check 1: Warn about pushing to protected branches."""
    print(f"\n{YELLOW}Checking branch protection...{NC}")

    if branch in PROTECTED_BRANCHES:
        print(f"{RED}========================================{NC}")
        print(f"{RED}WARNING: Pushing directly to {branch}!{NC}")
        print(f"{RED}========================================{NC}")
        print(f"{YELLOW}Consider using a feature branch and PR instead.{NC}")
        # Note: Can't do interactive prompt in pre-commit framework
        # Just warn and continue
        print(f"{YELLOW}  [WARN] Pushing to protected branch{NC}")
        return True

    print(f"{GREEN}  [OK] Branch check passed{NC}")
    return True


def check_build_exists() -> bool:
    """Check 2: Verify build directory exists."""
    print(f"\n{YELLOW}Checking build configuration...{NC}")

    if Path('CMakeLists.txt').exists():
        build_dir = Path('build')
        if build_dir.exists() and (build_dir / 'CMakeCache.txt').exists():
            print(f"{GREEN}  [OK] Build directory exists{NC}")
        else:
            print(f"{YELLOW}  [SKIP] No build directory (run cmake to create){NC}")
    else:
        print(f"{YELLOW}  [SKIP] No CMakeLists.txt found{NC}")

    return True  # Not a blocker


def check_debug_markers(changed_files: list) -> bool:
    """Check 4: Look for debug code markers."""
    print(f"\n{YELLOW}Checking for debug code markers...{NC}")

    if not changed_files:
        print(f"{YELLOW}  [SKIP] No changed files to check{NC}")
        return True

    warnings = 0
    combined_pattern = '|'.join(DEBUG_PATTERNS)

    for filepath in changed_files:
        if not Path(filepath).exists():
            continue

        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    if re.search(combined_pattern, line):
                        if warnings == 0:
                            print(f"{YELLOW}  Debug markers found (review before release):{NC}")
                        if warnings < 10:  # Limit output
                            print(f"{YELLOW}    {filepath}:{line_num}: {line.strip()[:60]}{NC}")
                        warnings += 1
        except Exception:
            pass

    if warnings == 0:
        print(f"{GREEN}  [OK] No debug markers found{NC}")
    else:
        print(f"{YELLOW}  [WARN] Found {warnings} debug markers{NC}")

    return True  # Warning only, not a blocker


def check_commit_format(commits: list) -> bool:
    """Check 5: Verify conventional commit format."""
    print(f"\n{YELLOW}Checking commit message format...{NC}")

    if not commits:
        print(f"{YELLOW}  [SKIP] No commits to check{NC}")
        return True

    bad_commits = 0
    for msg in commits:
        if not CONVENTIONAL_PATTERN.match(msg):
            if bad_commits == 0:
                print(f"{YELLOW}  Non-conventional commit messages:{NC}")
            if bad_commits < 5:
                print(f"{YELLOW}    - {msg[:70]}{NC}")
            bad_commits += 1

    if bad_commits > 0:
        print(f"{YELLOW}  [WARN] {bad_commits} commits don't follow conventional format{NC}")
    else:
        print(f"{GREEN}  [OK] All commits follow conventional format{NC}")

    return True  # Warning only


def check_conflict_markers(changed_files: list) -> tuple[bool, int]:
    """Check 6: Look for merge conflict markers."""
    print(f"\n{YELLOW}Checking for conflict markers...{NC}")

    if not changed_files:
        print(f"{YELLOW}  [SKIP] No files to check{NC}")
        return True, 0

    errors = 0
    conflict_pattern = re.compile(r'^(<<<<<<<|=======|>>>>>>>)')

    for filepath in changed_files:
        if not Path(filepath).exists():
            continue

        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    if conflict_pattern.match(line):
                        print(f"{RED}  [FAIL] Conflict markers in: {filepath}{NC}")
                        errors += 1
                        break
        except Exception:
            pass

    if errors == 0:
        print(f"{GREEN}  [OK] No conflict markers{NC}")

    return errors == 0, errors


def check_gitignore() -> bool:
    """Check 7: Look for files that should be ignored."""
    print(f"\n{YELLOW}Checking for ignored files...{NC}")

    code, output = run_cmd(['git', 'ls-files', '-i', '--exclude-standard'])

    if code == 0 and output.strip():
        files = output.strip().split('\n')[:5]
        print(f"{YELLOW}  [WARN] These files are tracked but should be ignored:{NC}")
        for f in files:
            print(f"{YELLOW}    - {f}{NC}")
    else:
        print(f"{GREEN}  [OK] No improperly tracked files{NC}")

    return True  # Warning only


def check_cpp_formatting() -> tuple[bool, int]:
    """Check 8: Verify ALL C++ files are formatted before push."""
    print(f"\n{YELLOW}Checking C++ formatting (all files)...{NC}")

    clang_format = find_clang_format()
    if not clang_format:
        print(f"{YELLOW}  [SKIP] clang-format not installed{NC}")
        return True, 0

    # Get all C++ files in the repository
    code, output = run_cmd(['git', 'ls-files', '*.cpp', '*.hpp', '*.h', '*.c'])
    if code != 0 or not output.strip():
        print(f"{YELLOW}  [SKIP] No C++ files found{NC}")
        return True, 0

    cpp_files = output.strip().split('\n')
    errors = 0
    unformatted_files = []

    for filepath in cpp_files:
        if not Path(filepath).exists():
            continue
        code, _ = run_cmd([clang_format, '--dry-run', '--Werror', filepath])
        if code != 0:
            unformatted_files.append(filepath)
            errors += 1

    if errors > 0:
        print(f"{RED}  [FAIL] {errors} file(s) need formatting:{NC}")
        for f in unformatted_files[:10]:  # Show max 10
            print(f"{RED}         - {f}{NC}")
        if len(unformatted_files) > 10:
            print(f"{RED}         ... and {len(unformatted_files) - 10} more{NC}")
        print(f"\n{YELLOW}         Run: {clang_format} -i <file> to fix{NC}")
        print(f"{YELLOW}         Or:  {clang_format} -i $(git ls-files '*.cpp' '*.hpp' '*.h' '*.c'){NC}")
        return False, errors
    else:
        print(f"{GREEN}  [OK] All {len(cpp_files)} C++ files are properly formatted{NC}")
        return True, 0


def main() -> int:
    """Main entry point."""
    branch = get_current_branch()
    print(f"{YELLOW}Running pre-push checks on branch: {branch}{NC}")

    changed_files = get_changed_files(branch)
    commits = get_commits(branch)
    errors = 0

    # Run all checks
    check_protected_branch(branch)
    check_build_exists()

    # Check C++ formatting (blocking check)
    passed, format_errors = check_cpp_formatting()
    if not passed:
        errors += format_errors

    check_debug_markers(changed_files)
    check_commit_format(commits)

    passed, conflict_errors = check_conflict_markers(changed_files)
    if not passed:
        errors += conflict_errors

    check_gitignore()

    # Final result
    print()
    if errors > 0:
        print(f"{RED}========================================{NC}")
        print(f"{RED}Pre-push checks FAILED ({errors} errors){NC}")
        print(f"{RED}========================================{NC}")
        print(f"{YELLOW}Fix the issues above or use 'git push --no-verify' to skip{NC}")
        return 1
    else:
        print(f"{GREEN}========================================{NC}")
        print(f"{GREEN}All pre-push checks passed!{NC}")
        print(f"{GREEN}========================================{NC}")
        return 0


if __name__ == '__main__':
    sys.exit(main())
