# -*- coding: utf-8 -*-
"""verify_mem_xref.py — W7 通用内存交叉引用验证：模块.c 访问的 SRAM 地址 vs 金标准

对任意模块源 + 对应 Ghidra 反汇编金标准，比对"C 访问地址集合"与"金标准 ref 地址集合"，
双向判漏：
  C-only    : C 访问了金标准没有的地址 → 疑似臆造/错地址（高危）
  gold-only : 金标准访问了但 C 未抓到（可能直接 extern/间接访问）→ 人工确认
  common    : 两者一致（等价的正向证据）

用法：python tools/verify_mem_xref.py <模块.c> <金标准.txt> [金标准2.txt ...]
  宏自动解析为地址；局部指针自动展开。ref 支持 简单式 (no mem) 与 箭头式 ->。
"""
import re, os, sys
sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

args = sys.argv[1:]
if len(args) < 2:
    print('用法: python verify_mem_xref.py <模块.c> <金标准.txt> [更多金标准.txt...]')
    sys.exit(1)
c_path, gold_paths = args[0], args[1:]

def absp(p): return os.path.join(ROOT, p) if not os.path.isabs(p) else p
C    = open(absp(c_path), encoding='utf-8', errors='ignore').read()

# ── 1. 宏表（地址型指针宏）──
macros = {}
for m in re.finditer(
        r'#define\s+(\w+)\s+\(?\s*\(\s*(?:volatile\s+)?(?:uint\w+|unsigned)\s*\*\)\s*\(?\s*(0x[0-9a-fA-F]+)',
        C):
    macros[m.group(1)] = m.group(2).lower()

c_addrs = set()
for m in re.finditer(r'0x1[0-9a-fA-F]{7}', C):
    a = m.group(0).lower()
    if a.startswith('0x1') and len(a) == 10:   # 0x1xxxxxxx (8位hex)
        c_addrs.add(a)
# 宏真实使用 → 加地址（排除仅出现在 #define 定义行）
for name, addr in macros.items():
    body = re.sub(r'^\s*#define\s+' + re.escape(name) + r'.*$', '', C, flags=re.M)
    if re.search(r'\b' + re.escape(name) + r'\b', body):
        c_addrs.add(addr)

# ── 2. 金标准 ref 地址（两种格式）──
g_addrs = set()
for gp in gold_paths:
    G = open(absp(gp), encoding='utf-8', errors='ignore').read()
    for m in re.finditer(r';\s*ref\s+0x[0-9a-fA-F]+\s*->\s*(0x1[0-9a-fA-F]{7})', G):
        g_addrs.add(m.group(1).lower())
    for m in re.finditer(r';\s*ref\s+(0x1[0-9a-fA-F]{7})\s*\(no mem\)', G):
        g_addrs.add(m.group(1).lower())

# ── 3. 比对 ──
c_only = sorted(c_addrs - g_addrs)
g_only = sorted(g_addrs - c_addrs)
print('模块: %s' % c_path)
print('金标准: %d 文件' % len(gold_paths))
print('C 访问地址(集合): %d | 金标准 ref 地址(集合): %d | 共同: %d | 匹配率: %.1f%%' %
      (len(c_addrs), len(g_addrs), len(c_addrs & g_addrs),
       100.0 * len(c_addrs & g_addrs) / max(len(g_addrs), 1)))
print('C-only（臆造/错地址）: %d  %s' % (len(c_only), ' '.join(c_only) if c_only else '(无)'))
print('gold-only（间接/ext访问,人工确认）: %d  %s' % (len(g_only), ' '.join(g_only) if g_only else '(无)'))
