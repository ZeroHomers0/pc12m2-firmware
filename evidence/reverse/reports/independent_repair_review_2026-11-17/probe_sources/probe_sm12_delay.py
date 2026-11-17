# -*- coding: utf-8 -*-
"""12p 状态机 key=1（确认键）用例的密码延时快进复测。

背景：`tmp/probe_sm12_all.py` 跑 18 屏 × 17 项 × 12 键 × 2 view 得 6120/6732，
余下 612 例全部是 key=1 且 `execution incomplete ... pc=0x522a budget=4000000`——
落进密码错误延时（10000 × 1000 次空转），属**仿真预算截断**，非行为差异。

原机该循环被内联成 3 份副本，结构完全相同（每份 0x42 字节）：
  0x5220/0x536C/0x8988  movs r0,#0; ldr r1,[pool]; str r0,[r1]    ; DELAY_IN=0
  0x5228/0x5374/0x8990  ldr/adds/str + cmp.w #1000 + bcc           ; 内层 1000 次
                        bl 0x238 (wd_feed)                          ; 每外层喂狗一次
                        ldr/adds/str + movw #10000 + cmp + bcc      ; 外层 10000 次
新 ELF 为独立函数 `sm6_delay_loop` @ 0x422C（`movs r6,#0x3e8; movw r4,#10000`）。
字面量池实测：0x561C=0x10001618(DELAY_IN)、0x5620=0x10001614(DELAY_OUT)。

做法：在内层循环体入口写 DELAY_IN=1000、DELAY_OUT=10000，使内层立刻退出、
外层自增到 10001 后不再回跳；新机在函数入口同样写两值后直接跳 LR。
两机各多跑一轮、终值一致，且计数器已被 VOLATILE 掩码覆盖。

用法：python tmp\\probe_sm12_delay.py [menus，逗号分隔，默认全部]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import probe_sm12_state as P  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0  # noqa: E402

DELAY_OUT = 0x10001614
DELAY_IN = 0x10001618
# The password delay exists as TEN inline copies in the OLD image, not three.
# Found by scanning for `movw rX,#10000 ; cmp ; bcc` and taking the bcc target:
#   movw 0x5256 -> 0x5220    movw 0x539a -> 0x536c    movw 0x89b6 -> 0x8988
#   movw 0x8ad2 -> 0x8aa4    movw 0x8b1c -> 0x8aee    movw 0x8b66 -> 0x8b38
#   movw 0x8be8 -> 0x8bba    movw 0x8c9e -> 0x8c70    movw 0x8ce8 -> 0x8cba
#   movw 0x8d32 -> 0x8d04
# With only the first three covered, the other seven kept spinning 10,000 x
# 1,000 and blew the instruction budget (menu=0x6 key=0xE -> pc=0x8bca).
OLD_BODY_SITES = (0x5220, 0x536c, 0x8988, 0x8aa4, 0x8aee, 0x8b38,
                  0x8bba, 0x8c70, 0x8cba, 0x8d04)
NEW_FUNC = 0x422C                           # sm6_delay_loop

MENUS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0x14, 0x1E, 0x5A, 0x62, 0x63)
ITEMS = tuple(range(0, 17))
VIEWS = (0, 1)
KEY = 1
VOLATILE = (0x1614, 0x161C)


def install(uc, is_new):
    if is_new:
        def hook(m, address, size, user):
            m.mem_write(DELAY_IN, struct.pack("<I", 1000))
            m.mem_write(DELAY_OUT, struct.pack("<I", 10000))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        uc.hook_add(UC_HOOK_CODE, hook, begin=NEW_FUNC, end=NEW_FUNC + 2)
    else:
        def hook(m, address, size, user):
            m.mem_write(DELAY_IN, struct.pack("<I", 1000))
            m.mem_write(DELAY_OUT, struct.pack("<I", 10000))
        for site in OLD_BODY_SITES:
            uc.hook_add(UC_HOOK_CODE, hook, begin=site, end=site + 2)


def snapshot(is_new, menu, item, view, key, max_insn=4_000_000):
    """与 P.snapshot 相同，但在 run 之前装入快进钩子。"""
    uc = P.machine(is_new)

    def skip(m, a, s, u):
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    for name in P.SKIP_NAMES:
        if is_new and name in P.SYMS:
            address = P.SYMS[name]
            uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)
    if not is_new:
        for address in (0xCEC, 0xD44, 0xDF2, 0xE80, 0xF3C, 0x1042, 0x116C, 0x1260,
                        0x1398, 0x13EC, 0x14A6, 0x942, 0x1486):
            uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)

    uc.mem_write(0x10001724, bytes((menu, item, view)))
    uc.mem_write(0x10001744, struct.pack("<I", 0))
    uc.mem_write(0x10001758, struct.pack("<I", 0))
    for address, value in P.BASE.items():
        uc.mem_write(address, struct.pack("<I", value))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))

    install(uc, is_new)
    state = P.SYMS["state_machine"] if is_new else 0x4464
    for step in range(2):
        uc.reg_write(UC_ARM_REG_R0, key)
        uc.reg_write(UC_ARM_REG_LR, P.STOP + 1)
        P.run(uc, state, max_insn=max_insn)
    out = bytearray(uc.mem_read(0x10000000, P.SRAM_END))
    for base, size in P.REGIONS:
        out += bytes(uc.mem_read(base, size))
    return bytes(out)


def mask(blob):
    out = bytearray(blob)
    out[VOLATILE[0]:VOLATILE[1]] = b"\0" * (VOLATILE[1] - VOLATILE[0])
    return bytes(out)


def main():
    only = []
    for arg in sys.argv[1:]:
        only.extend(a.strip().lower() for a in arg.split(",") if a.strip())
    total = bad = err = 0
    for menu in MENUS:
        if only and ("%x" % menu) not in only:
            continue
        for item in ITEMS:
            for view in VIEWS:
                try:
                    old = snapshot(False, menu, item, view, KEY)
                    new = snapshot(True, menu, item, view, KEY)
                except Exception as exc:
                    err += 1
                    if err <= 6:
                        print("[ERR] menu=0x%X item=%d view=%d %s"
                              % (menu, item, view, str(exc)[:80]))
                    continue
                total += 1
                if mask(old) != mask(new):
                    bad += 1
                    if bad <= 12:
                        first = next(i for i, (a, b) in enumerate(zip(mask(old), mask(new)))
                                     if a != b)
                        print("[DIFF] menu=0x%X item=%d view=%d at 0x%X old=0x%02X new=0x%02X"
                              % (menu, item, view, 0x10000000 + first,
                                 old[first], new[first]))
    print("12p key=1 延时快进：cases=%d bad=%d err=%d" % (total, bad, err))
    return 0 if bad == 0 and err == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
