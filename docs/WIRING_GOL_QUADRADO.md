# Reverse Camera Installation - VW Gol Quadrado AP 1.8

This guide covers the installation of the reverse gear detection system for
VW Gol Quadrado (and other classic Brazilian VW cars) with AP 1.8 engine,
both naturally aspirated and turbocharged versions with Speeduino.

## System Overview

```
+-------------------------------------------------------------------------+
|                    GOL QUADRADO - REVERSE SYSTEM                         |
+-------------------------------------------------------------------------+
|                                                                         |
|   GEARBOX                           RASPBERRY PI 4                      |
|   +-----------+                     +-----------+                       |
|   | Reverse   |    INTERFACE        |           |                       |
|   | Switch    |--->(12V->3.3V)----->|  GPIO 17  |                       |
|   | (+12V)    |    CIRCUIT          |           |                       |
|   +-----------+                     +-----+-----+                       |
|         |                                 |                             |
|         |                                 v                             |
|         |                         +-----------+                         |
|         |                         |  Reverse  |                         |
|         v                         |  Camera   |                         |
|   +-----------+                   +-----------+                         |
|   | Reverse   |                                                         |
|   | Light     |                                                         |
|   +-----------+                                                         |
|                                                                         |
+-------------------------------------------------------------------------+
```

## Locating the Reverse Signal

### Gearbox Reverse Switch

```
    TOP VIEW OF ENGINE (Gol Quadrado)

              FRONT OF CAR
                   ^
    +------------------------------+
    |                              |
    |     +----------------+       |
    |     |                |       |
    |     |   AP ENGINE    |       |
    |     |                |       |
    |     +-------+--------+       |
    |             |                |
    |      +------+------+         |
    |      |   GEARBOX   |         |
    |      |             |         |
    |   -->| (X) <- REVERSE SWITCH |
    |      |             |         |
    |      +-------------+         |
    |                              |
    +------------------------------+

    Location: LEFT side of gearbox
              (looking from above, driver's side)
```

### Wire Colors (Reference)

| Function            | Common Color        | Alternative      |
|---------------------|---------------------|------------------|
| Reverse Signal (+12V)| Black/Green        | Green            |
| Ground (GND)        | Brown               | Black            |

**IMPORTANT:** Always confirm with a multimeter before connecting!

## Multimeter Test

```
    TEST PROCEDURE

    1. Locate the reverse switch connector

    2. With ignition ON (engine can be off):

       +---------------------------------------------+
       |  MULTIMETER in DC Volts mode (20V)         |
       |                                            |
       |  Red Probe   -> Switch wire                |
       |  Black Probe -> Chassis (GND)              |
       |                                            |
       |  NEUTRAL:    ~0V                           |
       |  REVERSE ENGAGED: ~12-14V                  |
       +---------------------------------------------+

    3. If reading ~12V with reverse engaged, you found the correct wire!
```

## Interface Circuit (12V to 3.3V)

### OPTION A: PC817 Optocoupler (RECOMMENDED)

Full galvanic isolation - safer for Raspberry Pi.

```
    ELECTRICAL SCHEMATIC - OPTOCOUPLER


        CAR SIDE (12V)                |            RASPBERRY SIDE (3.3V)
                                      |
                                      |
    Reverse Signal ------+            |
    (+12V)               |            |
                        +-+           |
                        | | R1        |
                        | | 1K        |
                        +-+           |
                         |            |                    3.3V (Pin 1)
                         |    +-------+-------------------------+----
                         |    |  PC817|                         |
                         v    | +-----+   |                    +-+
                       --+--  | |1   4|   |                    | | R2
                         |    | | o---+---+--------------------+ | 10K
                      [LED]   | |     |   |                    +-+
                         |    | |2   3|   |                     |
                       --+--  | | o   |   |                     +------ GPIO17 (Pin 11)
                         |    | +--+--+   |                     |
                         |    |    |      |                     |
    Car GND -------------+----+----+      |                    GND (Pin 9)
                              |           |
                              |           |

    PC817 PINOUT:
    +--------+
    | 1    4 |   1 = LED Anode (input +)
    | o    o |   2 = LED Cathode (input -)
    |        |   3 = Phototransistor Emitter
    | o    o |   4 = Phototransistor Collector
    | 2    3 |
    +--------+
```

**Component List:**
- 1x PC817 or 4N25 Optocoupler
- 1x 1K 1/4W Resistor
- 1x 10K 1/4W Resistor
- Wires, connectors, heat shrink tubing

### OPTION B: Voltage Divider with Zener

Simpler, but without isolation.

```
    ELECTRICAL SCHEMATIC - VOLTAGE DIVIDER


    Reverse Signal (+12V)
         |
         |
        +-+
        | | R1 = 10K
        | |
        +-+
         |
         +-------------------------> GPIO17 (Pin 11)
         |
        +-+
        | | R2 = 3.3K
        | |
        +-+
         |
        -+-  D1 = Zener 3.3V
        ---  (extra protection)
         |
         |
        GND


    CALCULATION:
    Vout = Vin x R2/(R1+R2)
    Vout = 12V x 3.3K/(10K+3.3K)
    Vout = 12V x 0.248
    Vout = 2.98V (safe for GPIO)
```

**Component List:**
- 1x 10K 1/4W Resistor
- 1x 3.3K 1/4W Resistor
- 1x 3.3V 500mW Zener Diode
- Wires, connectors

## Raspberry Pi 4 Pinout

```
    GPIO HEADER - RASPBERRY PI 4

           3.3V  (1) (2)  5V
    GPIO2  SDA1  (3) (4)  5V
    GPIO3  SCL1  (5) (6)  GND
          GPIO4  (7) (8)  GPIO14 TXD
            GND  (9) (10) GPIO15 RXD
   ====> GPIO17 (11) (12) GPIO18        <==== USE THIS PIN!
         GPIO27 (13) (14) GND
         GPIO22 (15) (16) GPIO23
           3.3V (17) (18) GPIO24
  GPIO10  MOSI  (19) (20) GND
   GPIO9  MISO  (21) (22) GPIO25
  GPIO11  SCLK  (23) (24) GPIO8 CE0
            GND (25) (26) GPIO7 CE1
          GPIO0 (27) (28) GPIO1
          GPIO5 (29) (30) GND
          GPIO6 (31) (32) GPIO12
         GPIO13 (33) (34) GND
         GPIO19 (35) (36) GPIO16
         GPIO26 (37) (38) GPIO20
            GND (39) (40) GPIO21


    REQUIRED CONNECTIONS:

    +--------------------------------------+
    |  Function        |  Pin   |  GPIO   |
    +------------------+--------+---------+
    |  3.3V (pull-up)  |   1    |   -     |
    |  GND             |   9    |   -     |
    |  Reverse Signal  |  11    |  GPIO17 |
    +--------------------------------------+
```

## Complete Installation Diagram

```
+-----------------------------------------------------------------------------+
|                                                                             |
|  +-------------+                                                            |
|  |   BATTERY   |                                                            |
|  |    12V      |                                                            |
|  +------+------+                                                            |
|         |                                                                   |
|         | +12V                                                              |
|         |                                                                   |
|  +------+------+         +-------------+                                    |
|  |    FUSE     |         |   REVERSE   |                                    |
|  |    10A      |-------->|   SWITCH    |                                    |
|  +-------------+         | (gearbox)   |                                    |
|                          +------+------+                                    |
|                                 |                                           |
|                    +------------+------------+                              |
|                    |                         |                              |
|                    v                         v                              |
|            +-------------+          +-----------------+                     |
|            | REVERSE     |          |    INTERFACE    |                     |
|            | LIGHT       |          |    CIRCUIT      |                     |
|            | (original)  |          |  (12V -> 3.3V)  |                     |
|            +------+------+          +--------+--------+                     |
|                   |                          |                              |
|                   v                          v                              |
|                  GND                 +-----------------+                    |
|                                      |  RASPBERRY PI   |                    |
|                                      |                 |                    |
|                                      |   +---------+   |     +----------+   |
|                                      |   | GPIO17  |<--+-----| INTERFACE|   |
|                                      |   +----+----+   |     +----------+   |
|                                      |        |        |                    |
|                                      |        v        |                    |
|                                      |  +----------+   |     +----------+   |
|                                      |  | REVERSE  |   |     |  CAMERA  |   |
|                                      |  | DETECTOR |---+---->|  REVERSE |   |
|                                      |  +----------+   |     +----------+   |
|                                      |        |        |                    |
|                                      |        v        |                    |
|                                      |  +----------+   |     +----------+   |
|                                      |  |   HMI    |---+---->|  TOUCH   |   |
|                                      |  | LAUNCHER |   |     |  SCREEN  |   |
|                                      |  +----------+   |     +----------+   |
|                                      |                 |                    |
|                                      +-----------------+                    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

## Software Configuration

### Configuration File

Create/edit the file `/etc/speeduino-ui/reverse.yaml`:

```yaml
# Reverse Gear Detection Configuration
# VW Gol Quadrado AP 1.8 (Naturally Aspirated or Turbo)

reverse:
  # Detection mode: "gpio" for classic cars without CAN
  detection_mode: "gpio"

  # Preset for quick configuration (optional)
  # Values: "gol_quadrado", "classic_vw", "speeduino_can", "haltech"
  preset: "gol_quadrado"

  # ===================================================================
  # CAN - DISABLED for Gol Quadrado
  # ===================================================================
  can_enabled: false

  # ===================================================================
  # GPIO - ENABLED
  # ===================================================================
  gpio_enabled: true
  gpio_chip: "gpiochip0"      # Raspberry Pi 4
  gpio_line: 17               # GPIO17 = Pin 11 on header

  # active_low depends on your circuit:
  # - PC817 Optocoupler: true (LED on = transistor conducts = LOW)
  # - Voltage divider: false (12V = proportional HIGH)
  gpio_active_low: true

  # ===================================================================
  # DEBOUNCE
  # ===================================================================
  # Older cars may have switches with more bounce
  # Increase if false activations occur
  debounce_ms: 100
```

### GPIO Test

Before running the full system, test the GPIO:

```bash
# Install gpioget (if needed)
sudo apt install gpiod

# Test GPIO17 reading
# With gear in NEUTRAL:
gpioget gpiochip0 17
# Should return: 1 (if active_low) or 0 (if active_high)

# With REVERSE ENGAGED:
gpioget gpiochip0 17
# Should return: 0 (if active_low) or 1 (if active_high)
```

## Turbo Version Considerations

For Gol Quadrado with Turbo AP Engine + Speeduino:

```
+-----------------------------------------------------------------+
|                     GOL QUADRADO TURBO                           |
+-----------------------------------------------------------------+
|                                                                 |
|   SPEEDUINO MANAGES:                                            |
|   * Electronic fuel injection                                   |
|   * Ignition (timing advance)                                   |
|   * Boost control (if configured)                               |
|   * Wideband / Lambda                                           |
|                                                                 |
|   ORIGINAL CAR WIRING MAINTAINS:                                |
|   * Reverse light <- USE THIS SIGNAL (GPIO)                     |
|   * Brake lights                                                |
|   * Turn signals                                                |
|   * Headlights                                                  |
|                                                                 |
|   FUTURE (if Speeduino has CAN):                                |
|   - Can add CAN detection as well                               |
|   - Configure detection_mode: "both"                            |
|   - GPIO works as fallback                                      |
|                                                                 |
+-----------------------------------------------------------------+
```

## Troubleshooting

### Camera does not activate when engaging reverse

1. **Check 12V signal:**
   ```bash
   # With multimeter on switch wire
   # Reverse engaged should show ~12V
   ```

2. **Check GPIO:**
   ```bash
   gpioget gpiochip0 17
   # Should toggle between 0 and 1 when engaging/disengaging reverse
   ```

3. **Check logs:**
   ```bash
   journalctl -u reverse_service -f
   # Should show "Reverse gear ENGAGED/DISENGAGED"
   ```

### False activations (rapidly turning on/off)

- Increase `debounce_ms` to 150 or 200
- Check connections (poor contact causes noise)
- Check circuit ground

### GPIO always at 0 or always at 1

- Check if optocoupler/circuit is working
- Test with LED before connecting to Pi
- Check if `gpio_active_low` is correct for your circuit

## Safety

```
WARNING - WORKING WITH AUTOMOTIVE ELECTRICAL SYSTEM

1. ALWAYS disconnect the negative battery terminal before
   making any electrical connections

2. Use a protection fuse in the circuit (5A or 10A)

3. Use appropriate automotive connectors (waterproof
   if possible)

4. Protect wiring with corrugated tubing or spiral wrap

5. Securely mount all components to prevent vibration

6. Interface circuit MUST be used - NEVER connect
   12V directly to Raspberry Pi GPIO!
```

## Next Steps

1. [ ] Build interface circuit on breadboard for testing
2. [ ] Test with multimeter before connecting to Pi
3. [ ] Install and test in vehicle with engine off
4. [ ] Test with engine running (check for electrical noise)
5. [ ] Complete permanent installation
6. [ ] Adjust debounce if necessary

---

**Version:** 1.0
**Date:** 2024
**Compatibility:** VW Gol Quadrado, Gol G1, Saveiro, Parati, Voyage (with AP engine)
