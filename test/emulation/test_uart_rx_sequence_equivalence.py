# -*- coding: utf-8 -*-
"""UART3 RX 组帧子例程的连续调用 A/B。

原 BIN @0xAED0 与编译符号 func_0x0000aed0 在相同初态和字节序列下执行，
比较 state/gap/index 以及完整 256 字节接收区。这里验证还原一致性，不评价
原固件索引回绕行为是否合理。
"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "test", "support"))

from unicorn.arm_const import UC_ARM_REG_LR
from unicorn_harness import load_firmware, load_original, lookup

ORIG = 0xAED0
STATE = 0x10001790
UART3_RBR = 0x4009C000
FRAME = 0x100022A4
RETURN = 0xFF000000


def map_uart(uc):
    try:
        uc.mem_map(0x4009C000, 0x1000)
    except Exception:
        pass


def run(load_fn, entry, initial, data):
    uc = load_fn()
    map_uart(uc)
    uc.mem_write(STATE, bytes(initial))
    uc.mem_write(FRAME, bytes([0xCC]) * 256)
    for value in data:
        uc.mem_write(UART3_RBR, bytes([value]))
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        uc.emu_start(entry | 1, RETURN)
    return bytes(uc.mem_read(STATE, 3)), bytes(uc.mem_read(FRAME, 256))


def main():
    new = lookup("func_0x0000aed0")
    cases = [
        ("空闲态首字节后进入接收态", (0, 7, 19), [0xA5]),
        ("接收态连续8字节", (1, 9, 3), list(range(0x10, 0x18))),
        ("非接收态不改变缓冲区", (5, 11, 23), [0x55, 0xAA]),
        ("索引254跨越255并回绕", (1, 13, 254), [0x31, 0x32, 0x33]),
        ("索引255单步回绕到0", (1, 15, 255), [0x7E]),
    ]
    failed = 0
    for name, initial, data in cases:
        old_state, old_frame = run(load_original, ORIG, initial, data)
        new_state, new_frame = run(load_firmware, new, initial, data)
        ok = old_state == new_state and old_frame == new_frame
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  state={old_state.hex()}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
