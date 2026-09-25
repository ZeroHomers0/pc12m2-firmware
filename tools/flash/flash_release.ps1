# =============================================================================
# tools/flash/flash_release.ps1 — 从当前目录 release 取固件并 SWD 烧写（Windows 原生版）
#
# 目的：让其他电脑无需安装任何编译环境（arm-none-eabi-gcc / Python / Unicorn / Git Bash），
#       只需 PowerShell（Windows 10/11 自带）+ 本仓库（自带免安装打包版 J-Link），
#       即可把 CI 构建好的固件烧进板子。
#
# 用法（PowerShell / CMD 中执行，仓库根目录下）：
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -Bin x.bin
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -DryRun
#   powershell -ExecutionPolicy Bypass -File tools\flash\flash_release.ps1 -Serial <SN>
#
# 依赖：Windows PowerShell 5.1+（内置 Get-FileHash）。J-Link 用仓库
#       打包版 tools\jlink\JLink.exe，无需安装。首次插 J-Link 未被识别时，先跑
#       tools\jlink\USBDriver\InstDrivers.exe。
#
# 烧写序列与 操作文档.md §3.4 一致：connect → 备份 → CRP 检查 → erase →
# loadbin → verifybin → 复位运行。铁律：erase 前自动 savebin 备份当前 Flash。
# =============================================================================
[CmdletBinding()]
param(
  [string]$Device = "LPC1765",
  [string]$Bin = "",            # 本地固件路径（默认 .\release\firmware.bin）
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
$Work = Join-Path (Get-Location) "release"   # 固件与临时产物（相对于当前目录）
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$BinFile = Join-Path $Work "firmware.bin"
$ShaFile = Join-Path $Work "firmware.bin.sha256"

Write-Host "== 设备：$Device =="

# ---- 1. 从本地取固件 ----
if ($Bin) {
  $BinFile = $Bin
  $ShaFile = "$Bin.sha256"
  Write-Host "== 使用本地固件：$BinFile =="
} else {
  Write-Host "== 使用当前目录固件：$BinFile =="
}

if (-not (Test-Path $BinFile)) { Write-Error "固件文件不存在: $BinFile"; exit 1 }
$BinSize = (Get-Item $BinFile).Length
Write-Host "固件: $BinFile ($BinSize B)"

# ---- 2. 校验 SHA-256 ----
$Actual = (Get-FileHash -Algorithm SHA256 $BinFile).Hash.ToLower()
if (Test-Path $ShaFile) {
  $Expected = (Get-Content $ShaFile).Trim().Split(' ')[0]
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

if ($DryRun) { Write-Host "== dry-run：仅校验，不烧写。完成。"; exit 0 }

# ---- 3. 检查打包版 J-Link ----
if (-not $JLink) { Write-Error "未找到打包版 J-Link（脚本旁 jlink\ 或仓库 tools\jlink\ 均无）"; exit 1 }

# ---- 3.1 检查/安装 J-Link USB 驱动 ----
function Test-JLinkUsbDriver {
  try {
    $PnPUtil = Join-Path $env:SystemRoot "System32\pnputil.exe"
    $DriverList = (& $PnPUtil /enum-drivers 2>&1 | Out-String)
    if ($LASTEXITCODE -eq 0 -and $DriverList -match '(?i)JLink(?:WinUSB|CDC)?\.inf') {
      return $true
    }
  } catch {
    # pnputil 检查不可用时，再检查当前已识别的 SEGGER USB 设备。
  }
  try {
    $Device = Get-CimInstance Win32_PnPSignedDriver -ErrorAction Stop |
      Where-Object { $_.DeviceID -like 'USB\VID_1366*' } |
      Select-Object -First 1
    return $null -ne $Device
  } catch {
    return $false
  }
}

if (Test-JLinkUsbDriver) {
  Write-Host "== J-Link USB 驱动已安装 =="
} else {
  $DriverInstaller = Join-Path (Split-Path $JLink -Parent) "USBDriver\InstDrivers.exe"
  if (-not (Test-Path $DriverInstaller)) {
    Write-Error "未检测到 J-Link USB 驱动，且缺少驱动安装器: $DriverInstaller"
    exit 1
  }
  Write-Host "== 未检测到 J-Link USB 驱动，即将请求管理员权限安装 ==" -ForegroundColor Yellow
  try {
    $DriverProcess = Start-Process -FilePath $DriverInstaller -Verb RunAs -Wait -PassThru
    if ($DriverProcess.ExitCode -ne 0) {
      throw "驱动安装器退出码 $($DriverProcess.ExitCode)"
    }
    Write-Host "== J-Link USB 驱动安装完成 =="
  } catch {
    Write-Error "J-Link USB 驱动安装失败或 UAC 被取消: $($_.Exception.Message)"
    exit 1
  }
}

# ---- 4. 两阶段安全门：先备份/检查，通过后才允许擦除 ----
$RunStamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$BackupDir = Join-Path (Get-Location) "backup"
New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null
$Pre = Join-Path $BackupDir "pre_flash_$RunStamp.bin"
$BinWin = (Resolve-Path $BinFile).Path
$PreWin = [System.IO.Path]::GetFullPath($Pre)
$PreflightScript = Join-Path $Work "preflight_$RunStamp.jlink"
$FlashScript = Join-Path $Work "flash_$RunStamp.jlink"
$PreflightLog = Join-Path $Work "preflight_$RunStamp.log"
$FlashLog = Join-Path $Work "flash_$RunStamp.log"

function New-JLinkHeader {
  $Header = @("si SWD", "speed 100")
  if ($Serial) { $Header += "SelectEmuBySN $Serial" }
  return $Header + @("device $Device", "connect")
}

function Invoke-JLinkPhase([string]$ScriptPath, [string]$LogPath) {
  & $JLink -CommanderScript $ScriptPath 2>&1 | Tee-Object -FilePath $LogPath
  $ExitCode = $LASTEXITCODE
  if ($ExitCode -ne 0) { throw "J-Link 退出码 $ExitCode（见 $LogPath）" }
  if (Select-String -Path $LogPath -Pattern '(?i)(\*+\s*Error:|^Error:|^Syntax:)' -Quiet) {
    throw "J-Link 日志中包含错误（见 $LogPath）"
  }
}

try {
  $PreflightLines = New-JLinkHeader
  $PreflightLines += @(
    "savebin `"$PreWin`", 0x0, $FLASH_SIZE",
    "mem32 0x000002FC, 1",
    "exit"
  )
  $PreflightLines -join "`r`n" | Set-Content -Path $PreflightScript -Encoding ASCII
  Write-Host "== 阶段 1/2：连接、VTref 检查、备份与 CRP 校验 =="
  Invoke-JLinkPhase $PreflightScript $PreflightLog

  if (-not (Test-Path $Pre) -or (Get-Item $Pre).Length -ne $FLASH_SIZE) {
    throw "备份未生成或尺寸不是 $FLASH_SIZE B；为保护原固件，禁止擦除"
  }
  $PreflightText = Get-Content $PreflightLog -Raw
  $CrpMatch = [regex]::Match($PreflightText, '(?im)(?:0x)?0*2FC\s*=\s*(?:0x)?([0-9A-F]{8})')
  if (-not $CrpMatch.Success) { throw "无法解析 CRP 字；禁止擦除" }
  $CrpValue = $CrpMatch.Groups[1].Value.ToUpper()
  if ($CrpValue -ne 'FFFFFFFF') { throw "CRP 字为 0x$CrpValue（预期 0xFFFFFFFF）；禁止擦除" }
  $VtrefMatch = [regex]::Match($PreflightText, '(?im)VTref\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*V')
  if ($VtrefMatch.Success) {
    $Vtref = [double]$VtrefMatch.Groups[1].Value
    if ($Vtref -lt 1.0) { throw "VTref=${Vtref}V 过低；请检查目标板供电与 VTref 接线" }
    Write-Host "== VTref=${Vtref}V，CRP=0x$CrpValue，备份已确认 =="
  } else {
    Write-Host "警告: 当前 J-Link 版本未输出可解析的 VTref；连接与完整备份已成功。" -ForegroundColor Yellow
  }

  $FlashLines = New-JLinkHeader
  $FlashLines += @(
    "erase",
    "loadbin `"$BinWin`", 0x0",
    "verifybin `"$BinWin`", 0x0",
    "mem32 0x00000000, 8",
    "mem32 0x000002FC, 1",
    "mem32 0x00006B78, 16",
    "SetRESET",
    "sleep 200",
    "ClrRESET",
    "sleep 500",
    "exit"
  )
  $FlashLines -join "`r`n" | Set-Content -Path $FlashScript -Encoding ASCII
  Write-Host "== 阶段 2/2：擦除、烧写、全镜像校验与关键地址读回 =="
  Invoke-JLinkPhase $FlashScript $FlashLog
  if (-not (Select-String -Path $FlashLog -SimpleMatch "Verify successful" -Quiet)) {
    throw "J-Link 未报告 Verify successful（见 $FlashLog）"
  }
} catch {
  Write-Host "错误: J-Link 安全检查或烧写失败: $($_.Exception.Message)" -ForegroundColor Red
  Write-Host "如连接了多台 J-Link，请用 -Serial <SN> 指定序列号。"
  Write-Host "请检查 SWDIO/SWCLK/VTref/GND、目标板供电及日志。"
  exit 1
}

Write-Host "== 烧写完成 ==" -ForegroundColor Green
Write-Host "  固件 SHA-256: $Actual"
Write-Host "  烧写前备份: $Pre"
Write-Host "  预检日志: $PreflightLog"
Write-Host "  烧写日志: $FlashLog"
Write-Host "  完成后请物理断电再上电。"
