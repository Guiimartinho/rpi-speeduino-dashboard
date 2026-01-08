# UI Preview Environment

Preview the Speeduino UI on Windows without deploying to Raspberry Pi.

## Quick Start

### Option 1: Qt Online Installer (Recommended)

1. Download Qt Online Installer:
   https://www.qt.io/download-qt-installer

2. Run installer and select:
   - Qt 6.6 or later
   - Qt Quick (QML)
   - MinGW 64-bit

3. Add Qt to PATH (example for Qt 6.6):
   ```
   C:\Qt\6.6.0\mingw_64\bin
   ```

4. Run preview:
   ```cmd
   cd preview
   qml PreviewMain.qml
   ```

### Option 2: aqtinstall (Command Line)

```cmd
pip install aqtinstall
aqt install-qt windows desktop 6.6.0 win64_mingw
set PATH=C:\Qt\6.6.0\mingw_64\bin;%PATH%
qml preview\PreviewMain.qml
```

### Option 3: Qt Design Studio (Visual)

1. Download Qt Design Studio (free):
   https://www.qt.io/product/ui-design-tools

2. Open `preview/PreviewMain.qml`

## Preview Features

### Control Panel

The preview includes a control panel on the right side with:

- **Screen Size**: Test different display resolutions
  - 7" (800x480) - Reference design
  - 10" (1280x800) - Larger displays
  - Custom sizes

- **Engine Simulation**:
  - Idle, Rev, Drive modes
  - RPM/TPS sliders
  - Gear selector

- **Status Toggles**:
  - CAN connection status
  - Check Engine Light (CEL)
  - Overheat warning
  - Reverse gear

- **Live Values**: Real-time mock data display

### Keyboard Shortcuts

| Key     | Action              |
|---------|---------------------|
| R       | Toggle Reverse      |
| C       | Toggle CEL          |
| Space   | Rev to 6000 RPM     |
| Escape  | Return to Idle      |
| F1-F6   | Screen shortcuts    |
| F11     | Toggle fullscreen   |

## File Structure

```
preview/
├── PreviewMain.qml      # Main preview window with controls
├── MockDataProvider.qml # Simulated engine data
└── README.md            # This file
```

## Testing Scenarios

### 1. Normal Operation
- Set to "Idle" mode
- Verify gauges show ~850 RPM, 85°C CLT

### 2. High RPM
- Click "Rev 7K" or use RPM slider
- Verify RPM gauge animates smoothly
- Check warning colors at high RPM

### 3. Driving
- Click "Drive"
- Select gear 3-5
- Verify speed increases based on RPM/gear

### 4. Warning States
- Click "CEL ON" - verify warning banner
- Click "HOT!" - verify overheat warning blinks
- Click "CAN ERR" - verify disconnected state

### 5. Reverse Camera
- Click "REVERSE" or press R
- Verify overlay appears
- Press R again to disengage

### 6. Screen Sizes
- Test each resolution preset
- Verify responsive layout adapts
- Check gauge sizes scale properly

## Troubleshooting

### "Module not found" errors

The preview loads modules from `../src/hmi_launcher/qml/`. Ensure you run from the `preview/` directory:

```cmd
cd C:\...\speeduino-ui-openauto\preview
qml PreviewMain.qml
```

### Qt not in PATH

Add Qt bin directory to PATH:
```cmd
set PATH=C:\Qt\6.6.0\mingw_64\bin;%PATH%
```

Or use full path:
```cmd
C:\Qt\6.6.0\mingw_64\bin\qml.exe PreviewMain.qml
```

### Slow performance

- Disable animations in Theme.qml for testing
- Use Release build of Qt
- Close other GPU-intensive applications

## Development Workflow

1. Edit QML files in `src/hmi_launcher/qml/`
2. Save changes
3. Press Ctrl+R in preview to reload (or restart qml)
4. Test on different resolutions
5. Commit when satisfied

## Notes

- Mock data simulates realistic engine behavior
- Preview doesn't require C++ compilation
- Some features (camera, OpenAuto) show placeholders
- Performance may differ from Raspberry Pi
