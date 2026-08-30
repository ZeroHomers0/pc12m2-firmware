# -*- coding: utf-8 -*-
# =============================================================================
# test_relay_state_machine_equivalence.py — A/B 差分：state_machine 继电器行为护栏
#
# 背景（1-10 上电预案，2026-08-27 定论）：state_machine 有两条吸合路径：
#   · 375 行 if (*RUN != 0) fio0_pin22_ctrl(1)     —— RLY1 运行继电器（设计意图）
#   · 397 行 if (*SYNC_2C != 1) RLY3/RLY2/RLY1 全吸合 —— 锁机强制（SYNC_2C=0x1000172C
#     锁机标志，main 认证 01_startup.c:291 强制置 1=放行）
# 本测试断言：原始 LPC1765.bin(0x458C) 与编译固件对 FIO0SET/CLR 的写序列逐 bit 一致
# （A/B 等价），且三场景符合上电预期（放行不吸合 / 锁机全吸合 / 运行仅 RLY1 吸合）。
#
# 实现复用 tools/w8/w8_relay_ab.py 的 run_case/relay_final（同一断言，避免两份维护）。
# 注意：Unicorn 不模拟 FIO0SET→FIO0PIN 硬件联动，out_relay 用 RMW，故以写序列末态判定。
# =============================================================================
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))          # decompiled/test/emulation
ROOT = os.path.dirname(os.path.dirname(HERE))              # decompiled
sys.path.insert(0, os.path.join(ROOT, 'tools', 'w8'))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass


def main():
    try:
        import unicorn  # noqa
        import w8_relay_ab as R
    except Exception as ex:
        print(f"  [SKIP] 依赖不可用（{ex}）")
        return 0

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    cases = [
        ("上电放行 SYNC_2C=1 RUN=0", dict(sync_val=1, run=0),
         {'RLY3/P0.20': '断开', 'RLY2/P0.21': '断开', 'RLY1/P0.22': '断开'}),
        ("锁机     SYNC_2C=0 RUN=0", dict(sync_val=0, run=0),
         {'RLY3/P0.20': '吸合', 'RLY2/P0.21': '吸合', 'RLY1/P0.22': '吸合'}),
        ("运行态   SYNC_2C=1 RUN=1", dict(sync_val=1, run=1),
         {'RLY3/P0.20': '断开', 'RLY2/P0.21': '断开', 'RLY1/P0.22': '吸合'}),
    ]
    for label, kw, expect in cases:
        w_o, err_o = R.run_case(R.load_original, R.SM_ORIG, **kw)
        w_n, err_n = R.run_case(R.load_firmware, R.SM_NEW, **kw)
        f_o, f_n = R.relay_final(w_o), R.relay_final(w_n)
        ab_same = (f_o == f_n) and (err_o is None) == (err_n is None)
        exp_ok = (f_o == expect) and (f_n == expect)
        check(f"{label} A/B 等价", ab_same, f"原始={f_o} 编译={f_n}")
        check(f"{label} 符合上电预期", exp_ok,
              f"预期={expect} 原始={f_o} 编译={f_n}")
        if err_o or err_n:
            check(f"{label} 无模拟错误", False, f"原始err={err_o} 编译err={err_n}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
