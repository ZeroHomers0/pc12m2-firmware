"""独立比较可编译源码中的 CRC 表与原始固件，不调用现有测试套件。"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "firmware/src/crc16_table.c").read_text(encoding="utf-8", errors="ignore")
binary = (ROOT / "LPC1765.bin").read_bytes()

for name, address in (("crc16_hi_tbl", 0x11034), ("crc16_lo_tbl", 0x11134)):
    match = re.search(rf"{name}\s*\[256\]\s*=\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise SystemExit(f"{name}: 未找到")
    values = bytes(int(item, 16) for item in re.findall(r"0x([0-9a-fA-F]+)", match.group(1)))
    expected = binary[address:address + 256]
    diffs = [index for index, pair in enumerate(zip(values, expected)) if pair[0] != pair[1]]
    print(f"{name}: source={len(values)} original={len(expected)} diffs={len(diffs)}")
    if len(values) != 256 or diffs:
        raise SystemExit(1)
