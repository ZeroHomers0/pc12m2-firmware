"""独立审查 globals.c 的 DAT/PTR 字面量是否忠实于原始固件。

不依赖项目既有测试。符号名末尾的 8 位十六进制数视为原始 flash
字面量槽地址，并与 LPC1765.bin 对应小端 32 位值逐项比较。
"""
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
blob = (ROOT / "LPC1765.bin").read_bytes()
source = (ROOT / "firmware" / "globals.c").read_text(encoding="utf-8")

definition = re.compile(
    r"(?:volatile\s+)?uint(?:8|16|32)_t\s+"
    r"(?P<pointer>\*)?(?P<name>(?:DAT|PTR)_[A-Za-z0-9_]+)\s*=\s*"
    r"(?:\(uint(?:8|16|32)_t\s*\*\)\s*)?0x(?P<value>[0-9A-Fa-f]+)"
)
suffix = re.compile(r"_([0-9A-Fa-f]{8})$")

checked = 0
skipped = []
mismatches = []
for item in definition.finditer(source):
    name = item.group("name")
    match = suffix.search(name)
    if not match:
        skipped.append(name)
        continue
    slot = int(match.group(1), 16)
    if slot + 4 > len(blob):
        skipped.append(name)
        continue
    expected = struct.unpack_from("<I", blob, slot)[0]
    actual = int(item.group("value"), 16)
    checked += 1
    if actual != expected:
        mismatches.append((name, slot, expected, actual))

print(f"checked={checked} skipped={len(skipped)} mismatches={len(mismatches)}")
for name, slot, expected, actual in mismatches:
    print(f"{name}: slot=0x{slot:08X} expected=0x{expected:08X} actual=0x{actual:08X}")

raise SystemExit(1 if mismatches else 0)
