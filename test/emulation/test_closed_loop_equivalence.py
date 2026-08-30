# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_closed_loop.py — A/B 差分：原始 vs 编译 的 closed_loop_integral
#
# 「原始固件」= decompiled/LPC1765.bin 闪存里的原机码函数（0x108B0）；
# 「编译固件」= firmware.elf 里由反编译 C 重新编译的 closed_loop_integral。
# 用 Unicorn 在同一 RAM 种子下分别真执行，比较【返回值(0x10002120 累加器) + 参数区末态】。
# 不一致 → 反编译重构对位置式 PID 的除数表/死区三段/钳位/地址映射与原机码语义背离。
#
# 工作区（已由原始 flash 指针槽证实，0x1000-0x10003F 区内的全局）：
#   coef_a=0x10002100 coef_b=0x10002104 const3=0x10002108 setpoint=0x100020F8
#   feedback=0x100020FC prev_err_roll=0x10002110/0x10002114/0x1000210C err_reg=0x10002118
#   deadband_gain=0x1000211C p_pid=0x10002124 divisor(cf4=f40同址)=0x10002128
#   累加器=0x10002120 钳位: 上=0x116520 下=0x5CC60(硬编码常量)
#   控制面: gain_sel=0x10001634 gain_a=0x1000163C gain_b=0x10001638
#           cl_thresh_hi=0x10001722 lo=0x10001723 big=0x10001724 mid=0x10001725 small=0x10001726
# 若 unicorn 不可用 → SKIP。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
from unicorn import *  # noqa

FUNC_ORIG = 0x108B0                 # 原始固件里的 closed_loop_integral
LO, HI = 0x10001000, 0x10003F00     # 参数/全局比较区

# 全局地址（原始布局唯一编号，编译版由 lpc1765.ld 钉址于同址）
GAIN_SEL   = 0x10001634
GAIN_A     = 0x1000163C
GAIN_B     = 0x10001638
THR_HI     = 0x10001722  # u8
THR_LO     = 0x10001723  # u8
GAIN_BIG   = 0x10001724  # u8
GAIN_MID   = 0x10001725  # u8
GAIN_SMALL = 0x10001726  # u8
ACC        = 0x10002120  # 累加器（唯一外部持久态，跨调用保留）

# (setpoint, feedback, coef_a, coef_b, gain_sel, gain_a, gain_b, acc_init, 说明)
CASES = [
    (1000, 800, 5, 3, 0, 150, 0,   0x1F4, 'ch1 err=200 正路径, 死区上界→big, 除数<0xdc→8'),
    (100,  100, 5, 3, 0, 500, 0,   0x000, 'ch1 err=0 →small, 除数 0xdb..0x226→15'),
    (100,  500, 7, 2, 0, 4000, 0,  0x064, 'ch1 err=400 负路径, 上界→big, 除数 3999..5000→0x96'),
    (500,  200, 3, 1, 1, 0, 150,   0x000, 'ch2 err=300, 除数 gain_b<0xdc→8'),
    (50,   20,  2, 4, 1, 0, 700,   0x0A0, 'ch2 err=30, 除数 0x226..1000→0x1e'),
    (900,  100, 9, 5, 2, 9999, 9999, 0x000, 'ch2 固定除数 0x46 (gain_sel==2)'),
    (100,  55,  1, 1, 0, 2500, 0,  0x3E8, 'ch1 err=45 mid(10<45<120), 除数 0x9c4..3000→100'),
    # 非饱和用例（公式直接被比较，不被钳位掩盖）：
    (3000, 0,   5, 5, 0, 150, 0,   0x000, 'ch1 err=3000 落两钳位之间, 除数<0xdc→8, 测公式'),
    (4500, 0,   5, 5, 0, 150, 0,   0x000, 'ch1 err=4500 超上限→钳 0x116520 (上钳), 测上限'),
]

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup, differential
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('closed_loop_integral')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for sp, fb, ca, cb, gsel, ga, gb, acc0, desc in CASES:
        def seed(e):
            e.mem_write(LO, b'\x00' * (HI - LO))             # 归一化参数区
            e.mem_write(GAIN_SEL, gsel.to_bytes(4, 'little'))
            e.mem_write(GAIN_A,     ga.to_bytes(4, 'little'))
            e.mem_write(GAIN_B,     gb.to_bytes(4, 'little'))
            e.mem_write(THR_HI, bytes([120]))                # 死区上界 120 / 下界 10
            e.mem_write(THR_LO, bytes([10]))
            e.mem_write(GAIN_BIG,   bytes([50]))
            e.mem_write(GAIN_MID,   bytes([30]))
            e.mem_write(GAIN_SMALL, bytes([10]))
            e.mem_write(ACC,        acc0.to_bytes(4, 'little'))
        args = [sp, fb, ca, cb]
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, args, seed)
        check(f"({sp},{fb},{ca},{cb},gsel={gsel}) A/B 等价", same,
              f"{desc} | ret_o=0x{ret_o:08X}h ret_n=0x{ret_n:08X}h")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
