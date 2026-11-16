# -*- coding: utf-8 -*-
"""PC12M-2 枚举参数越界时的绘制契约（原 BIN 越界值整行不绘制）。

用户实测指出：通信校验为 2、3 等越界值时原版不画选项，旧源码 `if (==1) 开启 else 关闭`
会把任意非 1 值画成「关闭」。12p 原 BIN 依据：0x87CC 只用 `cmp #3; bne` 画 '1 ST0P'，
0x8840 `cbnz` / 0x8852 `cmp #1; bne` 只在 0/1 时画关闭/开启；入口块 0x632C/0x6340 同理。

注意：12p 页面只在 `*TIMEOUT3 == 0xFB` 时整页重绘，故这里把 0x10001758 预置 0xFA
（++ 后正好 0xFB），否则两版都不绘制、比较会变成空跑。
"""
import os
import struct
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.arm_const import (  # noqa: E402
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_LR, UC_ARM_REG_PC,
)
from verify_firmware_equivalence_12 import machine, run, SYMS, STOP  # noqa: E402

ORIG_STATE = 0x4464
ORIG_FUNCS = {
    0xCEC: "disp_string", 0xD44: "disp_digit", 0xDF2: "disp_number3",
    0xE80: "disp_uint4", 0xF3C: "disp_uint5", 0x1042: "disp_number",
    0x116C: "disp_signed_angle", 0x1260: "disp_offset", 0x1398: "disp_uint2",
    0x13EC: "disp_fixed_1dec", 0x14A6: "disp_decimal1", 0x942: "disp_clear",
    0x40B0: "disp_splash_screen",
}
ARGC = {"disp_digit": 1, "disp_clear": 0, "disp_splash_screen": 0}

PARITY = 0x100016FC
COM_CHK = 0x100016FD
CTRL_MODE = 0x1000162C
DISP_SEL = 0x1000164D
ESTOP = 0x1000164F

BASE = {
    0x10001624: 90, 0x10001634: 1000, 0x10001630: 1000, 0x10001638: 1000,
    0x10001650: 0, 0x10001708: 0, 0x1000175D: 0, 0x1000175E: 0, 0x1000175F: 0,
    0x1000161C: 0, 0x10001620: 0,
}


def resolve(is_new):
    if not is_new:
        return dict(ORIG_FUNCS)
    return {SYMS[name]: name for name in ORIG_FUNCS.values() if name in SYMS}


def trace(is_new, menu, item, params, key=0):
    uc = machine(is_new)
    table = resolve(is_new)
    calls = []

    def hook(m, address, size, user):
        name = table[address]
        args = [m.reg_read(UC_ARM_REG_R0), m.reg_read(UC_ARM_REG_R1),
                m.reg_read(UC_ARM_REG_R2), m.reg_read(UC_ARM_REG_R3)]
        calls.append((name,) + tuple(args[:ARGC.get(name, 4)]))
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    for address in table:
        uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address + 1)
    uc.mem_write(0x10001724, bytes((menu, item, 0)))
    uc.mem_write(0x10001744, struct.pack("<I", 0xFA))
    uc.mem_write(0x10001758, struct.pack("<I", 0xFA))
    for address, value in params.items():
        uc.mem_write(address, struct.pack("<I", value))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    uc.reg_write(UC_ARM_REG_R0, key)
    uc.reg_write(UC_ARM_REG_LR, STOP + 1)
    run(uc, state, max_insn=4_000_000)
    return calls


def cases():
    out = []
    for value in range(5):
        for item in (2, 3):
            params = dict(BASE)
            params[PARITY] = value
            params[COM_CHK] = value
            out.append(("parity/comchk=%d item=%d" % (value, item), 5, item, params))
    for value in range(4):
        for address, item in ((CTRL_MODE, 0), (DISP_SEL, 10), (ESTOP, 12)):
            params = dict(BASE)
            params[address] = value
            out.append(("enum 0x%X=%d item=%d" % (address, value, item), 3, item, params))
    return out


def main():
    failed = 0
    total = 0
    empty = 0
    for tag, menu, item, params in cases():
        old = trace(False, menu, item, params)
        new = trace(True, menu, item, params)
        total += 1
        if not old and not new:
            empty += 1
        if old != new:
            failed += 1
            print("  [FAIL] %s" % tag)
            print("     OLD:", old)
            print("     NEW:", new)
    print("  [%s] 12p 枚举越界绘制 %d/%d（双方都空 %d）"
          % ("PASS" if not failed else "FAIL", total - failed, total, empty))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
