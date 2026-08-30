# -*- coding: utf-8 -*-
# IAR 压缩 .data 解压 → 落盘镜像 + 对拍 Ghidra 已知 SRAM 初始常量
# 2026-08-21 修正确认：重复循环 0x156 bpl 语义 => 重复次数 = r5+2
# 输出: firmware/assets/ram_data_image.bin + 校验报告
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
b = bytearray((ROOT / "LPC1765.bin").read_bytes())
FLASH_LEN = len(b)
SRAM = bytearray(0x40000)
SRC = 0x11254; DST = 0; END = 0x213C

s = SRC; d = 0
step = 0; bad_ref = 0; s_over = False
r3 = b[s]; s += 1
while d < END:
    step += 1
    if step > 50000:
        print("循环超限 step=%d d=0x%X" % (step, d)); break
    r4 = r3 & 3
    if r4 == 0:
        if s >= FLASH_LEN: s_over = True; break
        r4 = b[s]; s += 1
    r5 = r3 >> 4
    if r5 == 0:
        if s >= FLASH_LEN: s_over = True; break
        r5 = b[s]; s += 1
    r4 -= 1
    while r4 != 0:
        if s >= FLASH_LEN: s_over = True; break
        SRAM[d] = b[s]; s += 1
        r4 -= 1; d += 1
    if d >= END: break
    if r5 != 0:
        rb = b[s]; s += 1
        mid = r3 & 0xC
        if mid == 0xC:
            hi = b[s]; s += 1
            back = rb + (hi << 8)
        else:
            back = rb + (mid << 6)
        rp = d - back
        if rp < 0: bad_ref += 1
        cnt = r5 + 2
        for _ in range(cnt):
            v = SRAM[rp] if 0 <= rp < len(SRAM) else 0
            SRAM[d] = v; d += 1; rp += 1
    if d >= END: break
    r3 = b[s]; s += 1

print("解压完成: 步数=%d 回退越界=%d s溢出=%s d=0x%X/0x%X s=0x%X" % (step, bad_ref, s_over, d, END, s))

# 落盘镜像（前 0x213C 字节）
img = bytes(SRAM[:0x213C])
(ROOT / "firmware/assets/ram_data_image.bin").write_bytes(img)
print("已写入 firmware/assets/ram_data_image.bin (%d 字节)" % len(img))

# ── 对拍 Ghidra 已知常量 ──
def u32(off): return int.from_bytes(SRAM[off:off+4],'little')
def u16(off): return int.from_bytes(SRAM[off:off+2],'little')
checks = [
    ("0xB024 波特率索引指针", 0xB024, 0x10001700, "=="),
    ("0xB028 波特率表指针",   0xB028, 0x100017BC, "=="),
    ("0xB02C PCLK基准",       0xB02C, 0x16E360,   "=="),
    ("0xB00C UART3基址",      0xB00C, 0x4009C000, "=="),
    ("0xB010 PCONP位掩码",    0xB010, 0x0,        "!="),
    ("0xB014 外设时钟寄存器", 0xB014, 0x400FC0C4, "=="),
    ("0xB01C 数据位索引",     0xB01C, 0x0,        "!="),
    ("0xB018 PCLKSEL寄存器",  0xB018, 0x4009C000, "!="),
]
print("\n--- 对拍 UART3 指针表 (DATA_SEGMENT §5) ---")
ok = 0
for name, off, expect, op in checks:
    v = u32(off)
    match = (v == expect) if op == "==" else (v != expect)
    if match: ok += 1
    print("  %s: 0x%08X %s (期望 %s)%s" % (name, v, "✓" if match else "✗", hex(expect), "" if match else "  ⚠"))
print("UART3 表对拍: %d/%d" % (ok, len(checks)))

print("\n--- 波特率表 0x100017BC ---")
for i in range(8):
    print("  [%d] 0x%08X = %d" % (i, u32(0x17BC+i*4), u32(0x17BC+i*4)))

print("\n--- 其他已知全局 ---")
for name, off in [("0x1000EDF4 保护位域", 0xEDF4),
                  ("0x10001624 菜单控制", 0x1624),
                  ("0x10001744 菜单状态标志", 0x1744),
                  ("0x10001700 参数RAM区", 0x1700)]:
    print("  %s: 前4字节 %s 前16字节 %s" % (name,
        " ".join("%02X"%x for x in SRAM[off:off+4]),
        " ".join("%02X"%x for x in SRAM[off:off+16])))
