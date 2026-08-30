# -*- coding: utf-8 -*-
"""主界面故障/复位/急停/RUN组合的连续多拍状态机A/B矩阵。"""
import os
import sys
import struct

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
from verify_firmware_equivalence import machine, run, SYMS

ORIG_STATE = 0x458C
RETURN = 0x3FF01
ORIG_DISPLAY = (0x0D3C, 0x0ED0, 0x0E42, 0x0992, 0x427C)
NEW_DISPLAY = tuple(SYMS[n] for n in
                    ("disp_string", "disp_uint4", "disp_number3", "disp_clear", "disp_splash_screen"))


def execute(is_new, menu, fault, reset_mode, estop, run_flag, ticks):
    uc = machine(is_new)
    display = NEW_DISPLAY if is_new else ORIG_DISPLAY

    def skip(machine_, address, size, user):
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    for address in display:
        uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    uc.mem_write(0x10001744, bytes((menu, 0, 0)))
    uc.mem_write(0x10001624, struct.pack("<I", fault))
    uc.mem_write(0x10001657, bytes((estop, reset_mode)))
    uc.mem_write(0x10001785, bytes((run_flag,)))
    uc.mem_write(0x10001660, struct.pack("<I", 90))
    uc.mem_write(0x10001698, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    uc.mem_write(0x10001710, bytes((2, 10, 10, 0, 0, 0, 2, 2, 2)))
    # 输入保持高，避免矩阵意外进入原厂“复位后死等看门狗”的终止设计。
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    snapshots = []
    for _ in range(ticks):
        uc.reg_write(UC_ARM_REG_R0, 0)
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        run(uc, state, max_insn=2_000_000)
        snapshots.append(bytes(uc.mem_read(0x10000000, 0x2200)))
    return snapshots


def main():
    cases = []
    for menu in (1, 0x1E):
        for fault in (0, 1, 8, 0x10, 0x20, 0x4000):
            for reset_mode in (0, 1, 2):
                for estop in (0, 1, 2):
                    for run_flag in (0, 1):
                        cases.append((menu, fault, reset_mode, estop, run_flag, 2))
    # 对不会进入复位死循环的代表场景再跑10拍，覆盖计数累积。
    for menu in (1, 0x1E):
        for fault in (0, 8, 0x20):
            cases.append((menu, fault, 1, 1, 0, 10))

    failed = 0
    for case in cases:
        old = execute(False, *case)
        new = execute(True, *case)
        if old != new:
            failed += 1
            tick = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
            diff = next(i for i, (a, b) in enumerate(zip(old[tick], new[tick])) if a != b)
            print(f"  [FAIL] case={case} tick={tick+1} addr=0x{0x10000000+diff:08X} "
                  f"原=0x{old[tick][diff]:02X} 新=0x{new[tick][diff]:02X}")
    passed = len(cases) - failed
    print(f"  [{'PASS' if not failed else 'FAIL'}] 状态机跨拍组合矩阵 {passed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
