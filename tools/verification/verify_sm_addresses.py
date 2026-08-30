# -*- coding: utf-8 -*-
"""verify_sm_addresses.py — W7: 07_state_machine.c 访问的 SRAM 地址 vs 金标准

工作：解析 07_state_machine.c 的所有 SRAM 地址访问来源（字面量 + #define 宏 + 局部指针），
展开成"07 访问地址集合"，与 state_machine 金标准反汇编（_disasm/0000458c）的 `; ref`
地址集合比对。两个方向：
  C-only     : 07 访问了金标准没有的 SRAM 地址 → 疑似臆造/错地址（高危）
  gold-only  : 金标准访问了但 07 未抓到（可能用 extern/间接访问）→ 需人工确认，非错误

用法：cd decompiled && python tools/verify_sm_addresses.py
"""
import re, os, sys
sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

C    = open(os.path.join(ROOT, 'firmware', 'src', '07_state_machine.c'), encoding='utf-8').read()
GOLD = open(os.path.join(ROOT, 'evidence', 'reverse', 'disassembly', 'functions', '0000458c_FUN_0000458c.txt'), encoding='utf-8').read()

ADDR = re.compile(r'0x1[0-9a-fA-F]{9}|0x1000[0-9a-fA-F]{4}')

# ── 1. 宏表：#define NAME ((volatile uintN_t*)0xADDR) / ((uintN_t*)0xADDR) ──
macros = {}
for m in re.finditer(
        r'#define\s+(\w+)\s+\(?\s*\(\s*(?:volatile\s+)?(?:uint\w+|unsigned)\s*\*\)\s*\(?\s*(0x[0-9a-fA-F]+)',
        C):
    macros[m.group(1)] = m.group(2).lower()
# 宏表同时覆盖 `(volatile uintN_t*)0xADDR` 无外括号定义
for m in re.finditer(r'#define\s+(\w+)\s+[\s\S]{0,40}?(volatile\s+uint\w+\s*\*)\s*\(?\s*(0x[0-9a-fA-F]+)', C):
    macros.setdefault(m.group(1), m.group(3).lower())
print('宏表 %d 个:' % len(macros))

c_addrs = set()
for m in re.finditer(r'0x1000[0-9a-fA-F]{4}|0x1001[0-9a-fA-F]{4}', C):
    c_addrs.add(m.group(0).lower())
# 宏使用 => 加入其地址（宏均为地址型指针）
for name, addr in macros.items():
    if re.search(r'\b' + re.escape(name) + r'\b', C):
        c_addrs.add(addr)

# ── 2. 金标准 ref 地址（两种格式都抓）──
#   简单式   : ; ref 0x1000xxxx (no mem)             → 抓 0x1... 直接地址
#   箭头式   : ; ref 0x00008f54 -> 0x100015f2         → 抓箭头右的 SRAM 地址
g_addrs = set()
for m in re.finditer(r';\s*ref\s+0x[0-9a-fA-F]+\s*->\s*(0x1[0-9a-fA-F]{7})', GOLD):
    g_addrs.add(m.group(1).lower())
for m in re.finditer(r';\s*ref\s+(0x1[0-9a-fA-F]{7})\s*\(no mem\)', GOLD):
    g_addrs.add(m.group(1).lower())

# ── 3. 比对 ──
c_only = sorted(c_addrs - g_addrs)
g_only = sorted(g_addrs - c_addrs)

print('\n=== 规模 ===')
print('07 访问地址数(集合): %d | 金标准 ref 地址数(集合): %d | 共同: %d' %
      (len(c_addrs), len(g_addrs), len(c_addrs & g_addrs)))
print('\n=== C-only（金标准无，疑似臆造/错地址 → 高危）===\n%s --> %d 个' %
      (' '.join(c_only) if c_only else '（无）', len(c_only)))
print('\n=== gold-only（金标准有但 07 未抓到 extern/间接 → 人工确认）===\n%s --> %d 个' %
      (' '.join(g_only) if g_only else '（无）', len(g_only)))
