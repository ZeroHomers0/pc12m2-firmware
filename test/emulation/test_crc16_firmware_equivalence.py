# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_crc16_ab.py — A/B 差分：原始 vs 编译 的 crc16
#
# 原始 = LPC1765.bin @0xAF64；编译 = firmware.elf lookup('crc16')。
# 输入 buffer（r0）与长度（r1）为参数；处理全部 len 字节（A/B 实证，标准 Modbus CRC）。
# 种子：把 buffer 填已知字节（两种 load 一致），遍历若干长度，比较返回值。
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

FUNC_ORIG = 0xAF64                 # 原始固件里的 crc16
BUF = 0x10002600
LO, HI = 0x10001000, 0x10003F00    # region：清零基态，避免两 loader .fw_image 镜像差异干扰

# 已知字节填充（CRC 算法输入，须两 loader 完全一致）
DATA = bytes([0x01, 0x03, 0x10, 0x02, 0x01, 0x00, 0x00, 0x11, 0xD8, 0xAB, 0xCD])
LENS = [3, 6, 7, 9, 11]            # crc16(ptr, len) 处理全部 len 字节（A/B 实证，非 len-1）

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup, differential
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    func_new = lookup('crc16')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    check("编译版符号已解析", func_new is not None, hex(func_new) if func_new else "None")

    for n in LENS:
        def seed(e):
            e.mem_write(LO, b'\x00' * (HI - LO))    # 基态清零（两 loader 一致）
            e.mem_write(BUF, DATA)                  # 输入缓冲
        ret_o, ret_n, same, _, _ = differential(FUNC_ORIG, func_new, [BUF, n], seed)
        check(f"crc16(len={n}) A/B 等价", same,
              f"ret_o=0x{ret_o:04X}h ret_n=0x{ret_n:04X}h")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
