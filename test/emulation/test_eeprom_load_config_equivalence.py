# -*- coding: utf-8 -*-
"""load_config 双银行读取/默认恢复的原 BIN / 新 ELF A/B。"""
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

ORIG_LOAD = 0x25DC
ORIG_WRITE = 0x1E88
ORIG_READ = 0x1EBC
RETURN = 0xFF000000
RAM_IMAGE = os.path.join(ROOT, "firmware", "assets", "ram_data_image.bin")


def run(load_fn, load_addr, read_addr, write_addr, eeprom):
    uc = load_fn()
    with open(RAM_IMAGE, "rb") as f:
        uc.mem_write(0x10000000, f.read())
    writes = []

    def read_hook(machine, address, size, user):
        out = machine.reg_read(UC_ARM_REG_R0)
        reg = machine.reg_read(UC_ARM_REG_R1) & 0xFF
        machine.mem_write(out, bytes([eeprom[reg]]))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    def write_hook(machine, address, size, user):
        data = machine.reg_read(UC_ARM_REG_R0) & 0xFF
        reg = machine.reg_read(UC_ARM_REG_R1) & 0xFF
        writes.append((reg, data))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, read_hook, begin=read_addr, end=read_addr + 1)
    uc.hook_add(UC_HOOK_CODE, write_hook, begin=write_addr, end=write_addr + 1)
    uc.reg_write(UC_ARM_REG_LR, RETURN)
    uc.emu_start(load_addr | 1, RETURN, count=2_000_000)
    return bytes(uc.mem_read(0x10000000, 0x4000)), writes


def main():
    new_load = lookup("load_config")
    new_read = lookup("i2c_read_reg")
    new_write = lookup("i2c_write_reg")
    base = bytearray(((i * 73 + 19) & 0xFF) for i in range(256))
    cases = []
    valid = bytearray(base)
    valid[5] = valid[6] = 0x55
    valid[7] = valid[8] = 0x66
    cases.append(("双银行魔数有效：全部从EEPROM装载", valid))
    bad_a = bytearray(valid)
    bad_a[5] = bad_a[6] = 0
    cases.append(("银行A无效：回写A默认值", bad_a))
    bad_b = bytearray(valid)
    bad_b[7] = bad_b[8] = 0
    cases.append(("银行B无效：回写B默认值", bad_b))
    bad_both = bytearray(base)
    bad_both[5] = bad_both[6] = bad_both[7] = bad_both[8] = 0
    cases.append(("双银行无效：依次回写两组默认值", bad_both))

    failed = 0
    for name, image in cases:
        old_ram, old_writes = run(load_original, ORIG_LOAD, ORIG_READ, ORIG_WRITE, image)
        new_ram, new_writes = run(load_firmware, new_load, new_read, new_write, image)
        ok = old_ram == new_ram and old_writes == new_writes
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  writes={len(old_writes)}")
        if not ok:
            if old_writes != new_writes:
                print(f"    写序列不一致：原={old_writes[:8]} 新={new_writes[:8]}")
            else:
                diff = next((i for i, (a, b) in enumerate(zip(old_ram, new_ram)) if a != b), -1)
                print(f"    RAM首差异：0x{0x10000000 + diff:08X}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
