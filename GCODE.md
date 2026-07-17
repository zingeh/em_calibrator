# EM Calibrator — G-Code Command Reference

## Transport

| Channel | Port | Baud | Protocol |
|---------|------|------|----------|
| USB Serial JTAG | Type-C | — | Line-based ASCII, `\r\n` terminated |
| WiFi TCP | 8888 | — | Same protocol over TCP socket |

## Motion Commands

### G0 — Distance Axis (Motor 1)

| Command | Description |
|---------|-------------|
| `G0 D<mm>` | Absolute move to `<mm>` (e.g. `G0 D200.0` → go to 20 cm) |
| `G0 D<mm> R` | Relative move by `<mm>` (e.g. `G0 D-50.0 R` → -5 cm) |

D value is in millimetres.  10 mm = 1 cm.

### G1 — Base Axes (Motor 2: Yaw, Motor 3: Pitch)

| Command | Description |
|---------|-------------|
| `G1 B<deg>` | Base Yaw absolute (e.g. `G1 B45.0`) |
| `G1 P<deg>` | Base Pitch absolute (e.g. `G1 P-30.0`) |

Values in degrees.  Yaw: ±180°,  Pitch: -90° ~ 0°.

### G2 — Tracker Axes (Motor 4: Yaw, Motor 5: Pitch)

| Command | Description |
|---------|-------------|
| `G2 T<deg>` | Tracker Yaw absolute (e.g. `G2 T45.0`) |
| `G2 Q<deg>` | Tracker Pitch absolute (e.g. `G2 Q-15.0`) |

Values in degrees.  Yaw: ±180°,  Pitch: -90° ~ 0°.

## System Commands

| Command | Description | Response |
|---------|-------------|----------|
| `G28` | Home all 5 motors (hardware homing) | `ok homing N motors` |
| `M115` | Firmware info | `ok EM_CALIBRATOR v1.0.0` |
| `M114` | Current position report | `ok D10.0 B45.0 P-30.0 T10.0 Q-15.0` |

## WiFi Configuration

| Command | Description |
|---------|-------------|
| `$SSID=<name>` | Save WiFi SSID to NVS |
| `$PASS=<password>` | Save WiFi password to NVS |
| `$WIFI=1` | Connect to saved WiFi AP |
