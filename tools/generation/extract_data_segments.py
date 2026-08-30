# -*- coding: utf-8 -*-
# 从 LPC1765.bin 提取数据段常量（字符串池/查表/指针字），供数据段清单使用。
# 输出: evidence/reverse/reports/_data_extract.txt (UTF-8)
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
b = (ROOT / "LPC1765.bin").read_bytes()
out = []
def P(s): out.append(s)

def dword(addr):
    return struct.unpack('<I', b[addr:addr+4])[0] if addr+4 <= len(b) else None

def word(addr):
    return struct.unpack('<H', b[addr:addr+2])[0] if addr+2 <= len(b) else None

def ascii_at(addr, maxn=40):
    end = addr
    while end < len(b) and b[end] != 0 and end < addr + maxn:
        end += 1
    return b[addr:end]

def hexs(chunk):
    return ' '.join('%02X' % c for c in chunk)

def asc(chunk):
    return ''.join(chr(c) if 32 <= c < 127 else '.' for c in chunk)

# [1] UART3 波特率分频系数表
P("=" * 66)
P("[1] UART3 波特率分频系数表 @0x0000B028 (8 x u32)")
for i in range(8):
    v = dword(0xB028 + i * 4)
    P("    idx%d: 0x%08X = %d" % (i, v, v))

# [2] CRC16 双表
P("[2] CRC16 表A @0x00011034 前8项(u16), 表B @0x00011134 前8项(u16)")
for i in range(8):
    P("    crcA[%d]=0x%04X (%d)" % (i, word(0x11034 + i * 2), word(0x11034 + i * 2)))
for i in range(8):
    P("    crcB[%d]=0x%04X (%d)" % (i, word(0x11134 + i * 2), word(0x11134 + i * 2)))

# [3] 字符串池（菜单/通讯/版本/字符映射）
str_addrs = [
    0x6540, 0x6554, 0x6568, 0x657C,          # 基本参数屏 0-3
    0x6FE4, 0x6FF8, 0x700C, 0x7020,          # 基本参数屏 4-7
    0x7034, 0x7048, 0x705C, 0x7070,          # 基本参数屏 8-11
    0x7084, 0x7098, 0x70AC, 0x70C0,          # 基本参数屏 12-15
    0x6A18, 0x6A2C, 0x6A40, 0x6A54,          # 通讯参数 4 屏
    0x6B78, 0x6B98, 0x6B40, 0x6BD0,          # 版本/相位校准/恢复出厂
    0x12369,                                 # 字符映射串
]
P("[3] 字符串池（GBK 中文以 hex 呈现）")
for a in str_addrs:
    ck = ascii_at(a, 40)
    P("    0x%08X: [%s]  '%s'" % (a, hexs(ck), asc(ck)))

# [4] 关键字串搜索
P("[4] 关键字串位置")
for kw in [b"SINEP0WER", b"ST33C", b"V2.0", b"SINE", b"ST0P"]:
    pos, found = 0, []
    while True:
        i = b.find(kw, pos)
        if i < 0:
            break
        found.append("0x%08X" % i)
        pos = i + 1
        if len(found) >= 10:
            break
    P("    %s : %s" % (kw, ', '.join(found) if found else 'NOT FOUND'))

# [5] DAT_0000b00c..0b0a0 flash 指针字（UART3 常量区）
P("[5] UART3 常量区 flash 指针字 0x0000B00C..0x0000B0A0")
for off in range(0xB00C, 0xB0A0, 4):
    P("    0x%08X = 0x%08X" % (off, dword(off)))

with open(ROOT / "evidence/reverse/reports/_data_extract.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
print("done, lines:", len(out))
