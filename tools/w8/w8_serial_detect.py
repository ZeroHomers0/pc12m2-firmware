#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
w8_serial_detect.py — 枚举电脑上的所有串口(COM)，帮你在 W8 阶段 B 认出 USB-RS485 是哪个口。

用法：
    python tools/w8/w8_serial_detect.py

依赖：pyserial（`python -m pip install pyserial`）
预期：列出每个 COM 口 + 描述；USB-RS485 一般显示 CH340 / FTDI / USB Serial。
"""
import sys
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

try:
    import serial.tools.list_ports as list_ports
except ImportError:
    print("[错误] 缺 pyserial 库。请运行:  python -m pip install pyserial")
    sys.exit(1)

def main():
    ports = list_ports.comports()
    if not ports:
        print("未发现任何串口。请确认 USB-RS485 已插入且驱动已装（设备管理器里看有没有黄色感叹号）。")
        return

    print(f"发现 {len(ports)} 个串口：")
    print("-" * 60)
    for p in ports:
        dev = p.device            # 如 COM5
        desc = p.description      # 如 USB-SERIAL CH340 (COM5)
        hwid = p.hwid or ""
        print(f"{dev:8s}  {desc}")
        print(f"{'':8s}  HWID: {hwid}")
        print("-" * 60)

    # 提示：通常是带"USB"/"CH340"/"FTDI"字样的那个
    guess = [p.device for p in ports if any(k in (p.description or '').upper()
             for k in ('USB', 'CH340', 'FTDI', 'SERIAL'))]
    if guess:
        print(f"提示：最可能是 {', '.join(guess)}（含 USB/CH340/FTDI 字样的那个）。")
        print("下一命令示例:  python tools/w8/w8_modbus_test.py --port {0} --addr 1".format(guess[0]))
    else:
        print("提示：没找到带 USB/CH340/FTDI 字样的口，请对照设备管理器确认。")

if __name__ == '__main__':
    main()
