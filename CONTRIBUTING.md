# Contributing to RPi Speeduino Dashboard

## Development Setup

### Prerequisites

```bash
# Ubuntu/Debian/Raspberry Pi OS
sudo apt update
sudo apt install -y \
    python3 python3-pip \
    cppcheck clang-format clang-tidy \
    shellcheck \
    cmake ninja-build
```

### Install Git Hooks

```bash
# Clone repository
git clone git@github.com:Guiimartinho/rpi-speeduino-dashboard.git
cd rpi-speeduino-dashboard

# Install hooks
./.hooks/install-hooks.sh
```

## Git Hooks

This project uses pre-commit and pre-push hooks for code quality enforcement.

### Pre-Commit Hooks (Fast, <10s)

Run automatically on every `git commit`:

| Hook | Description | Config |
|------|-------------|--------|
| `trailing-whitespace` | Remove trailing whitespace | Built-in |
| `end-of-file-fixer` | Ensure files end with newline | Built-in |
| `mixed-line-ending` | Convert to LF line endings | Built-in |
| `check-yaml` | Validate YAML syntax | Built-in |
| `check-json` | Validate JSON syntax | Built-in |
| `check-xml` | Validate XML syntax | Built-in |
| `check-added-large-files` | Block files >500KB | Built-in |
| `check-merge-conflict` | Detect merge conflict markers | Built-in |
| `detect-private-key` | Detect private keys | Built-in |
| `detect-secrets` | Detect hardcoded credentials | `.secrets.baseline` |
| `codespell` | Check spelling in code | `.codespellrc` |
| `shellcheck` | Lint shell scripts | Built-in |
| `black` | Format Python code | Built-in |
| `flake8` | Lint Python code | Built-in |
| `clang-format` | Format C/C++ code | `.clang-format` |
| `cppcheck-quick` | Quick C++ static analysis | `.hooks/cppcheck-quick.sh` |
| `check-nesting-depth` | MISRA 15.6 nesting check (max 3) | `.hooks/check-nesting.py` |
| `check-todos` | List TODO/FIXME markers | `.hooks/check-todos.sh` |
| `qmllint` | Lint QML files | `.hooks/qmllint-check.sh` |

### Pre-Push Hooks (Slower, <2min)

Run automatically on every `git push`:

| Hook | Description | Status |
|------|-------------|--------|
| `unit-tests` | Run C++ GoogleTest tests | Active |
| `python-tests` | Run pytest tests | Active |
| `build-check` | Verify CMake build | Disabled* |
| `clang-tidy` | Deep C++ static analysis | Disabled* |

*Enable in `.hooks/pre-push` when build environment is ready.

## Code Style

### C/C++ (`.clang-format`)

```cpp
// 4-space indentation
// 100 character line limit
// Braces on same line (K&R style)
// Pointer/reference aligned left: int* ptr

if (condition) {
    doSomething();
} else {
    doOther();
}
```

### MISRA C++ Compliance

Key rules enforced:

| Rule | Description | Enforcement |
|------|-------------|-------------|
| 15.6 | Max 3 nesting levels | `check-nesting.py` |
| 5.0 | No implicit conversions | `clang-tidy` |
| 6.4 | Switch must have default | `cppcheck` |
| 0.1 | No unused code | `cppcheck` |

### Python

- Black formatter (line length 100)
- Flake8 linter

### QML

- 4-space indentation
- qmllint validation

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting (no code change)
- `refactor`: Code restructure
- `test`: Add/modify tests
- `chore`: Maintenance

Examples:
```
feat(can_service): add VAG protocol support
fix(hmi): resolve gauge flickering on RPM > 8000
docs: update bring-up guide for Pi 5
test(can_parser): add edge case tests for temperature
```

## Running Hooks Manually

```bash
# Run all pre-commit hooks
pre-commit run --all-files

# Run specific hook
pre-commit run clang-format --all-files
pre-commit run cppcheck-quick --all-files

# Update hooks to latest versions
pre-commit autoupdate

# Skip hooks (emergency only)
git commit --no-verify -m "message"
git push --no-verify
```

## Static Analysis

### cppcheck (quick, pre-commit)

```bash
cppcheck --enable=warning,style,performance \
         --suppress=missingIncludeSystem \
         src/
```

### clang-tidy (thorough, manual/CI)

```bash
# Generate compile_commands.json first
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/**/*.cpp
```

## Testing

### C++ Unit Tests (GoogleTest)

```bash
cd build
ctest --output-on-failure
# or
./src/can_service/tests/test_can_parser
```

### Python Integration Tests

```bash
# Requires vcan0 setup
pytest tests/ -v
```

## Troubleshooting

### Hook fails on clang-format

```bash
# Check clang-format version
clang-format --version
# Need version 14+ for all features
```

### cppcheck not found

```bash
sudo apt install cppcheck
```

### pre-commit not found

```bash
pip3 install --user pre-commit
# Add to PATH
export PATH="$HOME/.local/bin:$PATH"
```

### Bypass failing hook (last resort)

```bash
# Single commit
git commit --no-verify -m "WIP: fixing later"

# Single push
git push --no-verify
```

## Editor Integration

### VS Code

Install extensions:
- C/C++ (Microsoft)
- Clang-Format
- EditorConfig

Settings (`.vscode/settings.json`):
```json
{
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "file",
    "[cpp]": {
        "editor.defaultFormatter": "xaver.clang-format"
    }
}
```

### CLion

- Enable EditorConfig support
- Set ClangFormat as formatter
- Enable clang-tidy inspections

## Questions?

Open an issue on GitHub.
