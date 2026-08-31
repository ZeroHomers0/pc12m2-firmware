#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""state_machine 0x4464 12p dump 结构分析器
解析 evidence/reverse/disassembly/functions/00004464_FUN_00004464.txt，
模式：找出 SRAM 字节读取点（pool->addr）及后继指令；打印 MENU/关键变量读取与分发。"""
import re, sys

SRC = r"D:\code\PC12M-2\evidence\reverse\disassembly\functions\00004464_FUN_00004464.txt"

# 解析指令流：每指令 {addr, ins, ref(池槽->值), sram(值即SRAM)}
instr = []
cur = None
with open(SRC, encoding='utf-8', errors='replace') as f:
    for line in f:
        m = re.match(r'^0000([0-9a-f]{4})\s+(.+)', line)
        if m:
            cur = {'addr': '0x' + m.group(1), 'ins': m.group(2).strip(), 'ref': None}
            instr.append(cur)
        elif cur and '; ref' in line:
            mm = re.search(r'ref 0x[0-9a-f]+ -> 0x([0-9a-f]{8})', line)
            if mm:
                cur['ref'] = '0x' + mm.group(1).lower()

def show(want, span=6, title=None):
    """打印 ref 命中 want 的读取点及后继 span 条指令"""
    if title: print(title)
    for i, c in enumerate(instr):
        if c['ref'] == want and c['ins'].startswith('ldr'):
            out = [f"  {c['addr']} {c['ins']}   <-- {want}"]
            for cc in instr[i+1:i+1+span]:
                out.append(f"  {cc['addr']} {cc['ins']}")
            print('\n'.join(out))
            print('  ---')

if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else 'menu'
    if mode == 'menu':
        show('0x10001744', span=5, title="=== MENU 读取点 + 后继分发 ===")
    elif mode == 'key':
        # 所有 cmp r4,#N（key 分发）
        print("=== 所有 cmp r4,#N（key 分发点） ===")
        for c in instr:
            m = re.match(r'cmp\s+r4,\s+#0x([0-9a-f]+)', c['ins'])
            if m:
                print(f"  {c['addr']} cmp r4,#0x{m.group(1)}")
    elif mode == 'bl':
        print("=== 所有 bl（调用） ===")
        for c in instr:
            m = re.match(r'bl\s+0x0000([0-9a-f]{4})', c['ins'])
            if m:
                print(f"  {c['addr']} bl 0x{m.group(1)}")
    elif mode == 'case':
        # 找 case 起点：cmp r4 后跟 bne/beq 到后续，或 ldrb MENU 后 cmp r0
        print("=== 候选 case 起点（MENU ldrb 后 cmp r0,#N） ===")
        for i, c in enumerate(instr):
            if c['ref'] == '0x10001744' and c['ins'].startswith('ldr'):
                # 下一条应为 ldrb r0,[r0]
                if i+1 < len(instr) and instr[i+1]['ins'].startswith('ldrb'):
                    cc = instr[i+2] if i+2 < len(instr) else None
                    if cc and re.match(r'cmp\s+r0,\s+#0x', cc['ins']):
                        m = re.match(r'cmp\s+r0,\s+#0x([0-9a-f]+)', cc['ins'])
                        nxt = instr[i+3] if i+3 < len(instr) else None
                        nb = nxt['ins'] if nxt else ''
                        print(f"  {cc['addr']} cmp r0,#0x{m.group(1)}  {nb}  [{c['addr']}]")
