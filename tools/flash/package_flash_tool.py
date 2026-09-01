#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
# tools/flash/package_flash_tool.py — 打包独立烧写工具 zip（烧写脚本 + 免安装 J-Link）
#
# 用途：把烧写工具从仓库里分离出来，打成独立 zip，放进 GitHub Release。
#       其他电脑无需 clone 仓库，只需下载解压这个 zip，即可免编译环境烧写固件。
#
# zip 布局（解压后即用，脚本已支持"jlink 在脚本旁"的独立包布局）：
#   flash-tool/
#     flash_release.ps1        # Windows PowerShell 版
#     flash_release.sh         # Git Bash 版
#     README.md                # 独立包使用说明
#     jlink/                   # 免安装打包版 J-Link（JLink.exe + DLL + 驱动）
#       JLink.exe
#       JLinkARM.dll
#       *.dll
#       USBDriver/
#
# 用法：
#   python tools/flash/package_flash_tool.py [--out <path>] [--name <zip名>]
#
# 默认输出：<仓库>/release/flash-tool-<设备>.zip
# =============================================================================
import argparse
import os
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]          # 仓库根
JLINK_DIR = ROOT / "tools" / "jlink"
FLASH_DIR = ROOT / "tools" / "flash"
README = FLASH_DIR / "README.md"
DEFAULT_NAME = "flash-tool"


def main():
    ap = argparse.ArgumentParser(description="打包独立烧写工具 zip")
    ap.add_argument("--out", default=str(ROOT / "release"),
                    help="输出目录（默认 <仓库>/release）")
    ap.add_argument("--name", default=DEFAULT_NAME,
                    help="zip 文件名（不含扩展名，默认 flash-tool）")
    args = ap.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    zip_path = out_dir / f"{args.name}.zip"

    # 校验关键文件存在
    scripts = [FLASH_DIR / "flash_release.ps1", FLASH_DIR / "flash_release.sh"]
    jlink_exe = JLINK_DIR / "JLink.exe"
    for p in scripts + [jlink_exe, README]:
        if not p.exists():
            print(f"错误: 缺少 {p}", file=sys.stderr)
            sys.exit(1)

    # 打包根目录名（解压后是一个独立文件夹）
    pkg_root = args.name

    added = 0
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        # 脚本与 README 放包根
        for src in scripts + [README]:
            arc = f"{pkg_root}/{src.name}"
            zf.write(src, arc)
            added += 1
        # 整个 tools/jlink 目录递归打包
        for f in sorted(JLINK_DIR.rglob("*")):
            if f.is_file():
                rel = f.relative_to(JLINK_DIR)
                arc = f"{pkg_root}/jlink/{rel.as_posix()}"
                zf.write(f, arc)
                added += 1

    size = zip_path.stat().st_size
    print(f"已生成: {zip_path} ({size} B, {added} 个文件)")
    print(f"解压后运行: cd {args.name} && powershell -ExecutionPolicy Bypass -File flash_release.ps1")


if __name__ == "__main__":
    main()
