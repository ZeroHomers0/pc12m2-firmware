# -*- coding: utf-8 -*-
"""rename_locals.py — 按函数边界做局部变量/参数重命名（L0 语义化）

安全前提：只替换指定函数【函数体】内的标识符，不做全局替换，绝不改 DAT_ 符号、
类型、或控制流。用于把 param_N / uVar / iVar 等反编译名换成语义名。
用法：python tools/rename_locals.py <file> "<func> <old>=<new> [,<old2>=<new2>...]"
例：  python tools/rename_locals.py firmware/src/08_uart3_modbus.c crc16 param_1=data,param_2=len,uVar2=crc_lo
"""
import sys, re

def find_func_body(text, name):
    """定位函数体（签名行首 -> 匹配括号深度归零的 '}'）。返回 (body_start, body_end_exclusive)。
    依赖函数签名行以 '<ret> name(' 开头且函数体顶层括号平衡。"""
    # 用正则匹配签名行：可选限定符 + 返回类型 + 空格 + name + '('（非 ');' 声明）
    pat = re.compile(
        r'(?m)^[ \t]*[A-Za-z_][A-Za-z0-9_]*[ \t]+'
        + re.escape(name) + r'\s*\(')
    m = pat.search(text)
    if not m:
        return None
    # 从 'name(' 的左括号开始往下数括号深度
    open_paren = m.end() - 1          # 指向 '('
    depth = 0
    i = open_paren
    n = len(text)
    while i < n:
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                break
        i += 1
    # 现在 i 指向签名右括号。从函数体 '{' 数到配对的 '}'
    body_start = i + 1
    while body_start < n and text[body_start] not in '{}':
        body_start += 1
    if text[body_start] != '{':
        return None
    depth = 0
    j = body_start
    while j < n:
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                return (m.start(), j + 1)   # 从签名行首到配对 '}'（含参数声明）
        j += 1
    return None

def main():
    if len(sys.argv) < 3:
        print(__doc__); return 1
    path = sys.argv[1]
    name = sys.argv[2]
    mapping = {}
    for pair in sys.argv[3].split(','):
        old, new = pair.split('=')
        mapping[old] = new
    src = open(path, encoding='utf-8').read()
    span = find_func_body(src, name)
    if not span:
        print('未找到函数体:', name); return 1
    body_start, body_end = span
    body = src[body_start:body_end]
    new_body = body
    for old, new in mapping.items():
        # 词边界替换（\b），只在本函数体内
        new_body = re.sub(r'\b' + re.escape(old) + r'\b', new, new_body)
    # 只统计实际替换次数
    cnt = sum(len(re.findall(r'\b' + re.escape(o) + r'\b', body)) for o in mapping)
    src2 = src[:body_start] + new_body + src[body_end:]
    open(path, 'w', encoding='utf-8').write(src2)
    print('%s: %s 函数体替换 %d 处 -> %s' % (path, name, cnt, ','.join('%s=%s' % (k, v) for k, v in mapping.items())))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
