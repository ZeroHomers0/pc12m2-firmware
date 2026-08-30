# -*- coding: utf-8 -*-
"""param_sync_live_to_eeprom逐字节扰动与批量扰动A/B。"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "test", "support"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1
from unicorn_harness import load_firmware, load_original, lookup

ORIG_SYNC, ORIG_WRITE = 0x35F2, 0x1E88
RETURN = 0xFF000000
RAM_IMAGE = os.path.join(ROOT, "firmware", "assets", "ram_data_image.bin")


def execute(load_fn, sync, write, mutations):
    uc = load_fn()
    with open(RAM_IMAGE, "rb") as f:
        uc.mem_write(0x10000000, f.read())
    for address, value in mutations:
        uc.mem_write(address, bytes([value & 0xFF]))
    writes = []

    def write_hook(machine, address, size, user):
        writes.append((machine.reg_read(UC_ARM_REG_R1) & 0xFF,
                       machine.reg_read(UC_ARM_REG_R0) & 0xFF))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, write_hook, begin=write, end=write + 1)
    uc.reg_write(UC_ARM_REG_LR, RETURN)
    uc.emu_start(sync | 1, RETURN, count=2_000_000)
    return bytes(uc.mem_read(0x10001000, 0x800)), writes


def main():
    new_sync = lookup("param_sync_live_to_eeprom")
    new_write = lookup("i2c_write_reg")
    with open(RAM_IMAGE, "rb") as f:
        image = f.read()
    cases = []
    # 覆盖live参数密集区的每个字节；非参数字节应产生零写且两侧仍一致。
    for address in range(0x10001620, 0x10001730):
        old = image[address - 0x10000000]
        cases.append((f"byte@0x{address:08X}", [(address, old ^ 0x5A)]))
    # 批量扰动覆盖一次同步中多个8/16位参数连续写入的顺序。
    for seed in range(8):
        muts = []
        for i, address in enumerate(range(0x10001620, 0x10001730, 7)):
            muts.append((address, (image[address - 0x10000000] + seed * 29 + i + 1) & 0xFF))
        cases.append((f"batch{seed}", muts))

    failed = 0
    for name, mutations in cases:
        old = execute(load_original, ORIG_SYNC, ORIG_WRITE, mutations)
        new = execute(load_firmware, new_sync, new_write, mutations)
        if old != new:
            failed += 1
            print(f"  [FAIL] {name} 原写={old[1][:8]} 新写={new[1][:8]}")
    print(f"  [{'PASS' if not failed else 'FAIL'}] EEPROM同步扰动矩阵 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
