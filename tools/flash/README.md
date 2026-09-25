# PC12M-2 独立烧写工具

本包用于把 CI 构建好的固件烧写到板子，**无需安装任何编译环境**
（arm-none-eabi-gcc / Python / Unicorn 都不需要），J-Link 已随包附带（免安装）。

## 内容

| 文件 | 说明 |
|---|---|
| `flash_release.bat` | Windows 双击启动器，自动调用 PowerShell 烧写脚本 |
| `flash_release.ps1` | Windows PowerShell 版烧写脚本（Windows 10/11 自带 PowerShell） |
| `flash_release.sh` | Git Bash 版烧写脚本（需 Git for Windows） |
| `release/firmware.bin` | 打包时的最新固件 |
| `release/firmware.bin.sha256` | 固件 SHA-256 校验值 |
| `jlink/` | 免安装打包版 J-Link（`JLink.exe` + DLL + USB 驱动） |

## 使用方法（Windows）

解压本 zip 后，直接双击 `flash_release.bat` 即可烧写。窗口会在完成或报错后暂停，
按任意键关闭。烧写前会自动检查 J-Link USB 驱动；未安装时会弹出 Windows UAC，
获得管理员权限后直接运行包内官方驱动安装器。

也可在 PowerShell / CMD 中执行：

```powershell
powershell -ExecutionPolicy Bypass -File flash_release.ps1
```

包内已含有打包时的最新固件。默认从运行命令时的当前目录读取 `release/firmware.bin`，如果存在
`release/firmware.bin.sha256` 则先校验哈希，全程不会从 GitHub 下载。常用参数：

| 参数 | 说明 |
|---|---|
| `-Bin x.bin` | 指定其他本地固件文件 |
| `-Serial <SN>` | 多台 J-Link 时指定序列号 |
| `-DryRun` | 只校验本地固件，不连接设备、不烧写 |

### Git Bash 版

```bash
bash flash_release.sh [--bin x.bin] [--dry-run]
```

## 烧写流程

脚本使用两阶段安全门。第一阶段只连接、检查 VTref/CRP 并生成带时间戳的完整备份；
只有备份大小和 CRP 校验通过，才会启动第二阶段擦除：

```
connect → VTref/CRP 检查 → savebin 完整备份 → 安全门 → erase → loadbin → verifybin → 关键地址读回 → 复位运行
```

成功标志：`verifybin` 输出 `Verify successful`（板上内容与固件完全一致）。备份保存在
`backup/`，预检和烧写日志保存在 `release/`，文件名均含时间戳。
烧写日志还会记录中断向量表、CRP 字和 `0x6B78` 起的产品版本信息区原始数据。

## 硬件前置（重要）

- 断开**市电 / 门极 / 功率负载**，板子仅接**控制电**。
- P12 排针接线：VTref=P1、GND=P2、SWDIO=P6、nRESET=P7、SWCLK=P8。
- 完成后请**物理断电再上电**（J-Link 驱动复位可能悬挂 SWD）。

## 首次插 J-Link 未被识别

双击烧写时脚本会自动检查并安装。如自动安装失败，可手动运行
`jlink\USBDriver\InstDrivers.exe`。

## 排查

| 现象 | 处置 |
|---|---|
| `connect` 找不到 SW-DP | 先查四根主信号线（SWDIO/SWCLK/VTref/GND）接触，重新插紧 |
| 固件复用 SWD 脚连不上 | 需 connect-under-reset（见仓库 `操作文档.md` §3.2） |
| 校验失败 / 烧写报错 | 查接线、供电、驱动 |
