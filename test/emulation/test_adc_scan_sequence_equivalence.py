# -*- coding: utf-8 -*-
"""ADC 六通道连续扫描、平均与换算的原 BIN / 新 ELF A/B。"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "test", "support"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
from unicorn_harness import load_firmware, load_original, lookup

ORIG_SCAN, ORIG_START, ORIG_WAIT = 0x1FBC, 0x1F80, 0x1FA6
RETURN = 0xFF000000
ADC = 0x40034000
RAM_IMAGE = os.path.join(ROOT, "firmware", "assets", "ram_data_image.bin")


def run(load_fn, scan, start, wait, samples, calls, overrides=()):
    uc = load_fn()
    uc.mem_map(ADC, 0x1000)
    with open(RAM_IMAGE, "rb") as f:
        uc.mem_write(0x10000000, f.read())
    for address, data in overrides:
        uc.mem_write(address, data)
    cursor = [0]

    def start_hook(machine, address, size, user):
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    def wait_hook(machine, address, size, user):
        value = samples[cursor[0] % len(samples)] & 0xFFF
        cursor[0] += 1
        machine.reg_write(UC_ARM_REG_R0, value)
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, start_hook, begin=start, end=start + 1)
    uc.hook_add(UC_HOOK_CODE, wait_hook, begin=wait, end=wait + 1)
    for _ in range(calls):
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        uc.emu_start(scan | 1, RETURN, count=1_000_000)
    return (bytes(uc.mem_read(0x10000000, 0x4000)),
            bytes(uc.mem_read(ADC, 0x40)), cursor[0])


def main():
    new_scan = lookup("adc0_scan_channels")
    new_start = lookup("adc0_start")
    new_wait = lookup("adc0_wait_done")
    cases = [
        ("零值连续12拍", [0], 12, ()),
        ("六通道固定梯度连续12拍", [100, 300, 500, 700, 900, 1100], 12, ()),
        ("边界与交错样本连续24拍", [0, 1, 9, 10, 2047, 4095, 123, 3960], 24, ()),
    ]
    for cfg in (0, 1):
        for gain_sel in (0, 1, 2):
            cases.append((f"cfg={cfg}/gain_sel={gain_sel}", [9, 10, 2047, 4095, 800, 1000], 18,
                          ((0x10001628, bytes((cfg,))), (0x10001634, bytes((gain_sel,))))))
    for divisor in (1, 10, 0xFFFF, 0xFFFFFFFF):
        packed = divisor.to_bytes(4, "little")
        overrides = tuple((address, packed) for address in
                          (0x10001698, 0x100016A0, 0x100016A8, 0x100016B0, 0x100016B8))
        cases.append((f"标定除数边界0x{divisor:X}", [0, 1, 4095, 3960, 10, 2047], 18, overrides))
    failed = 0
    for name, samples, calls, overrides in cases:
        old = run(load_original, ORIG_SCAN, ORIG_START, ORIG_WAIT, samples, calls, overrides)
        new = run(load_firmware, new_scan, new_start, new_wait, samples, calls, overrides)
        ok = old == new
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  conversions={old[2]}")
        if not ok:
            diff = next((i for i, (a, b) in enumerate(zip(old[0], new[0])) if a != b), -1)
            print(f"    RAM首差异：0x{0x10000000 + diff:08X}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
