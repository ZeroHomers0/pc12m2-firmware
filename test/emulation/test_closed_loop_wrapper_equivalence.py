# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_closed_loop_wrapper.py — A/B 差分：原始 vs 编译 的 closed_loop_wrapper
#
# 原始 = LPC1765.bin @0x10F0A；编译 = firmware.elf lookup('closed_loop_wrapper')。
# 包装逻辑：*cnt(0x100020F4)++ → 非 0 则清零并调一次 closed_loop_integral 缓存到 0x1000212C，
#   否则直接返回缓存 *g_cl_cached_out。本测试覆盖「重算分支」与「回绕不重算分支」两种。
# 种子 = 值=地址（最严苛，能抓地址/位宽/值错位），再显式设 cnt。
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

FUNC_ORIG = 0x10F0A                # 原始固件里的 closed_loop_wrapper
CNT  = 0x100020F4                  # 重算计数（DAT_00010f84）
LO, HI = 0x10001000, 0x10003F00

# (setpoint, feedback, coef_a, coef_b, cnt_init, 说明)
CASES = [
    (1000, 800, 5, 3, 0,          'cnt=0 → ++=1 非零 → 重算并缓存'),
    (2000, 0,   5, 5, 5,          'cnt=5 → ++=6 非零 → 重算并缓存'),
    (1000, 800, 5, 3, 0xFFFFFFFF, 'cnt=0xFFFFFFFF → ++回绕0 → 不重算返回缓存'),
]

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup, differential, seed_addr_value
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('closed_loop_wrapper')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for sp, fb, ca, cb, cnt0, desc in CASES:
        def seed(e):
            seed_addr_value(e, LO, HI)             # 值=地址
            e.mem_write(CNT, cnt0.to_bytes(4, 'little'))   # 显式设计数
        args = [sp, fb, ca, cb]
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, args, seed)
        check(f"({sp},{fb},{ca},{cb},cnt=0x{cnt0:X}) A/B 等价", same,
              f"{desc} | ret_o=0x{ret_o:08X}h ret_n=0x{ret_n:08X}h")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
