# -*- coding: utf-8 -*-
# gen_strpool.py — 生成 firmware/src/strpool.c（GBK 字符串表 blob + 簇表 + strpool_map）
# 2026-08-21 目标B W7a 行为等价验证
#
# 问题：反编译源码把 disp_string 第一实参直传**原固件 flash 字符串地址**（如 0x47dc）。
#   GCC 重链接后这些地址处是指令字节而非 GBK 字符串 → 菜单/状态/提示全部乱码。
# 修复：W7a 在此生成 strpool.c——把全部被 disp_string 引用的 flash 字符串区间原样拷进
#   .rodata blob；运行时 strpool_map(addr) 把 flash 地址映射为 blob 内偏移。未命中（RAM 等）
#   原样返回。disp_string 入口调用 strpool_map 即可，调用点零改动。
#
# 复用 gen_globals.py 的 BIN 读取（ROOT/BIN/FLASH_LEN），dword()/classify() 同款。
# 策略：地址清单 = src/*.c + stub.c 中 disp_string 第一实参（裸/base±off/cast，大小写混合）
#   ，过滤 <0x400（误译残渣 0x25/0x41/0x56）+ 显式加 W7b 修正后的真实地址
#   {0x7974('V'),0x7980('A'),0x86e0('%')}；每地址读串到 NUL（或 40 字节上限）定 end；
#   聚类（间隙>0x40 分簇）；每簇抽 [min_addr, max_end]，整体拷入 blob。
# 输出：firmware/src/strpool.c + docs/_strpool_report.txt（地址/簇/映射校验）
import re, struct, sys, glob, os
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')

ROOT = str(Path(__file__).resolve().parents[2])
BIN = open(ROOT + r"\LPC1765.bin", "rb").read()
FLASH_LEN = len(BIN)          # 0x40000 = 262144

SCAN_FILES = glob.glob(ROOT + r"\firmware\src\*.c") + [ROOT + r"\firmware\stub.c"]
# 生成产物 strpool.c 除外（避免重复扫描自身输出，尽管它不含 disp_string 调用）
SCAN_FILES = [f for f in SCAN_FILES if not f.endswith("strpool.c") and not f.endswith("08_modbus_dispatch.c")]
# 08_modbus_dispatch.c（W1a 还原）纯 Modbus 数值路径，Agent1 已确认无 disp_string —— 排除减噪。

# ── 1. 地址清单 ───────────────────────────────────────────────
def addrs_from_src():
    """扫描每行 disp_string 第一实参。格式（实证）：
        0x754 / (int)0x4370 / 0x47dc + 0xc / 0x6514 - 0x3c / (int)0x4d9c + 0x10
      大小写混合（0x47dc vs 0x47F0）。只取行内第一个逗号前的第一实参，规避尾注释。"""
    found = set()
    for fp in SCAN_FILES:
        src = open(fp, "rb").read().decode("utf-8", errors="replace")
        for line in src.splitlines():
            # 同一行可有多个调用（07_state_machine.c 大量使用此形式）。
            for m in re.finditer(r'\bdisp_string\s*\(', line):
                first = line[m.end():].split(',', 1)[0]   # 该调用的第一实参
                if '/*' in first or '//' in first:
                    continue
                # 剥掉可选 cast，再提取 base 与可选 ± off。
                first = re.sub(r'^\s*\([A-Za-z_][A-Za-z0-9_ *]*\)\s*', '', first)
                n = re.match(r'0x([0-9a-fA-F]+)\s*(?:([+-])\s*0x([0-9a-fA-F]+))?', first)
                if not n:
                    continue
                base = int(n.group(1), 16)
                if n.group(2) and n.group(3):
                    off = int(n.group(3), 16)
                    base = base + off if n.group(2) == '+' else base - off
                found.add(base)
    return found

# W7b 修正后的真实单位字符地址（修正前 src 为误译 ASCII 值 0x56/0x41/0x25，扫描不到）
EXTRA = {0x7974, 0x7980, 0x86e0}   # 'V' 过压/欠压 · 'A' IF/CT 过载 · '%' 三相平衡

def addrs_final():
    raw = addrs_from_src() | EXTRA
    # 过滤：仅保留 flash 字符串区（>=0x400，排除 0x25/0x41/0x56 误译残渣；<FLASH_LEN）
    out = {a for a in raw if 0x400 <= a < FLASH_LEN}
    return raw, out

raw, addrs = addrs_final()

# ── 2. 每地址串尾（读到 NUL 或 40 字节上限） ─────────────────
# 注意：end 必须**含结尾 NUL**（返回 NUL 的下一字节），否则单字符簇（如 0x86e0 '%%'）
#   在 blob 里缺尾 NUL → disp_string 读 '%%' 后会继续读到下一簇首字节，显示乱串。
#   len = end - start 即含 NUL 的字符串总长。
def str_end(a):
    n = 0
    while a + n < FLASH_LEN and n < 40:
        if BIN[a + n] == 0:
            return a + n + 1   # 含结尾 NUL
        n += 1
    return a + n

# ── 3. 聚类（升序，间隙>0x40 分簇） ──────────────────────────
addrs_sorted = sorted(addrs)
clusters = []
cur = [addrs_sorted[0], addrs_sorted[0]]
for a in addrs_sorted[1:]:
    if a - cur[1] > 0x40:
        clusters.append(cur)
        cur = [a, a]
    else:
        cur[1] = a
clusters.append(cur)

# ── 4. 每簇区间 [min_addr, max_end]，生成 blob ──────────────
def dword(addr):
    if addr + 4 > FLASH_LEN:
        return None
    return struct.unpack("<I", BIN[addr:addr + 4])[0]

blob = bytearray()
records = []          # (base, len, offset_in_blob)
for lo, hi in clusters:
    start = lo
    end = max(str_end(a) for a in addrs_sorted if lo <= a <= hi)
    if dword(start) is None:
        continue
    if start + 1 > end:
        continue
    off = len(blob)
    blob += BIN[start:end]
    records.append((start, end - start, off))

# ── 5. 写 strpool.c ──────────────────────────────────────────
def byt_str(data):
    return ''.join('%02x' % b for b in data)   # 交给 C 用 \x 前缀

csrc = []
csrc.append("/* 自动生成：tools/generation/generate_string_pool.py（目标B W7a）。勿手改。")
csrc.append(" * GBK 字符串表 blob + 簇表 + strpool_map。")
csrc.append(" * 反编译把 disp_string 第一实参直传原固件 flash 字符串地址；")
csrc.append(" * GCC 重链接后该地址是指令字节。strpool_map 把 flash 地址映射到本 blob 内偏移。")
csrc.append(" * 未命中（RAM/外设地址）原样返回。 */")
csrc.append("#include <stdint.h>")
csrc.append("")
csrc.append("typedef struct { uint32_t base; uint32_t len; const uint8_t *blob; } strpool_cluster_t;")
csrc.append("")
# blob 用 \xNN 转义字符串字面量（GBK 双字节 0xA1-0xFF 均合法；每个 \x 恰 2 位，无贪婪歧义）
csrc.append("static const uint8_t strpool_blob[%d + 1] =" % len(blob))
N = len(blob)
# 每行约 64 个 \xNN
lines = []
for i in range(0, N, 32):
    chunk = blob[i:i + 32]
    lines.append('  "' + ''.join('\\x%02x' % b for b in chunk) + '"')
if not lines:
    lines.append('  ""')
csrc.append("\n".join(lines) + ";")
csrc.append("")
csrc.append("static const strpool_cluster_t strpool_clusters[] = {")
for base, ln, off in records:
    csrc.append("  {%d, %d, strpool_blob + %d}," % (base, ln, off))
csrc.append("};")
csrc.append("")
csrc.append("uint32_t strpool_map(uint32_t addr)")
csrc.append("{")
csrc.append("  uint32_t i;")
csrc.append("  for (i = 0; i < sizeof(strpool_clusters) / sizeof(strpool_clusters[0]); i++) {")
csrc.append("    if (addr >= strpool_clusters[i].base && addr < strpool_clusters[i].base + strpool_clusters[i].len)")
csrc.append("      return (uint32_t)(strpool_clusters[i].blob + (addr - strpool_clusters[i].base));")
csrc.append("  }")
csrc.append("  return addr;")
csrc.append("}")
csrc.append("")

open(ROOT + r"\firmware\src\strpool.c", "w", encoding="utf-8").write("\n".join(csrc))

# ── 6. 报告 + 校验 ───────────────────────────────────────────
OUT = []
OUT.append("地址清单：raw=%d （含 EXTRA），有效=%d（>=0x400 & <FLASH_LEN）" % (len(raw), len(addrs)))
OUT.append("聚类：%d 簇" % len(clusters))
OUT.append("blob：%d 字节" % len(blob))
OUT.append("")
OUT.append("簇明细（base / len / blob偏移）：")
for base, ln, off in records:
    OUT.append("  0x%04X  len=%3d  blob+0x%04X  首字节=0x%02X"
               % (base, ln, off, BIN[base]))
OUT.append("")
OUT.append("地址→映射校验（strpool_map 语义 = 返回 blob 内偏移）：")
dmap = {}
for base, ln, off in records:
    for a in addrs_sorted:
        if base <= a < base + ln:
            dmap.setdefault(a, off + (a - base))
for a in addrs_sorted:
    hit = dmap.get(a)
    if hit is not None:
        # 能读到的字符串（读 NUL 或 16 字节；blob 边界保护）
        s = []
        for i in range(16):
            if hit + i >= len(blob):
                break
            b = blob[hit + i]
            if b == 0:
                break
            s.append(b)
        tail = ''.join('%02X ' % b for b in s[:8])
        OUT.append("  0x%04X → blob+0x%04X  [%s]" % (a, hit, tail.strip()))
    else:
        OUT.append("  0x%04X → 未命中簇!?" % a)
open(ROOT + r"\evidence\reverse\reports\_strpool_report.txt", "w", encoding="utf-8").write("\n".join(OUT))

print("done: %d addrs, %d clusters, blob %d bytes, strpool.c + _strpool_report.txt written"
      % (len(addrs), len(records), len(blob)))
