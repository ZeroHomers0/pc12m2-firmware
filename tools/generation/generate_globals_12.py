# -*- coding: utf-8 -*-
# generate_globals_12.py — PC12M-2 数据层 globals 生成（BIN 驱动）
# 对标 6p generate_globals.py：符号初值 = pc12m2_orig.bin 池槽内容（= 12p SRAM/外设/表地址）
# 符号集 = 12p 全部代码 literal 池槽引用全集
# 类型 = 优先继承 6p 对应符号类型（_pool_6to12_map.json），否则启发式
# 165 命名（_pc12m2_verified_vars.json）→ 主池槽语义名 + 全池槽尾注释
# 输出：firmware/inc/globals.h + firmware/globals.c + evidence/reverse/reports/_globals_report_12.txt
import struct, json, re, sys
from pathlib import Path
from collections import defaultdict

sys.stdout.reconfigure(encoding='utf-8')

ROOT = Path(__file__).resolve().parents[2]
BINP = ROOT / "pc12m2_orig.bin"
REFERENCE_ROOT = ROOT.parent / "pc6m10-firmware"
BIN6P = REFERENCE_ROOT / "LPC1765.bin"
# 完整 6p→12p 池槽映射（Ghidra 完整反汇编版，含第二代码段池槽）
POOLMAP = ROOT / "tools/_ghidra_proj/_pool_6to12_map_v2.json"
VARS = ROOT / "tools/_ghidra_proj/_pc12m2_verified_vars.json"
G6 = REFERENCE_ROOT / "firmware" / "globals.c"

D = BINP.read_bytes()
D6 = BIN6P.read_bytes()
FLASH_LEN = len(D)

def u32(d, off):
    return struct.unpack_from("<I", d, off)[0]

# ── 1. 6p globals 类型表 {6p_pool_addr: type} ──────────────
# 6p 符号名形态：DAT_xxxx / PTR_DAT_xxxx / _DAT_xxxx；PTR_DAT_ 多为字节基址
t6 = {}
_SYM = re.compile(r'(?:_?PTR_DAT_|_?DAT_)([0-9A-Fa-f]{4,8})')
for ln in G6.read_text(encoding="utf-8").splitlines():
    ln = ln.strip()
    if ln.startswith("uint32_t"):
        m = _SYM.search(ln)
        if m:
            t6[int(m.group(1), 16)] = "value"
    elif ln.startswith("volatile uint8_t *"):
        m = _SYM.search(ln)
        if m:
            t6[int(m.group(1), 16)] = "ptr_byte"
    elif ln.startswith("volatile uint32_t *"):
        m = _SYM.search(ln)
        if m:
            t6[int(m.group(1), 16)] = "ptr_word"

# ── 2. 6p→12p 池槽映射（12p_pool -> [6p_pool...]）──────────
poolmap = json.load(open(POOLMAP, encoding="utf-8"))
pool12_to_6 = {}   # 12p pool -> list of 6p pools
for p6, lst in poolmap.items():
    a6 = int(p6, 16)
    for p12 in lst:
        pool12_to_6.setdefault(int(p12, 16), []).append(a6)

# ── 3. 165 命名 {sram_addr: name} ────────────────────────────
varsd = json.load(open(VARS, encoding="utf-8"))
name_of = {}
def merge(dd):
    for a, n in dd.items():
        name_of[int(a, 16)] = n
merge(varsd.get("TIER3_12ONLY_NAMING", {}).get("HIGH", {}))
merge(varsd.get("TIER3_12ONLY_NAMING", {}).get("PLAUS", {}))
merge(varsd.get("TIER3_12ONLY_NAMING", {}).get("NEW", {}))
for k in ("HIGH", "NEW_DISCOVERED", "SAME_ADDR_PLAUSIBLE"):
    if k in varsd:
        merge(varsd[k])

# ── 4. 收集 12p 全部代码 literal 池槽 → 全集 ──
# 从 Ghidra 完整反汇编（evidence/reverse/disassembly/functions/*.txt，DumpAllDisasm12.java 生成）
# 解析 `; ref 0x池槽 -> 0x值` 标注。Ghidra 已正确处理字面池数据区（第二代码段池槽不丢），
# 替代原 Capstone 连续反汇编（在池槽数据区中断，漏 0xf70 字形表等池槽）。
DISASM12 = ROOT / "evidence/reverse/disassembly/functions"
_REF = re.compile(r'; ref 0x([0-9a-f]{4,8})\s+->')
_INS = re.compile(r'^([0-9a-f]{8})\s+(\S+)')
slots = defaultdict(list)   # pool -> [ (insn_addr, mnemonic) ]
for fp in sorted(DISASM12.glob("*.txt")):
    cur_a = None
    for ln in fp.read_text(encoding="utf-8", errors="replace").splitlines():
        rm = _REF.search(ln)
        if rm:
            pool = int(rm.group(1), 16)
            if 0x1c0 <= pool < FLASH_LEN and pool % 4 == 0:
                slots[pool].append((cur_a, None))
            continue
        im = _INS.match(ln)
        if im:
            cur_a = int(im.group(1), 16)

SRAM_LO, SRAM_HI = 0x10000000, 0x10008000
PERI_LO, PERI_HI = 0x40000000, 0x50000000

def classify(v):
    if SRAM_LO <= v < SRAM_HI:
        return "sram"
    if (0x20000000 <= v < 0x20100000) or (0x40000000 <= v < 0x40100000) or (0xE0000000 <= v < 0xE0100000):
        return "peri"
    if 0x100 < v < FLASH_LEN:
        return "flash"
    return "const"

# 类型统一按「目标地址」决定：同一 SRAM/外设变量被多个池槽引用，类型必须一致。
# 任一池槽有 6p 继承 → 全变量继承；否则启发式。常量/表池槽按池槽级继承或 value。
def _inherit(pool):
    for p6 in pool12_to_6.get(pool, []):
        if p6 in t6:
            return t6[p6]
    return None

PERI_T = (0x20000000, 0x20100000, 0x40000000, 0x40100000, 0xE0000000, 0xE0100000)
def is_addr_target(v):
    return (SRAM_LO <= v < SRAM_HI or PERI_T[0] <= v < PERI_T[1] or
            PERI_T[2] <= v < PERI_T[3] or PERI_T[4] <= v < PERI_T[5])

addr_type = {}   # SRAM/外设目标地址 -> 类型（统一）
for pool in slots:
    v = u32(D, pool)
    if not is_addr_target(v):
        continue
    t = _inherit(pool)
    if t is not None:
        addr_type.setdefault(v, t)   # 首个继承者决定；同地址已存在则保持
for pool in slots:
    v = u32(D, pool)
    if is_addr_target(v) and v not in addr_type:
        addr_type[v] = "ptr_word"

def decide_type(pool, v):
    """类型：SRAM/外设目标统一；常量/表继承或 value"""
    if is_addr_target(v):
        return addr_type.get(v, "ptr_word")
    t = _inherit(pool)
    return t if t else "value"

# ── 5. 语义名主池槽分配 ────────────────────────────────────
# 每个 SRAM 变量 → 引用它的池槽们；选第一个（地址最小）命名，其余 DAT_+注释
sram_slots = {}     # sram_addr -> [pool, ...]
for pool in slots:
    v = u32(D, pool)
    if SRAM_LO <= v < SRAM_HI:
        sram_slots.setdefault(v, []).append(pool)
named_pool = {}     # pool -> semantic name
for addr, plist in sram_slots.items():
    if addr in name_of:
        plist.sort()
        named_pool[plist[0]] = name_of[addr]

# ── 6. 写 globals.h/.c ─────────────────────────────────────
H = ["/* 自动生成：tools/generation/generate_globals_12.py（PC12M-2 数据层）。勿手改。",
     " * 符号初值 = backup/pc12m2_orig.bin flash 字面量池内容（= 12p SRAM/外设/表地址）。",
     " * 类型优先继承 6p 对应符号（_pool_6to12_map.json），否则启发式；访问宽度由 src 修正。 */",
     "#ifndef GLOBALS_H", "#define GLOBALS_H", "#include <stdint.h>", ""]
C = ["/* 自动生成：tools/generation/generate_globals_12.py（PC12M-2 数据层）。勿手改。 */",
     '#include "inc/globals.h"', ""]

lines_h, lines_c = [], []
slot_names = {}      # pool -> 符号名（P3 6p→12p DAT_ 替换用）
for pool in sorted(slots):
    v = u32(D, pool)
    cls = classify(v)
    ty = decide_type(pool, v)
    sname = named_pool.get(pool)
    nm = sname if sname else "DAT_%08x" % pool
    slot_names[pool] = nm
    # 语义名可能已被同名池槽占用（同 SRAM 变量主池槽唯一，已保证）
    cmt = "/* %s value */" % cls
    if sname:
        cmt = "/* %s value; semantic %s @0x%08x */" % (cls, sname, v)
    if ty == "ptr_byte":
        lines_h.append("extern volatile uint8_t *%s;" % nm)
        lines_c.append("volatile uint8_t *%s = (uint8_t *)0x%08X;  %s" % (nm, v, cmt))
    elif ty == "ptr_word":
        lines_h.append("extern volatile uint32_t *%s;" % nm)
        lines_c.append("volatile uint32_t *%s = (uint32_t *)0x%08X;  %s" % (nm, v, cmt))
    else:
        lines_h.append("extern uint32_t %s;" % nm)
        lines_c.append("uint32_t %s = 0x%08X;  %s" % (nm, v, cmt))

H += lines_h + ["", "#endif /* GLOBALS_H */"]
C += lines_c + [""]
(ROOT / "firmware/inc/globals.h").write_text("\n".join(H), encoding="utf-8")
(ROOT / "firmware/globals.c").write_text("\n".join(C), encoding="utf-8")
# 池槽 -> 符号名 映射（P3 用：6p DAT_ 池槽映射后取 12p 符号名）
json.dump({("0x%08x" % p): slot_names[p] for p in sorted(slot_names)},
          open(ROOT / "tools/_ghidra_proj/_globals_slot_names.json", "w", encoding="utf-8"),
          indent=1, sort_keys=True)

# ── 7. 报告 ────────────────────────────────────────────────
from collections import Counter
c_cls = Counter(classify(u32(D, p)) for p in slots)
c_ty = Counter(decide_type(p, u32(D, p)) for p in slots)
report = [
    "globals_12 生成报告",
    "池槽总数: %d（12p 代码 literal 引用全集）" % len(slots),
    "分类: " + ", ".join("%s=%d" % (k, v) for k, v in c_cls.most_common()),
    "类型: " + ", ".join("%s=%d" % (k, v) for k, v in c_ty.most_common()),
    "语义命名池槽: %d / %d 变量" % (len(named_pool), len(sram_slots)),
    "",
    "6p 类型继承: %d 池槽" % sum(1 for p in slots if pool12_to_6.get(p)),
    "",
    "语义命名清单（主池槽）:",
]
for pool in sorted(named_pool):
    v = u32(D, pool)
    report.append("  %-22s DAT_%08x -> 0x%08X (%s)" % (named_pool[pool], pool, v,
                  ", ".join("DAT_%08x" % q for q in sram_slots.get(v, []))))
(ROOT / "evidence/reverse/reports/_globals_report_12.txt").write_text(
    "\n".join(report), encoding="utf-8")
print("done: %d slots -> globals.h/.c (%d lines), 165-named %d" %
      (len(slots), len(lines_c), len(named_pool)))
