#!/usr/bin/env bash
# =============================================================================
# tools/flash/flash_release.sh — 从 GitHub Release 拉取已构建固件并 SWD 烧写
#
# 目的：让其他电脑无需安装任何编译环境（arm-none-eabi-gcc / Python / Unicorn），
#       只需拉取本仓库（自带免安装打包版 J-Link），即可把 CI 构建好的固件烧进板子。
#
# 用法（在仓库根目录的 Git Bash 中执行）：
#   bash tools/flash/flash_release.sh                 # 拉取 latest 并烧写
#   bash tools/flash/flash_release.sh --tag v1.0      # 指定 tag
#   bash tools/flash/flash_release.sh --bin x.bin     # 用本地 bin 替代下载
#   bash tools/flash/flash_release.sh --mirror https://ghproxy.com/  # 国内镜像加速下载
#   bash tools/flash/flash_release.sh --dry-run       # 只下载+校验，不烧写
#   bash tools/flash/flash_release.sh --serial <SN>   # 指定 J-Link 序列号（多台时）
#
# 依赖：Git Bash（自带 curl / sha256sum）。J-Link 用仓库打包版 tools/jlink/JLink.exe，
#       无需安装。首次插 J-Link 未被识别时，先跑 tools/jlink/USBDriver/InstDrivers.exe。
#
# 烧写序列与 操作文档.md §3.4 一致：connect → 备份 → CRP 检查 → erase →
# loadbin → verifybin → 复位运行。铁律：erase 前自动 savebin 备份当前 Flash。
# =============================================================================
set -euo pipefail

# ---- 仓库与设备（可按需改） ----
REPO="${GITHUB_REPOSITORY:-ZeroHomers0/pc12m2-firmware}"
TAG="latest"
DEVICE="LPC1765"
FLASH_SIZE=0x40000            # LPC1765 = 256 KiB
MIRROR=""                     # 国内镜像前缀（默认空=直连 GitHub），如 https://ghproxy.com/
BIN=""
DRY_RUN=0
SERIAL=""

# ---- 解析参数 ----
while [[ $# -gt 0 ]]; do
  case "$1" in
    --tag)    TAG="$2"; shift 2 ;;
    --bin)    BIN="$2"; shift 2 ;;
    --device) DEVICE="$2"; shift 2 ;;
    --repo)   REPO="$2"; shift 2 ;;
    --mirror) MIRROR="$2"; shift 2 ;;
    --serial) SERIAL="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help)
      sed -n '5,24p' "$0"; exit 0 ;;
    *) echo "未知参数: $1"; exit 1 ;;
  esac
done

# ---- 路径 ----
# 支持两种布局：
#   A. 独立烧写工具包（Release 里的 zip）：脚本与 jlink/ 同目录
#   B. 仓库内：脚本位于 <仓库>/tools/flash/，jlink 在 <仓库>/tools/jlink/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"           # 脚本所在目录
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"               # 仓库布局时 = 仓库根
if [[ -f "$SCRIPT_DIR/jlink/JLink.exe" ]]; then
  JLINK="$SCRIPT_DIR/jlink/JLink.exe"                # 独立包布局
else
  JLINK="$ROOT/tools/jlink/JLink.exe"                # 仓库布局
fi
WORK="$PWD/release"                                   # 下载/临时产物（随运行目录，可重建）
mkdir -p "$WORK"
BIN_FILE="$WORK/firmware.bin"
SHA_FILE="$WORK/firmware.bin.sha256"

echo "== 目标：$REPO @ tag=$TAG，设备 $DEVICE =="

# ---- 1. 获取固件（本地 bin 或从 Release 下载） ----
if [[ -n "$BIN" ]]; then
  BIN_FILE="$BIN"
  echo "== 使用本地固件：$BIN_FILE =="
else
  GH_BIN_URL="https://github.com/$REPO/releases/download/$TAG/firmware.bin"
  GH_SHA_URL="https://github.com/$REPO/releases/download/$TAG/firmware.bin.sha256"
  # 镜像前缀拼接：<mirror> + <完整 GitHub 地址>（国内访问 GitHub 不稳时使用）
  BIN_URL="${MIRROR}${GH_BIN_URL}"
  SHA_URL="${MIRROR}${GH_SHA_URL}"
  [[ -n "$MIRROR" ]] && echo "== 使用镜像：$MIRROR =="
  echo "== 下载固件：$BIN_URL =="
  curl -fL --retry 3 -o "$BIN_FILE" "$BIN_URL"
  curl -fL --retry 3 -o "$SHA_FILE" "$SHA_URL" || { echo "警告: 未取到 sha256（将跳过校验）"; rm -f "$SHA_FILE"; }
fi

[[ -f "$BIN_FILE" ]] || { echo "错误: 固件文件不存在: $BIN_FILE"; exit 1; }
BIN_SIZE=$(stat -c%s "$BIN_FILE")
echo "固件: $BIN_FILE ($BIN_SIZE B)"

# ---- 2. 校验 SHA-256 ----
if [[ -f "$SHA_FILE" ]]; then
  EXPECT=$(awk '{print $1}' "$SHA_FILE")
  ACTUAL=$(sha256sum "$BIN_FILE" | awk '{print $1}')
  echo "期望 SHA-256: $EXPECT"
  echo "实际 SHA-256: $ACTUAL"
  if [[ "$EXPECT" != "$ACTUAL" ]]; then
    echo "错误: SHA-256 不匹配，固件可能损坏，已中止。" >&2
    exit 1
  fi
  echo "== SHA-256 校验通过 =="
else
  echo "警告: 无 sha256 参考文件，跳过哈希校验。"
fi

# 尺寸合理性检查（flash 容量内）
if (( BIN_SIZE > FLASH_SIZE )); then
  echo "错误: 固件尺寸 $BIN_SIZE B 超过 Flash 容量 $FLASH_SIZE B。" >&2
  exit 1
fi

[[ "$DRY_RUN" -eq 1 ]] && { echo "== dry-run：仅下载+校验，不烧写。完成。"; exit 0; }

# ---- 3. 检查打包版 J-Link ----
[[ -f "$JLINK" ]] || { echo "错误: 未找到打包版 J-Link: $JLINK"; exit 1; }

# ---- 4. 生成 CommanderScript（基于 操作文档 §3.4 flash.jlink） ----
# JLink.exe 是 Windows 程序，路径需转成 Windows 绝对路径；backup 目录确保存在。
BACKUP_DIR="$PWD/backup"   # 备份随运行目录（独立包内同样可用）
mkdir -p "$BACKUP_DIR"
PRE="$BACKUP_DIR/pre_flash.bin"
BIN_WIN="$(cygpath -w "$BIN_FILE" 2>/dev/null || echo "$BIN_FILE")"
PRE_WIN="$(cygpath -w "$PRE" 2>/dev/null || echo "$PRE")"

SCRIPT="$WORK/flash_release.jlink"
{
  echo "si SWD"
  echo "speed 100"
  [[ -n "$SERIAL" ]] && echo "SelectEmuBySN $SERIAL"
  echo "device $DEVICE"
  echo "connect"
  echo "savebin $PRE_WIN, 0x0, $FLASH_SIZE   ; 烧前自动备份当前 Flash（铁律：不备份不擦除）"
  echo "mem32 0x000002FC, 1                   ; 板上 CRP 字，应为 0xFFFFFFFF（无保护）"
  echo "erase"
  echo "loadbin $BIN_WIN, 0x0"
  echo "verifybin $BIN_WIN, 0x0               ; 全镜像读回校验"
  echo "SetRESET"
  echo "sleep 200"
  echo "ClrRESET"
  echo "sleep 200"
  echo "go"
  echo "sleep 500"
  echo "exit"
} > "$SCRIPT"

echo "== 生成 CommanderScript: $SCRIPT =="
echo "== 调用打包版 J-Link 烧写（擦除前自动备份至 $PRE） =="

# ---- 5. 执行烧写 ----
"$JLINK" -CommanderScript "$SCRIPT" || {
  rc=$?
  echo "错误: J-Link 烧写失败 (rc=$rc)。" >&2
  echo "排查：①四根主信号线(SWDIO/SWCLK/VTref/GND)接触是否良好；" >&2
  echo "      ②固件复用 SWD 脚连不上 → 需 connect-under-reset（见 操作文档.md §3.2）；" >&2
  echo "      ③首次插 J-Link 未识别 → 跑 tools/jlink/USBDriver/InstDrivers.exe 装驱动。" >&2
  exit $rc
}

echo "== 烧写完成 =="
echo "  校验：verifybin 应输出 Verify successful（板上内容与固件完全一致）。"
echo "  完成后请物理断电再上电（J-Link 驱动复位可能悬挂 SWD）。"
