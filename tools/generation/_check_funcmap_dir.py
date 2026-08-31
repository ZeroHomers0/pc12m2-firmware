# -*- coding: utf-8 -*-
import sys, re, json
sys.stdout.reconfigure(encoding='utf-8')

def load6(p):
    d = {}
    for l in open(p, encoding='utf-8'):
        l = l.strip()
        if not l or l.startswith('#'):
            continue
        m = re.match(r'([0-9a-fA-F]{4,8})\s+(\S+)\s+body=(\d+)', l)
        if m:
            d[int(m.group(1), 16)] = (m.group(2), int(m.group(3)))
    return d

def load12(p):
    d = {}
    for l in open(p, encoding='utf-8'):
        l = l.strip()
        if not l or l.startswith('#'):
            continue
        m = re.match(r'([0-9a-fA-F]{4,8})\s+(\S+)\s+body=(\d+)', l)
        if m:
            d[int(m.group(1), 16)] = (m.group(2), int(m.group(3)))
    return d

f6 = load6(r'D:\code\LPC1765FBD100\decompiled\evidence\reverse\reports\_all_functions.txt')
f12 = load12('tools/_ghidra_proj/_pc12m2_functions.txt')
m = json.load(open('tools/_ghidra_proj/_func_6to12_map.json', encoding='utf-8'))

for a in [0x758, 0x7a8, 0x1e4, 0x29a, 0x5cc, 0x4464]:
    print('0x%04x  6p:%s  12p:%s  funcmap:%s' % (a, f6.get(a), f12.get(a), m.get('0x%08x' % a)))
print()
print('6p 函数总数:', len(f6), ' 12p:', len(f12))
keys = list(m.keys())
print('funcmap key(%d): 在12p=%d 在6p=%d' %
      (len(keys), sum(1 for k in keys if int(k, 16) in f12), sum(1 for k in keys if int(k, 16) in f6)))
vals = [v[0] for v in m.values() if isinstance(v, list) and len(v) >= 2 and v[0] != 'DIFF']
print('funcmap value[0](%d): 在6p=%d 在12p=%d' %
      (len(vals), sum(1 for x in vals if int(x, 16) in f6), sum(1 for x in vals if int(x, 16) in f12)))
