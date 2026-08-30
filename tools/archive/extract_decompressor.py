# -*- coding: utf-8 -*-
# 提取 Ghidra 反汇编落盘文件中的 0x100-0x164 区间（解压器权威反汇编）
import re, sys, json

path = sys.argv[1]
raw = open(path, encoding="utf-8").read()
# 文件是 JSON 数组: [{"type":"text","text":"..."}]
try:
    data = json.loads(raw)
    text = data[0]["text"]
except Exception:
    # 否则手动解析 "text" 字段
    i = raw.find('"text"')
    body = raw[raw.find(':', i) + 1:]
    # 逐字符解码转义
    out = []
    k = 0
    while k < len(body):
        c = body[k]
        if c == '"':
            break
        if c == '\\':
            nxt = body[k+1]
            if nxt == 'n':
                out.append('\n'); k += 2
            elif nxt == 't':
                out.append('\t'); k += 2
            elif nxt == '"':
                out.append('"'); k += 2
            elif nxt == '\\':
                out.append('\\'); k += 2
            elif nxt == 'u':
                out.append(body[k+1:k+6]); k += 6
            else:
                out.append(nxt); k += 2
        else:
            out.append(c); k += 1
    text = "".join(out)

lines = text.split("\n")
print("总行数:", len(lines))
lo, hi = 0xF8, 0x1C0
for ln in lines:
    m = re.match(r"^\s*(0x[0-9a-fA-F]+):", ln)
    if m:
        addr = int(m.group(1), 16)
        if lo <= addr <= hi:
            print(ln)
