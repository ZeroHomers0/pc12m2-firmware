# =============================================================================
# tools/flash/flash_release.ps1 — 从 GitHub Release 拉取已构建固件并 SWD 烧写（Windows 原生版）
#
# 目的：让其他电脑无需安装任何编译环境（arm-none-eabi-gcc / Python / Unicorn / Git Bash），
#       只需 PowerShell（Windows 10/11 自带）+ 本仓库（自带免安装打包版 J-Link），
#       即可把 CI 构建好的固件烧进板子。
#
# 用法（PowerShell / CMD 中执行，仓库根目录下）：
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -Tag v1.0
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -Bin x.bin
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -DryRun
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -Serial <SN>
#
# 依赖：Windows PowerShell 5.1+（内置 Invoke-WebRequest / Get-FileHash）。J-Link 用仓库
#       打包版 tools\jlink\JLink.exe，无需安装。首次插 J-Link 未被识别时，先跑
#       tools\jlink\USBDriver\InstDrivers.exe。
#
# 烧写序列与 操作文档.md §3.4 一致：connect → 备份 → CRP 检查 → erase →
# loadbin → verifybin → 复位运行。铁律：erase 前自动 savebin 备份当前 Flash。
# =============================================================================
[CmdletBinding()]
param(
  [string]$Repo = "ZeroHomers0/pc12m2-firmware",
  [string]$Tag = "latest",
  [string]$Device = "LPC1765",
  [string]$Bin = "",            # 本地固件路径（替代下载）
  [string]$Mirror = "",         # 国内镜像前缀（默认空=直连 GitHub），如 https://ghproxy.com/
  [string]$Serial = "",         # J-Link 序列号（多台时）
  [switch]$DryRun               # 只下载+校验，不烧写
)

$ErrorActionPreference = 'Stop'
$FLASH_SIZE = 0x40000   # LPC1765 = 256 KiB

# ---- 路径 ----
# 支持两种布局：
#   A. 独立烧写工具包（Release 里的 zip）：脚本与 jlink\ 同目录
#   B. 仓库内：脚本位于 <仓库>/tools/flash/，jlink 在 <仓库>/tools/jlink/
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent   # 仓库布局时 = 仓库根
$JLink = @(
  (Join-Path $PSScriptRoot "jlink\JLink.exe"),
  (Join-Path $Root "tools\jlink\JLink.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
$Work = Join-Path (Get-Location) "release"   # 下载/临时产物（随运行目录，可重建）
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$BinFile = Join-Path $Work "firmware.bin"
$ShaFile = Join-Path $Work "firmware.bin.sha256"

Write-Host "== 目标：$Repo @ tag=$Tag，设备 $Device =="

# ---- 1. 获取固件（本地 bin 或从 Release 下载） ----
if ($Bin) {
  $BinFile = $Bin
  Write-Host "== 使用本地固件：$BinFile =="
} else {
  $GhBinUrl = "https://github.com/$Repo/releases/download/$Tag/firmware.bin"
  $GhShaUrl = "https://github.com/$Repo/releases/download/$Tag/firmware.bin.sha256"
  # 镜像前缀拼接：<mirror> + <完整 GitHub 地址>（国内访问 GitHub 不稳时使用）
  $BinUrl = "$Mirror$GhBinUrl"
  $ShaUrl = "$Mirror$GhShaUrl"
  if ($Mirror) { Write-Host "== 使用镜像：$Mirror ==" }
  Write-Host "== 下载固件：$BinUrl =="
  try {
    Invoke-WebRequest -Uri $BinUrl -OutFile $BinFile -UseBasicParsing
  } catch {
    Write-Error "下载固件失败: $($_.Exception.Message)"
    exit 1
  }
  try {
    Invoke-WebRequest -Uri $ShaUrl -OutFile $ShaFile -UseBasicParsing
  } catch {
    Write-Host "警告: 未取到 sha256（将跳过校验）"
    Remove-Item -Force $ShaFile -ErrorAction SilentlyContinue
  }
}

if (-not (Test-Path $BinFile)) { Write-Error "固件文件不存在: $BinFile"; exit 1 }
$BinSize = (Get-Item $BinFile).Length
Write-Host "固件: $BinFile ($BinSize B)"

# ---- 2. 校验 SHA-256 ----
if (Test-Path $ShaFile) {
  $Expected = (Get-Content $ShaFile).Trim().Split(' ')[0]
  $Actual   = (Get-FileHash -Algorithm SHA256 $BinFile).Hash.ToLower()
  Write-Host "期望 SHA-256: $Expected"
  Write-Host "实际 SHA-256: $Actual"
  if ($Expected -ne $Actual) {
    Write-Error "SHA-256 不匹配，固件可能损坏，已中止。"
    exit 1
  }
  Write-Host "== SHA-256 校验通过 =="
} else {
  Write-Host "警告: 无 sha256 参考文件，跳过哈希校验。"
}

# 尺寸合理性检查（flash 容量内）
if ($BinSize -gt $FLASH_SIZE) {
  Write-Error "固件尺寸 $BinSize B 超过 Flash 容量 $FLASH_SIZE B。"
  exit 1
}

if ($DryRun) { Write-Host "== dry-run：仅下载+校验，不烧写。完成。"; exit 0 }

# ---- 3. 检查打包版 J-Link ----
if (-not $JLink) { Write-Error "未找到打包版 J-Link（脚本旁 jlink\ 或仓库 tools\jlink\ 均无）"; exit 1 }

# ---- 4. 生成 CommanderScript（基于 操作文档 §3.4 flash.jlink） ----
$BackupDir = Join-Path (Get-Location) "backup"   # 备份随运行目录（独立包内同样可用）
New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null
$Pre   = Join-Path $BackupDir "pre_flash.bin"
$BinWin = (Resolve-Path $BinFile).Path   # Windows 绝对路径
$PreWin = (Resolve-Path $Pre).Path
$Script = Join-Path $Work "flash_release.jlink"

$lines = @(
  "si SWD",
  "speed 100"
)
if ($Serial) { $lines += "SelectEmuBySN $Serial" }
$lines += @(
  "device $Device",
  "connect",
  "savebin $PreWin, 0x0, $FLASH_SIZE   ; 烧前自动备份当前 Flash（铁律：不备份不擦除）",
  "mem32 0x000002FC, 1                   ; 板上 CRP 字，应为 0xFFFFFFFF（无保护）",
  "erase",
  "loadbin $BinWin, 0x0",
  "verifybin $BinWin, 0x0               ; 全镜像读回校验",
  "SetRESET",
  "sleep 200",
  "ClrRESET",
  "sleep 200",
  "go",
  "sleep 500",
  "exit"
)
$lines -join "`r`n" | Set-Content -Path $Script -Encoding ASCII

Write-Host "== 生成 CommanderScript: $Script =="
Write-Host "== 调用打包版 J-Link 烧写（擦除前自动备份至 $Pre） =="

# ---- 5. 执行烧写 ----
try {
  & $JLink -CommanderScript $Script
  if ($LASTEXITCODE -ne 0) { throw "J-Link 退出码 $LASTEXITCODE" }
} catch {
  Write-Host "错误: J-Link 烧写失败: $($_.Exception.Message)" -ForegroundColor Red
  Write-Host "排查：①四根主信号线(SWDIO/SWCLK/VTref/GND)接触是否良好；"
  Write-Host "      ②固件复用 SWD 脚连不上 → 需 connect-under-reset（见 操作文档.md §3.2）；"
  Write-Host "      ③首次插 J-Link 未识别 → 跑 tools\jlink\USBDriver\InstDrivers.exe 装驱动。"
  exit 1
}

Write-Host "== 烧写完成 =="
Write-Host "  校验：verifybin 应输出 Verify successful（板上内容与固件完全一致）。"
Write-Host "  完成后请物理断电再上电（J-Link 驱动复位可能悬挂 SWD）。"
