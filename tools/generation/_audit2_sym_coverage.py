# -*- coding: utf-8 -*-
# _audit2_sym_coverage.py — 修正版：finditer 逐行解析 6p globals.c 全部符号（一行可多符号）
import re, json, glob, sys
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')

poolmap = json.load(open(r'D:\code\PC12M-2\tools\_ghidra_proj\_pool_6to12_map.json', encoding='utf-8'))
slotnames = json.load(open(r'D:\code\PC12M-2\tools\_ghidra_proj\_globals_slot_names.json', encoding='utf-8'))
G6 = open(r'D:\code\LPC1765FBD100\decompiled\firmware\globals.c', encoding='utf-8').read()

# 1. 6p globals.c 全符号: 符号名 -> (池槽 or None, 初值)
# 行内多语句，用 finditer 匹配每个 "NAME = value" 片段
syms6 = {}
_line = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*'
                   r'(?:\((?:uint8_t|uint32_t)\s*\*\))?(0x[0-9A-Fa-f]+)')
for ln in G6.splitlines():
    for m in _line.finditer(ln):
        nm = m.group(1)
        val = int(m.group(2), 16)
        # 池槽后缀提取：DAT_/PTR_DAT_/PTR_xxx_xxxxxx
        pm = re.match(r'(?:PTR_)?DAT_([0-9A-Fa-f]{4,8})$', nm)
        pm2 = re.match(r'PTR_[A-Za-z_0-9]+_([0-9A-Fa-f]{4,8})$', nm)
        pool = None
        if pm: pool = int(pm.group(1), 16)
        elif pm2: pool = int(pm2.group(1), 16)
        syms6[nm] = (pool, val)
print('6p globals 符号总数(全匹配):', len(syms6))

# 2. 6p src 符号引用（含大小写严格匹配——C 区分大小写）
SRC = glob.glob(r'D:\code\LPC1765FBD100\decompiled\firmware\src\*.c')
toks = set()
tokcnt = 0
loc = {}          # sym -> [(file, lineno)]
for fp in SRC:
    for i, ln in enumerate(open(fp, encoding='utf-8', errors='replace'), 1):
        for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\b', ln):
            t = m.group(1)
            if t in syms6:
                toks.add(t); tokcnt += 1
                loc.setdefault(t, []).append((Path(fp).name, i))
print('6p src 引用 globals 符号: 唯一 %d, 总引用 %d' % (len(toks), tokcnt))

# 3. 覆盖率
no_pool, not_in_pool, no_12name, covered = [], [], [], 0
covered_where = {}
for t in sorted(toks):
    pool, val = syms6[t]
    if pool is None:
        no_pool.append((t, val)); continue
    p6 = '0x%08x' % pool
    if p6 not in poolmap:
        not_in_pool.append((t, pool, val)); continue
    hit = None
    for p12 in poolmap[p6]:
        if p12 in slotnames:
            hit = slotnames[p12]; break
    if hit:
        covered += 1
        covered_where[t] = (p6, poolmap[p6], hit)
    else:
        no_12name.append((t, p6))

print('有池槽且已映射: %d/%d' % (covered, len(toks)))
print('纯语义名(无池槽)(%d):' % len(no_pool))
for t, v in no_pool:
    print('   %-28s v=0x%08X  @%s' % (t, v, loc[t][0]))
print('池槽不在poolmap(%d):' % len(not_in_pool))
for t, p, v in not_in_pool:
    fl = sorted(set(f for f, _ in loc[t]))
    print('   %-28s 池槽=0x%04x v=0x%08X  @%s' % (t, p, v, fl))
print('池槽在poolmap但无12p符号名(%d):' % len(no_12name))
for t, p in no_12name:
    print('   %-28s %s' % (t, p))
print()
print('=== 已映射示例(前15) ===')
for t in sorted(covered_where)[:15]:
    p6, p12s, hit = covered_where[t]
    print('   %-28s %s -> %s -> %s' % (t, p6, p12s, hit))
