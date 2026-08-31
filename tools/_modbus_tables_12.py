import re, json

DUMPDIR = 'evidence/reverse/disassembly/functions'
POOLMAP = json.load(open('tools/_ghidra_proj/_pc12m2_pool_sram_map.json', encoding='utf-8'))

def read_dump(fn):
    return open(f'{DUMPDIR}/{fn}', encoding='utf-8', errors='replace').read().splitlines()

def parse_tbb(fn, default_jump):
    lines = read_dump(fn)
    # code lines: (addr, code, dump_idx)
    code = []
    for di, l in enumerate(lines):
        m = re.match(r'^([0-9a-f]{8})  (.*)$', l)
        if m:
            code.append((int(m.group(1), 16), m.group(2), di))
    by_addr = {a: i for i, (a, _, _) in enumerate(code)}
    # 收集 tbb 后的 jump 目标（按顺序 = idx 0..）
    jumps = []
    tbb_seen = False
    for l in lines:
        if 'tbb  [pc,r4]' in l:
            tbb_seen = True
            continue
        if tbb_seen and l.strip().startswith('; jump ->'):
            jumps.append(re.search(r'0x[0-9a-f]{8}', l).group(0))
        elif tbb_seen and l.strip() and not l.strip().startswith(';'):
            # 注释行结束后不一定是 tbb 结束——继续收集直到非 jump 注释
            pass
    # 简化：只收集 tbb 行之后紧邻的连续 jump 注释
    jumps = []
    tbb_idx = next((i for i, l in enumerate(lines) if 'tbb  [pc,r4]' in l), None)
    if tbb_idx is None:
        return []
    i = tbb_idx + 1
    while i < len(lines) and lines[i].strip().startswith('; jump ->'):
        jumps.append(re.search(r'0x[0-9a-f]{8}', lines[i]).group(0))
        i += 1
    # 每个 jump 目标块：跟随 b 链到非 default_jump 的终点，找 slot ref + strb/str
    def resolve(addr, depth=0):
        pat = addr.replace('0x', '') + ' '
        j = next((k for k, c in enumerate(code) if f'{int(addr,16):08x}  ' in c[1].split(';')[0] or f'{int(addr,16):08x}' == c[1][:8]), None)
        # 精确: 找 addr 行
        j = by_addr.get(int(addr, 16))
        if j is None:
            return (None, None)
        # 跟随无条件 b 链（除 default_jump）
        m = re.search(r'\bb\s+(0x[0-9a-f]{8})', code[j][1])
        if m and m.group(1) != default_jump:
            if depth > 4:
                return (None, None)
            return resolve(m.group(1), depth + 1)
        # 在块内找 slot ref 与 str/strb
        tgt = None; width = None
        for q in range(j, min(j + 8, len(code))):
            mm = re.search(r'ldr\s+r4, \[(0x[0-9a-f]{8})\]', code[q][1])
            if mm:
                sl = mm.group(1).upper()
                tgt = POOLMAP.get(sl.lower()) or POOLMAP.get(sl)
                if tgt:
                    tgt = tgt.upper()
                else:
                    tgt = '0x' + sl.replace('0X', '')
            if 'strb' in code[q][1] and 'strb.w' not in code[q][1]:
                width = 'B'
            if re.search(r'\bstr\s+r3', code[q][1]):
                if width is None:
                    width = 'W'
        return (tgt, width)
    out = []
    for idx, jp in enumerate(jumps):
        tgt, width = resolve(jp, 0)
        out.append({'idx': idx, 'jump': jp, 'tgt': tgt, 'w': width})
    return out

rm = parse_tbb('0000ad04_FUN_0000ad04.txt', '0x0000b04c')
wm = parse_tbb('0000b050_FUN_0000b050.txt', '0x0000b3ae')

json.dump({'read': rm, 'write_multi': wm}, open('tools/_modbus_tables_12.json', 'w', encoding='utf-8'), indent=1)
print('read entries:', len(rm), ' write_multi entries:', len(wm))
for i, r in enumerate(rm):
    w = wm[i] if i < len(wm) else None
    rd  = '=0' if r['tgt'] is None else f"{r['tgt']}{r['w']}"
    wd  = '-' if not w or w['tgt'] is None else f"{w['tgt']}{w['w']}"
    d = []
    if w and w['tgt'] is not None and w['tgt'] != r['tgt'] and r['tgt'] is not None:
        d.append('RWdiff')
    print(f"idx{i:>2} (reg{i+1:>2}) R={rd:>14} W={wd:>14} {' '.join(d)}")
