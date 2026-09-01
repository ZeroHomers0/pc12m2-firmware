# -*- coding: utf-8 -*-
# dump_disasm_12.py — PC12M-2（12 相）逐函数反汇编金标准生成
# 对标 6p DumpAllDisasm.java 输出格式：每函数一个 evidence/reverse/disassembly/0000xxxx_FUN_xxx.txt
#   # FUN_xxx entry=0x... body=...
#   addr  mnemonic  operands
#       ; ref 0x<pool> -> 0x<值>      （literal-pool 数据引用：池槽->内容）
#       ; call -> 0x<目标>            （bl/blx）
#       ; jump -> 0x<目标>            （b）
# 数据源：backup/pc12m2_orig.bin + tools/_ghidra_proj/_pc12m2_functions.txt
# 池槽内容经 _pc12m2_pool_sram_map.json 标注（池槽->SRAM/外设地址）
import struct, json, re, os, sys
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM

sys.stdout.reconfigure(encoding='utf-8')

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / "pc12m2_orig.bin"
FUNCS = ROOT / "tools" / "_ghidra_proj" / "_pc12m2_functions.txt"
POOLMAP = ROOT / "tools" / "_ghidra_proj" / "_pc12m2_pool_sram_map.json"
OUTDIR = ROOT / "evidence" / "reverse" / "disassembly" / "raw"

d = BIN.read_bytes()
poolmap = json.load(open(POOLMAP, encoding="utf-8"))  # "0x<flash_pool>" -> "0x<addr>"

# 函数清单（入口, 名, body）
funcs = []
for ln in FUNCS.read_text(encoding="utf-8").splitlines():
    ln = ln.strip()
    if not ln or ln.startswith('#'):
        continue
    m = re.match(r'([0-9a-fA-F]{8})\s+(\S+)\s+body=(\d+)', ln)
    if m:
        funcs.append((int(m.group(1), 16), m.group(2), int(m.group(3))))
funcs.sort()
entries = [f[0] for f in funcs]

def func_for(addr):
    import bisect
    i = bisect.bisect_right(entries, addr) - 1
    return funcs[i][1] if i >= 0 else "?"

def u32(off):
    return struct.unpack_from("<I", d, off)[0]

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True

SRAM_LO, SRAM_HI = 0x10000000, 0x10008000
PERI_LO, PERI_HI = 0x40000000, 0x50000000

def fmt_val(v):
    """值分类注释：SRAM/外设地址 or flash 地址 or 常量"""
    if SRAM_LO <= v < SRAM_HI:
        return "0x%08x (SRAM)" % v
    if PERI_LO <= v < PERI_HI:
        return "0x%08x (PERI)" % v
    if 0x100 < v < len(d):
        return "0x%06x (flash)" % v
    return "0x%08x" % v

def literal_pool_targets(insn):
    """返回 literal-pool 访问的池槽地址列表（ldr rX,[pc,#imm] / adr rX, #imm）"""
    mnem = insn.mnemonic
    pools = []
    if mnem in ("ldr", "ldr.w", "ldrb", "ldrb.w", "ldrh", "ldrh.w"):
        for op in insn.operands:
            # Capstone 的 PC 寄存器 ID 因版本而异，用 reg_name 判断
            if op.type == ARM_OP_MEM and md.reg_name(op.mem.base) == "pc":
                imm = op.mem.disp
                pool = (insn.address & ~3) + 4 + imm
                pools.append(pool)
    elif mnem == "adr":
        # adr rX, [pc, #imm]  /  adr rX, #imm —— 目标即池槽
        for op in insn.operands:
            if op.type == ARM_OP_IMM:
                # Capstone 对 adr 的 imm 是绝对地址（已按 pc 计算），但实际语义仍需定位到字
                pool = op.imm & ~3
                if 0 < pool < len(d):
                    pools.append(pool)
    return pools

def call_jump_targets(insn):
    """bl/blx（函数调用）及所有 b* 分支（条件跳转）的目标 → [(tgt, 'call'|'jump')]"""
    out = []
    m = insn.mnemonic
    # call：bl / bl.<cond> / blx（blt/ble/bls 是条件分支，不是调用）
    is_call = m == "bl" or m == "blx" or m.startswith("bl.")
    # jump：所有 b 前缀且非 bl/blx/bx（寄存器跳转）
    is_jump = m.startswith("b") and not m.startswith("bx") and not is_call
    kind = "call" if is_call else ("jump" if is_jump else None)
    if kind:
        for op in insn.operands:
            if op.type == ARM_OP_IMM:
                out.append((op.imm, kind))
    return out

total_refs = 0
for entry, name, body in funcs:
    out = []
    out.append("# %s entry=0x%08x body=%d" % (name, entry, body))
    code = d[entry:entry + body]
    for insn in md.disasm(code, entry):
        line = "%08x  %-8s %s" % (insn.address, insn.mnemonic, insn.op_str)
        out.append(line)
        # literal-pool 数据引用注释
        for pool in literal_pool_targets(insn):
            if pool + 4 > len(d):
                continue
            val = u32(pool)
            k = "0x%08x" % pool
            pv = poolmap.get(k)
            note = " ; ref 0x%08x -> %s" % (pool, fmt_val(val))
            if pv:
                note += "  [pool->%s]" % pv
            out.append(note)
            total_refs += 1
        # call/jump 注释
        for tgt, kind in call_jump_targets(insn):
            out.append(" ; %s -> 0x%08x (%s)" % (kind, tgt, func_for(tgt)))
    # 写文件
    fname = "%08x_%s.txt" % (entry, name)
    (OUTDIR / fname).write_text("\n".join(out) + "\n", encoding="utf-8")
    print("  %s  (%d insn)" % (fname, body and len(list(md.disasm(code, entry)))))

print("done: %d functions, %d literal-pool refs -> %s" % (len(funcs), total_refs, OUTDIR))
