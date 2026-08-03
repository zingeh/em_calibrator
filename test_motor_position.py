#!/usr/bin/env python3
"""
Test: read actual position of all 5 motors via M1 query.

Usage:
  python test_motor_position.py
  python test_motor_position.py --host 192.168.123.181
"""

import socket
import argparse

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


def main():
    parser = argparse.ArgumentParser(description="Read all 5 motor positions")
    parser.add_argument("--host", default=HOST, help=f"IP address (default: {HOST})")
    parser.add_argument("--port", type=int, default=PORT, help=f"TCP port (default: {PORT})")
    args = parser.parse_args()

    s = connect(args.host, args.port)

    # Firmware info
    print(send(s, "M115"))
    print("-" * 60)

    # Read each motor
    for mid in range(1, 6):
        resp = send(s, f"M1 {mid}")
        name = MOTOR_NAMES.get(mid, f"Motor {mid}")
        print(f"[{mid}] {name:15s} | {resp}")

    print("-" * 60)
    # Also print M114 for comparison
    print("M114:", send(s, "M114"))

    s.close()


if __name__ == "__main__":
    main()
