# 从 12p dispatch 反汇编生成写单块权威规格（修正 v3：统一用 dump 全行索引）
import re, json

DUMP = r'D:\code\PC12M-2\evidence\reverse\disassembly\functions\0000b3b2_FUN_0000b3b2.txt'
dump = open(DUMP, encoding='utf-8', errors='replace').read().splitlines()
code = []   # [(addr, code, dump_idx)]
for di, l in enumerate(dump):
    m = re.match(r'^([0-9a-f]{8})  (.*)$', l)
    if m:
        code.append((int(m.group(1), 16), m.group(2), di))
N = len(code)
by_addr = {a: i for i, (a, _, _) in enumerate(code)}

def next_code(di):
    """从 dump 行 di 起找到下一条代码行的 code 索引"""
    for k in range(di + 1, len(dump)):
        m = re.match(r'^([0-9a-f]{8})  (.*)$', dump[k])
        if m:
            return by_addr[int(m.group(1), 16)]
    return None

def fwd(ci, pat, limit=6):
    """从 code 索引 ci 起向后(最多 limit 条代码行)找匹配 pat 的 code 索引"""
    for q in range(ci + 1, min(ci + limit, N)):
        if re.search(pat, code[q][1]):
            return q
    return None

def ref_near(di, addr_lo, addr_hi):
    """在 dump 行 di 附近(±6 行)找 'ref 0x... -> 0x1000...' 最近的映射"""
    best = None
    for p in range(di, max(di - 7, 0) - 1, -1):
        mr = re.search(r'ref\s+0x[0-9a-f]+ -> (0x1000[0-9a-f]+)', dump[p])
        if mr:
            best = mr.group(1).upper()
            break
    return best

# 定位块
raw = []
i = 0
while i < N - 6:
    if not re.search(r'ldrb\s+r0, \[r0,#0x2\]', code[i][1]):
        i += 1
        continue
    j = fwd(i, r'cmp\s+r0, #0x10')
    if j is None:
        i += 1
        continue
    k = fwd(j, r'ldrb\s+r0, \[r0,#0x3\]')
    if k is None:
        i += 1
        continue
    l = fwd(k, r'cmp\s+r0, #0x([0-9a-f]+)')
    if l is None:
        i += 1
        continue
    m = re.search(r'cmp\s+r0, #0x([0-9a-f]+)', code[l][1])
    if not m:
        i += 1
        continue
    reg = int(m.group(1), 16)
    if not (1 <= reg <= 0x3e):
        i += 1
        continue
    r = fwd(l, r'bne\s+(0x[0-9a-f]{8})')
    if r is None:
        i += 1
        continue
    mb = re.search(r'bne\s+(0x[0-9a-f]{8})', code[r][1])
    if not mb:
        i += 1
        continue
    skip = int(mb.group(1), 16)
    raw.append({'reg': reg, 'bi': i, 'skip': skip})
    i = l + 1

seen = set()
blocks = []
for b in raw:
    if b['reg'] not in seen:
        seen.add(b['reg'])
        blocks.append(b)

def block_spec(b):
    reg = b['reg']
    bi = b['bi']
    si = by_addr.get(b['skip'])
    win_end = si if si is not None else bi + 200
    # 目标存储：协议值存后首个非 0x100017d0/0x100017d4 的 str/strb r0,[r1,#0]
    tgt, width, store_i = None, None, None
    for q in range(bi, min(win_end, N)):
        if not re.search(r'\bstrb?\s+r0, \[r1,#0x0\]', code[q][1]):
            continue
        ref = ref_near(code[q][2], code[q][0], code[q][0])
        if ref in ('0X100017D0', '0X100017D4'):
            continue
        tgt, width = ref, 'B' if 'strb' in code[q][1] else 'W'
        store_i = q
        break
    # 存前边界
    bounds = []
    movw = {}
    end_b = store_i if store_i is not None else win_end
    for q in range(bi, min(end_b, N)):
        c = code[q][1]
        mm = re.search(r'\bmovw\s+r(\d+),\s*#0x([0-9a-f]+)', c)
        if mm:
            movw[int(mm.group(1))] = int(mm.group(2), 16)
        mz = re.search(r'\b(cbz|cbnz)\s+r0,\s*(0x[0-9a-f]{8})', c)
        if mz:
            t = int(mz.group(2), 16)
            if t != b['skip']:
                bounds.append({'op': 'cbz' if mz.group(1) == 'cbz' else 'cbnz', 'imm': 0, 'err': hex(t)})
            continue
        mc = re.search(r'\bcmp\s+r0,\s*#0x([0-9a-f]{1,5})', c)
        imm = None
        if mc:
            imm = int(mc.group(1), 16)
        else:
            mcn = re.search(r'\bcmp\s+r0,\s*r(\d+)$', c)
            if mcn and int(mcn.group(1)) in movw:
                imm = movw[int(mcn.group(1))]
        if imm is None:
            continue
        nxt = next_code(code[q][2])
        if nxt is None or nxt >= min(end_b, N):
            continue
        mb2 = re.search(r'\b(bcs|bls|bhi|blo|bhs|bne|beq|bcc|blt|bgt|ble|bge)\b\s+(0x[0-9a-f]{8})', code[nxt][1])
        if mb2:
            t = int(mb2.group(2), 16)
            if t != b['skip']:
                bounds.append({'op': 'cmp', 'cond': mb2.group(1), 'imm': imm, 'err': hex(t)})
    # 复制块：store 后到 ps 前其它 0x1000 目标写
    copy = []
    if store_i is not None:
        for q in range(store_i + 1, min(win_end, N)):
            if 'bl  0x00003534' in code[q][1]:
                break
            if re.search(r'\bstrb?\s+r\d+,\s*\[r\d+,#0x0\]', code[q][1]):
                r2 = ref_near(code[q][2], 0, 0)
                if r2 and r2 not in ('0X100017D0', '0X100017D4', '0X10002340'):
                    copy.append(r2)
    ps = any('bl  0x00003534' in code[q][1] for q in range(bi, min(win_end, N)))
    i2c = any('bl  0x00001e38' in code[q][1] for q in range(bi, min(win_end, N)))
    dead = None
    for q in range(bi, min(win_end, N)):
        m = re.search(r'\bb\s+(0x[0-9a-f]{8})', code[q][1])
        if m and int(m.group(1), 16) == code[q][0]:
            dead = hex(code[q][0])
    echo = None
    for q in range((store_i or bi) + 1, min(win_end, N)):
        if re.search(r'\bldrh\s+r0, \[r0,#0x0\]', code[q][1]):
            echo = 'ldrh'
            break
        if re.search(r'\bldrb\s+r0, \[r0,#0x0\]', code[q][1]) and q + 1 < N and re.search(r'\basrs\s+r1, r0, #0x1f', code[q + 1][1]):
            echo = 'ldrb'
            break
    return {'reg': reg, 'tgt': tgt, 'width': width, 'bounds': bounds,
            'copy': copy, 'ps': ps, 'i2c': i2c, 'dead': dead, 'echo': echo}

spec = [block_spec(b) for b in blocks]
spec.sort(key=lambda x: x['reg'])
json.dump(spec, open(r'D:\code\PC12M-2\tools\_w6_spec.json', 'w', encoding='utf-8'), indent=1)
for s in spec:
    bs = ' '.join(f"{b['op']}{b.get('cond','')}@{b['imm']}(->{b['err']})" for b in s['bounds'])
    print(f"reg{s['reg']:>3} tgt={s['tgt']}{s['width']} bounds=[{bs}] copy={s['copy']} "
          f"ps={s['ps']} echo={s['echo']}{' DEAD' if s['dead'] else ''}{' I2C' if s['i2c'] else ''}")
