# -*- coding: utf-8 -*-
# 定位 SRAM 数据在 flash 的 .data 初始镜像 + 字符串池补扫（2026-08-21，W2 收尾 v2）
# 输出: evidence/reverse/reports/_sram_mirrors.txt (UTF-8)
#
# === 最终结论（2026-08-21，本脚本 v1/v2 均未直接命中，改用 IAR 解压）===
# 1. 波特率表 @0x100017BC 初始值 = [2400,4800,9600,14400,19200,38400,57600,115200]
#    （波特率数值，非分频系数——v1 按"系数"扫描必然失败）
#    定位方法：见 tools/generation/extract_ram_data_image.py（IAR 压缩 .data 流解压）
# 2. flash 高位区 0x36000-0x3FFF0 全 0xFF 空白（本脚本 §2 证实）
# 3. 字符串池：DATA_SEGMENT §2 已含 GBK 全表；本脚本 §3 GBK 判定误伤代码区，已废弃
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
b = (ROOT / "LPC1765.bin").read_bytes()
out = []
def P(s): out.append(s)

def dword(addr):
    return struct.unpack('<I', b[addr:addr+4])[0] if addr+4 <= len(b) else None

# ── [1] 波特率分频系数表 flash 镜像定位（允许任意档位顺序）────────
# 公式(08 模块 uart3_init 0xAC24 确证):
#   分频 = pclk(0x16E360=1500000) / (coeff × factor/1000)
#   => 波特率 = 1500000000 / (coeff × factor)
# factor 按档: idx0-2=955 3=950 4=945 5=938 6=925 7=915
FACTOR = [0x3BB, 0x3BB, 0x3BB, 0x3B6, 0x3B1, 0x3AA, 0x39D, 0x393]
STD_BAUDS = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]

def baud_of(coeff, factor):
    if coeff <= 0:
        return None
    return 1500000000.0 / (coeff * factor)

def score_cand(cand):
    """对 8 个 coeff 任意映射到标准波特率，返回(命中数, 覆盖不同标准档数, 映射列表)"""
    matched = []
    for idx in range(8):
        baud = baud_of(cand[idx], FACTOR[idx])
        if baud is None:
            matched.append(None)
            continue
        best = min(STD_BAUDS, key=lambda s: abs(baud - s))
        if abs(baud - best) / best < 0.05:
            matched.append(best)
        else:
            matched.append(None)
    hits = sum(1 for m in matched if m is not None)
    uniq = len({m for m in matched if m is not None})
    return hits, uniq, matched

P("=" * 66)
P("[1] 波特率系数表 flash 镜像搜索（8×u32，允许任意档序，命中=波特率落标准档±5%）")
found_b = []
for off in range(0x100, len(b) - 32, 4):
    cand = [dword(off + i * 4) for i in range(8)]
    if None in cand:
        break
    hits, uniq, matched = score_cand(cand)
    if hits >= 7 or (hits >= 6 and uniq >= 6):
        found_b.append((off, hits, uniq, cand, matched))
for off, hits, uniq, cand, matched in found_b[:8]:
    P("  候选 0x%08X 命中 %d/8 档(覆盖%d档):" % (off, hits, uniq))
    P("    coeff   = " + ", ".join(str(c) for c in cand))
    P("    →波特率 = " + ", ".join(str(m) if m else "-" for m in matched))
if not found_b:
    P("  未找到（u32 全盘扫描）。尝试 u16 半字扫描...")
    found16 = []
    for off in range(0x100, len(b) - 16, 2):
        cand = [struct.unpack('<H', b[off+i*2:off+i*2+2])[0] for i in range(8)]
        hits, uniq, matched = score_cand(cand)
        if hits >= 7 or (hits >= 6 and uniq >= 6):
            found16.append((off, hits, uniq, cand, matched))
    for off, hits, uniq, cand, matched in found16[:8]:
        P("  候选(u16) 0x%08X 命中 %d/8 档:" % (off, hits))
        P("    coeff = " + ", ".join(str(c) for c in cand))
        P("    →波特率 = " + ", ".join(str(m) if m else "-" for m in matched))
    if not found16:
        P("  u16 亦未找到。")

# ── [2] 已知区手动核验：flash 高位 .data 镜像概览 ────────────────
P("=" * 66)
P("[2] flash 高位区(0x36000-0x3FFF0) u32 抽样（找 .data 镜像可疑块）:")
for off in range(0x36000, 0x3FFF0, 0x1000):
    row = [dword(off + i * 4) for i in range(8)]
    P("  0x%08X: %s" % (off, ", ".join("0x%08X" % v for v in row)))

# ── [3] 字符串池补扫：聚焦菜单串区 0x2000-0x7000 + 0x12000 段 ────
P("=" * 66)
P("[3] 字符串补扫（区域 0x2000-0x7200 与 0x12000-0x13000，要求含 GBK 中文字符）")
def is_ascii(c):   return 32 <= c < 127
def is_gbk_hi(c):  return 0x81 <= c <= 0xFE   # GBK 双字节首字节
def is_gbk_lo(c):  return 0x40 <= c <= 0xFE and c != 0x7F

def scan_cjk_strings(start, end, min_len=4):
    """在区域内找连续串：含 GBK 中文字符（两个连续 hi+lo），容忍 ASCII 间隔"""
    results = []
    i = start
    while i < end - 2:
        # 中文字符 = GBK 双字节 hi+lo
        if is_gbk_hi(b[i]) and is_gbk_lo(b[i+1]):
            j = i
            buf = []
            # 允许 GBK 串内部夹少量 ASCII（数字/标点），但遇到非 GBK 非 ASCII 则断开
            while j < end - 1:
                if is_gbk_hi(b[j]) and is_gbk_lo(b[j+1]):
                    buf.append(b[j]); buf.append(b[j+1]); j += 2
                elif is_ascii(b[j]):
                    buf.append(b[j]); j += 1
                else:
                    break
            # 反向回退尾部残留的孤立 GBK hi（无 lo）
            while buf and is_gbk_hi(buf[-1]):
                buf.pop()
            if len(buf) >= min_len:
                try:
                    txt = bytes(buf).decode('gbk', errors='replace')
                    # 过滤纯 ASCII（无中文）与明显乱码（含大量替换符）
                    has_cjk = any('\u4e00' <= ch <= '\u9fff' for ch in txt)
                    if has_cjk and txt.count('\ufffd') < len(txt) * 0.4:
                        results.append((i, txt))
                except Exception:
                    pass
            i = j if j > i else i + 1
        else:
            i += 1
    return results

for start, end, label in [(0x2000, 0x7200, "菜单字符串区"), (0x12000, 0x13000, "LCD映射/杂项区")]:
    strs = scan_cjk_strings(start, end)
    P("  区域 %s (0x%X-0x%X): 发现 %d 条" % (label, start, end, len(strs)))
    for addr, txt in strs[:40]:
        P("    0x%08X: %s" % (addr, txt))

with open(ROOT / "evidence/reverse/reports/_sram_mirrors.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
print("done, lines:", len(out))
