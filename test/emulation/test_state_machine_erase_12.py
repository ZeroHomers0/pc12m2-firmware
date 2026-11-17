# -*- coding: utf-8 -*-
"""擦除帧的项号必须重读（PC12M-2，MENU==3 基本参数屏）。

背景（2026-11-17 状态机全矩阵发现并修复的真实差异）：

    firmware/src/07_state_machine.c 的 case3 入口缓存了 `uint32_t it = *MENU2;`，
    而 key==4（保存并退回）分支会把 `*MENU2` 清 0。公共尾部的擦除块原本写成
    `switch (it)`，于是沿用**本次调用入口时**的项号。

    原机在擦除块里是逐分支重新读 0x10001725（MENU2）的：
        0x771a  ldr r0,[pc,#388]  @ (0x78a0)   0x78a0 -> 0x10001725
        0x771c  ldrb r0,[r0,#0]
        0x771e  cbnz r0,0x772e
        0x772e  ldr r0,[pc,#368]  @ (0x78a0)   …共 15 处
    所以 key==4 那一帧擦的是**第 0 行**（MENU2 已被清 0），而不是离开时的项号。

    旧实现因此在 `key==4 且 TIMEOUT3 由 0x1f4 自增到 0x1f5` 的那一帧擦错行：
    原机 disp_string(空格串, 0, 0x0b, 0)，旧实现会在 item1..14 各自的行上擦。

本测试：
  1) 断言擦除帧的 disp_string 调用序列与原 BIN 完全一致（含 row/col）；
  2) 覆盖门禁：断言原机擦除块的 15 处 MENU2 重读地址都被执行过，
     否则说明用例根本没进到该分支（避免假通过）。
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
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,  # noqa: E402
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
import verify_firmware_equivalence_12 as V  # noqa: E402

MENU = 0x10001724
MENU2 = 0x10001725
MENU3 = 0x10001726
TIMEOUT3 = 0x10001758

# EEPROM 写周期延时（原机 FUN_00001c32 / 新 ELF i2c_delay @0x1CD0）：按 r0 次
# 外层 × 10000 次内层空转。key==4 会触发 param_sync_live_to_eeprom → 大量写，
# 不跳过必然耗尽指令预算（实测停在 0x1c40 的外层循环）。
EEPROM_DELAY_OLD = 0x1C32
EEPROM_DELAY_NEW = 0x1CD0

# 原机擦除块（0x7708..0x7908）内 15 处 MENU2 重读的 ldr 指令地址。
# 提取方式：在该区间内匹配 `ldr r0,[pc,#N] @ (0x78a0)`，而 0x78a0 处 4 字节
# 即为 0x10001725（MENU2）。早期手写的 0x78bc/0x78d8 等落在字面量池里，是
# objdump 把池字节误解成指令，不能作为门禁地址。
OLD_MENU2_RELOADS = (
    0x771A, 0x772E, 0x7744, 0x775A, 0x7770, 0x77A2, 0x77D4, 0x77EA,
    0x7800, 0x781A, 0x7830, 0x7846, 0x785C, 0x7872, 0x7888,
)

KEYS = (0, 1, 2, 3, 4, 0x16, 0x21)
ITEMS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
# TIMEOUT3 预置 0x1f4 -> 尾部 ++ 得 0x1f5 -> 触发擦除帧
T3_PRESETS = (0x1F4,)


def _disp_map(is_new):
    if not is_new:
        return {0xCEC: "disp_string", 0xE80: "disp_uint4", 0xDF2: "disp_number3"}
    return {V.SYMS[n]: n for n in ("disp_string", "disp_uint4", "disp_number3")
            if n in V.SYMS}


def _run(is_new, item, key, t3, want_sites):
    uc = V.machine(is_new)
    V._seed_state_machine(uc, 3, item, 1)          # MENU=3、编辑态
    uc.mem_write(TIMEOUT3, struct.pack("<I", t3))

    calls = []
    for addr, name in _disp_map(is_new).items():
        def hook(m, a, s, u, nm=name):
            calls.append((nm,
                          m.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
                          m.reg_read(UC_ARM_REG_R1) & 0xFF,
                          m.reg_read(UC_ARM_REG_R2) & 0xFF,
                          m.reg_read(UC_ARM_REG_R3) & 0xFF))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr + 1)

    sites = []

    def site_hook(m, a, s, u):
        sites.append(a)

    if not is_new:
        for addr in want_sites:
            uc.hook_add(UC_HOOK_CODE, site_hook, begin=addr, end=addr + 1)

    # 跳过纯空转的 EEPROM 写周期延时（无副作用，只等时间）
    uc.hook_add(UC_HOOK_CODE,
                lambda m, a, s, u: m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR)),
                begin=EEPROM_DELAY_NEW if is_new else EEPROM_DELAY_OLD,
                end=(EEPROM_DELAY_NEW if is_new else EEPROM_DELAY_OLD) + 1)

    entry = V.SYMS["state_machine"] if is_new else V.PAIRS["state_machine"][0]
    uc.reg_write(UC_ARM_REG_R0, key)
    V.run(uc, entry, max_insn=8_000_000)
    return calls, sites


def main():
    failures = 0
    covered = set()
    erase_cases = 0
    for item in ITEMS:
        for key in KEYS:
            for t3 in T3_PRESETS:
                old_calls, old_sites = _run(False, item, key, t3, OLD_MENU2_RELOADS)
                new_calls, _ = _run(True, item, key, t3, OLD_MENU2_RELOADS)
                covered |= set(old_sites)
                if old_sites:
                    erase_cases += 1
                if old_calls != new_calls:
                    failures += 1
                    print("  [FAIL] item=%d key=%#x t3=%#x 显示调用序列不同"
                          % (item, key, t3))
                    for i, (o, n) in enumerate(zip(old_calls, new_calls)):
                        if o != n:
                            print("     #%d old=%r new=%r" % (i, o, n))
                            break
                    if len(old_calls) != len(new_calls):
                        print("     长度 old=%d new=%d" % (len(old_calls), len(new_calls)))

    missing = set(OLD_MENU2_RELOADS) - covered
    if missing:
        failures += 1
        print("  [FAIL] 擦除块 MENU2 重读未被覆盖: %s"
              % sorted("0x%X" % a for a in missing))

    if failures:
        print("  [FAIL] 擦除帧项号重读契约 %d 项失败" % failures)
        return 1
    print("  [PASS] 擦除帧项号重读契约：%d 例一致，擦除块 %d/%d 处 MENU2 重读已覆盖"
          % (len(ITEMS) * len(KEYS) * len(T3_PRESETS),
             len(covered), len(OLD_MENU2_RELOADS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
