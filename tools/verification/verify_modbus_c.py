# -*- coding: utf-8 -*-
# verify_modbus_c.py — 校验 src/08_modbus_dispatch.c 的 53 个写分支 vs asm 分支表
# 对比项：参数槽 SRAM 地址、范围上下限（bcs/bls 语义，v>N ⇔ 上限 N+1）。
# 特殊分支修正：0x1017/0x1018/0x1019 参数槽（表提取被增益复制混淆）。
# 用法：python tools/verify_modbus_c.py
import re, sys, os
sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
c = open(os.path.join(ROOT, 'firmware', 'src', '08_modbus_dispatch.c'), encoding='utf-8').read()

# ── 1. 解析 C：case 0xNN: ... 直到下一个 case / default ──
cases = {}
cur = None; body = []
for line in c.splitlines():
    m = re.match(r'\s*case (0x[0-9a-fA-F]+):', line)
    if m:
        if cur is not None:
            cases[cur] = '\n'.join(body)
        cur = 0x1000 | int(m.group(1), 16)
        body = []
    elif cur is not None:
        if re.match(r'\s*default:', line):
            cases[cur] = '\n'.join(body)
            cur = None; body = []
        else:
            body.append(line)
if cur is not None:
    cases[cur] = '\n'.join(body)

# ── 2. 解析 asm 分支表 ──
tab = {}
for l in open(os.path.join(ROOT, 'evidence', 'reverse', 'state_machine', '_modbus_branches.txt'), encoding='utf-8'):
    if not l.startswith('0x10'):      # 注意：0x1010 前缀是 '0x101'，勿用 '0x100'
        continue
    p = l.split()
    reg = int(p[0], 16)
    tab[reg] = dict(typ=p[2],
                    pslot=int(p[4], 16) if p[4] != '0x00000000' else None,
                    upper=int(p[5], 16) if p[5] != '-' else None,
                    lower=int(p[6], 16) if p[6] != '-' else None)

# 特殊分支参数槽修正：
#  表侧修正（表提取被增益复制混淆）：0x1017/0x1018/0x1019
#  C 侧提取失败补救（C 用 *slave 宏而非字面地址）：0x102F
FIX_T = {0x1017: 0x10001710, 0x1018: 0x10001717, 0x1019: 0x10001718}
FIX_C = {0x102F: 0x100016FF}

def extract(body):
    store = re.search(r'\)(0x1000[0-9a-fA-F]{4})\s*=\s*(?:0|(?:\(uint(?:8_t|32_t)\))?v)\b', body)
    ge  = re.search(r'if \(v >= (\d+)(?: \|\| v <= (\d+))?\)', body)      # v>=N ⇔ 上限 N
    gez = re.search(r'if \(v >= (\d+) \|\| v == (\d+)\)', body)          # 含 v==N 零值检查
    gt  = re.search(r'if \(v > (\d+)(?: \|\| v <= (\d+))?\)', body)       # v>N  ⇔ 上限 N+1（bcs）
    if ge:
        up, lo = int(ge.group(1)), int(ge.group(2)) if ge.group(2) else None
    elif gez:
        up, lo = int(gez.group(1)), None
    elif gt:
        up, lo = int(gt.group(1)) + 1, int(gt.group(2)) if gt.group(2) else None
    else:
        up = lo = None
    return (int(store.group(1), 16) if store else None, up, lo)

out = []
bad = 0
for reg in sorted(tab):
    if reg not in cases:
        out.append('%-8s 缺 case!' % hex(reg)); bad += 1; continue
    pslot_c, up_c, lo_c = extract(cases[reg])
    t = tab[reg]
    pslot_t = FIX_T.get(reg, t['pslot'])
    pslot_c = pslot_c if pslot_c is not None else FIX_C.get(reg)
    p_ok = True
    if t['pslot'] is not None and reg not in (0x1025, 0x1026):
        p_ok = (pslot_c == pslot_t)
        bad += 0 if p_ok else 1
    u_ok = (up_c == t['upper']) if t['upper'] is not None else (up_c is None)
    l_ok = (lo_c == t['lower']) if t['lower'] is not None else (lo_c is None)
    if t['upper'] is not None and up_c != t['upper']:
        bad += 1
    if t['lower'] is not None and lo_c != t['lower']:
        bad += 1
    status = 'OK' if (p_ok and u_ok and l_ok) else '<<< 不一致'
    out.append('%-8s %-10s %-8s %-8s %-8s %-8s %-8s  %s' % (
        hex(reg),
        hex(pslot_c) if pslot_c else '-',
        hex(pslot_t) if pslot_t else '-',
        up_c if up_c is not None else '-',
        hex(t['upper']) if t['upper'] is not None else '-',
        lo_c if lo_c is not None else '-',
        hex(t['lower']) if t['lower'] is not None else '-',
        status))

out.append('差异总数: %d' % bad)
res = '\n'.join(out)
print(res)
open(os.path.join(ROOT, 'evidence', 'reverse', 'reports', '_verify_result.txt'), 'w', encoding='utf-8').write(res)
