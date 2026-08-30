# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_modbus_read_reg.py — A/B 差分：原始 vs 编译 的 modbus_read_reg
#
# 原始 = LPC1765.bin @0xAF94；编译 = firmware.elf lookup('modbus_read_reg')。
# 该函数恒返回 0（序言 movs r0,#0），真值写进 *out_val(r0 所指)。
# 测试：把参数区每个 4 字节字种子为「=其地址」，关掉 cfg_pid_sel 组基址读（避免越区读），
# 遍历 reg 0x00..0x3F，比较 返回值 + *out_val + 参数区末态 是否在原/编译间一致。
# 值=地址 的种子最严苛：原码与编译码读到不同地址/位宽/值都会立刻失配。
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

FUNC_ORIG = 0xAF94                 # 原始固件里的 modbus_read_reg
OUT_VAL = 0x10002600               # r0 → out_val 缓冲区
LO, HI = 0x10001000, 0x10003F00
G_CFG_PID_SEL = 0x10001710         # 置 0 → 不读组基址（0x1000B06C 越区）
REGS = list(range(0x00, 0x40))     # reg 0x00..0x3F 全覆盖

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup, differential, seed_addr_value
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('modbus_read_reg')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for reg in REGS:
        def seed(e):
            seed_addr_value(e, LO, HI)                    # 值=地址
            e.mem_write(G_CFG_PID_SEL, b'\x00\x00\x00\x00')  # 关组基址读
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [OUT_VAL, reg], seed)
        check(f"reg 0x{reg:02X} A/B 等价", same,
              f"ret_o=0x{ret_o:08X} ret_n=0x{ret_n:08X}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
