#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
W8 阶段 0 —— 原固件备份（只读，绝不擦写）

用 SEGGER J-Link Commander 以 SWD 连接 LPC1765，读取 CRP 字，并连续读两份
256 KiB 原 Flash（0x00000000..0x00040000），核对两份 SHA-256 一致后报告。

铁律：
  1. 本脚本只执行 savebin / mem32 只读命令，绝不 unlock / recover / mass erase / LoadFile。
  2. 若 CRP 非 0xFFFFFFFF（已使能代码读保护），读取可能不完整或失败，须立即停止评估，
     不得尝试解锁（解锁较高保护会整片擦除，原固件不可恢复）。
  3. 备份产物绝不提交到公共仓库；请另复制到第二处独立物理位置。

用法：
  python tools/w8/w8_backup_orig.py
  python tools/w8/w8_backup_orig.py --jlink tools/jlink/JLink.exe --out backup
  python tools/w8/w8_backup_orig.py --device LPC176x          # connect 报错时换设备名
"""
import argparse
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GOLDEN_BIN = REPO_ROOT / "pc12m2_orig.bin"
FLASH_SIZE = 0x40000  # LPC1765 Flash = 256 KiB


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().lower()


def main() -> int:
    ap = argparse.ArgumentParser(description="W8 阶段 0：原固件双备份（只读）")
    ap.add_argument("--jlink", default=str(REPO_ROOT / "tools" / "jlink" / "JLink.exe"),
                    help="JLink.exe 路径")
    ap.add_argument("--out", default=str(REPO_ROOT / "backup"),
                    help="备份输出目录（默认仓库根 backup/，已 gitignore）")
    ap.add_argument("--device", default="LPC1765", help="J-Link 设备名")
    ap.add_argument("--speed", default="100", help="SWD 初始速率 kHz")
    args = ap.parse_args()

    jlink = Path(args.jlink)
    if not jlink.exists():
        print(f"[FAIL] 找不到 JLink.exe：{jlink}")
        print("       应使用仓库内 tools/jlink/JLink.exe，或用 --jlink 指定。")
        return 2

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    b1 = out_dir / "backup_orig_1.bin"
    b2 = out_dir / "backup_orig_2.bin"

    # 生成 J-Link 命令文件（绝对路径，正斜杠，避免引号/空格问题）
    script = f"""si SWD
speed {args.speed}
device {args.device}
connect
mem32 0x000002FC, 1
savebin {b1.as_posix()}, 0x00000000, 0x{FLASH_SIZE:X}
savebin {b2.as_posix()}, 0x00000000, 0x{FLASH_SIZE:X}
exit
"""
    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False,
                                     encoding="ascii") as tf:
        tf.write(script)
        cmd_file = tf.name

    print(f"[*] 运行 J-Link Commander：{jlink}")
    print(f"[*] 设备 {args.device} / SWD {args.speed} kHz / 只读 CRP + 两份 {FLASH_SIZE} 字节 Flash")
    print(f"[*] 输出目录：{out_dir}")
    print("-" * 70)
    try:
        proc = subprocess.run(
            [str(jlink), "-CommanderScript", cmd_file],
            capture_output=True, text=True, timeout=300,
        )
    except subprocess.TimeoutExpired:
        print("[FAIL] J-Link 连接超时（300 s）。检查 P12-1/2/6/8 接线与共地，降到 100 kHz。")
        return 3

    # 打印完整日志便于回填
    sys.stdout.write(proc.stdout or "")
    if proc.stderr:
        sys.stdout.write("[stderr]\n" + proc.stderr)

    # 提取 CRP 字（日志里 mem32 会打印形如 "000002FC = XXXXXXXX"）
    crp = None
    for line in (proc.stdout or "").splitlines():
        s = line.strip().replace(",", "")
        if "000002FC" in s and "=" in s:
            crp = s.split("=")[-1].strip().split()[0]

    print("=" * 70)
    if crp is not None:
        print(f"CRP 字 @0x2FC = {crp}")
        if crp.upper() != "FFFFFFFF":
            print("[!] 注意：CRP ≠ 0xFFFFFFFF，说明已使能代码读保护，读取可能不完整！")
            print("    请停止，评估后再继续；严禁 unlock / recover / mass erase。")
        else:
            print("[OK] CRP = 0xFFFFFFFF（未使能读保护）")

    if not b1.exists() or not b2.exists():
        print("[FAIL] 未生成备份文件。连接或 savebin 失败，见上方日志。")
        return 4

    h1, h2 = sha256(b1), sha256(b2)
    print(f"备份1 {b1.name}  SHA-256 = {h1}")
    print(f"备份2 {b2.name}  SHA-256 = {h2}")
    if h1 != h2:
        print("[FAIL] 两份备份哈希不一致！可能存在读不稳定，请重新接线并重试。")
        return 5
    print("[OK] 两份备份哈希一致。")

    if GOLDEN_BIN.exists():
        hg = sha256(GOLDEN_BIN)
        print(f"PC12M-2 已归档金标准 SHA-256 = {hg}")
        if h1 == hg:
            print("[OK] 板上原固件与仓库金标准完全一致 —— 逆向对象就是这块板。")
        else:
            print("[!] 重要：板上固件与仓库金标准哈希不一致！")
            print("    说明这块板的固件版本与当前反编译基线不同，需在分析前核对。")
    else:
        print("[!] backup/pc12m2_orig.bin 尚不存在，跳过与已归档金标准比对。")

    print("=" * 70)
    print("下一步：")
    print("  1. 把两个备份各复制一份到第二处独立物理位置（U 盘 / 另一台机）。")
    print("  2. 用 W8_HARDWARE_TEST 回填模板记录 CRP、双哈希。")
    print("  3. 未核对哈希一致前，绝不执行任何擦写。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
