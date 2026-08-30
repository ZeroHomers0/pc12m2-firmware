# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_modbus_write_multi.py — A/B 差分：原始 vs 编译 的 modbus_write_multi
#
# 「原始固件」= decompiled/LPC1765.bin 闪存 0x0 镜像里的原机码函数（0xB2E0）；
# 「编译固件」= firmware.elf 里由反编译 C 重新编译的 modbus_write_multi。
# 用 Unicorn 在同一 RAM 种子下分别真执行两者，比较【返回值 + 参数区内存末态】。
# 若一致 → 反编译重构对写寄存分支的地址/位宽忠实于原机码；不一致 → W7 抓 bug。
#
# 写寄存器分支（reg 0x00-0x3F）：对每个 reg 把 *src_val（r0）写入对应全局，字节/半字/字。
# 覆盖写-only 保留区（0x1A-0x1F 落 g_scratch）与关键参数/通讯全局。
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

FUNC_ORIG = 0xB2E0                 # 原始固件里的 modbus_write_multi
SRC  = 0x10002600                  # src_val 缓冲区（r0 指向此处；值被写入目标全局）
LO, HI = 0x10001000, 0x10003F00    # 参数/全局比较区（避开栈区）

# reg(0基) → 说明；值取 0x0000ABCD 以区分 byte(取低字节 0xCD) 与 word(取全字)
CASES = [
    (0x00, 0x0000ABCD, 'reg00 → g_gain_sel (byte)'),
    (0x01, 0x0000ABCD, 'reg01 → g_gain_a   (word)'),
    (0x02, 0x0000ABCD, 'reg02 → g_gain_b   (word)'),
    (0x03, 0x0000ABCD, 'reg03 → DAT_0000b4c4 (word)'),
    (0x1A, 0x00001122, 'reg1A → g_scratch  (word, 保留区)'),
    (0x1F, 0x00001122, 'reg1F → g_scratch  (word, 保留区)'),
    (0x26, 0x00000011, 'reg26 → g_run_flag (byte)'),
    (0x2E, 0x00000055, 'reg2E → g_slave_addr (byte)'),
    (0x2F, 0x0000ABCD, 'reg2F → g_baud_idx (word)'),
    (0x3C, 0x00001234, 'reg3C → g_reg61_remote_en (word)'),
    (0x3D, 0x00005678, 'reg3D → g_reg62_start_phase (word)'),
]

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup, differential
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('modbus_write_multi')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    # 验证原地址/新地址都已解析
    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for reg, val, desc in CASES:
        def seed(e):
            e.mem_write(LO, b'\x00' * (HI - LO))          # 归一化参数区（消除 .fw_image 差异）
            e.mem_write(SRC, val.to_bytes(4, 'little'))    # *src_val = 待写值
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [SRC, reg], seed)
        check(f"reg 0x{reg:02X} A/B 等价", same,
              f"{desc} | ret_o=0x{ret_o & 0xffff} ret_n=0x{ret_n & 0xffff}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
