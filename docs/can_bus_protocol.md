# CAN Bus Protocol Documentation - SCG-ECU 2.0

This document describes the CAN messages sent and received by the ECU, for implementation in the HMI dashboard.

---

## 1. CAN TX Commands (ECU Sends)

### 1.1 BMW PT-CAN (500 kbps)

#### 0x316 - DME1 (30Hz)

| Byte | Bits | Description           | Formula/Value                  | Unit        |
|------|------|-----------------------|--------------------------------|-------------|
| 0    | 0-7  | Status flags          | 0x05 = Terminal 15 ON + ASC OK | bitfield    |
| 1    | 0-7  | Indexed Engine Torque | 0x0C (12% placeholder)         | % C_TQ_STND |
| 2    | 0-7  | RPM LSB               | (RPM × 6.4) & 0xFF             | -           |
| 3    | 0-7  | RPM MSB               | (RPM × 6.4) >> 8               | RPM × 6.4   |
| 4    | 0-7  | Indicated Torque      | 0x0C (12%)                     | % C_TQ_STND |
| 5    | 0-7  | Torque Loss           | 0x15 (21%)                     | %           |
| 6    | 0-7  | Not used              | 0x00                           | -           |
| 7    | 0-7  | Theoretical Torque    | 0x35 (53%)                     | %           |

**RPM Decoding:** `RPM = ((byte[3] << 8) | byte[2]) / 6.4`

---

#### 0x329 - DME2 (30Hz)

| Byte | Bits | Description         | Formula/Value              | Unit      |
|------|------|---------------------|----------------------------|-----------|
| 0    | 0-7  | Multiplexed Info    | 0x11                       | -         |
| 1    | 0-7  | Coolant Temp        | ((CLT + 48) × 4) / 3       | °C scaled |
| 2    | 0-7  | Barometric Pressure | baro direct                | kPa       |
| 3    | 0-7  | Status bitfield     | 0x08 = engine running      | bitfield  |
| 4    | 0-7  | TPS Virtual Cruise  | 0x00 (not used)            | -         |
| 5    | 0-7  | TPS                 | map(TPS, 0, 200, 1, 254)   | 0x01-0xFE |
| 6    | 0-7  | Brake/Clutch Status | bit0=brake, bit1=clutch    | bitfield  |
| 7    | 0-7  | Cruise Control      | 0x00 (not used)            | -         |

**Coolant Temp Decoding:** `CLT = (byte[1] × 3 / 4) - 48`

**TPS Decoding:** `TPS = map(byte[5], 1, 254, 0, 200)` (0.5% resolution)

---

#### 0x338 - DME3 (30Hz)

| Byte | Bits | Description      | Formula/Value | Unit |
|------|------|------------------|---------------|------|
| 0-7  | -    | Pedal position   | -             | -    |
| -    | -    | Torque requests  | -             | -    |

---

#### 0x545 - DME4 (5Hz)

| Byte | Bits | Description           | Formula/Value                       | Unit |
|------|------|-----------------------|-------------------------------------|------|
| 0    | 0-7  | Oil Temp LSB          | (OilTemp + 48) × 4 (low nibble)     | °C   |
| 1    | 0-7  | Oil Temp MSB          | (OilTemp + 48) × 4 (high nibble)    | °C   |
| 2    | 0-7  | Coolant Temp (backup) | same as 0x329                       | °C   |
| 3    | 0-7  | Ambient Temp          | (AmbientTemp + 48)                  | °C   |
| 4    | 0-7  | Fuel Consumption LSB  | consumption × 10                    | L/h  |
| 5    | 0-7  | Fuel Consumption MSB  | consumption × 10                    | L/h  |
| 6    | 0-7  | CEL Status            | bit0=CEL on                         | bit  |
| 7    | 0-7  | Cruise Status         | 0x00                                | -    |

**Oil Temp Decoding:** `OilTemp = ((byte[1] << 8 | byte[0]) / 4) - 48`

---

#### 0x613 - Cluster Text (1Hz)

| Byte | Bits | Description  | Formula/Value    | Unit |
|------|------|--------------|------------------|------|
| 0    | 0-7  | Message ID   | Text identifier  | -    |
| 1-7  | -    | Text payload | ASCII characters | -    |

---

#### 0x615 - Check Control (1Hz)

| Byte | Bits | Description | Formula/Value       | Unit |
|------|------|-------------|---------------------|------|
| 0    | 0-7  | CC Message  | Warning/Info codes  | -    |
| 1-7  | -    | Parameters  | Message-specific    | -    |

---

### 1.2 Speeduino Secondary CAN (500 kbps)

#### 0x5F0 - Sync Status (10Hz)

| Byte | Bits | Description                          | Formula/Value | Unit |
|------|------|--------------------------------------|---------------|------|
| 0-7  | -    | Sync status, trigger counter, errors | -             | -    |

---

#### 0x370 - VSS (15Hz)

| Byte | Bits | Description   | Formula/Value  | Unit  |
|------|------|---------------|----------------|-------|
| 0    | 0-7  | Speed LSB     | VSS × 100      | km/h  |
| 1    | 0-7  | Speed MSB     | VSS × 100      | km/h  |
| 2    | 0-7  | Odometer B0   | Total km       | km    |
| 3    | 0-7  | Odometer B1   | Total km       | km    |
| 4    | 0-7  | Odometer B2   | Total km       | km    |
| 5    | 0-7  | Odometer B3   | Total km       | km    |
| 6-7  | -    | Reserved      | 0x00           | -     |

**Speed Decoding:** `Speed = ((byte[1] << 8) | byte[0]) / 100.0`

---

#### 0x3E0 - Engine Data 1 (20Hz)

| Byte | Bits | Description | Formula/Value       | Unit   |
|------|------|-------------|---------------------|--------|
| 0    | 0-7  | RPM LSB     | RPM direct          | RPM    |
| 1    | 0-7  | RPM MSB     | RPM direct          | RPM    |
| 2    | 0-7  | MAP         | MAP direct          | kPa    |
| 3    | 0-7  | TPS         | TPS × 2             | % × 2  |
| 4    | 0-7  | CLT         | CLT + 40            | °C     |
| 5    | 0-7  | IAT         | IAT + 40            | °C     |
| 6    | 0-7  | Battery     | Voltage × 10        | V × 10 |
| 7    | 0-7  | AFR Target  | AFR × 10            | AFR    |

**Decoding:**
- `RPM = (byte[1] << 8) | byte[0]`
- `MAP = byte[2]` (kPa)
- `TPS = byte[3] / 2.0` (%)
- `CLT = byte[4] - 40` (°C)
- `IAT = byte[5] - 40` (°C)
- `Battery = byte[6] / 10.0` (V)
- `AFR_Target = byte[7] / 10.0`

---

#### 0x3E1 - Engine Data 2 (20Hz)

| Byte | Bits | Description   | Formula/Value | Unit   |
|------|------|---------------|---------------|--------|
| 0    | 0-7  | AFR Actual    | AFR × 10      | AFR    |
| 1    | 0-7  | Ignition Adv  | Advance × 2   | ° × 2  |
| 2    | 0-7  | Fuel PW LSB   | PW × 10       | ms     |
| 3    | 0-7  | Fuel PW MSB   | PW × 10       | ms     |
| 4    | 0-7  | VE            | VE direct     | %      |
| 5    | 0-7  | Fuel Pressure | FP × 10       | bar    |
| 6    | 0-7  | Oil Pressure  | OP × 10       | bar    |
| 7    | 0-7  | Boost Target  | Boost direct  | kPa    |

**Decoding:**
- `AFR = byte[0] / 10.0`
- `Ignition = byte[1] / 2.0` (degrees BTDC)
- `PulseWidth = ((byte[3] << 8) | byte[2]) / 10.0` (ms)
- `VE = byte[4]` (%)
- `FuelPressure = byte[5] / 10.0` (bar)
- `OilPressure = byte[6] / 10.0` (bar)
- `BoostTarget = byte[7]` (kPa)

---

#### 0x3E2 - Engine Status (10Hz)

| Byte | Bits | Description     | Formula/Value          | Unit     |
|------|------|-----------------|------------------------|----------|
| 0    | 0    | Engine Running  | 1 = running            | bool     |
| 0    | 1    | Cranking        | 1 = cranking           | bool     |
| 0    | 2    | ASE Active      | 1 = after-start enrich | bool     |
| 0    | 3    | Warmup Active   | 1 = warmup enrichment  | bool     |
| 0    | 4    | Accel Enrich    | 1 = accel enrichment   | bool     |
| 0    | 5    | Decel Fuel Cut  | 1 = DFCO active        | bool     |
| 0    | 6    | Rev Limiter     | 1 = rev limit active   | bool     |
| 0    | 7    | Boost Cut       | 1 = boost cut active   | bool     |
| 1    | 0    | Fan On          | 1 = fan active         | bool     |
| 1    | 1    | Fuel Pump       | 1 = pump on            | bool     |
| 1    | 2    | CEL             | 1 = check engine       | bool     |
| 1    | 3    | EGO Heating     | 1 = O2 heater on       | bool     |
| 1    | 4    | Launch Control  | 1 = launch active      | bool     |
| 1    | 5    | Flat Shift      | 1 = flat shift active  | bool     |
| 1    | 6    | Nitrous Stage 1 | 1 = N2O stage 1        | bool     |
| 1    | 7    | Nitrous Stage 2 | 1 = N2O stage 2        | bool     |
| 2    | 0-7  | Error Count     | Number of active DTCs  | count    |
| 3    | 0-7  | Sync Status     | 0=lost, 1=synced       | enum     |
| 4    | 0-7  | Idle Target RPM | Target / 10            | RPM / 10 |
| 5    | 0-7  | Idle Valve Duty | 0-100                  | %        |
| 6-7  | -    | Reserved        | 0x00                   | -        |

---

#### 0x3E3 - Fuel/Ignition Trims (5Hz)

| Byte | Bits | Description    | Formula/Value    | Unit |
|------|------|----------------|------------------|------|
| 0    | 0-7  | Fuel Trim Cyl1 | (trim - 100) %   | %    |
| 1    | 0-7  | Fuel Trim Cyl2 | (trim - 100) %   | %    |
| 2    | 0-7  | Fuel Trim Cyl3 | (trim - 100) %   | %    |
| 3    | 0-7  | Fuel Trim Cyl4 | (trim - 100) %   | %    |
| 4    | 0-7  | Ign Trim Cyl1  | (trim - 128) / 2 | °    |
| 5    | 0-7  | Ign Trim Cyl2  | (trim - 128) / 2 | °    |
| 6    | 0-7  | Ign Trim Cyl3  | (trim - 128) / 2 | °    |
| 7    | 0-7  | Ign Trim Cyl4  | (trim - 128) / 2 | °    |

**Decoding:**
- `FuelTrim = byte[n] - 100` (% correction, -100 to +155)
- `IgnTrim = (byte[n] - 128) / 2.0` (degrees, -64 to +63.5)

---

## 2. CAN RX Commands (ECU Receives)

### 2.1 OBD-II via CAN (ISO 15765-4)

#### Request: 0x7DF (Broadcast) or 0x7E0 (ECU specific)

| Byte | Description      | Value               |
|------|------------------|---------------------|
| 0    | Length           | Number of data bytes|
| 1    | Service ID (SID) | 0x01, 0x09, etc.    |
| 2    | PID              | Parameter ID        |
| 3-7  | Additional data  | Depends on service  |

#### Response: 0x7E8

| Byte | Description          | Value               |
|------|----------------------|---------------------|
| 0    | Length               | Number of data bytes|
| 1    | Service ID + 0x40    | Response SID        |
| 2    | PID                  | Echo of request PID |
| 3-7  | Data                 | PID-specific data   |

---

### 2.2 Supported OBD-II Services

#### Service 01 - Current Data

| PID  | Description          | Bytes | Formula                    | Unit   |
|------|----------------------|-------|----------------------------|--------|
| 0x00 | PIDs supported 01-20 | 4     | Bitfield                   | -      |
| 0x04 | Engine Load          | 1     | A × 100 / 255              | %      |
| 0x05 | Coolant Temp         | 1     | A - 40                     | °C     |
| 0x06 | Short Term Fuel Trim | 1     | (A - 128) × 100 / 128      | %      |
| 0x07 | Long Term Fuel Trim  | 1     | (A - 128) × 100 / 128      | %      |
| 0x0B | MAP                  | 1     | A                          | kPa    |
| 0x0C | RPM                  | 2     | (A × 256 + B) / 4          | RPM    |
| 0x0D | Vehicle Speed        | 1     | A                          | km/h   |
| 0x0E | Timing Advance       | 1     | (A - 128) / 2              | °      |
| 0x0F | Intake Air Temp      | 1     | A - 40                     | °C     |
| 0x10 | MAF Rate             | 2     | (A × 256 + B) / 100        | g/s    |
| 0x11 | TPS                  | 1     | A × 100 / 255              | %      |
| 0x14 | O2 Sensor 1          | 2     | A/200, (B-128)×100/128     | V, %   |
| 0x1C | OBD Standard         | 1     | Enum                       | -      |
| 0x1F | Run Time             | 2     | A × 256 + B                | sec    |
| 0x20 | PIDs supported 21-40 | 4     | Bitfield                   | -      |
| 0x21 | Distance with MIL    | 2     | A × 256 + B                | km     |
| 0x2F | Fuel Level           | 1     | A × 100 / 255              | %      |
| 0x33 | Barometric Pressure  | 1     | A                          | kPa    |
| 0x40 | PIDs supported 41-60 | 4     | Bitfield                   | -      |
| 0x42 | Control Module Volt  | 2     | (A × 256 + B) / 1000       | V      |
| 0x46 | Ambient Temp         | 1     | A - 40                     | °C     |
| 0x5C | Oil Temperature      | 1     | A - 40                     | °C     |

---

#### Service 09 - Vehicle Information

| PID  | Description  | Notes                           |
|------|--------------|----------------------------------|
| 0x02 | VIN          | 17-byte Vehicle ID Number        |
| 0x0A | ECU Name     | 20-byte ECU identifier           |

---

#### Service 22 - Custom Read (Manufacturer Specific)

| PID    | Description        | Notes                          |
|--------|--------------------|--------------------------------|
| 0x77xx | AuxCAN Data        | Extended sensor data           |
| 0x78xx | currentStatus      | Speeduino internal status      |

---

### 2.3 Wideband Controller (AEM X-Series)

#### 0x180 - Wideband Data (50Hz)

| Byte | Bits | Description | Formula/Value        | Unit   |
|------|------|-------------|----------------------|--------|
| 0    | 0-7  | Lambda LSB  | Lambda × 10000       | λ      |
| 1    | 0-7  | Lambda MSB  | Lambda × 10000       | λ      |
| 2    | 0-7  | O2 %        | O2 × 100             | %      |
| 3    | 0-7  | Status      | 0=warmup, 1=ok, 2=err| enum   |
| 4    | 0-7  | Heater Duty | 0-100                | %      |
| 5    | 0-7  | Sensor Temp | Temp direct          | °C     |
| 6-7  | -    | Reserved    | 0x00                 | -      |

**Lambda Decoding:** `Lambda = ((byte[1] << 8) | byte[0]) / 10000.0`
**AFR Decoding:** `AFR = Lambda × 14.7` (for gasoline)

---

## 3. DTC (Diagnostic Trouble Codes)

### 3.1 Powertrain Codes (P0xxx)

#### Fuel/Air Metering
| Code  | Description              |
|-------|--------------------------|
| P0100 | MAF Circuit              |
| P0105 | MAP Circuit              |
| P0110 | IAT Circuit              |
| P0115 | ECT Circuit              |
| P0120 | TPS Circuit              |
| P0130 | O2 Sensor Bank 1         |
| P0171 | System Too Lean          |
| P0172 | System Too Rich          |

#### Ignition System
| Code  | Description              |
|-------|--------------------------|
| P0300 | Random Misfire           |
| P0301 | Cylinder 1 Misfire       |
| P0302 | Cylinder 2 Misfire       |
| P0303 | Cylinder 3 Misfire       |
| P0304 | Cylinder 4 Misfire       |
| P0335 | Crank Position Sensor    |
| P0340 | Cam Position Sensor      |

#### Idle Control
| Code  | Description              |
|-------|--------------------------|
| P0505 | Idle Control System      |
| P0506 | Idle RPM Lower Than Expected |
| P0507 | Idle RPM Higher Than Expected |

#### Oil Pressure
| Code  | Description              |
|-------|--------------------------|
| P0520 | Oil Pressure Sensor      |
| P0521 | Oil Pressure Range       |
| P0522 | Oil Pressure Low         |
| P0523 | Oil Pressure High        |
| P0524 | Oil Pressure Too Low     |

#### System Voltage
| Code  | Description              |
|-------|--------------------------|
| P0560 | System Voltage           |
| P0561 | System Voltage Unstable  |
| P0562 | System Voltage Low       |
| P0563 | System Voltage High      |

### 3.2 Manufacturer Specific (P1xxx)

#### BMW E46
| Code  | Description              |
|-------|--------------------------|
| P1083 | Fuel Control Mixture Lean|
| P1084 | Fuel Control Mixture Rich|
| P1188 | Linear O2 Sensor         |
| P1550 | Throttle Actuator        |

#### VW Gol
| Code  | Description              |
|-------|--------------------------|
| P1127 | Long Term Fuel Trim Mult |
| P1128 | Long Term Fuel Trim Add  |
| P1545 | Throttle Position Control|

---

## 4. HMI Implementation Notes

### 4.1 Priority Data (High Frequency Display)

| Data           | CAN ID | Update Rate | Display Element    |
|----------------|--------|-------------|---------------------|
| RPM            | 0x3E0  | 20Hz        | Tachometer          |
| Speed          | 0x370  | 15Hz        | Speedometer         |
| Coolant Temp   | 0x3E0  | 20Hz        | Temp Gauge          |
| AFR            | 0x3E1  | 20Hz        | AFR Gauge           |
| Boost/MAP      | 0x3E0  | 20Hz        | Boost Gauge         |
| TPS            | 0x3E0  | 20Hz        | TPS Bar             |

### 4.2 Secondary Data (Low Frequency)

| Data           | CAN ID | Update Rate | Display Element    |
|----------------|--------|-------------|---------------------|
| Oil Temp       | 0x545  | 5Hz         | Oil Temp Gauge      |
| Oil Pressure   | 0x3E1  | 20Hz        | Oil Press Gauge     |
| Fuel Pressure  | 0x3E1  | 20Hz        | Fuel Press Gauge    |
| Battery        | 0x3E0  | 20Hz        | Battery Indicator   |
| IAT            | 0x3E0  | 20Hz        | IAT Display         |

### 4.3 Status Indicators (from 0x3E2)

| Status         | Bit    | Display                  |
|----------------|--------|--------------------------|
| Engine Running | 0.0    | Main status              |
| CEL            | 1.2    | Warning light            |
| Rev Limiter    | 0.6    | Flash tach               |
| Launch Control | 1.4    | Launch indicator         |
| Flat Shift     | 1.5    | Shift indicator          |
| Fan On         | 1.0    | Fan icon                 |
| DFCO           | 0.5    | Fuel cut indicator       |

### 4.4 Warning Thresholds

| Parameter      | Warning   | Critical  | Action              |
|----------------|-----------|-----------|---------------------|
| Coolant Temp   | > 100°C   | > 110°C   | Flash gauge, alert  |
| Oil Temp       | > 120°C   | > 140°C   | Flash gauge, alert  |
| Oil Pressure   | < 1.5 bar | < 1.0 bar | Flash gauge, alert  |
| Battery        | < 12.5V   | < 11.5V   | Warning indicator   |
| AFR (WOT)      | > 13.5    | > 14.0    | Lean warning        |
| AFR (WOT)      | < 10.5    | < 10.0    | Rich warning        |

---

## 5. CAN Bus Configuration

### 5.1 Bus Speeds

| Bus             | Speed    | Interface |
|-----------------|----------|-----------|
| BMW PT-CAN      | 500 kbps | can0      |
| Speeduino CAN   | 500 kbps | can1      |
| OBD-II          | 500 kbps | can0/can1 |
| Wideband        | 500 kbps | can1      |

### 5.2 Message Filters (HMI)

```
# Primary engine data
can1: 0x3E0, 0x3E1, 0x3E2, 0x3E3

# Vehicle data
can1: 0x370

# BMW cluster
can0: 0x316, 0x329, 0x545

# Wideband
can1: 0x180

# OBD responses
can0/can1: 0x7E8
```

---

## Revision History

| Date       | Version | Changes                          |
|------------|---------|----------------------------------|
| 2025-01-22 | 1.0     | Initial documentation            |
