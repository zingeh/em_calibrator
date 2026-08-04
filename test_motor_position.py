#!/usr/bin/env python3
"""
Test: read actual position of all 5 motors via M1 query.

Waits for all motors to stop moving (moving=0) before printing,
so the reported pos reflects the settled position, not mid-motion.

Usage:
  python test_motor_position.py
  python test_motor_position.py --host 192.168.123.181
"""

import socket
import argparse
import time

HOST = "192.168.123.181"
PORT = 8888
TIMEOUT = 3.0

MOTOR_NAMES = {
    1: "Distance",
    2: "Base Yaw",
    3: "Base Pitch",
    4: "Track Yaw",
    5: "Track Pitch",
}


def connect(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((host, port))
    return s


def send(sock, cmd):
    sock.sendall((cmd + "\r\n").encode())
    try:
        return sock.recv(1024).decode().strip()
    except socket.timeout:
        return "(timeout)"


def query(sock, motor_id):
    return send(sock, f"M1 {motor_id}")


def wait_for_idle(sock, motor_ids=(1, 2, 3, 4, 5), timeout=30.0, interval=0.3):
    """Poll M1 until all motors report moving=0. Returns last responses."""
    t0 = time.time()
    last = {}
    while time.time() - t0 < timeout:
        busy = []
        for mid in motor_ids:
            resp = query(sock, mid)
            last[mid] = resp
            if "moving=1" in resp:
                busy.append(mid)
        if not busy:
            return [last[m] for m in motor_ids]
        time.sleep(interval)
    print(f"wait_for_idle: timeout after {timeout}s, still moving: {busy}")
    return [last.get(m, "?") for m in motor_ids]


def main():
    parser = argparse.ArgumentParser(description="Read all 5 motor positions")
    parser.add_argument("--host", default=HOST, help=f"IP address (default: {HOST})")
    parser.add_argument("--port", type=int, default=PORT, help=f"TCP port (default: {PORT})")
    args = parser.parse_args()

    s = connect(args.host, args.port)

    # Firmware info
    print(send(s, "M115"))
    print("-" * 60)

    # Wait until all motors settle, then read each
    print("Waiting for motors to settle ...")
    wait_for_idle(s)
    print("-" * 60)

    for mid in range(1, 6):
        resp = query(s, mid)
        name = MOTOR_NAMES.get(mid, f"Motor {mid}")
        print(f"[{mid}] {name:15s} | {resp}")

    print("-" * 60)
    # Also print M114 for comparison
    print("M114:", send(s, "M114"))

    s.close()


if __name__ == "__main__":
    main()
