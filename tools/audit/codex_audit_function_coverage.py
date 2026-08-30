"""比较原始函数入口与可编译源码/ELF 覆盖，不使用既有测试结论。"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
sources = sorted((ROOT / "firmware" / "src").glob("*.c")) + [ROOT / "firmware" / "stub.c"]

# 地址注释之后的第一个函数定义作为该原始入口的源码映射。
mapped = {}
for path in sources:
    text = path.read_text(encoding="utf-8", errors="ignore")
    marks = list(re.finditer(r"(?m)^/\*\s*0x([0-9A-Fa-f]{4,9})\b", text))
    for mark in marks:
        tail = text[mark.end():mark.end() + 3500]
        fn = re.search(
            r"(?:^|\n)\s*(?:__attribute__\s*\(\([^\n]*?\)\)\s*)?"
            r"(?:static\s+)?(?:void|int|uint\w*|undefined\w*|byte|ushort|short)"
            r"(?:\s*\*)?\s+([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", tail
        )
        if fn:
            mapped.setdefault(int(mark.group(1), 16), (fn.group(1), path.name))

# 超大手工还原文件的入口写在模块头，不是紧邻函数注释。
mapped.setdefault(0x458C, ("state_machine", "07_state_machine.c"))
mapped.setdefault(0xB642, ("modbus_dispatch", "08_modbus_dispatch.c"))

original = {}
for line in (ROOT / "evidence/reverse/reports/_all_functions.txt").read_text().splitlines():
    match = re.match(r"([0-9A-Fa-f]{8})\s+\S+\s+body=(\d+)", line)
    if match:
        original[int(match.group(1), 16)] = int(match.group(2))

nm = Path(r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-nm.exe")
nm_text = subprocess.check_output(
    [nm, "-S", "--size-sort", ROOT / "firmware" / "firmware.elf"], text=True
)
elf_symbols = {}
for line in nm_text.splitlines():
    match = re.match(r"[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)\s+[TtWw]\s+(\S+)$", line)
    if match:
        elf_symbols[match.group(2)] = int(match.group(1), 16)

missing_source = []
missing_elf = []
rows = []
for address, old_size in sorted(original.items()):
    if address not in mapped:
        missing_source.append(address)
        continue
    name, filename = mapped[address]
    if name not in elf_symbols:
        missing_elf.append((address, name, filename))
        continue
    rows.append((address, old_size, elf_symbols[name], name, filename))

print(f"original={len(original)} mapped={len(rows)} missing_source={len(missing_source)} missing_elf={len(missing_elf)}")
print("missing_source:", " ".join(f"0x{x:08X}" for x in missing_source) or "none")
runtime_replaced = {0xCC, 0x180, 0x184, 0x188, 0x19A, 0x1C4, 0x2EE,
                    0x10FD4, 0x11020, 0x11028}
unexpected = [address for address in missing_source if address not in runtime_replaced]
print(f"expected_IAR_runtime_replaced={len(missing_source) - len(unexpected)} "
      f"unexpected_missing={len(unexpected)}")
for item in missing_elf:
    print("missing_elf: 0x%08X %s %s" % item)
print("\ncompiled functions <=4 bytes while original >4 bytes:")
for address, old_size, new_size, name, filename in rows:
    if old_size > 4 and new_size <= 4:
        print(f"0x{address:08X} old={old_size:5d} new={new_size:3d} {name} ({filename})")
