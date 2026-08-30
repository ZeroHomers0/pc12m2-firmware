"""独立核对 strpool.c 的每个映射簇与原始 LPC1765.bin。"""
from pathlib import Path
import ast
import re

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "firmware/src/strpool.c").read_text(encoding="utf-8")
original = (ROOT / "LPC1765.bin").read_bytes()

blob_area = source.split("static const uint8_t strpool_blob", 1)[1].split(
    "static const strpool_cluster_t", 1
)[0]
literals = re.findall(r'"(?:\\.|[^"\\])*"', blob_area)
blob = b"".join(ast.literal_eval(token).encode("latin1") for token in literals)

clusters = []
for base, length, offset in re.findall(
    r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*strpool_blob\s*\+\s*(\d+)\s*\}", source
):
    clusters.append((int(base), int(length), int(offset)))

bad = []
for base, length, offset in clusters:
    actual = blob[offset:offset + length]
    expected = original[base:base + length]
    if actual != expected:
        bad.append((base, length, offset))

print(f"clusters={len(clusters)} mapped_bytes={sum(x[1] for x in clusters)} blob_bytes={len(blob)} mismatches={len(bad)}")
for base, length, offset in bad:
    print(f"base=0x{base:X} len={length} blob_offset={offset}")

# 直接传入原 flash 字面量的显示调用必须全部能被 strpool_map 命中。
call_addresses = []
for path in list((ROOT / "firmware/src").glob("*.c")) + [ROOT / "firmware/stub.c"]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    for first_arg in re.findall(r"disp_string\s*\(\s*([^,\n]+)", text):
        value = re.search(r"0x([0-9a-fA-F]+)", first_arg)
        if value:
            call_addresses.append(int(value.group(1), 16))
unmapped = [
    address for address in call_addresses
    if not any(base <= address < base + length for base, length, _ in clusters)
]
print(f"constant_disp_calls={len(call_addresses)} unmapped={len(unmapped)}")
if unmapped:
    print("unmapped addresses:", " ".join(f"0x{x:X}" for x in sorted(set(unmapped))))

raise SystemExit(1 if bad or unmapped else 0)
