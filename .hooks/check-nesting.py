#!/usr/bin/env python3
"""
MISRA C++ 2008 Rule 6-4-2 / MISRA C 2012 Rule 15.6
Check for excessive nesting depth in C/C++ code.

Maximum allowed nesting: 3 levels (configurable)

Counts:
- if/else if/else
- for
- while
- do-while
- switch
- try-catch
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

# Configuration
MAX_NESTING_DEPTH = 3

# Colors for terminal output
RED = '\033[0;31m'
YELLOW = '\033[0;33m'
GREEN = '\033[0;32m'
NC = '\033[0m'

# Patterns that increase nesting
NESTING_INCREASE = re.compile(
    r'^\s*('
    r'if\s*\(|'
    r'else\s+if\s*\(|'
    r'else\s*\{|'
    r'for\s*\(|'
    r'while\s*\(|'
    r'do\s*\{|'
    r'switch\s*\(|'
    r'try\s*\{|'
    r'catch\s*\('
    r')',
    re.MULTILINE
)

# Pattern for opening brace
OPEN_BRACE = re.compile(r'\{')
CLOSE_BRACE = re.compile(r'\}')


def remove_comments_and_strings(content: str) -> str:
    """Remove comments and string literals to avoid false positives."""
    # Remove single-line comments
    content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
    # Remove multi-line comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Remove string literals (simple approach)
    content = re.sub(r'"(?:[^"\\]|\\.)*"', '""', content)
    content = re.sub(r"'(?:[^'\\]|\\.)*'", "''", content)
    return content


def check_nesting_depth(filepath: Path) -> List[Tuple[int, int, str]]:
    """
    Check nesting depth in a file.
    Returns list of (line_number, depth, context) for violations.
    """
    violations = []

    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"{YELLOW}Warning: Could not read {filepath}: {e}{NC}")
        return []

    # Keep original lines for reporting
    original_lines = content.splitlines()

    # Remove comments and strings for analysis
    clean_content = remove_comments_and_strings(content)
    lines = clean_content.splitlines()

    current_depth = 0
    max_depth_in_function = 0
    brace_stack = []  # Track brace positions
    in_function = False

    for line_num, line in enumerate(lines, 1):
        stripped = line.strip()

        # Skip empty lines and preprocessor directives
        if not stripped or stripped.startswith('#'):
            continue

        # Count braces on this line
        open_count = len(OPEN_BRACE.findall(line))
        close_count = len(CLOSE_BRACE.findall(line))

        # Check for nesting constructs
        if NESTING_INCREASE.search(line):
            current_depth += 1
            if current_depth > MAX_NESTING_DEPTH:
                # Get context from original file
                context = original_lines[line_num - 1].strip()[:60]
                violations.append((line_num, current_depth, context))

        # Track braces for depth management
        for _ in range(open_count):
            brace_stack.append(current_depth)

        for _ in range(close_count):
            if brace_stack:
                prev_depth = brace_stack.pop()
                # If we're closing a nesting construct, decrease depth
                if current_depth > 0 and (not brace_stack or prev_depth >= current_depth):
                    current_depth = max(0, current_depth - 1)

    return violations


def main() -> int:
    """Main entry point."""
    if len(sys.argv) < 2:
        print(f"{YELLOW}Usage: check-nesting.py <file1> [file2] ...{NC}")
        return 0

    files = sys.argv[1:]
    total_violations = 0
    files_with_issues = []

    for filepath in files:
        path = Path(filepath)
        if not path.exists():
            continue

        violations = check_nesting_depth(path)

        if violations:
            files_with_issues.append(filepath)
            print(f"\n{RED}MISRA 15.6 violations in {filepath}:{NC}")
            for line_num, depth, context in violations:
                print(f"  Line {line_num}: depth={depth} (max={MAX_NESTING_DEPTH})")
                print(f"    {context}...")
            total_violations += len(violations)

    if total_violations > 0:
        print(f"\n{RED}Found {total_violations} nesting violation(s) in {len(files_with_issues)} file(s).{NC}")
        print(f"{YELLOW}MISRA 15.6: Nesting depth should not exceed {MAX_NESTING_DEPTH} levels.{NC}")
        print(f"{YELLOW}Consider refactoring into smaller functions.{NC}")
        return 1

    print(f"{GREEN}MISRA 15.6: All nesting checks passed{NC}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
