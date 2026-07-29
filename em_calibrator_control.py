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

# ──────────────────────────────────────────────────────
#  High-level motion helpers
# ──────────────────────────────────────────────────────

def dist_abs(sock, cm):
    """Move distance axis to absolute position (cm)."""
    mm = cm * 10.0
    return send(sock, f"G0 D{mm:.1f}")

def dist_rel(sock, cm):
    """Move distance axis relative (cm, positive = extend)."""
    mm = cm * 10.0
    return send(sock, f"G0 D{mm:.1f} R")

def base_yaw(sock, deg):
    return send(sock, f"G1 B{deg:.1f}")

def base_pitch(sock, deg):
    return send(sock, f"G1 P{deg:.1f}")

def track_yaw(sock, deg):
    return send(sock, f"G2 T{deg:.1f}")

def track_pitch(sock, deg):
    return send(sock, f"G2 Q{deg:.1f}")

def home_all(sock):
    return send(sock, "G28")

# ──────────────────────────────────────────────────────
#  Demo sequence
# ──────────────────────────────────────────────────────

def demo(host=HOST):
    """Run a short calibration demo sequence."""
    s = connect(host)
    print("=== EM Calibrator Demo ===")

    info(s)

    print("\n[1] Home all axes ...")
    home_all(s)
    time.sleep(2)

    print("[2] Distance → 20 cm ...")
    print(dist_abs(s, 20.0))
    time.sleep(2)

    print("[3] Base yaw → +45° ...")
    print(base_yaw(s, 45.0))
    time.sleep(1.5)

    print("[4] Base pitch → -30° ...")
    print(base_pitch(s, -30.0))
    time.sleep(1)

    print("[5] Tracker yaw → -45° ...")
    print(track_yaw(s, -45.0))
    time.sleep(1.5)

    print("[6] Tracker pitch → -15° ...")
    print(track_pitch(s, -15.0))
    time.sleep(1)

    print("[7] Distance → 10 cm ...")
    print(dist_abs(s, 10.0))
    time.sleep(2)

    print("[8] Return all to zero ...")
    print(dist_abs(s, 0.0))
    print(base_yaw(s, 0.0))
    print(base_pitch(s, 0.0))
    print(track_yaw(s, 0.0))
    print(track_pitch(s, 0.0))
    time.sleep(2)

    info(s)
    s.close()
    print("\n=== Demo complete ===")

# ──────────────────────────────────────────────────────
#  Interactive shell
# ──────────────────────────────────────────────────────

HELP = """Commands:
  da <cm>         distance absolute   e.g. da 20.0
  dr <cm>         distance relative   e.g. dr -5.0
  by <deg>        base yaw            e.g. by 45.0
  bp <deg>        base pitch          e.g. bp -30.0
  ty <deg>        tracker yaw         e.g. ty 45.0
  tp <deg>        tracker pitch       e.g. tp -15.0
  home            home all axes
  info            firmware + position
  demo            run demo sequence
  h / help        this help
  q / quit        exit"""

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
                print(dist_abs(s, 0.0))
                print(base_yaw(s, 0.0))
                print(base_pitch(s, 0.0))
                print(track_yaw(s, 0.0))
                print(track_pitch(s, 0.0))
            elif cmd == "da" and len(parts) >= 2:
                print(dist_abs(s, float(parts[1])))
            elif cmd == "dr" and len(parts) >= 2:
                print(dist_rel(s, float(parts[1])))
            elif cmd == "by" and len(parts) >= 2:
                print(base_yaw(s, float(parts[1])))
            elif cmd == "bp" and len(parts) >= 2:
                print(base_pitch(s, float(parts[1])))
            elif cmd == "ty" and len(parts) >= 2:
                print(track_yaw(s, float(parts[1])))
            elif cmd == "tp" and len(parts) >= 2:
                print(track_pitch(s, float(parts[1])))
            else:
                # try raw G-code
                print(send(s, line))
        except (ConnectionError, socket.timeout) as e:
            print(f"Connection error: {e}")
            print("Reconnecting...")
            try:
                s.close()
            except:
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
