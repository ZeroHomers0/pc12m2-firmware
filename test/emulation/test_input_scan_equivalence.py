# -*- coding: utf-8 -*-
# =============================================================================
# test_input_scan_equivalence.py — A/B 差分：原始 vs 编译 的 input_scan_state
#
# 原始 = LPC1765.bin @0x15FE；编译 = firmware.elf lookup('input_scan_state')。
# 编码器/RUN/STOP 输入扫描：读 FIO1PIN(0x2009C034)/FIO0PIN(0x2009C014)/FIO3PIN(0x2009C074)
# 各相引脚状态，按 A/B 相组合锁存方向（0x10001570）并产生按键事件码 0x0B/0x16/0x21/0x17/0x0E。
#
# 2026-08-27 W8：编译版曾把 `DAT_00001974 + 0xNN`（DAT_00001974 是 uint32_t*）按元素
# 偏移编译成 +4 倍字节 → 读 0x2009C0D0/0x2009C050/0x2009C1D0 而非 0x2009C034/0x014/0x074。
# 后果：引脚全部读到 0（未就绪）→ 扫描计数永不触发 → key 恒 0 → 背光不亮 + 按键无效。
# 修复：`(uint32_t)DAT_00001974 + 0xNN`（同 ADC 修复模式）。本测试是回归护栏：
# 全 6 位引脚组合 × 计数初值差分，任一读址错位必露馅。
#
# 测试要点：FIO 块按正确地址播不同引脚状态；若编译版错读高地址（0x1D0/0xD0/0x50），
# 分支决策与原始不一致 → 返回/计数/锁存立即 FAIL。
# =============================================================================
import os, sys, struct
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'test', 'support'))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
from unicorn import *  # noqa

FUNC_ORIG  = 0x15FE          # 原始固件 input_scan_state
FIO_BASE   = 0x2009C000
FIO1PIN    = FIO_BASE + 0x34 # P1.19(0x80000)/P1.18(0x40000)
FIO0PIN    = FIO_BASE + 0x14 # P0.30(0x40000000)/P0.29(0x20000000)
FIO3PIN    = FIO_BASE + 0x74 # P3.25(0x2000000)/P3.26(0x4000000)

CNT  = 0x10001588            # DAT_00001978 扫描计数（word）
LATCH = 0x10001570           # DAT_0000197c 方向锁存（byte）
SLOW  = 0x10001571           # DAT_00001980/DAT_00001c10 慢/组合计数（byte）

P1 = 0x80000 | 0x40000       # FIO1PIN 相关位
P0 = 0x40000000 | 0x20000000 # FIO0PIN 相关位
P3 = 0x2000000 | 0x4000000   # FIO3PIN 相关位

def make_seed(p1, p0, p3, counter, latch, slow):
    def seed(e):
        # 差异区清零（两 loader 基态一致，避免 .fw_image 镜像差异干扰）
        e.mem_write(0x10001000, b'\x00' * (0x10003F00 - 0x10001000))
        e.mem_write(CNT, struct.pack('<I', counter))
        e.mem_write(LATCH, bytes([latch]))
        e.mem_write(SLOW, bytes([slow]))
        e.mem_map(FIO_BASE, 0x1000, UC_PROT_ALL)
        e.mem_write(FIO_BASE, b'\x00' * 0x1000)
        e.mem_write(FIO1PIN, struct.pack('<I', p1))
        e.mem_write(FIO0PIN, struct.pack('<I', p0))
        e.mem_write(FIO3PIN, struct.pack('<I', p3))
    return seed

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import lookup, differential
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('input_scan_state')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    # ── 1) 全 6 位引脚组合 × 计数初值 差分扫描 ──
    # 把 m 的低 6 位映射到 6 个相关引脚位：bit0→P1.19 bit1→P1.18 bit2→P0.30
    # bit3→P0.29 bit4→P3.25 bit5→P3.26
    def combo(m):
        p1 = (0x80000 if m & 1 else 0) | (0x40000 if m & 2 else 0)
        p0 = (0x40000000 if m & 4 else 0) | (0x20000000 if m & 8 else 0)
        p3 = (0x2000000 if m & 16 else 0) | (0x4000000 if m & 32 else 0)
        return p1, p0, p3
    combos = [combo(m) for m in range(64)]
    counters = [0, 1, 0x18, 0x19, 0x1a, 0xf5, 0xf9, 0xfa]
    sweep = 0
    for p1, p0, p3 in combos:
        for c in counters:
            seed = make_seed(p1, p0, p3, c, 0, 0)
            ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [], seed)
            if not same or ret_o != ret_n:
                check(f"组合 p1=0x{p1:08X} p0=0x{p0:08X} p3=0x{p3:08X} cnt=0x{c:X}",
                      False, f"原始=0x{ret_o:X} 编译=0x{ret_n:X} same={same}")
                return 1
            sweep += 1
    print(f"  [PASS] 引脚组合×计数 差分 {sweep} 例（未发现读址/分支差异）")

    # ── 2) 关键按键事件定向用例（计数 > 0xF9 进入事件段） ──
    EV_CASES = [
        # (名称, p1, p0, p3, 期望事件)
        ("0x16 快加 A=1,B=0,P0.30/29,P3.25/26 就绪", 0x80000, 0x60000000, 0x6000000, 0x16),
        ("0x21 快减 A=1,B=1,P0.30=0,P0.29=1",        0xC0000, 0x20000000, 0x6000000, 0x21),
        ("0x17 慢减 A=1,B=0,P0.30=0,P0.29=1(计数0x1D)", 0x80000, 0x20000000, 0x6000000, 0x17),
        ("0x0E 组合 A=0,B=1,P0.30=1,P0.29=0(计数0x1D)", 0x40000, 0x40000000, 0x6000000, 0x0E),
        ("0x0B 慢加 A=0,B=1,P0.30/29=1(慢计数9)",      0x40000, 0x60000000, 0x6000000, 0x0B),
    ]
    for name, p1, p0, p3, ev in EV_CASES:
        seed = make_seed(p1, p0, p3, 0xFA, 0, 0x1D if ev in (0x17, 0x0E) else 0x09 if ev == 0x0B else 0)
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [], seed)
        ok = same and ret_o == ev and ret_n == ev
        check(f"{name} → 0x{ev:02X}", ok,
              f"原始=0x{ret_o:02X} 编译=0x{ret_n:02X}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
