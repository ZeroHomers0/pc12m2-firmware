# -*- coding: utf-8 -*-
"""verify_periph_xref.py — 外设地址交叉引用验证（金标准 vs 新固件）

对「金标准反汇编 ref 中的外设地址」与「C 源码实际访问的外设字节地址」做双向比对，
把 verify_mem_xref.py（仅 SRAM 0x1xxxxxxx）的盲区——外设 0x2xxxxxxx/0x4xxxxxxx——
纳入验证。B1 2026-08-27 新增。

C 侧地址展开支持四类形态（这是本工具与 verify_mem_xref 的关键差异）：
  1. 字面量     ：*(volatile uint*)0x40034004
  2. 元素索引   ：DAT_xxx[N]            → base + N*elem   (uint32_t* elem=4, uint8_t* elem=1)
  3. 字节偏移   ：(uint)DAT_xxx + 0xNN  → base + 0xNN
  4. 局部别名   ：fio = DAT_xxx; fio[N]/fio+0xNN/*fio  （函数内赋值跟踪）
  5. 结构体宏   ：TIMER0->TCR 等（reg.h，基址+成员偏移）

输出：
  C-only    : C 访问了金标准没有的外设地址 → 臆造/错位（高危）
  gold-only : 金标准访问了但 C 未展开到 → 遗漏 或 解析形态差异（人工确认）
  common    : 一致（正向证据）

用法：cd decompiled && python tools/verification/verify_periph_xref.py [模块.c ...]
  不传参数 = 全库扫描所有 firmware/src/*.c
"""
import re, sys, glob, os
from collections import defaultdict

sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

# ═══════════════════════════════════════════════════════════════════
# 1. globals.c 符号表：name -> (base_addr:int, elem:int)
# ═══════════════════════════════════════════════════════════════════
def load_syms():
    syms = {}
    text = open('firmware/globals.c', encoding='utf-8', errors='ignore').read()
    # 指针类型：volatile uint32_t *NAME = (uint32_t *)0xADDR
    for m in re.finditer(
            r'(?:volatile\s+)?(uint32_t|uint8_t)\s*\*\s*(DAT_\w+|g_\w+|PTR_\w+)\s*=\s*'
            r'\((?:uint32_t|uint8_t)\s*\*\)\s*0x([0-9A-Fa-f]+)', text):
        syms[m.group(2)] = (int(m.group(3), 16), 4 if m.group(1) == 'uint32_t' else 1)
    # 值类型：uint32_t DAT_xxx = 0xADDR（+N 为字节偏移）
    for m in re.finditer(r'uint32_t\s+(DAT_\w+|g_\w+)\s*=\s*0x([0-9A-Fa-f]+)', text):
        if m.group(1) not in syms:
            syms[m.group(1)] = (int(m.group(2), 16), 1)
    return syms

SYMS = load_syms()

def is_periph(addr):
    return (0x2009C000 <= addr < 0x200A0000) or (0x40000000 <= addr < 0x50000000)

PERI_SYMS = {k: v for k, v in SYMS.items() if is_periph(v[0])}

# ═══════════════════════════════════════════════════════════════════
# 2. reg.h 结构体/寄存器宏（TIMER0->TCR 等）
# ═══════════════════════════════════════════════════════════════════
def load_reg_macros():
    """返回 { (宏名, 成员名): 基址+偏移 } 与 { 寄存器宏: 地址 }"""
    base_of = {}
    off_of = {}
    # 基址宏
    text = open('firmware/inc/reg.h', encoding='utf-8', errors='ignore').read()
    for m in re.finditer(r'#define\s+(TIMER[0-3])\s+\(\(LPC_TIM_TypeDef\s*\*\)\s*(0x[0-9A-Fa-f]+)UL?', text):
        base_of[m.group(1)] = int(m.group(2), 16)
    # TIMER 结构体成员偏移（LPC_TIM_TypeDef，reg.h 已知布局 IR=0 TCR=4 TC=8 PR=0xC PC=0x10 MCR=0x14 MR0-3=0x18-0x24）
    for i, name in enumerate(('IR', 'TCR', 'TC', 'PR', 'PC', 'MCR')):
        off_of[name] = i * 4
    for i, name in enumerate(('MR0', 'MR1', 'MR2', 'MR3')):
        off_of[name] = 0x18 + i * 4
    # REG32(WDT_BASE+0x04) 形式
    for m in re.finditer(r'#define\s+(\w+)\s+REG32\((\w+)\s*\+\s*0x([0-9A-Fa-f]+)\)', text):
        base_name, off = m.group(2), int(m.group(3), 16)
        if base_name in base_of:
            base_of[m.group(1)] = base_of[base_name] + off
        elif m.group(1) in base_of:
            pass
    return base_of, off_of

REG_BASE, REG_OFF = load_reg_macros()
# WDT 区特殊：reg.h 注释说 WDT 基址 0x40000000
for name, off in (('WDMOD', 0), ('WDTC', 0x04), ('WDFEED', 0x08), ('WDTV', 0x0C)):
    REG_BASE.setdefault(name, 0x40000000 + off)
# TIMER 寄存器全名（TIMER0->TCR 中宏名 = 成员名）
for t in ('TIMER0', 'TIMER1', 'TIMER2', 'TIMER3'):
    if t in REG_BASE:
        for mem, off in REG_OFF.items():
            REG_BASE.setdefault(f'{t}->{mem}', REG_BASE[t] + off)

def expand_macro(name):
    return REG_BASE.get(name)

# ═══════════════════════════════════════════════════════════════════
# 3. 金标准外设 ref 地址（直接访问 no-mem 与指针基址 ->）
# ═══════════════════════════════════════════════════════════════════
def gold_periph_addrs():
    direct, ptr = set(), set()
    for p in glob.glob('evidence/reverse/disassembly/functions/*.txt'):
        for line in open(p, encoding='utf-8', errors='ignore'):
            m = re.search(r';\s*ref\s+(0x[0-9a-fA-F]+)\s*\(no mem\)', line)
            if m and is_periph(int(m.group(1), 16)):
                direct.add(int(m.group(1), 16))
            m2 = re.search(r';\s*ref\s+0x[0-9a-fA-F]+\s*->\s*(0x[0-9a-fA-F]+)', line)
            if m2 and is_periph(int(m2.group(1), 16)):
                ptr.add(int(m2.group(1), 16))
    return direct, ptr

GOLD_DIRECT, GOLD_PTR = gold_periph_addrs()

# ═══════════════════════════════════════════════════════════════════
# 4. C 源码外设访问展开（含局部别名 + 宏）
# ═══════════════════════════════════════════════════════════════════
def parse_c_addrs(src_path):
    text = open(src_path, encoding='utf-8', errors='ignore').read()
    # 去注释但保行数（块注释替换为等量换行，防止后续溯源行号偏移）
    code = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    code = re.sub(r'//.*$', '', code, flags=re.M)

    direct = set()   # 被解引用/访问的字节地址
    ptr_used = set() # 指针值被使用（基址）
    aliases = {}     # 当前活跃别名：local -> 符号名（顺序扫描，最近赋值覆盖）

    def resolve(name):
        """返回 (base, elem) 或 None"""
        if name in PERI_SYMS:
            return PERI_SYMS[name]
        tgt = aliases.get(name)
        if tgt:
            if tgt in PERI_SYMS:
                return PERI_SYMS[tgt]
            if tgt in REG_BASE:
                return (REG_BASE[tgt], 1)
        return None

    def scan_sym(ln, name, base, elem):
        r"""在单行 ln 上展开符号 name(base,elem) 的全部访问形态，返回命中地址集合。
        \b 前缀防 PTR_DAT_xxx 误配子串 DAT_xxx；(?<![\w)]) 排除 (uint)NAME 右括号前导
        （字节转换形态）；(?![0-9a-fA-F])/(?!\d) 防 0x34 被回溯截断成 0x3。"""
        out = set()
        esc = re.escape(name)
        # 裸解引用 *NAME（无转换；*(volatile uint*)NAME 形态若带 +N 由下方分支覆盖）
        for m in re.finditer(r'\*(?!\s*\(?volatile)(?=\s*' + esc + r'\b)', ln):
            out.add(base)
        # 解引用转换形态 *(volatile uint*)NAME（无偏移）
        for m in re.finditer(r'\*\s*\(?volatile\s+uint\w*\s*\*\)?\s*' + esc + r'\b', ln):
            out.add(base)
        # NAME[N] 元素索引
        for m in re.finditer(r'\b' + esc + r'\[(0x[0-9a-fA-F]+|\d+)\]', ln):
            n = int(m.group(1), 16) if m.group(1).startswith('0x') else int(m.group(1))
            out.add(base + n * elem)
        # (uint)NAME + 0xNN 字节偏移（无 \b：((uint)g_adc 双层括号之间无单词边界；转换前导已锚定）
        for m in re.finditer(r'\((?:uint|uint32_t|int)\)\s*' + esc + r'\s*\+\s*(0x[0-9a-fA-F]+|\d+)', ln):
            off = int(m.group(1), 16) if m.group(1).startswith('0x') else int(m.group(1))
            out.add(base + off)
        # NAME + 0xNN 无转换：uint32_t* → ×4（危险形态）；值/uint8_t* → 字节
        for m in re.finditer(r'(?<![\w)])\b' + esc + r'\s*\+\s*(0x[0-9a-fA-F]+(?![0-9a-fA-F])|\d+(?!\d))', ln):
            off = int(m.group(1), 16) if m.group(1).startswith('0x') else int(m.group(1))
            out.add(base + (off * 4 if elem == 4 else off))
        return out

    for ln in code.split('\n'):
        # ① 先更新别名（赋值语句；本行若兼有访问按新值展开——真实代码赋值行无访问）
        for m in re.finditer(r'(\w+)\s*=\s*(DAT_\w+|g_\w+|PTR_\w+|TIMER[0-3])\s*;', ln):
            aliases[m.group(1)] = m.group(2)
        # ② 字面量直接访问
        for m in re.finditer(r'(?<![0-9a-zA-Z_])(0x[0-9a-fA-F]{7,8})(?![0-9a-zA-Z_])', ln):
            a = int(m.group(1), 16)
            if is_periph(a):
                direct.add(a)
        # ③ 结构体宏 TIMER0->TCR / TIMER1->MR0 等
        for m in re.finditer(r'(TIMER[0-3])\s*->\s*(\w+)', ln):
            a = expand_macro(f'{m.group(1)}->{m.group(2)}')
            if a:
                direct.add(a)
        # ④ 纯寄存器宏 WDTC / WDMOD / WDFEED / WDTV
        for name in ('WDTC', 'WDMOD', 'WDFEED', 'WDTV'):
            if name in REG_BASE and re.search(r'\b' + name + r'\b', ln):
                direct.add(REG_BASE[name])
        # ⑤ 符号形态（PERI_SYMS 中本行出现的符号）
        for name, (base, elem) in PERI_SYMS.items():
            if name in ln:
                direct |= scan_sym(ln, name, base, elem)
        # ⑥ 别名形态（当前活跃别名）
        for local in list(aliases):
            res = resolve(local)
            if res:
                direct |= scan_sym(ln, local, res[0], res[1])
    return direct, ptr_used

# ═══════════════════════════════════════════════════════════════════
# 5. 主流程
# ═══════════════════════════════════════════════════════════════════
def main():
    targets = sys.argv[1:] or sorted(glob.glob('firmware/src/*.c'))
    all_c, all_gold_only, all_c_only = set(), set(), set()
    print(f"=== 外设地址 xref（金标准 {len(GOLD_DIRECT)} 直接 / {len(GOLD_PTR)} 指针）===")
    for src in targets:
        direct, _ = parse_c_addrs(src)
        all_c |= direct
    c_only = sorted(all_c - GOLD_DIRECT, key=lambda x: x)
    gold_only = sorted(GOLD_DIRECT - all_c, key=lambda x: x)
    print(f"\nC 访问外设地址集合: {len(all_c)} | 金标准直接访问: {len(GOLD_DIRECT)}")
    print(f"共同: {len(all_c & GOLD_DIRECT)} | 匹配率: {100.0*len(all_c & GOLD_DIRECT)/max(len(GOLD_DIRECT),1):.1f}%")

    print("\n=== C-only（C 访问了金标准没有 → 臆造/错位 高危）===")
    if c_only:
        for a in c_only:
            print(f"  0x{a:08X}")
        print(f"  ⚠ {len(c_only)} 个需确认")
    else:
        print("  (无) ✓")

    print("\n=== gold-only（金标准访问但 C 未展开到 → 遗漏 或 形态差异）===")
    if gold_only:
        # 常见形态差异：TIMER 结构体宏（已展开）、WDT 宏（已展开）——若仍出现则是真遗漏或未覆盖形态
        for a in gold_only:
            print(f"  0x{a:08X}")
        print(f"  {len(gold_only)} 个（若已由宏/别名覆盖则人工确认）")
    else:
        print("  (无) ✓")

if __name__ == '__main__':
    main()
