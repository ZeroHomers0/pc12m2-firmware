# PC12M-2 独立烧写工具

本包用于把 CI 构建好的固件烧写到板子，**无需安装任何编译环境**
（arm-none-eabi-gcc / Python / Unicorn 都不需要），J-Link 已随包附带（免安装）。

## 内容

| 文件 | 说明 |
|---|---|
| `flash_release.ps1` | Windows PowerShell 版烧写脚本（Windows 10/11 自带 PowerShell） |
| `flash_release.sh` | Git Bash 版烧写脚本（需 Git for Windows） |
| `jlink/` | 免安装打包版 J-Link（`JLink.exe` + DLL + USB 驱动） |

## 使用方法（Windows，推荐 .ps1）

解压本 zip 后，在解压目录打开 PowerShell / CMD，执行：

```powershell
powershell -ExecutionPolicy Bypass -File flash_release.ps1
```

默认拉取最新构建（`latest`）并烧写。常用参数：

| 参数 | 说明 |
|---|---|
| `-Tag v1.0` | 指定版本 tag（默认 `latest`） |
| `-Bin x.bin` | 用本地固件文件（不联网下载） |
| `-Repo 用户/仓库` | 指定固件所在仓库 |
| `-Mirror https://ghproxy.com/` | 国内镜像加速下载（访问 GitHub 不稳时用） |
| `-Serial <SN>` | 多台 J-Link 时指定序列号 |
| `-DryRun` | 只下载 + 校验，不烧写 |

### Git Bash 版

```bash
bash flash_release.sh [--tag v1.0] [--bin x.bin] [--mirror https://ghproxy.com/] [--dry-run]
```

## 烧写流程

脚本按以下标准序列执行（擦除前自动备份当前 Flash）：

```
connect → savebin 备份 → CRP 检查 → erase → loadbin → verifybin → 复位运行
```

成功标志：`verifybin` 输出 `Verify successful`（板上内容与固件完全一致）。

## 硬件前置（重要）

- 断开**市电 / 门极 / 功率负载**，板子仅接**控制电**。
- P12 排针接线：VTref=P1、GND=P2、SWDIO=P6、nRESET=P7、SWCLK=P8。
- 完成后请**物理断电再上电**（J-Link 驱动复位可能悬挂 SWD）。

## 首次插 J-Link 未被识别

运行一次 `jlink\USBDriver\InstDrivers.exe` 安装 USB 驱动即可。

## 排查

| 现象 | 处置 |
|---|---|
| `connect` 找不到 SW-DP | 先查四根主信号线（SWDIO/SWCLK/VTref/GND）接触，重新插紧 |
| 固件复用 SWD 脚连不上 | 需 connect-under-reset（见仓库 `操作文档.md` §3.2） |
| 校验失败 / 烧写报错 | 查接线、供电、驱动 |
