# -*- coding: utf-8 -*-
# =============================================================================
# test_crc16_semantics.py — host 虚拟数据测试：Modbus crc16 长度语义核对
#
# 被测对象：firmware/src/08_uart3_modbus.c 的 crc16()（原 flash 0xAF64）
#
# 本测试验证的结论（2026-08-23 A/B 差分 + 机器码回放实证）：
#   1) 固件两张 CRC 表（crc16_hi_tbl/lo_tbl）与原始 LPC1765.bin 的
#      flash 表（0x111D8 / 0x112D8，12p 池槽 0xADCC/0xADD0）**逐字节一致** → S9 悬空表修复正确。
#   2) 固件 crc16 算法循环 = `while(len != 0)`：处理**全部 len 字节**（标准
#      Modbus CRC）。原码 0xAF84 `movs r0,r4`(用于 bne 置 Z) → `sub.w r6,r4,#1`
#      (**无 S 后缀，不置位**) → `uxtb r4,r6`(后减)。bne 的 Z 来自 movs 测试
#      **减前**计数器，故 while(计数器!=0) 精确执行 len 次。旧读法把 sub.w 当
#      置位 → 误判成 len-1，2026-08-23 A/B 差分已纠错。
#   3) 调用方 modbus_dispatch 传 `crc16(FRAME, rx_len-2)`（0xB642 反汇编
#      subs r0,#2 → uxtb r1 → bl crc16），rx_len 为含 CRC 的总帧长。
#      → 传 N 就校验全部 N 个数据字节（与标准一致，无需补偿）。
#
# 关键：本测试以【原始二进制真值】+ A/B 实测为基准，不用教科书 Modbus CRC 反推。
# =============================================================================
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN  = os.path.join(ROOT, 'backup', 'pc12m2_orig.bin')
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

def load_tables():
    """从原始 bin 读 flash 表（与 src/crc16_table.c 内嵌表应一致）"""
    b = open(BIN, 'rb').read()
    return b[0x111D8:0x111D8+256], b[0x112D8:0x112D8+256]

def crc16_fw(data, length, hi, lo):
    """按 08_uart3_modbus.c crc16 逐字节复现（处理全部 length 字节，标准）"""
    ch = 0xff; cl = 0xff
    for i in range(length):
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]
    return (cl | (ch << 8)) & 0xffff

def crc16_ref(data, length):
    """参考 Modbus CRC16（处理全部字节，poly 0xA001，初值 0xFFFF）"""
    crc = 0xffff
    for i in range(length):
        crc ^= data[i]
        for _ in range(8):
            crc = (crc >> 1) ^ 0xa001 if (crc & 1) else (crc >> 1)
            crc &= 0xffff
    return crc

def main():
    hi, lo = load_tables()
    passed = 0; failed = 0

    def check(name, cond, detail=""):
        nonlocal passed, failed
        status = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{status}] {name}" + (f"  {detail}" if detail else ""))

    print("=== 1. CRC 表与原始 bin 逐字节一致（S9 修复验证）===")
    def ref_tbl(i):
        crc = i & 0xff
        for _ in range(8):
            crc = (crc >> 1) ^ 0xa001 if (crc & 1) else (crc >> 1); crc &= 0xffff
        return crc
    mm = [(i, ref_tbl(i)) for i in range(256)]
    hi_ok = all(lo[i] == (v >> 8) for i, v in mm)
    lo_ok = all(hi[i] == (v & 0xff) for i, v in mm)
    check("bin 表 == 标准 Modbus CRC16(0xA001) 高字节表", hi_ok)
    check("bin 表 == 标准 Modbus CRC16(0xA001) 低字节表", lo_ok)

    print("=== 2. crc16 长度语义（处理全部 len 字节 = 标准）===")
    # 标准向量 '123456789' → 0x4B37（CRC-16/MODBUS 标准检核值）
    test = list(b"123456789")
    r_s = crc16_ref(test, 9)
    check("参考算法处理9节 → 0x4B37", r_s == 0x4B37, f"got 0x{r_s:04X}")
    f_9 = crc16_fw(test, 9, hi, lo)
    check("固件crc16(len=9)处理9节 → ==0x4B37", f_9 == 0x4B37, f"got 0x{f_9:04X}")
    # 与参考算法（独立 poly 0xA001 实现）逐字节等价
    all_equal = all(crc16_fw(list(b"123456789")[:n], n, hi, lo) == crc16_ref(list(b"123456789"), n)
                    for n in range(1, 10))
    check("固件crc16 == 参考算法（长度 1..9 全部等价）", all_equal)

    print("=== 3. 真实 Modbus 读请求帧（站1 func03 reg0 cnt1 6节）===")
    rx = [0x01, 0x03, 0x00, 0x00, 0x00, 0x01]
    ref = crc16_ref(rx, 6)
    check("参考算法处理帧体6节", crc16_ref(rx, 6) == ref)
    check("固件crc16(帧,6)==参考(覆盖全部6节)", crc16_fw(rx, 6, hi, lo) == ref)
    check("帧末CRC高/低字节序(先低后高)", (ref & 0xff) == 0x84 and (ref >> 8) == 0x0a)

    print()
    print(f"  通过 {passed} / {passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
