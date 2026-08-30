# -*- coding: utf-8 -*-
"""状态机 + 输出级在持久RAM上的连续多拍 A/B。"""
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

RETURN = 0x3FF01
ORIG_STATE = 0x458C
ORIG_OUTPUT = 0xE9AC
ORIG_DISPLAY = (0x0D3C, 0x0ED0, 0x0E42, 0x0992, 0x427C)
NEW_DISPLAY = tuple(SYMS[n] for n in
                    ("disp_string", "disp_uint4", "disp_number3", "disp_clear", "disp_splash_screen"))


def prepare(is_new):
    uc = machine(is_new)
    display = NEW_DISPLAY if is_new else ORIG_DISPLAY

    def skip(machine_, address, size, user):
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    for address in display:
        uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    uc.mem_write(0x10001744, bytes((1, 0, 0)))
    uc.mem_write(0x10001698, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    uc.mem_write(0x10001660, struct.pack("<I", 90))
    uc.mem_write(0x10001710, bytes((2, 10, 10, 0, 0, 0, 2, 2, 2)))
    uc.mem_write(0x10002078, b"\x09")
    uc.mem_write(0x100015CE, struct.pack("<I", 0))
    return uc


def execute(is_new, stimuli):
    uc = prepare(is_new)
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    output = SYMS["output_stage"] if is_new else ORIG_OUTPUT
    snapshots = []
    for key, run_flag, fault in stimuli:
        uc.mem_write(0x10001785, bytes((run_flag,)))
        uc.mem_write(0x10001624, struct.pack("<I", fault))
        uc.reg_write(UC_ARM_REG_R0, key)
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        run(uc, state, max_insn=2_000_000)
        uc.reg_write(UC_ARM_REG_LR, RETURN)
        run(uc, output, max_insn=2_000_000)
        snapshots.append(bytes(uc.mem_read(0x10000000, 0x2200)))
    return snapshots


def main():
    cases = [
        ("空闲连续30拍", [(0, 0, 0)] * 30),
        ("运行/停止与故障交错", [(0, i % 2, (0, 8, 0x10, 0x20)[i % 4]) for i in range(32)]),
        ("主界面按键序列", [((0, 1, 2, 3, 4, 5, 6, 0x16, 0x17, 0x21)[i % 10], 0, 0)
                          for i in range(30)]),
    ]
    failed = 0
    for name, stimuli in cases:
        old = execute(False, stimuli)
        new = execute(True, stimuli)
        ok = old == new
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  ticks={len(stimuli)}")
        if not ok:
            tick = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
            diff = next(i for i, (a, b) in enumerate(zip(old[tick], new[tick])) if a != b)
            print(f"    首差异：tick={tick + 1}, addr=0x{0x10000000 + diff:08X}, "
                  f"原=0x{old[tick][diff]:02X}, 新=0x{new[tick][diff]:02X}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
