# tools/jlink — 免安装 J-Link 最小集

来源：SEGGER J-Link V9.70（本机 `D:\software\SEGGER\JLink_V970` 安装目录，2026-08-27 拷贝）。
用途：本目录 `JLink.exe` 是**全项目 J-Link 唯一调用路径**（所有 SWD 烧写/探测命令与 AI 工具/脚本一律从这里调用，命令见 `操作文档.md` §4），**无需安装 J-Link 软件**，即拷即用。

## 内容

| 文件 | 说明 |
|---|---|
| `JLink.exe` + `JLinkARM.dll` | 32 位 Commander 核心。已验证脱离安装独立运行：同目录加载 DLL 成功 |
| `vcruntime140*.dll` / `msvcp140*.dll` / `concrt140.dll` / `vccorlib140.dll` | VC++ 运行库，免装 redist |
| `USBDriver/x64/` + `InstDrivers.exe` | 驱动兜底：首次插 J-Link 未自动识别时运行 `InstDrivers.exe` 装驱动 |

## 验证

独立目录运行 `JLink.exe` 输出（证明 exe + DLL 依赖完整）：

```text
SEGGER J-Link Commander V9.70 (Compiled Aug 19 2026 12:12:14)
DLL version V9.70, compiled Aug 19 2026 12:11:13
```

## 驱动说明

- Win10/11 插上 J-Link 一般**自动识别**（Windows Update / 内置 winusb），无需手动。
- 若个别机器未识别（如禁用了驱动自动更新）：运行 `USBDriver\InstDrivers.exe` 一次性装驱动。

## 许可

SEGGER J-Link 软件免费分发，允许随开发项目打包使用（绑定 SEGGER 硬件），禁止单独转售软件本体。
