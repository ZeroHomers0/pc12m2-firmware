# -*- coding: utf-8 -*-
# extract_modbus_branches.py — 从 08_modbus_dispatch_asm.txt 提取全部写单寄存器分支
# 每个分支：寄存器号(0x10NN)、值缓存地址、范围上下限、参数槽地址。
# 输出：tools/_modbus_branches.txt（供 W1a modbus_dispatch C 还原）
import re, struct, sys

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r"D:\code\LPC1765FBD100\decompiled"
BIN = open(ROOT + r"\LPC1765.bin", "rb").read()

inst = []   # (addr, text)
for l in open(ROOT + r"\08_modbus_dispatch_asm.txt", encoding="utf-8", errors="replace"):
    m = re.match(r"^([0-9a-f]+):\s*(.*)$", l.strip())
    if m:
        inst.append((int(m.group(1), 16), m.group(2).strip()))

def load(addr):
    return struct.unpack("<I", BIN[addr:addr+4])[0] if addr + 4 <= len(BIN) else None

# ── 1. 写分支入口：帧[2]==0x10 且帧[3]==NN ──
branches = []
for i, (a, t) in enumerate(inst):
    if t == "ldrb r0,[r0,#0x2]" and i + 2 < len(inst) \
            and inst[i+1][1] == "cmp r0,#0x10" \
            and inst[i+2][1].startswith("bne "):
        # 帧[3] 读取前有 `ldr r0,[frame]` 重新加载指针（前两个 ldrb 消耗 r0）→ 在 i+4
        j = i + 4
        if j < len(inst) and inst[j][1] == "ldrb r0,[r0,#0x3]" and j + 1 < len(inst):
            m = re.match(r"cmp r0,#((?:0x[0-9a-fA-F]+)|\d+)", inst[j+1][1])
            if m:
                raw = m.group(1)
                reglo = int(raw, 16 if raw.startswith("0x") else 10)
                branches.append({"addr": a, "reg": 0x1000 | reglo, "start": i})

def branch_body(addr):
    """分支体 = [addr, 下一分支入口)"""
    end = 0xE19E
    for b in branches:
        if b["addr"] > addr and b["addr"] < end:
            end = b["addr"]
            break
    return [t for a, t in inst if addr <= a < end]

def word_stores(body):
    """`ldr r1,[lit]` + 紧随 `str r0,[r1,#0]`（word 存）或 `strb r0,[r1,#0]`（byte 存）"""
    res = []
    for k in range(len(body) - 1):
        m = re.match(r"ldr r1,\[0x([0-9a-f]+)\]", body[k])
        if m:
            win = " ".join(body[k+1:k+4])
            if "strb r0,[r1,#0x0]" in win:
                res.append((int(m.group(1), 16), k, "byte"))
            elif "str r0,[r1,#0x0]" in win:
                res.append((int(m.group(1), 16), k, "word"))
    return res

def psync_pos(body):
    for k, t in enumerate(body):
        if t == "bl 0x000035f2":
            return k
    return -1

def ranges(body):
    """越界跳转：movw r1,#X+cmp+bcs（上限1）、cmp r0,#X+bcs/bls（上限2/下限）、
    cbnz/cbz 值==0 判空。upper=(值, 越界条件)，None 表示无。"""
    upper = lower = zero_check = None
    for k in range(len(body)):
        # movw r1,#X; cmp r0,r1; bcs → 上限
        m = re.match(r"movw r1,#(0x[0-9a-fA-F]+)", body[k])
        if m and "cmp r0,r1" in body[k+1:k+2]:
            nxt = " ".join(body[k+2:k+6])
            if "bcs" in nxt: upper = (int(m.group(1), 16), "bcs")
        # cmp r0,#X; bcs/bls → 上限或下限
        m = re.match(r"cmp r0,#((?:0x[0-9a-fA-F]+)|\d+)", body[k])
        if m:
            raw = m.group(1)
            v = int(raw, 16 if raw.startswith("0x") else 10)
            nxt = " ".join(body[k+1:k+5])
            if "bcs" in nxt and upper is None: upper = (v, "bcs")
            if "bls" in nxt: lower = (v, "bls")
        # cbnz/cbz → 值==0 特殊处理
        if "cbnz r0" in body[k] or "cbz r0" in body[k]:
            zero_check = body[k].strip()
    return upper, lower, zero_check

out = []
out.append("写分支总数: %d" % len(branches))
out.append("reg     入口     类型   值缓存SRAM   参数槽SRAM    上限      下限      值==0")
for b in branches:
    body = branch_body(b["addr"])
    ps = psync_pos(body)
    stores = word_stores(body)
    valcache = stores[0][0] if stores else None
    v_sram = load(valcache) if valcache else None
    params = None
    ptype = "-"
    if ps != -1:
        before = [s for s in stores if s[1] < ps]
        if before:
            params, _, ptype = before[-1]     # param_sync 前最近的 store（word/byte）
    p_sram = load(params) if params else None
    upper, lower, zero = ranges(body)
    spec = ""
    if ps == -1:
        spec = "NO_PSYNC" + (" I2C_WRITE" if "bl 0x00001e88" in " ".join(body) else "")
    out.append("0x%04X 0x%05X  %-6s 0x%08X    0x%08X    %-8s %-8s %s%s" % (
        b["reg"], b["addr"], ptype,
        v_sram if v_sram else 0, p_sram if p_sram else 0,
        "0x%X" % upper[0] if upper else "-",
        "0x%X" % lower[0] if lower else "-",
        zero if zero else "-", spec))

# 字面量地址 → SRAM 值 全表（用于把 flash 字面量换算成 SRAM 参数槽）
lits = set()
for b in branches:
    body = branch_body(b["addr"])
    for t in body:
        for m in re.finditer(r"\[0x([0-9a-f]{4,6})\]", t):
            lits.add(int(m.group(1), 16))
out.append("")
out.append("涉及字面量 → SRAM 值：")
for lit in sorted(lits):
    v = load(lit)
    out.append("  flash 0x%05X = 0x%08X" % (lit, v if v else 0))

open(ROOT + r"\tools\_modbus_branches.txt", "w", encoding="utf-8").write("\n".join(out))
print("\n".join(out))
print("\n== 已写入 tools/_modbus_branches.txt ==")
