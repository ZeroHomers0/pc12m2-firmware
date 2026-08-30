# -*- coding: utf-8 -*-
"""原BIN与新ELF软件延时的循环倍率和可观察副作用检查。"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_R0
from verify_firmware_equivalence import machine, run, SYMS

OLD = {"Delay": 0x7A8, "i2c_delay_short": 0x1C6C, "i2c_delay": 0x1C82}


def execute(is_new, name, arg):
    uc = machine(is_new)
    before = bytes(uc.mem_read(0x10000000, 0x2200))
    count = [0]
    uc.hook_add(UC_HOOK_CODE, lambda machine_, address, size, user: count.__setitem__(0, count[0] + 1))
    uc.reg_write(UC_ARM_REG_R0, arg)
    uc.reg_write(UC_ARM_REG_LR, 0x3FF01)
    run(uc, SYMS[name] if is_new else OLD[name], max_insn=2_000_000)
    after = bytes(uc.mem_read(0x10000000, 0x2200))
    return count[0], before == after


def linear(counts):
    base, one, two = counts
    return two - base == 2 * (one - base) and one > base


def main():
    failed = 0
    for name in ("Delay", "i2c_delay"):
        old = [execute(False, name, n) for n in (0, 1, 2)]
        new = [execute(True, name, n) for n in (0, 1, 2)]
        ok = linear([x[0] for x in old]) and linear([x[0] for x in new]) and all(x[1] for x in old + new)
        print(f"  [{'PASS' if ok else 'FAIL'}] {name} 循环倍率  原指令={[x[0] for x in old]} 新指令={[x[0] for x in new]}")
        failed += not ok
    old_short = execute(False, "i2c_delay_short", 0)
    new_short = execute(True, "i2c_delay_short", 0)
    ok = old_short[1] and new_short[1] and old_short[0] > 5 and new_short[0] > 5
    print(f"  [{'PASS' if ok else 'FAIL'}] i2c_delay_short 固定5轮  原指令={old_short[0]} 新指令={new_short[0]}")
    failed += not ok
    print("  注：循环次数/倍率一致；不同编译器的单轮指令数不同，绝对脉宽仍需示波器确认。")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
