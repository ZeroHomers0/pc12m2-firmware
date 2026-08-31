# -*- coding: utf-8 -*-
"""verify_strpool.py — 无硬件仿真：验证 12p strpool_map 字符串映射正确性（PC12M-2）

对照原始 backup/pc12m2_orig.bin，验证 firmware/src/strpool.c 的 blob 簇映射：
  1) 从 firmware/src/*.c 提取全部 disp_string(0x...) 实参（flash 字符串地址）；
  2) 对每个地址复现 strpool_map：应命中某簇，且 blob 段 == 原 BIN 同地址段（GBK 串到 \0）。
用法：cd PC12M-2 && python tools/verification/verify_strpool.py
"""
import re
import glob
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
orig = (ROOT / "backup" / "pc12m2_orig.bin").read_bytes()

src = (ROOT / "firmware" / "src" / "strpool.c").read_text(encoding="utf-8", errors="ignore")

# blob 十六进制字面量（\xNN）
byts = re.findall(r'\\x([0-9a-f]{2})', src)
blob = bytes(int(x, 16) for x in byts)
print('blob 字节数:', len(blob))

# 簇表三元组 (base, len, blob_offset)
clusters = []
for m in re.finditer(r'\{(\d+), (\d+), strpool_blob \+ (\d+)\}', src):
    clusters.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
print('簇数:', len(clusters), ' 簇表总 len:', sum(c[1] for c in clusters))


def strpool_map(addr):
    """复现 C 版 strpool_map：命中簇返回 blob 段 + 簇信息"""
    for base, ln, boff in clusters:
        if base <= addr < base + ln:
            off = addr - base
            return blob[boff + off:], (base, ln, boff + off)
    return None, None


# 从 12p 源码提取 disp_string 的 flash 字符串实参（0x 开头的常量地址）
SRC_FILES = [p for p in glob.glob(str(ROOT / "firmware" / "src" / "*.c"))
             if not p.endswith(("strpool.c", "crc16_table.c"))]
addrs = []
for p in SRC_FILES:
    text = Path(p).read_text(encoding="utf-8", errors="ignore")
    # disp_string(<0x 常量>, row, col, attr) — 首参为 flash 地址
    for m in re.finditer(r'disp_string\(\s*0x([0-9a-fA-F]+)', text):
        a = int(m.group(1), 16)
        if a < 0x40000:  # flash 范围
            addrs.append(a)
addrs = sorted(set(addrs))
print('源码 disp_string flash 实参去重:', len(addrs))

def seg_len(b, start):
    """从 b[start:] 到下一个 \\0（不含），上限 64；无 \\0 取 64"""
    lim = b.find(b'\x00', start)
    if lim < 0:
        return 64
    return min(lim - start, 64)

fails = []
pool_out = []
n_ok = 0
for addr in addrs:
    ptr, info = strpool_map(addr)
    if ptr is None:
        # 池外字符串：C 版 strpool_map 未命中返回 addr 原样（flash 直读，合法）。
        # 校验 BIN 该地址处确实有非零 GBK 内容，避免写入空/越界地址。
        seg = orig[addr:addr + 8]
        if len(seg) < 1 or seg.strip(b'\x00') == b'':
            fails.append((addr, '池外但 BIN 处为空', None))
        else:
            pool_out.append(addr)
        continue
    n = seg_len(ptr, 0)
    blob_seg = ptr[:n]
    bin_seg = orig[addr:addr + n]
    if blob_seg != bin_seg:
        fails.append((addr, 'blob≠BIN', (blob_seg.hex(), bin_seg.hex())))
    else:
        n_ok += 1

print('\n== 12p strpool_map 全量核验 ==')
print('池内命中且一致:', n_ok, '/', len(addrs))
print('池外直读(预期,flash 原样):', len(pool_out), sorted('0x%05x' % a for a in pool_out))
if fails:
    print('异常 (%d):' % len(fails))
    for addr, why, detail in fails[:40]:
        print('  0x%05x %s %s' % (addr, why, '' if not detail else 'blob=%s bin=%s' % detail))
else:
    print('全部 PASS')

print('\n==== 12p strpool_map 验证:', '全部 PASS' if not fails else '存在 FAIL', '====')
