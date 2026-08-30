# -*- coding: utf-8 -*-
"""状态机关键输入/GPIO子调用的次数、顺序和参数A/B。"""
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
OLD_CALLS = {
    "debounce_p117": 0x1B3E, "debounce_p06": 0x1B96,
    "scan_run_stop": 0x19C6, "fio1_pin20_ctrl": 0x105C8,
    "fio1_pin21_ctrl": 0x105E8,
}
NEW_CALLS = {name: SYMS[name] for name in OLD_CALLS}
ORIG_DISPLAY = (0x0D3C, 0x0ED0, 0x0E42, 0x0992, 0x427C)
NEW_DISPLAY = tuple(SYMS[n] for n in
                    ("disp_string", "disp_uint4", "disp_number3", "disp_clear", "disp_splash_screen"))


def execute(is_new, menu, fault, reset_mode, estop, run_flag, ticks):
    uc = machine(is_new)
    calls = NEW_CALLS if is_new else OLD_CALLS
    display = NEW_DISPLAY if is_new else ORIG_DISPLAY
    trace = []

    def skip(machine_, address, size, user):
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    def record(name):
        def hook(machine_, address, size, user):
            arg = machine_.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF if name.startswith("fio1_") else None
            trace.append((name, arg))
        return hook

    for address in display:
        uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    for name, address in calls.items():
        uc.hook_add(UC_HOOK_CODE, record(name), begin=address, end=address + 1)
    uc.mem_write(0x10001744, bytes((menu, 0, 0)))
    uc.mem_write(0x10001624, struct.pack("<I", fault))
    uc.mem_write(0x10001657, bytes((estop, reset_mode)))
    uc.mem_write(0x10001785, bytes((run_flag,)))
    uc.mem_write(0x10001660, struct.pack("<I", 90))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    per_tick = []
    for _ in range(ticks):
        start = len(trace)
        uc.reg_write(UC_ARM_REG_R0, 0)
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        run(uc, state, max_insn=2_000_000)
        per_tick.append(tuple(trace[start:]))
    return per_tick


def main():
    cases = []
    for menu in (1, 0x1E):
        for fault in (0, 8, 0x10, 0x20, 0x4000):
            for reset_mode, estop in ((0, 0), (1, 0), (2, 0), (0, 1), (0, 2)):
                for run_flag in (0, 1):
                    cases.append((menu, fault, reset_mode, estop, run_flag, 3))
    failed = 0
    for case in cases:
        old = execute(False, *case)
        new = execute(True, *case)
        if old != new:
            failed += 1
            tick = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
            print(f"  [FAIL] case={case} tick={tick+1}\n    原={old[tick]}\n    新={new[tick]}")
    print(f"  [{'PASS' if not failed else 'FAIL'}] 状态机关键子调用轨迹 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
