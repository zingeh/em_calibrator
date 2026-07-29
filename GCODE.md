# EM Calibrator — G-Code Command Reference

## Transport

| Channel | Port | Baud | Protocol |
|---------|------|------|----------|
| USB Serial JTAG | Type-C | — | Line-based ASCII, `\r\n` terminated |
| WiFi TCP | 8888 | — | Same protocol over TCP socket |

## Motion Commands

All motion commands accept an optional **speed override** `F<rpm>`.
If omitted, the default speed from `app_config.h` is used.

| Command | Description | Example |
|---------|-------------|---------|
| `G0 D<mm> [R] [F<rpm>]` | Distance: absolute or relative | `G0 D200.0` → 20 cm abs<br>`G0 D-50.0 R` → -5 cm rel<br>`G0 D100.0 F300` → 10 cm at 300 RPM |
| `G1 B<deg> [F<rpm>]` | Base Yaw absolute | `G1 B45.0`<br>`G1 B45.0 F180` |
| `G1 P<deg> [F<rpm>]` | Base Pitch absolute | `G1 P-30.0` |
| `G2 T<deg> [F<rpm>]` | Tracker Yaw absolute | `G2 T45.0` |
| `G2 Q<deg> [F<rpm>]` | Tracker Pitch absolute | `G2 Q-15.0` |
| `G28` | Home all 5 motors | `G28` |

**D** in mm (10 mm = 1 cm).  **B/P/T/Q** in degrees.
Yaw ±180°,  Pitch -110° ~ 0°,  Distance 0 ~ 120 cm.

## Query Commands

| Command | Description | Response |
|---------|-------------|----------|
| `M115` | Firmware info | `ok EM_CALIBRATOR v1.0.0` |
| `M114` | All positions | `ok D10.0 B45.0 P-30.0 T10.0 Q-15.0` |
| `M1 <id>` | Single motor (1-5) | `ok M1 pos=20.0 cm tgt=20.0 cm online=1 moving=0` |

## WiFi Configuration

| Command | Description |
|---------|-------------|
| `$SSID=<name>` | Save WiFi SSID to NVS |
| `$PASS=<password>` | Save WiFi password to NVS |
| `$WIFI=1` | Connect to saved WiFi AP |
