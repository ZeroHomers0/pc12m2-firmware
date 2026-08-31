# -*- coding: utf-8 -*-
# extract_ram_data_image_12.py — PC12M-2（12 相）IAR 压缩 .data 解压 → 落盘镜像
# 对标 6p extract_ram_data_image.py；12p 差异：
#   * copy table @0x113d8（8 words 两段）：
#       段1 SRC=0x113F8 DST=0x10000000 END=0x2110 ENTRY=0x100（LZ 解压器）
#       段2 SRC=0x12C68 DST=0x10002110 END=0x890 ENTRY=0x164（零填充器 .bss 清零）
#   * .data 初始镜像 = 段1 解压结果 0x2110 bytes；段2 由 startup 清零（不入镜像）
#   * 定位依据：FUN_000000cc adr r0,#0x28(imm8=0x0A,word偏移)=r0=0xf8,
#     flash[0xf8]=0x112e0 → copy table=0x113d8；镜像 END 0x29A0=_estack-0x10000000
# 输出: firmware/assets/ram_data_image.bin + 校验报告
import sys, json
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
ROOT = Path(__file__).resolve().parents[2]
b = (ROOT / "backup" / "pc12m2_orig.bin").read_bytes()
FLASH_LEN = len(b)
SRAM = bytearray(0x40000)

SRC, DST, END = 0x113F8, 0, 0x2110

s = SRC; d = DST
step = 0; bad_ref = 0; s_over = False
r3 = b[s]; s += 1
while d < DST + END:
    step += 1
    if step > 200000:
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
    if d >= DST + END: break
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
    if d >= DST + END: break
    r3 = b[s]; s += 1

print("解压完成: 步数=%d 回退越界=%d s溢出=%s d=0x%X/0x%X s_end=0x%X" %
      (step, bad_ref, s_over, d, END, s))

img = bytes(SRAM[:END])
(ROOT / "firmware/assets/ram_data_image.bin").write_bytes(img)
print("已写入 firmware/assets/ram_data_image.bin (%d 字节, .data 镜像 0x%X)" % (len(img), END))

# ── 对拍 165 命名映射已知初值 ──
def u32(off): return int.from_bytes(SRAM[off:off+4], 'little')
VARS = ROOT / "tools/_ghidra_proj/_pc12m2_verified_vars.json"
varsd = json.load(open(VARS, encoding="utf-8"))
name_of = {}
def merge(dd):
    for a, n in dd.items(): name_of[int(a, 16)] = n
nm = varsd.get("TIER3_12ONLY_NAMING", {})
for k in ("HIGH", "PLAUS", "NEW"):
    merge(nm.get(k, {}))
for k in ("HIGH", "NEW_DISCOVERED", "SAME_ADDR_PLAUSIBLE"):
    if k in varsd: merge(varsd[k])

print("\n--- 对拍命名变量初值（SRAM 区 < 0x2110）---")
import re
sel = [("comm_baud_table", "波特率表指针"), ("comm_baud_idx", "波特率索引"),
       ("startup_div", "启动分频"), ("out_fine", "输出精调"),
       ("pid_kp2", "PID 参数区"), ("softstart_angle", "软启动角"),
       ("pid_feedback", "PID 反馈"), ("menu_state", "菜单状态")]
ok = 0
for key, label in sel:
    addrs = [a for a in name_of if name_of[a] == key]
    if not addrs:
        print("  %-18s %s: 未在命名映射（跳过）" % (key, label)); continue
    a = addrs[0]; off = a - 0x10000000
    if off >= END:
        print("  %-18s %s: 0x%05X 在 .bss 区(>=0x2110)，不入镜像" % (key, label, off)); continue
    v = u32(off)
    flag = "✓" if v not in (0, 0xFFFFFFFF) else "?"
    if v: ok += 1
    print("  %-18s %s: 0x%05X = %08X %s" % (key, label, off, v, flag))

# 镜像结构摘要
print("\n--- 镜像结构 ---")
print("  首16字节: %s" % " ".join("%02X" % x for x in img[:16]))
print("  镜像长度: 0x%X (%d)  .bss 区: 0x%X-0x29A0 (startup 清零)" % (END, END, END))
print("  6p 对照: 0x213C  12p 镜像较短 %d 字节" % (0x213C - END))
