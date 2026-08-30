# -*- coding: utf-8 -*-
# =============================================================================
# test_adc_wait_done_equivalence.py — A/B 差分：原始 vs 编译 的 adc0_wait_done
#
# 原始 = LPC1765.bin @0x1FA6；编译 = firmware.elf lookup('adc0_wait_done')。
# 等待循环读 AD0GDR(0x40034004) bit31 DONE，置位后返回 (值>>4)&0xFFF。
#
# 2026-08-26 W8：编译版曾把 `g_adc + 4`（g_adc 是 uint32_t*）编译成 +16 字节偏移，
# 读 AD0DR0(0x40034010) 而非 AD0GDR —— 转换非 ch0 通道时 DONE 永不置位 → 主循环挂死
# → 看门狗复位循环（屏幕闪屏 + 按键死）。修复后读回 AD0GDR。
#
# 测试要点：同时给 AD0GDR 与 AD0DR0 播不同已知结果；若编译版错读 AD0DR0，
# 返回值必与原始不一致 → 立即 FAIL（回归护栏）。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
import struct
from unicorn import *  # noqa

FUNC_ORIG = 0x1FA6                 # 原始固件 adc0_wait_done
ADC_BASE = 0x40034000               # ADC0 外设块
AD0CR    = ADC_BASE + 0x00          # 控制寄存器（原始 bug 版本会把它当指针解引用）
AD0GDR   = ADC_BASE + 0x04          # 全局结果寄存器（DONE=bit31, RESULT=bits15:4）
AD0DR0   = ADC_BASE + 0x10          # ch0 专属结果寄存器（错读目标）
TRAP_PTR = 0x10002000               # AD0CR 播的"指针"→ [TRAP_PTR+16] 播 0x777
TRAP_VAL = 0x80000000 | (0x777 << 4)

# 每种寄存器状态：AD0GDR 与 AD0DR0 播不同结果，若编译版错读 AD0DR0 立即露馅；
# 原始 bug 版会按 AD0CR 值解引用 +16，读到 TRAP_VAL=0x777，同样露馅。
# DONE 必须置位（bit31=1），否则两边都死循环等待（原固件同行为，非本测试范围）。
CASES = [
    ("DONE+0xABC/0xDEF",   0x80000000 | (0xABC << 4), 0x80000000 | (0xDEF << 4)),
    ("DONE+0x000/0xFFF",   0x80000000 | (0x000 << 4), 0x80000000 | (0xFFF << 4)),
    ("DONE+0xFFF/0x001",   0x80000000 | (0xFFF << 4), 0x80000000 | (0x001 << 4)),
    ("DONE+0x555/0x2AA",   0x80000000 | (0x555 << 4), 0x80000000 | (0x2AA << 4)),
]

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, load_original, lookup, differential
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('adc0_wait_done')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for label, gdr, dr0 in CASES:
        def seed(e, _gdr=gdr, _dr0=dr0):
            # 差异区清零（两 loader 基态一致，避免 .fw_image/.data 镜像差异干扰）
            e.mem_write(0x10001000, b'\x00' * (0x10003F00 - 0x10001000))
            # 映射 ADC0 外设块（一页），播不同已知结果
            e.mem_map(ADC_BASE, 0x1000, UC_PROT_ALL)
            e.mem_write(AD0CR, struct.pack('<I', TRAP_PTR))     # bug 版会当指针解引用
            e.mem_write(TRAP_PTR + 16, struct.pack('<I', TRAP_VAL))
            e.mem_write(AD0GDR, struct.pack('<I', _gdr))
            e.mem_write(AD0DR0, struct.pack('<I', _dr0))
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [], seed)
        exp = (gdr >> 4) & 0xFFF
        check(f"{label} A/B 等价", same and ret_o == exp and ret_n == exp,
              f"期望=0x{exp:03X} 原始=0x{ret_o:03X} 编译=0x{ret_n:03X}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
