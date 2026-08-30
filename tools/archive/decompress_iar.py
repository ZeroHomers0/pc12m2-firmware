# -*- coding: utf-8 -*-
# IAR __iar_copy_data 压缩解压器（0x100-0x162 权威反汇编精确推导）
# 2026-08-21 修正：重复循环 0x156 为 bpl（带符号），r3 从 r5+1 减到 -1 退出
#   => 重复次数 = r5 + 2（非 r5+1）
# 用法: python decompress_iar.py [repeat_mode]
#   repeat_mode: bpl(默认, r5+2) / plus1(r5+1) / plus0(r5)
import sys

b = bytearray(open(r"D:\code\LPC1765FBD100\decompiled\LPC1765.bin","rb").read())
FLASH_LEN = len(b)
SRAM = bytearray(0x40000)      # 目标缓冲
SRC = 0x11254                  # 压缩流源
DST = 0x0000                   # 目标基址（相对 0x10000000）
END = 0x213C                   # 目标长度

mode = sys.argv[1] if len(sys.argv) > 1 else "bpl"
def rep_cnt(r5):
    if mode == "bpl":   return r5 + 2
    if mode == "plus1": return r5 + 1
    return r5

s = SRC; d = 0
step = 0; bad_ref = 0; s_over = False
r3 = b[s]; s += 1
while d < END:
    step += 1
    if step > 50000:
        print("循环超限 step=%d d=0x%X s=0x%X" % (step, d, s)); break
    # 低2位 → 字面拷贝数（0 则读扩展字节）
    r4 = r3 & 3
    if r4 == 0:
        if s >= FLASH_LEN: s_over = True; break
        r4 = b[s]; s += 1
    # 高4位 → 重复数（0 则读扩展字节）
    r5 = r3 >> 4
    if r5 == 0:
        if s >= FLASH_LEN: s_over = True; break
        r5 = b[s]; s += 1
    # 字面拷贝 r4-1 字节（0x118 subs #1 后 bne 循环）
    r4 -= 1
    while r4 != 0:
        if s >= FLASH_LEN: s_over = True; break
        if d >= len(SRAM): print("SRAM溢出 d=0x%X" % d); sys.exit(1)
        SRAM[d] = b[s]; s += 1
        r4 -= 1; d += 1
    if d >= END: break
    # 重复区（r5 != 0 才进入）
    if r5 != 0:
        if s >= FLASH_LEN: s_over = True; break
        rb = b[s]; s += 1                 # 回退低字节
        mid = r3 & 0xC
        if mid == 0xC:
            if s >= FLASH_LEN: s_over = True; break
            hi = b[s]; s += 1
            back = rb + (hi << 8)
        else:
            back = rb + (mid << 6)
        rp = d - back
        if rp < 0: bad_ref += 1
        cnt = rep_cnt(r5)
        for _ in range(cnt):
            v = SRAM[rp] if 0 <= rp < len(SRAM) else 0
            SRAM[d] = v; d += 1; rp += 1
            if d >= len(SRAM): print("SRAM溢出 d=0x%X" % d); sys.exit(1)
    if d >= END: break
    r3 = b[s]; s += 1

print("模式=%s 步数=%d 回退越界=%d s溢出=%s" % (mode, step, bad_ref, s_over))
print("最终 d=0x%X / 期望 0x%X  s=0x%X" % (d, END, s))
print("--- 波特率表 @0x100017BC ---")
FACTOR = [0x3BB, 0x3BB, 0x3BB, 0x3B6, 0x3B1, 0x3AA, 0x39D, 0x393]
STD = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
ok = 0
for i in range(8):
    c = int.from_bytes(SRAM[0x17BC+i*4:0x17BC+i*4+4], 'little')
    bd = 1500000000.0 / (c * FACTOR[i]) if c else 0
    best = min(STD, key=lambda s: abs(bd - s)) if bd else None
    hit = abs(bd-best)/best < 0.05 if best else False
    if hit: ok += 1
    print("  [%d] 0x%08X = %-6d → %12.1f  %s" % (i, c, c, bd, ("→%d ✓" % best) if hit else ""))
print("命中档位: %d/8" % ok)
