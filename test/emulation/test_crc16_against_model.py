# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_crc16.py — 真正执行编译产物 crc16，对照 Python 模型
#
# 用 Unicorn 加载 firmware.elf，在仿真 SRAM 中写入构造数据，调用编译的
# crc16（地址 lookup('crc16')，随源码变化），对比"真实编译产物执行结果"
# 与"Python 模型（从 .c 复现，处理全部 len 字节）"。
# 这是对反编译 C 编译成机器码后的【执行级】等价验证，胜于纯模型测试。
#
# 若 unicorn 不可用 → 报 SKIP（不判失败）。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

def load_model_tables():
    b = open(os.path.join(ROOT, 'LPC1765.bin'), 'rb').read()
    return b[0x11034:0x11034+256], b[0x11134:0x11134+256]

def crc16_py(data, length, hi, lo):
    # 标准 Modbus CRC：处理全部 length 字节（A/B 实证原始 crc16 非 len-1）
    ch = 0xff; cl = 0xff
    for i in range(length):
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]
    return (cl | (ch << 8)) & 0xffff

def main():
    try:
        import unicorn  # noqa
        from unicorn_harness import load_firmware, lookup
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}），跳过执行级测试")
        return 0  # SKIP 不算失败
    FUNC_crc16 = lookup('crc16')

    hi, lo = load_model_tables()
    # 在仿真 SRAM0（0x10000000..）放测试数据缓冲区，指针=0x10000800
    BUF = 0x10000800
    e = load_firmware()

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    testcases = [
        (b"123456789", 9),   # 标准 CRC-16/MODBUS 检核值 0x4B37
        (bytes([0x01,0x03,0x00,0x00,0x00,0x01]), 6),   # Modbus 读请求帧体（6 字节）
        (bytes([0x01,0x03,0x00,0x00,0x00,0x01,0x00]), 7),  # 7 字节帧体（含填充）
    ]
    for data, length in testcases:
        # 写数据到仿真内存
        for i, b in enumerate(data):
            e.mem_write(BUF + i, bytes([b]))
        # 调用编译产物 crc16（r0=数据指针, r1=len）
        from unicorn_harness import call
        got = call(e, FUNC_crc16, args=[BUF, length])
        exp = crc16_py(list(data), length, hi, lo)
        check(f"执行 crc16(len={length}) == Python模型 (0x{exp:04X})",
              got == exp, f"got 0x{got:04X}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
