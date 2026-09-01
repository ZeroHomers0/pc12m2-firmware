# -*- coding: utf-8 -*-
# generate_string_pool_12.py — PC12M-2（12 相）strpool 生成（6p 实参清单 → 12p 内容）
# 对标 6p generate_string_pool.py。12p 关键差异：
#   * P3 移植沿用 6p 源码，disp_string 第一实参仍是 **6p flash 字符串地址**；
#     但 12p 重新编译后字符串内容/位置变了（验证：156/158 内容相同，
#     "标准流程"→"标准模式" @0x071c、"型号:ST33C"→"型号:ST36C" @0x6acc 为真实差异）。
#   * 因此：从 6p 源码扫描实参清单 → 逐串在 12p BIN 匹配内容 → 得 12p 地址 → 12p blob。
#   * strpool_map 的 key = **12p 地址**；P3 移植时用 _strpool_6to12_map.json 把实参替换成 12p 地址。
# 输出：firmware/src/strpool.c + _strpool_6to12_map.json + evidence/reverse/reports/_strpool_report_12.txt
import re, struct, sys, glob, json
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
ROOT = Path(__file__).resolve().parents[2]
BIN12 = (ROOT / "backup" / "pc12m2_orig.bin").read_bytes()
FLASH_LEN = len(BIN12)
REFERENCE_ROOT = ROOT.parent / "PC6M-10"
BIN6 = (REFERENCE_ROOT / "LPC1765.bin").read_bytes()

# ── 产品信息定制覆写（2026-09-01 用户要求）────────────────────────────
# case9 产品版本信息屏 4 行文本：地址保持 **12p flash 地址**，strpool_map 前置
# 查表返回定制串（GBK 字节）。原厂内容：型号:ST36C / 版本:V2.0.2016 /
# 厂商:SINEP0WER / 电话:18938061832。改这里并重新生成即可。
PRODUCT_INFO_OVERRIDES = {
    0x6acc: "型号:PC12M-2",
    0x6ad8: "版本:V2.0",
    0x6ae8: "厂商:XIANPOWER",
    0x6af8: "电话:029-84205750",
}

SCAN_FILES = [str(p) for p in (REFERENCE_ROOT / "firmware" / "src").glob("*.c")] + \
             [str(REFERENCE_ROOT / "firmware" / "stub.c")]
SCAN_FILES = [f for f in SCAN_FILES if not f.endswith("strpool.c") and not f.endswith("08_modbus_dispatch.c")]

# 12p 逆向新增渲染函数（sm4/sm5/sm6 等）里的 disp_string 实参是 **12p 地址**（6p 源没有）。
# 这些地址须直接并入 addrs12，不再走 6p→12p 内容匹配。
SCAN_FILES_12 = [str(p) for p in (ROOT / "firmware" / "src").glob("*.c")]
SCAN_FILES_12 = [f for f in SCAN_FILES_12 if not f.endswith("strpool.c")]

# ── 1. 6p 实参清单 ─────────────────────────────────────────────
def addrs_from_src():
    found = set()
    for fp in SCAN_FILES:
        src = open(fp, "rb").read().decode("utf-8", errors="replace")
        for line in src.splitlines():
            for m in re.finditer(r'\bdisp_string\s*\(', line):
                first = line[m.end():].split(',', 1)[0]
                if '/*' in first or '//' in first:
                    continue
                first = re.sub(r'^\s*\([A-Za-z_][A-Za-z0-9_ *]*\)\s*', '', first)
                n = re.match(r'0x([0-9a-fA-F]+)\s*(?:([+-])\s*0x([0-9a-fA-F]+))?', first)
                if not n:
                    continue
                base = int(n.group(1), 16)
                if n.group(2) and n.group(3):
                    off = int(n.group(3), 16)
                    base = base + off if n.group(2) == '+' else base - off
                found.add(base)
    return {a for a in found if 0x400 <= a < len(BIN6)}

def addrs12_from_12p_src():
    """扫描 12p 源码里 disp_string 第一实参的 **12p 地址**（含 (int) 强转与 +/- 偏移），
    逆向还原的 sm4/sm5/sm6 等渲染函数用到的单位串/状态串 6p 源没有，直接并入簇表。"""
    found = set()
    for fp in SCAN_FILES_12:
        src = open(fp, "rb").read().decode("utf-8", errors="replace")
        for line in src.splitlines():
            for m in re.finditer(r'\bdisp_string\s*\(', line):
                first = line[m.end():].split(',', 1)[0]
                if '/*' in first or '//' in first:
                    continue
                first = re.sub(r'^\s*\([A-Za-z_][A-Za-z0-9_ *]*\)\s*', '', first)
                n = re.match(r'0x([0-9a-fA-F]+)\s*(?:([+-])\s*0x([0-9a-fA-F]+))?', first)
                if not n:
                    continue
                base = int(n.group(1), 16)
                if n.group(2) and n.group(3):
                    off = int(n.group(3), 16)
                    base = base + off if n.group(2) == '+' else base - off
                found.add(base)
    return {a for a in found if 0x400 <= a < FLASH_LEN}


def read_str(data, a, cap=40):
    s = bytearray()
    for i in range(cap):
        if a + i >= len(data):
            break
        b = data[a + i]
        if b == 0:
            break
        s.append(b)
    return bytes(s)

addrs6 = sorted(addrs_from_src())

# ── 2. 6p → 12p 地址映射（内容匹配）──────────────────────────
map_6to12 = {}   # 6p_addr -> 12p_addr
missing = []
for a6 in addrs6:
    c = read_str(BIN6, a6)
    if not c:
        continue
    pos = BIN12.find(c)
    if pos >= 0:
        map_6to12[a6] = pos
        continue
    # 内容有差异：用前 4 字节定位 12p 位置（已知差异串：标准流程/型号:ST33C）
    pfx = c[:4]
    if pfx and len(pfx) == 4:
        pos = BIN12.find(pfx)
        if pos >= 0:
            map_6to12[a6] = pos
            continue
    missing.append((a6, c[:8]))

print("6p 实参: %d -> 12p 匹配: %d  差异串(前4字节定位): %d  缺失: %d" %
      (len(addrs6), sum(1 for a in addrs6 if a in map_6to12 and read_str(BIN12, map_6to12[a]).startswith(read_str(BIN6, a))),
       sum(1 for a in addrs6 if a in map_6to12 and not read_str(BIN12, map_6to12[a]).startswith(read_str(BIN6, a))),
       len(missing)))
for a, c in missing:
    print("  MISSING 6p 0x%04x %s" % (a, c.hex()))

addrs12 = set(map_6to12.values()) | addrs12_from_12p_src()
extra12 = addrs12_from_12p_src() - set(map_6to12.values())

# ── 3. 聚类 + blob（12p 地址，12p 内容）──────────────────────
def str_end_12(a, cap=48):
    n = 0
    while a + n < FLASH_LEN and n < cap:
        if BIN12[a + n] == 0:
            return a + n + 1
        n += 1
    return a + n

clusters = []
cur = [addrs12, None]
for a in sorted(addrs12):
    if not clusters or a - clusters[-1][1] > 0x40:
        clusters.append([a, a])
    else:
        clusters[-1][1] = a

blob = bytearray()
records = []          # (12p_base, len, blob_off)
for lo, hi in clusters:
    start = lo
    end = max(str_end_12(a) for a in sorted(addrs12) if lo <= a <= hi)
    if start + 1 > end or start + 4 > FLASH_LEN:
        continue
    off = len(blob)
    blob += BIN12[start:end]
    records.append((start, end - start, off))

# ── 4. 写 strpool.c ───────────────────────────────────────────
csrc = []
csrc.append("/* 自动生成：tools/generation/generate_string_pool_12.py（PC12M-2 数据层）。勿手改。")
csrc.append(" * GBK 字符串表 blob + 簇表 + strpool_map（key = **12p flash 字符串地址**）。")
csrc.append(" * 12p 内容含真实差异：'标准模式'@0x071c、'型号:ST36C'@0x6acc（≠6p 标准流程/ST33C）。")
csrc.append(" * P3 移植已用 _strpool_6to12_map.json 把 disp_string 实参替换为 12p 地址；")
csrc.append(" * 逆向新增渲染函数的单位/状态串（0x7488/0x7490/0x8638/0xa070/0xa080/0xa0b0 等）")
csrc.append(" * 由 12p 源码扫描并入簇表（2026-08-31 修复 A/B 显示全执行差异）。")
csrc.append(" * 产品信息定制（2026-09-01 用户要求）：case9 版本屏 4 行文本覆写为定制内容")
csrc.append(" * （型号/版本/厂商/电话），地址不变，strpool_map 前置查表；见 PRODUCT_INFO_OVERRIDES。 */")
csrc.append("#include <stdint.h>")
csrc.append("")
csrc.append("typedef struct { uint32_t base; uint32_t len; const uint8_t *blob; } strpool_cluster_t;")
csrc.append("")
csrc.append("static const uint8_t strpool_blob[%d + 1] =" % len(blob))
lines = []
for i in range(0, len(blob), 32):
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

# ── 产品信息定制覆写段（strpool_map 前置查表）──────────────────
if PRODUCT_INFO_OVERRIDES:
    ov_off = {}
    ovb = bytearray()
    for a in sorted(PRODUCT_INFO_OVERRIDES):
        ov_off[a] = len(ovb)
        ovb += PRODUCT_INFO_OVERRIDES[a].encode("gbk") + b"\x00"
    csrc.append("/* 产品信息定制覆写（2026-09-01 用户要求）：case9 版本屏 4 行文本（GBK 字节）。")
    csrc.append(" * 地址保持 12p flash 地址，strpool_map 前置查表返回定制串；")
    csrc.append(" * 原厂内容仍在原簇 blob 中保留（未使用）。 */")
    csrc.append("static const uint8_t strpool_override_blob[] =")
    ovlines = []
    for a in sorted(PRODUCT_INFO_OVERRIDES):
        chunk = PRODUCT_INFO_OVERRIDES[a].encode("gbk") + b"\x00"
        ovlines.append('  "' + "".join("\\x%02x" % b for b in chunk) +
                       '" /* %s */' % PRODUCT_INFO_OVERRIDES[a])
    csrc.append("\n".join(ovlines) + ";")
    csrc.append("")
    csrc.append("typedef struct { uint32_t addr; uint32_t off; } strpool_override_t;")
    csrc.append("static const strpool_override_t strpool_override[] = {")
    csrc.append("  " + ", ".join("{ 0x%04x, %d }" % (a, ov_off[a])
                                 for a in sorted(PRODUCT_INFO_OVERRIDES)) + "};")
    csrc.append("")
    csrc.append("")

csrc.append("uint32_t strpool_map(uint32_t addr)")
csrc.append("{")
csrc.append("  uint32_t i;")
if PRODUCT_INFO_OVERRIDES:
    csrc.append("  for (i = 0; i < sizeof(strpool_override) / sizeof(strpool_override[0]); i++) {")
    csrc.append("    if (addr == strpool_override[i].addr)")
    csrc.append("      return (uint32_t)(strpool_override_blob + strpool_override[i].off);")
    csrc.append("  }")
csrc.append("  for (i = 0; i < sizeof(strpool_clusters) / sizeof(strpool_clusters[0]); i++) {")
csrc.append("    if (addr >= strpool_clusters[i].base && addr < strpool_clusters[i].base + strpool_clusters[i].len)")
csrc.append("      return (uint32_t)(strpool_clusters[i].blob + (addr - strpool_clusters[i].base));")
csrc.append("  }")
csrc.append("  return addr;")
csrc.append("}")
csrc.append("")

(ROOT / "firmware/src/strpool.c").write_text("\n".join(csrc), encoding="utf-8")

# ── 5. 映射 JSON（P3 替换实参用）──────────────────────────────
json.dump({("0x%04x" % k): ("0x%04x" % v) for k, v in sorted(map_6to12.items())},
          open(ROOT / "tools/_ghidra_proj/_strpool_6to12_map.json", "w", encoding="utf-8"),
          indent=1, sort_keys=True)

# ── 6. 报告 ───────────────────────────────────────────────────
OUT = ["strpool_12 生成报告",
       "6p 实参: %d -> 12p 地址: %d  缺失: %d" % (len(addrs6), len(addrs12), len(missing)),
       "12p 源码补充地址: %d（逆向渲染函数单位/状态串）" % len(extra12),
       "聚类: %d 簇  blob: %d 字节" % (len(records), len(blob)), ""]
for a in sorted(extra12):
    c = read_str(BIN12, a)
    OUT.append("  补充 12p 0x%04X  %r" % (a, c.decode('gbk', errors='replace')))
OUT.append("")
for base, ln, off in records:
    OUT.append("  0x%04X  len=%3d  blob+0x%04X  首字节=0x%02X" % (base, ln, off, BIN12[base]))
OUT.append("")
OUT.append("差异串确认（12p 内容）:")
for a6, a12 in sorted(map_6to12.items()):
    c6 = read_str(BIN6, a6)
    c12 = read_str(BIN12, a12)
    if not c12.startswith(c6):
        OUT.append("  6p 0x%04X '%s' -> 12p 0x%04X '%s'" %
                   (a6, c6.decode('gbk', errors='replace'), a12, c12.decode('gbk', errors='replace')))
(ROOT / "evidence/reverse/reports/_strpool_report_12.txt").write_text("\n".join(OUT), encoding="utf-8")
print("done: %d clusters, blob %d bytes -> strpool.c + _strpool_6to12_map.json + report" % (len(records), len(blob)))
