# -*- coding: utf-8 -*-
# dump_12_isrs.py — Capstone 反汇编 12p 向量表 ISR（补 Ghidra 未导出的真实中断函数）
# 12p 向量表（backup/pc12m2_orig.bin）：
#   WDT->0x1e4 TIMER0->0x29a TIMER1->0xfb0c TIMER2->0xfae8 UART3->0xac78 EINT3->0xf748 ADC->0xf76a
# 其中 0xac78/0xf748/0xf76a/0xfae8/0xfb0c 在 Ghidra 工程中未被识别为函数（DumpAllDisasm12 未导出），
# 且 6p 同名 ISR 文件曾误混入 functions/ 目录。本脚本用 Capstone 从 BIN 原始字节反汇编，
# 解析 ldr [pc,#imm] 池槽并标注 `; ref 池槽 -> 值`（格式对齐 DumpAllDisasm12）。
# 输出到 evidence/reverse/disassembly/functions/{addr}_FUN_{addr}.txt
import struct, sys, os
sys.path.insert(0, r'D:\code\LPC1765FBD100\decompiled\.venv\Lib\site-packages')
from capstone import *
from capstone.arm import *

ROOT = r'D:\code\PC12M-2'
D = open(os.path.join(ROOT, r'backup\pc12m2_orig.bin'), 'rb').read()
OUT = os.path.join(ROOT, r'evidence\reverse\disassembly\functions')

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB + CS_MODE_MCLASS)
md.detail = True

ISRS = {
    0xac78: 'uart3_isr',
    0xf748: 'eint3_isr',
    0xf76a: 'adc_isr',
    0xfae8: 'timer2_svc',
    0xfb0c: 'timer1_isr',
    # 向量表 ISR（Ghidra 未纳入 94 函数清单；此前人工文件 ref 标注为 6p 值，
    # 这里从 12p BIN 重新生成，ref 值 = 12p BIN 池槽真实值）：
    0x1e4: 'wdt_isr',
    0x29a: 'timer0_isr',
}

def align4(a):
    return (a + 4) & ~3

def ref_of(ins):
    """若指令是 ldr/ldrh/ldrb + [pc,#imm]，返回 (池槽地址, 池槽值)；否则 None"""
    if ins.mnemonic not in ('ldr', 'ldrh', 'ldrb', 'ldrsh', 'ldrsb', 'adr'):
        return None
    for op in ins.operands:
        if op.type == ARM_OP_MEM and op.mem.base == ARM_REG_PC:
            pool = align4(ins.address) + op.mem.disp
            if pool % 4 == 0 and 0 < pool < len(D):
                val = struct.unpack_from('<I', D, pool)[0]
                return (pool, val)
    return None

def dis_func(addr, max_insn=2000):
    """线性反汇编到 bx lr（函数返回）即停，避免越过边界解码后续函数/数据区。
    bx 非 lr（表跳转/条件返回）不终止——ISR 单一出口，遇 bx lr 即函数终点。"""
    out = []
    a = addr
    while a < addr + 0x10000 and len(out) < max_insn:
        code = D[a:a + 4]
        ins = next(md.disasm(code, a), None)
        if ins is None:
            break  # 数据区（池槽）
        out.append(ins)
        a += ins.size
        if ins.mnemonic == 'bx' and 'lr' in ins.op_str:
            break  # 函数返回指令，终止
    return out

def fmt_op(ins):
    """统一操作数字符串风格（去掉 # 前的空格差异，保持可读）"""
    return ins.op_str

for addr, name in sorted(ISRS.items()):
    insns = dis_func(addr)
    lines = ['# %s entry=0x%08x body=%d' % (name, addr, insns[-1].address + insns[-1].size - addr if insns else 0)]
    for ins in insns:
        lines.append('%08x  %-8s %s' % (ins.address, ins.mnemonic, fmt_op(ins)))
        r = ref_of(ins)
        if r:
            lines.append('      ; ref 0x%08x -> 0x%08x' % r)
    fp = os.path.join(OUT, '%08x_FUN_%08x.txt' % (addr, addr))
    with open(fp, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print('%08x %s: %d 条指令 -> %s' % (addr, name, len(insns), os.path.basename(fp)))
