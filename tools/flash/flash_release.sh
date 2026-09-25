#!/usr/bin/env bash
# Git Bash 入口：统一调用 PowerShell 主实现，保证两阶段备份/CRP 安全门一致。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PS_SCRIPT="$SCRIPT_DIR/flash_release.ps1"
[[ -f "$PS_SCRIPT" ]] || { echo "错误: 未找到 $PS_SCRIPT" >&2; exit 1; }
command -v powershell.exe >/dev/null 2>&1 || { echo "错误: 未找到 Windows PowerShell。" >&2; exit 1; }

PS_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin)
      [[ $# -ge 2 ]] || { echo "错误: --bin 需要路径" >&2; exit 1; }
      BIN_WIN="$(cygpath -w "$2" 2>/dev/null || printf '%s' "$2")"
      PS_ARGS+=("-Bin" "$BIN_WIN"); shift 2 ;;
    --device)
      [[ $# -ge 2 ]] || { echo "错误: --device 需要型号" >&2; exit 1; }
      PS_ARGS+=("-Device" "$2"); shift 2 ;;
    --serial)
      [[ $# -ge 2 ]] || { echo "错误: --serial 需要序列号" >&2; exit 1; }
      PS_ARGS+=("-Serial" "$2"); shift 2 ;;
    --dry-run) PS_ARGS+=("-DryRun"); shift ;;
    -h|--help)
      echo "用法: bash flash_release.sh [--bin x.bin] [--device LPC1765] [--serial SN] [--dry-run]"
      exit 0 ;;
    *) echo "错误: 未知参数 $1" >&2; exit 1 ;;
  esac
done

cd "$SCRIPT_DIR"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${PS_ARGS[@]}"
