# -*- coding: utf-8 -*-
"""verify_readwidth_all.py — 从反汇编自动提取 SRAM 访问宽度，对照 globals.c 找读宽 bug

原理：evidence/reverse/disassembly/functions/*.txt 每处内存解引用都有注释 `; ref 0x1000xxxx (no mem)`，
其上一条指令的操作码决定访问宽度：
  ldrb/strb  -> byte（1 字节）
  ldr/str    -> word（4 字节）
  ldrh/strh  -> half（2 字节）

对照 globals.c 里 `volatile uintN_t *DAT_xxx = (uintN_t *)0xADDR` 的定义宽度，
找出：
  A) 反汇编 byte 访问、但符号定义为 uint32_t*（word 读多/写多 → 污染相邻槽）  【高危】
  B) 反汇编 word 访问、但符号定义为 uint8_t*（word 读少/写少 → 数据截断）      【高危】

用法：cd decompiled && python tools/verify_readwidth_all.py
"""
import re, os, sys
sys.stdout.reconfigure(encoding='utf-8')

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DISASM = os.path.join(ROOT, 'evidence', 'reverse', 'disassembly', 'functions')
GLOBALS = os.path.join(ROOT, 'firmware', 'globals.c')

SRAM = re.compile(r';\s*ref\s+(0x1000[0-9a-fA-F]{4})\s*\(no mem\)')
OP = re.compile(r'^\s*[0-9a-fA-F]{8}\s+(\S+)\s')

# 1) 从反汇编提取每个 SRAM 地址的访问宽度集合
byte_addrs = set()      # 出现 ldrb/strb
word_addrs = set()      # 出现 ldr/str (word)
half_addrs = set()      # 出现 ldrh/strh
conflict = {}           # addr -> set(宽度)，若同地址出现多种宽度

def classify(op):
    o = op.lower()
    if o.startswith('ldrb') or o.startswith('strb'):
        return 'byte'
    if o.startswith('ldrh') or o.startswith('strh'):
        return 'half'
    if o.startswith('ldr') or o.startswith('str'):
        return 'word'
    return None

files = sorted(f for f in os.listdir(DISASM) if f.endswith('.txt'))
prev_line = ''
for fn in files:
    with open(os.path.join(DISASM, fn), encoding='latin-1') as f:
        for line in f:
            m = SRAM.search(line)
            if m:
                addr = int(m.group(1), 16)
                opm = OP.match(prev_line)
                w = classify(opm.group(1)) if opm else None
                if w == 'byte':
                    byte_addrs.add(addr)
                elif w == 'word':
                    word_addrs.add(addr)
                elif w == 'half':
                    half_addrs.add(addr)
                else:
                    # 无法判定（prev_line 不是指令），记录供人工
                    pass
            prev_line = line

# 2) 解析 globals.c 符号定义：addr -> (name, width)
DEF = re.compile(
    r'volatile\s+(uint32_t|uint8_t|uint16_t)\s+\*(DAT_[0-9a-fA-F]+)\s*=\s*'
    r'\((uint32_t|uint8_t|uint16_t)\s*\*\)\s*0x([0-9a-fA-F]+)'
)
def_by_addr = {}   # addr -> list of (name, width)
for m in DEF.finditer(open(GLOBALS, encoding='latin-1').read()):
    w = m.group(1)
    name = m.group(2)
    addr = int(m.group(4), 16)
    def_by_addr.setdefault(addr, []).append((name, w))

def gw(w):
    return {'uint8_t': 'byte', 'uint16_t': 'half', 'uint32_t': 'word'}[w]

print('== 反汇编提取：byte %d / word %d / half %d 个 SRAM 地址 ==' %
      (len(byte_addrs), len(word_addrs), len(half_addrs)))

# 纯 byte = 只出现 ldrb/strb，从不 word/half 访问（真·byte 槽）
byte_only = byte_addrs - word_addrs - half_addrs
# 纯 word = 只出现 ldr/str，从不 byte/half 访问（真·word 槽）
word_only = word_addrs - byte_addrs - half_addrs

issues = []
# A) 真·byte 槽，但符号定义为 uint32_t*（word 读多/写多 → 污染相邻槽）
for a in sorted(byte_only):
    if a in def_by_addr:
        for name, w in def_by_addr[a]:
            if gw(w) == 'word':
                issues.append(('A  byte槽(纯)定义为word', a, name, w))
# B) 真·word 槽，但符号定义为 uint8_t*（byte 读少/写少 → 截断）
for a in sorted(word_only):
    if a in def_by_addr:
        for name, w in def_by_addr[a]:
            if gw(w) == 'byte':
                issues.append(('B  word槽(纯)定义为byte', a, name, w))

print()
if issues:
    print('=== 发现 %d 处读宽不一致（真·纯 byte/word 槽）===' % len(issues))
    for kind, a, name, w in issues:
        print('%s  0x%08X  %-20s 定义=%s' % (kind, a, name, w))
else:
    print('=== 无读宽不一致（纯 byte/word 槽与反汇编一致）===')

# 3) 同地址多种宽度（16 位参数分高低字节，属正常，仅列数量）
print()
print('=== 16位参数分字节访问（byte+word 同址，属正常，共 %d 个）===' % len(byte_addrs & word_addrs))
print('=== half 访问地址数：%d（供参考）===' % len(half_addrs))
