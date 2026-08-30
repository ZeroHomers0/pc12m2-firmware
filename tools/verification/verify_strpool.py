# -*- coding: utf-8 -*-
"""verify_strpool.py — 无硬件仿真：验证 strpool_map 字符串映射正确性

对照原始 LPC1765.bin，验证 strpool blob 对关键 flash 字符串地址的映射，
尤其是 W7b 修正的 3 个单位字符地址（0x7974/'V'、0x7980/'A'、0x86e0/'%'）。
用法：cd decompiled && python tools/verify_strpool.py
"""
import re

orig = open('LPC1765.bin', 'rb').read()
src = open('firmware/src/strpool.c', encoding='utf-8', errors='ignore').read()

# blob 十六进制字面量（\xNN）
byts = re.findall(r'\\x([0-9a-f]{2})', src)
blob = bytes(int(x, 16) for x in byts)
print('blob 字节数:', len(blob))

# 簇表三元组 (base, len, blob_offset)，例如 {18396, 197, strpool_blob + 152}
clusters = []
for m in re.finditer(r'\{(\d+), (\d+), strpool_blob \+ (\d+)\}', src):
    clusters.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
print('簇数:', len(clusters), ' 簇表总 len:', sum(c[1] for c in clusters))
assert len(clusters) == 20

def strpool_map(addr):
    """复现 C 版 strpool_map：命中簇返回簇内 blob 指针 + 偏移"""
    for base, ln, boff in clusters:
        if base <= addr < base + ln:
            off = addr - base
            return blob[boff + off:], (boff, off)
    return None, None

def gbk(s):
    return s.encode('gbk')

tests = [
    (0x754,   '故障状态'),   # 启动 splash 首串（第一簇 base=0x754）
    (0x47dc,  '故障'),       # 状态/菜单 '故障'（第三簇 base=0x47dc）
    (0x7974,  'V'),          # W7b 修正: 单位 V
    (0x7980,  'A'),          # W7b 修正: 单位 A
    (0x86e0,  '%'),          # W7b 修正: 单位 %
]
print('\n== strpool_map 映射（对照原 bin）==')
allok = True
for addr, expect in tests:
    ptr, info = strpool_map(addr)
    seg = ptr[:len(gbk(expect))]
    orig_seg = orig[addr:addr + len(gbk(expect))]
    # 映射结果应等于原 bin 同地址内容
    ok1 = seg == gbk(expect)
    ok2 = seg == orig_seg
    ok = ok1 and ok2
    allok &= ok
    print('0x%04x: 映射[blob@%s] %s GBK=%s | 原bin %s | 匹配=%s' %
          (addr, str(info), seg.hex(), seg.decode('gbk', errors='replace'),
           orig_seg.hex(), 'PASS' if ok else 'FAIL'))

# RAM/外设地址应原样返回（未命中）
for ra in (0x100015cc, 0x40004000):
    ptr, info = strpool_map(ra)
    print('RAM/外设 0x%08x 未命中(原样): %s' % (ra, 'PASS' if info is None else 'FAIL'))
    allok &= (info is None)

print('\n==== strpool_map 验证:', '全部 PASS' if allok else '存在 FAIL', '====')
