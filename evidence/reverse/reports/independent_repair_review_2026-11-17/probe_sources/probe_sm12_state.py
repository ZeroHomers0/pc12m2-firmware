# -*- coding: utf-8 -*-
"""PC12M-2 状态机终态差分：比较 SRAM0 + 已映射外设区（不比较写迹，避免 IT 块漏报）。"""
import os
import struct
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "verification"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
from verify_firmware_equivalence_12 import machine, run, SYMS, STOP

REGIONS = ((0x2009C000, 0x1000), (0x40008000, 0x1000), (0x40090000, 0x1000),
           (0x4009C000, 0x1000), (0x400FC000, 0x2000), (0xE000E000, 0x2000))
SRAM_END = 0x2200

BASE = {
    0x10001624: 0, 0x10001628: 0, 0x1000162C: 50, 0x10001630: 50, 0x10001634: 0,
    0x10001638: 1000, 0x1000163C: 1000, 0x10001640: 1000, 0x10001644: 500,
    0x10001648: 800, 0x1000164C: 3, 0x1000164D: 2, 0x10001650: 5, 0x10001654: 10,
    0x10001657: 0, 0x10001659: 0, 0x1000165B: 0, 0x10001660: 90,
    0x1000170E: 10, 0x1000170F: 20, 0x10001710: 0,
    0x10001711: 1, 0x10001712: 2, 0x10001713: 3, 0x10001714: 4,
    0x10001715: 5, 0x10001716: 6, 0x10001717: 7, 0x10001718: 8,
    0x10001722: 50, 0x10001723: 40, 0x10001724: 30, 0x10001725: 20, 0x10001726: 10,
    0x1000172C: 0, 0x10001730: 0, 0x10001734: 0, 0x10001785: 0,
    0x100015A8: 500, 0x100015D0: 500, 0x10001788: 61, 0x10001774: 7,
    0x100015D8: 137, 0x100015D4: 9,
}

SKIP_NAMES = ("disp_string", "disp_uint4", "disp_number3", "disp_uint2", "disp_signed_angle",
              "disp_fixed_1dec", "disp_number", "disp_uint5", "disp_offset", "disp_digit",
              "disp_clear", "disp_splash_screen", "disp_decimal1")


def snapshot(is_new, menu, item, view, key, params, ticks=1, delay=0, max_insn=4_000_000):
    uc = machine(is_new)

    def skip(m, a, s, u):
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    for name in SKIP_NAMES:
        if is_new and name in SYMS:
            address = SYMS[name]
            uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    if not is_new:
        for address in (0xCEC, 0xD44, 0xDF2, 0xE80, 0xF3C, 0x1042, 0x116C, 0x1260,
                        0x1398, 0x13EC, 0x14A6, 0x942, 0x1486):
            uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    uc.mem_write(0x10001724, bytes((menu, item, view)))
    uc.mem_write(0x10001744, struct.pack("<I", 0))
    uc.mem_write(0x10001758, struct.pack("<I", 0))
    for address, value in params.items():
        uc.mem_write(address, struct.pack("<I", value))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    state = SYMS["state_machine"] if is_new else 0x4464
    for step in range(ticks):
        instant = key
        if isinstance(key, (list, tuple)):
            instant = key[step % len(key)]
        uc.reg_write(UC_ARM_REG_R0, instant)
        uc.reg_write(UC_ARM_REG_LR, STOP + 1)
        run(uc, state, max_insn=max_insn)
    out = bytearray(uc.mem_read(0x10000000, SRAM_END))
    for base, size in REGIONS:
        out += bytes(uc.mem_read(base, size))
    return bytes(out)


def compare(label, *args, **kwargs):
    old = snapshot(False, *args, **kwargs)
    new = snapshot(True, *args, **kwargs)
    if old == new:
        return None
    first = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
    return (label, 0x10000000 + first, old[first], new[first])


def main():
    cases = []
    for mode in (0, 1, 2):
        for method in (0, 1, 2, 3):
            params = dict(BASE)
            params.update({0x10001634: mode, 0x10001655: method})
            cases.append(("mode%d/method%d" % (mode, method), 1, 0, 0, 0, params))
    for menu in (1, 2, 3, 4, 5, 6, 7, 8, 9, 0x0A, 0x0B, 0x0C, 0x14, 0x1E, 0x5A, 0x63):
        for key in (0, 1, 2, 3, 4, 5, 6, 0x16, 0x17, 0x21):
            cases.append(("menu%#x/key%#x" % (menu, key), menu, 0, 0, key, dict(BASE)))
    for menu in (2, 6):
        for item in range(0, 10):
            cases.append(("menu%#x/item%d/enter" % (menu, item), menu, item, 1, 1, dict(BASE)))
            cases.append(("menu%#x/item%d/add" % (menu, item), menu, item, 1, 0x16, dict(BASE)))
            cases.append(("menu%#x/item%d/sub" % (menu, item), menu, item, 1, 0x21, dict(BASE)))

    failed = 0
    for label, menu, item, view, key, params in cases:
        result = compare(label, menu, item, view, key, params)
        if result:
            failed += 1
            print("[DIFF] %s addr=0x%08X OLD=0x%02X NEW=0x%02X"
                  % (result[0], result[1], result[2], result[3]))
    print("状态终态差分 %d/%d" % (len(cases) - failed, len(cases)))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
