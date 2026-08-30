# check_readwidth.py — 对照反汇编 strb/str 与 globals.c 类型，找出"byte 槽被定义为 word"的符号
import re, sys

# byte 地址集合（load_config + param_sync 反汇编 strb 目标，合并去重）
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

# 读 globals.c
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
with open(ROOT / "firmware/globals.c", encoding="latin-1") as f:
    lines = f.readlines()

# 匹配：volatile uint32_t *DAT_xxx = (uint32_t *)0xADDR;  (可能 uint8_t)
pat = re.compile(r'^(\s*)(volatile\s+)(uint32_t|uint8_t)(\s+\*DAT_\w+\s*=\s*\()(uint32_t|uint8_t)(\s*\*\))0x([0-9a-fA-F]+)')

byte_def_as_word = []   # 需要 word->byte 的
for i, line in enumerate(lines, 1):
    m = pat.match(line)
    if not m:
        continue
    typ = m.group(3)
    addr = "0x" + m.group(7).lower()
    if addr in BYTE_ADDRS and typ == "uint32_t":
        byte_def_as_word.append((i, line.rstrip("\n"), addr))

print(f"=== 需要 uint32_t* -> uint8_t* 的符号（byte 槽被定义为 word）共 {len(byte_def_as_word)} 个 ===")
for i, line, addr in byte_def_as_word:
    name = line.split("*")[1].split()[0]
    print(f"{i:4d}  {name:20s} {addr}")

# 反向检查：定义成 uint8_t* 但实际是 word 槽的（不该有，仅报告）
print()
print("=== 反向检查：uint8_t* 定义但不在 byte 集合（可能误标 byte）===")
for i, line in enumerate(lines, 1):
    m = pat.match(line)
    if not m:
        continue
    typ = m.group(3)
    addr = "0x" + m.group(7).lower()
    if typ == "uint8_t" and addr not in BYTE_ADDRS:
        print(f"{i:4d}  {line.rstrip()}")
