"""LPC17xx UART0 ISP read-only probe.

Only performs autobaud synchronization and read-only J/K/N commands.
It never sends erase, prepare, copy, write, or unlock commands.
"""

from __future__ import annotations

import argparse
import time

import serial


def read_until_quiet(port: serial.Serial, quiet_s: float = 0.25, limit_s: float = 2.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + limit_s
    quiet_deadline = time.monotonic() + quiet_s
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            data.extend(chunk)
            quiet_deadline = time.monotonic() + quiet_s
        elif data and time.monotonic() >= quiet_deadline:
            break
    return bytes(data)


def transact(port: serial.Serial, command: bytes) -> bytes:
    port.reset_input_buffer()
    port.write(command + b"\r\n")
    port.flush()
    return read_until_quiet(port)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--crystal-khz", type=int, default=12000)
    parser.add_argument("--wait", type=float, default=20.0)
    args = parser.parse_args()

    print(f"[*] {args.port} @ {args.baud}; 等待 ISP 同步 {args.wait:.0f}s")
    print("[*] 保持 P12-3=GND，然后复位一次")
    with serial.Serial(args.port, args.baud, timeout=0.08) as port:
        deadline = time.monotonic() + args.wait
        response = b""
        while time.monotonic() < deadline:
            port.reset_input_buffer()
            port.write(b"?")
            port.flush()
            response = read_until_quiet(port, quiet_s=0.12, limit_s=0.6)
            if b"Synchronized" in response:
                break
            time.sleep(0.15)
        else:
            print("[FAIL] 未收到 Synchronized；检查 ISP 进入条件和 TX/RX")
            return 2

        print("[OK] Boot ROM 响应:", response.decode("ascii", "replace").strip())
        for label, command in (
            ("同步确认", b"Synchronized"),
            ("晶振频率", str(args.crystal_khz).encode("ascii")),
            ("关闭回显", b"A 0"),
            ("Part ID", b"J"),
            ("Boot版本", b"K"),
            ("UID", b"N"),
        ):
            response = transact(port, command)
            print(f"[{label}] {response.decode('ascii', 'replace').strip()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
