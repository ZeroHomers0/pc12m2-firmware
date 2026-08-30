# -*- coding: utf-8 -*-
# =============================================================================
# test_case3_edit_equivalence.py — A/B 差分：case3 菜单编辑键（bug #5 控制方式互斥）
#
# 背景（2026-08-29 定论）：case3 编辑 switch 增量分支曾把枚举项 items 10-14
# （DISP_SEL/b56/ESTOP/FEEDBACK/INPUT_SEL）误写成"超上限环绕到 0"；BIN 实测
# （0x6DF0-0x6EFA 反汇编 + A/B 修正播种后全矩阵）是**钳位在上限**。item0 CTRL_MODE
# 例外：BIN 双向环绕（C 码原本就对，不得改回钳位）。
#
# 本测试断言：原始 LPC1765.bin(0x458C) 与编译固件对 case3 全 16 项 × 边界值 ×
# 4 键（2/3/0x16/0x21）编辑后的寄存器终值逐项一致（A/B 等价），防止模型把环绕/钳位
# 改混。
#
# 注意：
# · `_seed_display_items` 会把 ESTOP/FEEDBACK/INPUT_SEL 播成 1/0/0，测试目标项必须
#   在标准播种**之后**再覆盖，否则 BIN/ELF 都读被覆盖值 → 假一致（曾坑 08-29 排查）。
# · 只比较寄存器终值，不比 GPIO/LCD 显示 trace——GPIO 区挂 UC_HOOK_MEM_WRITE 会改变
#   ELF 执行（已知 Unicorn 翻译 bug，见 verify_firmware_equivalence.py）。
# =============================================================================
import os, sys, struct
HERE = os.path.dirname(os.path.abspath(__file__))          # decompiled/test/emulation
ROOT = os.path.dirname(os.path.dirname(HERE))              # decompiled
sys.path.insert(0, os.path.join(ROOT, 'tools', 'verification'))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

import verify_firmware_equivalence as V

# item -> (寄存器地址, 宽度字节)
ITEMS = {
    0: (0x10001634, 1),  1: (0x1000163c, 4),  2: (0x10001638, 4),  3: (0x10001640, 4),
    4: (0x10001648, 4),  5: (0x10001644, 4),  6: (0x1000164c, 1),  7: (0x1000164d, 1),
    8: (0x10001650, 4),  9: (0x10001654, 1),  10: (0x10001655, 1), 11: (0x10001656, 1),
    12: (0x10001657, 1), 13: (0x10001659, 1), 14: (0x1000165a, 1), 15: (0x10001660, 4),
}
# 每项取值集（覆盖上下限边界）
VALS = {
    0: (0, 1, 2, 3), 10: (0, 1, 2, 3), 11: (0, 1, 2, 3), 12: (0, 1, 2, 3),
    13: (0, 1, 2, 3), 14: (0, 1, 2, 3),
    6: (0, 0xc7, 0xc8, 0xc9), 7: (0, 0xc7, 0xc8, 0xc9), 9: (0x28, 0x9f, 0xa0, 0xa1),
    8: (0, 0xb3, 0xb4, 0xb5), 15: (0, 0xb3, 0xb4, 0xb5),
    1: (0x10, 0x176f, 0x1770, 0x1771), 2: (0x10, 0x176f, 0x1770, 0x1771),
    3: (0x10, 0x176f, 0x1770, 0x1771), 4: (0x10, 0x176f, 0x1770, 0x1771),
    5: (0x10, 0x176f, 0x1770, 0x1771),
}
KEYS = (2, 3, 0x16, 0x21)


def run(is_new, entry, item, val, key, cap=60_000_000):
    addr, width = ITEMS[item]
    uc = V.machine(is_new)
    V._seed_display_items(uc)                          # 标准播种
    uc.mem_write(0x1000163c, struct.pack("<I", 0x1770))  # V_RANGE 顶格（w48 上限 = V_RANGE+1）
    uc.mem_write(0x10001638, struct.pack("<I", 0x1770))  # A_RANGE 顶格（w44 上限 = A_RANGE+1）
    uc.mem_write(addr, struct.pack("<I" if width == 4 else "<B", val))  # 再覆盖目标项
    uc.mem_write(0x10001744, bytes((3, item, 1)))      # MENU=3, MENU2=item, MENU3=1
    uc.mem_write(0x10001778, struct.pack("<I", 0xfb))  # TIMEOUT3（编辑态整页重绘阈值）
    uc.reg_write(V.UC_ARM_REG_R0, key)
    V.run(uc, entry, max_insn=cap)
    return int.from_bytes(uc.mem_read(addr, width), "little")


def main():
    try:
        import unicorn  # noqa
    except Exception as ex:
        print(f"  [SKIP] 依赖不可用（{ex}）")
        return 0

    passed = failed = 0
    total = 0
    for item in range(16):
        for val in VALS[item]:
            for key in KEYS:
                total += 1
                fb = run(False, 0x458C, item, val, key)
                fe = run(True, V.SYMS['state_machine'], item, val, key)
                name = f"item{item:2d} val=0x{val:X} key=0x{key:X}"
                if fb == fe:
                    passed += 1
                else:
                    failed += 1
                    print(f"  [FAIL] {name}  BIN=0x{fb:X} ELF=0x{fe:X}")

    print()
    print(f"  case3 编辑 A/B：{passed}/{total} 通过")
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
