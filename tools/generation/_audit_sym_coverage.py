# -*- coding: utf-8 -*-
# _audit_sym_coverage.py — P3 前哨：6p 源码符号 → 12p 映射覆盖率审计
import re, json, glob, sys
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')

poolmap = json.load(open(r'D:\code\PC12M-2\tools\_ghidra_proj\_pool_6to12_map.json', encoding='utf-8'))
slotnames = json.load(open(r'D:\code\PC12M-2\tools\_ghidra_proj\_globals_slot_names.json', encoding='utf-8'))
G6 = open(r'D:\code\LPC1765FBD100\decompiled\firmware\globals.c', encoding='utf-8').read()

# 1. 6p globals.c 符号表: 符号名 -> (池槽 or None, 初值)
syms6 = {}
for ln in G6.splitlines():
    ln = ln.strip()
    # 形如: volatile uint32_t *g_pconp = (uint32_t *)0x400FC0C4;  /* peri word */
    #       uint32_t DAT_00000564 = 0x400FC1A0;  /* peri word */
    m = re.match(r'(?:(?:volatile\s+(?:uint8_t|uint32_t)\s*\*)|(?:uint32_t))\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*'
                 r'(?:\((?:uint8_t|uint32_t)\s*\*\))?(0x[0-9A-Fa-f]+)', ln)
    if m:
        nm = m.group(1)
        val = int(m.group(2), 16)
        pm = re.match(r'(?:PTR_)?DAT_([0-9A-Fa-f]{4,8})', nm)
        pool = int(pm.group(1), 16) if pm else None
        syms6[nm] = (pool, val)
print('6p globals 符号总数:', len(syms6))

# 2. 6p src 符号引用
SRC = glob.glob(r'D:\code\LPC1765FBD100\decompiled\firmware\src\*.c')
toks = set()
tokcnt = 0
byfile = {}
for fp in SRC:
    n = 0
    for ln in open(fp, encoding='utf-8', errors='replace'):
        for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\b', ln):
            t = m.group(1)
            if t in syms6:
                toks.add(t); tokcnt += 1; n += 1
    byfile[Path(fp).name] = n
print('6p src 引用 globals 符号: 唯一 %d, 总引用 %d' % (len(toks), tokcnt))
print('按文件引用次数:')
for k, v in sorted(byfile.items(), key=lambda x: -x[1]):
    print('   %-28s %d' % (k, v))

# 3. 覆盖率
no_pool, not_in_pool, no_12name, covered = [], [], [], 0
for t in sorted(toks):
    pool, val = syms6[t]
    if pool is None:
        no_pool.append((t, val)); continue
    p6 = '0x%08x' % pool
    if p6 not in poolmap:
        not_in_pool.append((t, pool, val)); continue
    if any(p12 in slotnames for p12 in poolmap[p6]):
        covered += 1
    else:
        no_12name.append((t, p6))
print()
print('=== 有池槽且已映射(poolmap+slotname): %d / %d ===' % (covered, len(toks)))
print('=== 纯语义名(无池槽)(%d) ===' % len(no_pool))
for t, v in no_pool:
    print('   %s = 0x%08X' % (t, v))
print('=== 池槽不在 poolmap(%d) ===' % len(not_in_pool))
for t, p, v in not_in_pool:
    print('   %s 6p池槽=0x%04x v=0x%08X' % (t, p, v))
print('=== 池槽在poolmap但无12p符号名(%d) ===' % len(no_12name))
for t, p in no_12name:
    print('   %s %s' % (t, p))
