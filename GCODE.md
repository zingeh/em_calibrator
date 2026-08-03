# EM Calibrator — G-Code Command Reference

## Transport

| Channel | Port | Baud | Protocol |
|---------|------|------|----------|
| USB Serial JTAG | Type-C | — | Line-based ASCII, `\r\n` terminated |
| WiFi TCP | 8888 | — | Same protocol over TCP socket |

## Motor ID Mapping

| ID | Axis | Type | Unit | Range |
|----|------|------|------|-------|
| 1 | Distance | Linear | cm | 0 ~ 120 |
| 2 | Base Yaw | Rotary | deg | ±180 |
| 3 | Base Pitch | Rotary | deg | -110 ~ 0 |
| 4 | Track Yaw | Rotary | deg | ±180 |
| 5 | Track Pitch | Rotary | deg | -110 ~ 0 |

## Motion Commands

Absolute move commands (G0 abs / G1 / G2) accept an optional **speed override** `F<rpm>`.
If omitted, the default speed is used (200 RPM distance, 120 RPM angle).

> **Note:** Relative moves (`G0 D<mm> R`, `G28`) use QPos mode which does not support
> per-command speed override. F is ignored for these commands.

| Command | Description | Example |
|---------|-------------|---------|
| `G0 D<mm> [F<rpm>]` | Distance absolute | `G0 D200.0` → 20 cm |
| `G0 D<mm> R` | Distance relative | `G0 D-50.0 R` → -5 cm |
| `G1 B<deg> [F<rpm>]` | Base Yaw absolute | `G1 B45.0 F180` |
| `G1 P<deg> [F<rpm>]` | Base Pitch absolute | `G1 P-30.0` |
| `G2 T<deg> [F<rpm>]` | Tracker Yaw absolute | `G2 T45.0` |
| `G2 Q<deg> [F<rpm>]` | Tracker Pitch absolute | `G2 Q-15.0` |
| `G28` | Home all 5 motors | `G28` |

## Query Commands

| Command | Description | Response |
|---------|-------------|----------|
| `M115` | Firmware info | `ok EM_CALIBRATOR v1.0.0` |
| `M114` | All positions (with offsets) | `ok D10.0 B45.0 P-30.0 T10.0 Q-15.0` |
| `M1 <id>` | Single motor (1-5) | `ok M1 pos=10.0 cm tgt=10.0 cm online=1 moving=0` |

**M1** response fields:

| Field | Meaning |
|-------|---------|
| `pos` | Actual encoder position (cm or deg) |
| `tgt` | Target/commanded position (cm or deg) |
| `online` | 1 = motor responding on RS485, 0 = offline |
| `moving` | 1 = motor is currently executing a move |

> Positions include the hardware `pos_offset` (e.g. Distance has a fixed 10 cm offset).
> A freshly-homed Distance motor reports `pos=10.0 cm`, not `0.0 cm`.

## WiFi Configuration

| Command | Description |
|---------|-------------|
| `$SSID=<name>` | Save WiFi SSID to NVS |
| `$PASS=<password>` | Save WiFi password to NVS |
| `$WIFI=1` | Connect to saved WiFi AP |
