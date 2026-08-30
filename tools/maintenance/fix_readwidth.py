# fix_readwidth.py — 把 globals.c/h 里指向 byte 槽但定义为 uint32_t* 的符号改为 uint8_t*
# 幂等 + 全局匹配（处理 gen_globals 挤在同一行的多符号定义）
import re

BYTE_ADDRS = set("""
0x10001634 0x1000164c 0x1000164d 0x10001654 0x10001655 0x10001656 0x10001657
0x10001658 0x10001659 0x1000165a 0x1000165b 0x10001664 0x1000167c 0x1000167d
0x10001684 0x10001685 0x10001686 0x10001687 0x10001688 0x10001689 0x1000168a
0x1000168b 0x10001694 0x10001695 0x100016c4 0x100016cc 0x100016d4 0x100016dc
0x100016dd 0x100016de 0x100016e4 0x100016ec 0x100016f4 0x100016fc 0x100016fd
0x100016fe 0x100016ff 0x10001704 0x10001705 0x10001706 0x1000170c 0x1000170d
0x1000170e 0x1000170f 0x10001710 0x10001711 0x10001712 0x10001713 0x10001714
0x10001715 0x10001716 0x10001717 0x10001718 0x10001719 0x1000171a 0x1000171b
0x1000171c 0x1000171d 0x1000171e 0x1000171f 0x10001720 0x10001721 0x10001722
0x10001724 0x10001725 0x10001726 0x10001727 0x10001728 0x10001729 0x1000172a
0x1000172b
""".split())

from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
GC = ROOT / "firmware/globals.c"
GH = ROOT / "firmware/inc/globals.h"

def norm(s):
    return s.strip().lower()

# 1) 反推所有指向 byte 地址的符号（全局 findall，含挤在一行的）
content = open(GC, encoding="latin-1").read()
gfind = re.compile(r'volatile\s+(uint32_t|uint8_t)\s+\*\s*(\w+)\s*=\s*\((uint32_t|uint8_t)\s*\*\)(0x[0-9a-fA-F]+)')
byte_syms = set()
for m in gfind.finditer(content):
    if norm(m.group(4)) in BYTE_ADDRS:
        byte_syms.add(m.group(2))

# 2) 改 globals.c：byte 符号 uint32_t -> uint8_t（全局替换，含注释）
def gsub(m):
    if m.group(2) in byte_syms and m.group(1) == "uint32_t":
        return "volatile uint8_t *%s = (uint8_t *)%s" % (m.group(2), m.group(4))
    return m.group(0)
content = gfind.sub(gsub, content)
open(GC, "w", encoding="latin-1").write(content)

# 3) 改 globals.h：byte_syms 的 extern uint32_t* -> uint8_t*
hcontent = open(GH, encoding="latin-1").read()
hfind = re.compile(r'extern\s+volatile\s+(uint32_t|uint8_t)\s+\*\s*(\w+)\s*;')
def hsub(m):
    if m.group(2) in byte_syms and m.group(1) == "uint32_t":
        return "extern volatile uint8_t *%s;" % m.group(2)
    return m.group(0)
hcontent = hfind.sub(hsub, hcontent)
open(GH, "w", encoding="latin-1").write(hcontent)

print("byte 地址符号共", len(byte_syms), "个")
