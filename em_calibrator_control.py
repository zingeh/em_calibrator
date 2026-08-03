#!/usr/bin/env python3
"""
EM Calibrator — TCP remote control script
Usage:
  python em_calibrator_control.py                          # interactive mode
  python em_calibrator_control.py --host 192.168.123.181   # specify host
  python em_calibrator_control.py --demo                   # run demo sequence
"""

import socket
import sys
import time
import argparse

HOST = "192.168.123.181"
PORT = 8888
TIMEOUT = 3.0


def connect(host=HOST, port=PORT):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((host, port))
    return s


def send(sock, cmd):
    """Send a G-code command and return the response."""
    sock.sendall((cmd + "\r\n").encode())
    try:
        resp = sock.recv(1024).decode().strip()
    except socket.timeout:
        resp = "(timeout)"
    return resp


def info(sock):
    """Print firmware version and current position."""
    print(send(sock, "M115"))
    print(send(sock, "M114"))


def query(sock, motor_id):
    """Query single motor position (1-5)."""
    return send(sock, f"M1 {motor_id}")


# ──────────────────────────────────────────────────────
#  High-level motion helpers
#  All accept optional `speed` in RPM (F parameter).
#  If speed is None or 0, the default speed is used.
# ──────────────────────────────────────────────────────

def _f(speed):
    """Format optional F parameter."""
    if speed:
        return f" F{int(speed)}"
    return ""


def dist_abs(sock, cm, speed=None):
    """Move distance axis to absolute position (cm)."""
    mm = cm * 10.0
    return send(sock, f"G0 D{mm:.1f}{_f(speed)}")


def dist_rel(sock, cm, speed=None):
    """Move distance axis relative (cm, positive = extend)."""
    mm = cm * 10.0
    return send(sock, f"G0 D{mm:.1f} R{_f(speed)}")


def base_yaw(sock, deg, speed=None):
    return send(sock, f"G1 B{deg:.1f}{_f(speed)}")


def base_pitch(sock, deg, speed=None):
    return send(sock, f"G1 P{deg:.1f}{_f(speed)}")


def track_yaw(sock, deg, speed=None):
    return send(sock, f"G2 T{deg:.1f}{_f(speed)}")


def track_pitch(sock, deg, speed=None):
    return send(sock, f"G2 Q{deg:.1f}{_f(speed)}")


def home_all(sock):
    return send(sock, "G28")


# ──────────────────────────────────────────────────────
#  Demo sequence (with speed overrides)
# ──────────────────────────────────────────────────────

def demo(host=HOST):
    """Run a short calibration demo sequence."""
    s = connect(host)
    print("=== EM Calibrator Demo ===")

    info(s)

    print("\n[1] Home all axes ...")
    print(home_all(s))
    time.sleep(2)

    print("[2] Distance → 20 cm (fast) ...")
    print(dist_abs(s, 20.0, speed=400))
    time.sleep(2)

    print("[3] Base yaw → +45° ...")
    print(base_yaw(s, 45.0))
    time.sleep(1.5)

    print("[4] Base pitch → -30° ...")
    print(base_pitch(s, -30.0, speed=1))
    time.sleep(1)

    print("[5] Tracker yaw → -45° ...")
    print(track_yaw(s, -45.0))
    time.sleep(1.5)

    print("[6] Tracker pitch → -15° ...")
    print(track_pitch(s, -15.0))
    time.sleep(1)

    print("[7] Distance → 10 cm (slow) ...")
    print(dist_abs(s, 10.0, speed=100))
    time.sleep(2)

    print("[8] Return all to zero ...")
    print(home_all(s))
    time.sleep(2)

    print("[9] Single motor query ...")
    for mid in range(1, 6):
        print(f"  M{mid}: {query(s, mid)}")

    info(s)
    s.close()
    print("\n=== Demo complete ===")


# ──────────────────────────────────────────────────────
#  Interactive shell
# ──────────────────────────────────────────────────────

HELP = """Commands:
  da <cm> [F<rpm>]    distance absolute   e.g. da 20.0
  dr <cm> [F<rpm>]    distance relative   e.g. dr -5.0 F300
  by <deg> [F<rpm>]   base yaw            e.g. by 45.0
  bp <deg> [F<rpm>]   base pitch          e.g. bp -30.0 F180
  ty <deg> [F<rpm>]   tracker yaw         e.g. ty 45.0
  tp <deg> [F<rpm>]   tracker pitch       e.g. tp -15.0
  m1 <id>             query motor 1-5     e.g. m1 2
  home                home all axes (G28)
  info                firmware + all positions
  demo                run demo sequence
  h / help            this help
  q / quit            exit

  Raw G-code is also accepted, e.g. G0 D100.0 F300"""


def _parse_float(s):
    try:
        return float(s)
    except ValueError:
        return None


def _parse_speed(parts, idx):
    """Parse F<rpm> from argument list at position idx."""
    if idx < len(parts):
        p = parts[idx]
        if p.upper().startswith("F"):
            try:
                return int(p[1:])
            except ValueError:
                pass
    return None


def interactive(host=HOST):
    s = connect(host)
    print("=== EM Calibrator Interactive ===")
    print(f"Connected to {host}:{PORT}")
    print("Type 'h' for help, 'q' to quit.\n")
    info(s)

    while True:
        try:
            line = input("EM> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye.")
            break

        if not line:
            continue

        parts = line.split()
        cmd = parts[0].lower()

        try:
            if cmd == "q" or cmd == "quit":
                break
            elif cmd == "h" or cmd == "help":
                print(HELP)
            elif cmd == "info":
                info(s)
            elif cmd == "demo":
                s.close()
                demo(host)
                s = connect(host)
            elif cmd == "home":
                print(home_all(s))
            elif cmd == "m1" and len(parts) >= 2:
                mid = int(parts[1])
                print(query(s, mid))
            elif cmd == "da" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(dist_abs(s, val, speed=_parse_speed(parts, 2)))
            elif cmd == "dr" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(dist_rel(s, val, speed=_parse_speed(parts, 2)))
            elif cmd == "by" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(base_yaw(s, val, speed=_parse_speed(parts, 2)))
            elif cmd == "bp" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(base_pitch(s, val, speed=_parse_speed(parts, 2)))
            elif cmd == "ty" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(track_yaw(s, val, speed=_parse_speed(parts, 2)))
            elif cmd == "tp" and len(parts) >= 2:
                val = _parse_float(parts[1])
                if val is not None:
                    print(track_pitch(s, val, speed=_parse_speed(parts, 2)))
            else:
                # try raw G-code
                print(send(s, line))
        except (ConnectionError, socket.timeout) as e:
            print(f"Connection error: {e}")
            print("Reconnecting...")
            try:
                s.close()
            except Exception:
                pass
            s = connect(host)

    s.close()
    print("Disconnected.")


# ──────────────────────────────────────────────────────
#  Main
# ──────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="EM Calibrator TCP control")
    parser.add_argument("--host", default=HOST, help=f"IP address (default: {HOST})")
    parser.add_argument("--port", type=int, default=PORT)
    parser.add_argument("--demo", action="store_true", help="Run demo sequence")
    args = parser.parse_args()

    if args.demo:
        demo(args.host)
    else:
        interactive(args.host)
