#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PC12M-2 —— 把 ISP 分块读出的 Intel HEX（part00..partNN.hex）合并成完整 256KiB bin。

用法：
  python tools/w8/w8_combine_hex.py                 # 默认读 backup/part*.hex → backup/pc12m2_orig.bin
  python tools/w8/w8_combine_hex.py --parts DIR --out OUT.bin
"""
import argparse
import hashlib
from pathlib import Path

FLASH_SIZE = 0x40000  # 256 KiB


def parse_hex(path: Path):
    """解析 Intel HEX 文件，返回 {abs_addr: byte}。处理 04（扩展线性地址）记录。"""
    data = {}
    base = 0
    with open(path, "r", encoding="ascii") as f:
        for line in f:
            line = line.strip()
            if not line or line[0] != ":":
                continue
            try:
                nbytes = int(line[1:3], 16)
                addr = int(line[3:7], 16)
                rectype = int(line[7:9], 16)
                payload = bytes.fromhex(line[9:9 + nbytes * 2])
                # checksum 校验（可选）
                check = int(line[9 + nbytes * 2:11 + nbytes * 2], 16)
                if (sum(bytes.fromhex(line[1:])[:-1]) + check) & 0xFF != 0:
                    print(f"[!] 校验和错误: {path.name} {line}")
            except (ValueError, IndexError):
                print(f"[!] 无法解析行: {path.name} {line}")
                continue
            if rectype == 0x04:
                base = int(payload.hex(), 16) << 16
            elif rectype == 0x00:
                for i in range(nbytes):
                    data[base + addr + i] = payload[i]
    return data


def main() -> int:
    ap = argparse.ArgumentParser(description="合并 ISP 分块 HEX → 完整 bin")
    ap.add_argument("--parts", default=str(Path(__file__).resolve().parents[2] / "backup"),
                    help="part*.hex 所在目录")
    ap.add_argument("--out", default=str(Path(__file__).resolve().parents[2] / "backup" / "pc12m2_orig.bin"),
                    help="输出 bin 路径")
    args = ap.parse_args()

    parts_dir = Path(args.parts)
    part_files = sorted(parts_dir.glob("part*.hex"))
    if not part_files:
        print(f"[FAIL] {parts_dir} 下没有 part*.hex 文件")
        return 2

    blob = bytearray(b"\xFF" * FLASH_SIZE)
    for pf in part_files:
        d = parse_hex(pf)
        if not d:
            print(f"[!] {pf.name} 为空或未解析到数据")
        for a, b in d.items():
            if 0 <= a < FLASH_SIZE:
                blob[a] = b
            else:
                print(f"[!] {pf.name} 地址越界: 0x{a:X}")

    out = Path(args.out)
    out.write_bytes(bytes(blob))

    n_nonff = sum(1 for b in blob if b != 0xFF)
    h = hashlib.sha256(bytes(blob)).hexdigest()
    print(f"[OK] 合并完成: {out}")
    print(f"     大小 {len(blob)} B (0x{len(blob):X}), 非 0xFF 字节 {n_nonff} 个")
    print(f"     SHA-256 = {h.upper()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
