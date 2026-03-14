# HMI Roadmap - Visual Improvements

This document outlines planned visual improvements for the Speeduino UI dashboard, inspired by research on OpenAuto Pro, FuelTech FT600, Speeduino/TunerStudio, and MegaSquirt dashboards.

## Features to Implement

### 1. Progressive Shift Bar (Download-style)

A horizontal progress bar that fills up as RPM approaches the optimal shift point - similar to a download progress bar.

**Concept:**
```
RPM: 3000                                    RPM: 6500 (shift point)
[██████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░]  45%

RPM: 5500
[██████████████████████████████████░░░░░░]  85%

RPM: 6400 (near shift point)
[████████████████████████████████████████]  98% + FLASH/PULSE
```

**Visual behavior:**
- Green → Yellow → Red color transition as RPM increases
- Pulse/flash animation when reaching shift point
- Configurable shift RPM threshold
- Smooth animation (not stepped LEDs)

---

### 2. TUNING Mode with VE Table Mini & AFR Histogram

A dedicated tuning screen with real-time data visualization for ECU tuning sessions.

**Components:**
- **VE Table Mini**: 8x8 or 16x16 grid preview of VE table
- **AFR Histogram**: Dual bar showing Target vs Actual AFR
- **Live data**: RPM, MAP, TPS, ADV, PW, CLT, IAT

---

### 3. G-Force Meter (Track Days)

Circular visualization showing lateral and longitudinal acceleration.

**Concept:**
```
        ↑ Braking
        │
   ←────●────→  Lateral
        │
        ↓ Acceleration

- Dot moves based on accelerometer data
- Trail showing last 2-3 seconds of movement
- Configurable scale (1G, 2G, 3G)
```

---

### 4. VE Table Trace (TunerStudio-inspired)

Mini-preview of the VE table on the dashboard with real-time position tracking.

**Features:**
- Mini-preview of VE table (8x8 grid minimum)
- Animated cursor showing current position (RPM x MAP intersection)
- Blue trail showing the path traveled over last 5-10 seconds
- Cell highlighting based on current interpolation zone
- Optional: Cell colors based on hit count or VE values

**Concept:**
```
┌─────────────────────────┐
│  VE TABLE (16x16)       │
│  ┌─┬─┬─┬─┬─┬─┬─┬─┐      │
│  ├─┼─┼─┼─┼─┼─┼─┼─┤      │
│  ├─┼─┼─●─┼─┼─┼─┼─┤  ← Current position (●)
│  ├─┼─┼─┼─┼─┼─┼─┼─┤      │
│  ├─┼─┼─┼─┼─┼─┼─┼─┤      │  ~~~~ = Trail
│  └─┴─┴─┴─┴─┴─┴─┴─┘      │
│  RPM →    MAP ↑         │
└─────────────────────────┘
```

---

### 5. Multi-Screen Navigation (FuelTech-inspired)

4 pre-configured layouts with swipe or button navigation.

**Screens:**
1. **SPORT** - Racing-focused with shift bar and large gauges
2. **STREET** - Minimalist daily driving view
3. **TUNING** - Data-dense for ECU tuning sessions
4. **DIAGNOSTIC** - Full grid with all parameters

**Navigation:**
- Swipe left/right to change screens
- Bottom tab bar with screen indicators
- Optional: Physical button support via GPIO

---

## Layouts

### Layout 1: SPORT (Racing Mode)

Optimized for track use with prominent shift indicator and essential gauges.

```
┌─────────────────────────────────────────────────────────────┐
│ [████████████████████████████░░░░░░░░░░░░░░] SHIFT BAR      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌───────────────┐    ┌────────┐    ┌───────────────┐     │
│   │               │    │        │    │               │     │
│   │     RPM       │    │  GEAR  │    │    SPEED      │     │
│   │   ◐ Arc       │    │   4    │    │     185       │     │
│   │    6500       │    │        │    │    km/h       │     │
│   │               │    │        │    │               │     │
│   └───────────────┘    └────────┘    └───────────────┘     │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│   CLT   │   AFR   │  BOOST  │   OIL   │   MAP   │  BATT    │
│   85°C  │  14.2   │  1.2bar │  4.2bar │  120kPa │  13.8V   │
└─────────────────────────────────────────────────────────────┘
```

**Components:**
- Progressive shift bar (top, full width)
- Large RPM arc gauge (left)
- Gear indicator (center, prominent)
- Digital speed display (right)
- 6 compact metric cards (bottom row)

---

### Layout 2: STREET (Daily Driving)

Clean, minimalist view for everyday use.

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   RPM [████████████████████░░░░░░░░░░░░░░░░░░░░░░] 4500     │
│                                                             │
│                    ┌─────────────────┐                      │
│                    │                 │                      │
│                    │       120       │                      │
│                    │      km/h       │                      │
│                    │                 │                      │
│                    └─────────────────┘                      │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│      4      │     85°C     │     14.7     │     13.8V      │
│    GEAR     │      CLT     │      AFR     │      BATT      │
└─────────────────────────────────────────────────────────────┘
```

**Components:**
- Horizontal RPM bar with value (top)
- Giant speed display (center, dominant)
- 4 essential metrics (bottom row)

---

### Layout 3: TUNING (ECU Tuning Session)

Data-dense layout for tuning with VE table visualization.

```
┌─────────────────────────────────────────────────────────────┐
│   RPM: 3500   │   MAP: 98 kPa   │   TPS: 45%   │  VE: 78%  │
├───────────────────────────────────┬─────────────────────────┤
│                                   │                         │
│   ┌───────────────────────────┐   │   AFR:  14.2  (T:14.7) │
│   │                           │   │   ADV:  28° BTDC       │
│   │       VE TABLE            │   │   PW1:  4.2 ms         │
│   │       (16x16)             │   │   IAT:  32°C           │
│   │          ●                │   │   CLT:  85°C           │
│   │       (cursor)            │   │   EGO:  1.02           │
│   │                           │   │   SYNC: ●              │
│   └───────────────────────────┘   │                         │
│                                   │                         │
├───────────────────────────────────┴─────────────────────────┤
│   AFR ACTUAL  [████████████████░░░░░░░░░░░░░░░░░░░] 14.2    │
│   AFR TARGET  [██████████████████░░░░░░░░░░░░░░░░░] 14.7    │
└─────────────────────────────────────────────────────────────┘
```

**Components:**
- Top row: Key parameters (RPM, MAP, TPS, VE%)
- Left: VE Table mini with cursor and trail
- Right: Tuning-critical values (AFR, ADV, PW, temps)
- Bottom: Dual AFR histogram (actual vs target)

---

### Layout 4: DIAGNOSTIC (Full Grid)

Complete parameter grid for diagnostics and troubleshooting.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│   RPM    │   MAP    │   TPS    │   CLT    │   IAT    │   AFR    │
│   3500   │  98 kPa  │   45%    │   85°C   │   32°C   │   14.2   │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│   ADV    │   PW1    │   INJ%   │   OIL P  │  FUEL P  │   BATT   │
│   28°    │  4.2 ms  │   45%    │  4.2 bar │  3.8 bar │  13.8V   │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│   VE%    │  TARGET  │   EGO    │   SYNC   │   FAN    │   PUMP   │
│    78    │   14.7   │   1.02   │    ●     │    ●     │    ●     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│  BOOST   │  B.TGT   │  B.DUTY  │   VVT1   │   VVT2   │  ERRORS  │
│  0.8 bar │  1.0 bar │   65%    │   12°    │   8°     │    0     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│  F.TRM1  │  F.TRM2  │  F.TRM3  │  F.TRM4  │  IDLE T  │  IDLE D  │
│   +2%    │   -1%    │   +1%    │   0%     │  850 RPM │   35%    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
```

**Components:**
- 6x5 grid (30 parameters)
- Color-coded values (green=OK, yellow=warning, red=danger)
- Status indicators for binary values (SYNC, FAN, PUMP)
- Per-cylinder fuel trims
- Idle control parameters

---

## Color Palette

### Primary Theme (Dark)

| Element | Color | Hex |
|---------|-------|-----|
| Background | Pure Black | `#000000` |
| Surface | Dark Gray | `#1A1A1A` |
| Card | Medium Gray | `#2A2A2A` |
| Text Primary | White | `#FFFFFF` |
| Text Secondary | Light Gray | `#B0B0B0` |
| Accent (Speeduino) | Neon Green | `#00FF00` |
| Warning | Yellow/Orange | `#FFD600` |
| Danger | Red | `#FF0000` |
| Info | Blue | `#00AAFF` |

### Shift Bar Colors (Gradient)

| RPM % | Color | Hex |
|-------|-------|-----|
| 0-60% | Green | `#00FF00` |
| 60-80% | Yellow | `#FFFF00` |
| 80-95% | Orange | `#FF8800` |
| 95-100% | Red | `#FF0000` |
| 100%+ | Red Flash | `#FF0000` (pulsing) |

---

## Implementation Priority

1. **Phase 1**: Multi-screen navigation infrastructure
2. **Phase 2**: Progressive Shift Bar component
3. **Phase 3**: SPORT and STREET layouts
4. **Phase 4**: VE Table Mini component
5. **Phase 5**: AFR Histogram component
6. **Phase 6**: TUNING layout
7. **Phase 7**: G-Force Meter component
8. **Phase 8**: DIAGNOSTIC layout refinement

---

## References

- OpenAuto Pro Dashboard Design
- FuelTech FT600 Multi-screen System
- TunerStudio Dashboard Designer
- MegaSquirt Community Dashboards
- RealDash Gallery Inspirations

---

*Document created: 2024-01-30*
*Last updated: 2024-01-30*
