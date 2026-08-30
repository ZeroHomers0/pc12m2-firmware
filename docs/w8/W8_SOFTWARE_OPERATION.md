# W8 软件清单 + 详细操作流程

> 日期：2026-08-22 ｜ 配合 `W8_HARDWARE_TEST_2026-08-22.md`（硬件接线/预期值/判定）使用
> 本篇聚焦**软件侧**：W8 需要哪些软件、怎么装、每一步具体怎么操作（示波器挡位、信号发生器设置、串口连接…）
> **构建固件与 SWD/ISP 烧写命令统一以根目录 `操作文档.md` 为准**（2026-08-29 命令单源定论）；
> 本篇不再重复这些命令，只保留 W8 特有的软件清单、仪器操作与调试验证。

---

## 1. W8 需要什么（软件 + 硬件配套）

### 1.1 免费软件固定选型（仅官方来源）

| # | 软件 | 费用/必要性 | 用途 | 官方下载地址 |
|---|---|---|---|---|
| 1 | **Arm GNU Toolchain 14.2.Rel1 (`arm-none-eabi`)** | 免费；**必装并固定版本** | 唯一固件编译器，内含 `arm-none-eabi-gdb` | [Windows x64 14.2.Rel1 官方安装器](https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.exe) / [Arm 官方发布页](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| 2 | **SEGGER J-Link 软件** | 软件免费供 J-Link/J-Trace 硬件用户使用；**本仓库已打包免安装最小集 `tools/jlink`**（`JLink.exe` + DLL + USB 驱动，全项目唯一调用路径，无需安装）；仅需 GDB Server 等附加功能时才装官方完整包 | 提供 `JLink.exe`（SWD 连接、备份、烧录、校验）与 USB 驱动；用于 P12 SWD 调试链路 | [SEGGER J-Link 官方下载页](https://www.segger.com/downloads/jlink/) |
| 3 | **Python 3.12（64-bit）** | 免费；**必装** | 运行 Modbus、串口枚举和 CSV 波形分析脚本 | [Python 3.12.10 Windows x64 官方安装器](https://www.python.org/ftp/python/3.12.10/python-3.12.10-amd64.exe) / [版本说明页](https://www.python.org/downloads/release/python-31210/) |
| 4 | **MinimalModbus** | 免费开源；阶段 B 必装 | Modbus-RTU 主站脚本 | [MinimalModbus 官方文档](https://minimalmodbus.readthedocs.io/en/stable/installation.html) / [PyPI 官方包页](https://pypi.org/project/MinimalModbus/) |
| 5 | **pySerial** | 免费开源；阶段 B 必装 | Windows COM 口和 USB-RS485 通信 | [pySerial PyPI 官方包页](https://pypi.org/project/pyserial/) |
| 6 | **Git for Windows** | 免费开源；建议安装 | 版本管理、保存测试记录；不要把原 Flash 备份提交到公共仓库 | [Git for Windows 官方下载](https://git-scm.com/download/win) |
| 7 | **PulseView / sigrok** | 免费开源；可选 | 配合兼容逻辑分析仪观察多路数字触发时序，不能替代示波器测模拟量/强电 | [PulseView 0.4.2 Windows x64 官方安装器](https://sigrok.org/download/binary/pulseview/pulseview-0.4.2-64bit-static-release-installer.exe) / [sigrok官方下载页](https://sigrok.org/wiki/Downloads) |
| 8 | **示波器厂商软件** | 仅选择仪器厂家免费版本；可选 | 截图、CSV 导出和远程控制 | 从示波器铭牌对应的厂家官网进入“支持/下载”，不使用第三方下载站 |
| 9 | **USB-RS485 官方驱动** | 通常免费；按硬件芯片选装 | 让转换器显示为 COM 口 | 先由 Windows Update 安装；仍缺驱动时只从转换器厂商或芯片原厂官网下载，不能仅凭外壳猜测 CH340/FTDI |

安装命令（Git / 工具链 / Python / pip 依赖）与版本确认见 `操作文档.md` §1。
本工程固定使用已经验证过的 Arm GNU Toolchain **14.2.Rel1**，不要在实机验证前切换到最新
15.x。Python 推荐 3.12；MinimalModbus 官方支持 Python 3.8 及以上。J-Link Commander 和
J-Link GDB Server 已包含在同一个免费软件包中，不需要另购 J-Flash，也不需要安装 Keil、
MCUXpresso、OpenOCD、pyOCD 或付费 Modbus 软件。

> **只需标准库、无需第三方**：`tools/verify_*.py`（逆向验证脚本）——纯标准库直接跑。
> **需要 pip 装**：W8 的 Modbus 脚本（依赖 minimalmodbus + pyserial），先装这两个包。
> **J-Link** 仓库已打包免安装最小集 `tools\jlink\JLink.exe`（**全项目 J-Link 唯一调用路径**，
> 无需安装 J-Link 软件）；USB 驱动缺失时运行 `tools\jlink\USBDriver\InstDrivers.exe` 安装（官方驱动）。
> HEX/ELF 用 `LoadFile`；BIN 用 `LoadFile 文件.bin 0x00000000`，无需另装 J-Flash。

### 1.2 Windows 环境安装与自检

手动安装（按 §1.1 表，仅官方来源）：Git for Windows、Arm GNU Toolchain 14.2.Rel1、
J-Link USB 驱动，具体命令见 `操作文档.md` §1。

构建与回归自检命令见 `操作文档.md` §2（`cd firmware && bash build.sh` +
`python test/run_tests.py` + `python tools/verification/verify_firmware_equivalence.py`）。
构建通过标准（尺寸 `text 61968 / data 3000 / bss 2188`、`firmware.bin` SHA-256、
测试 11/11、独立验证器全 PASS）见 `操作文档.md` §2。

### 1.3 硬件配套（跟软件配合用的仪器）

| # | 硬件 | 用途 | 阶段 |
|---|---|---|---|
| 1 | **SEGGER J-Link（SWD）** | 识别 / 备份 / 烧写 / 调试 LPC1765 | A |
| 2 | **USB-RS485 转换器** | 电脑 ↔ 板子 UART3 通信 | B |
| 3 | **示波器 ≥2 通道** | 抓触发脉冲 / 输出波形 | C / D |
| 4 | **差分探头 / 隔离通道** | **测强电必须**（普通探头测高压会烧示波器） | D |
| 5 | **信号发生器** | 输出 50Hz 方波模拟过零 | C |
| 6 | **万用表** | 量电压/电流/电阻 | A / B / D |

---

## A. 阶段 A：J-Link 调试/烧写链路（现在可做）

> 只证明「J-Link ↔ P12 ↔ LPC1765 能连、能读、能写、能跑」，不触碰 SCR/主整机。
> 硬件接线、P12 引脚定义见 `W8_HARDWARE_TEST_2026-08-22.md`；各阶段通过标准见 `W8_TEST_MASTER.md`。
> **连接 / 备份 / 烧写命令以根目录 `操作文档.md` §3 为准**（2026-08-29 实测定型）；
> 本篇只保留 W8 特有的 blink 最小验证与断点/单步调试验证。

### A.1 连接 / 备份 / 烧写（命令见 操作文档.md §3）
- 连接探测：`操作文档.md` §3.1（`speed 100` 普通 connect，成功标志 `Found SW-DP` + `IDCode` + `Cortex-M3`）
- connect-under-reset（应用固件复用 SWD 脚时必用）：`操作文档.md` §3.2
- 备份原固件 + CRP 检查：`操作文档.md` §3.3（`savebin` + `mem32 0x2FC` + 铁律）
- 完整烧写序列（erase → loadbin → verifybin → 复位运行）：`操作文档.md` §3.4
- 失败排查：`操作文档.md` §3.6

### A.2 烧写 / 运行最小验证（blink，W8 特有）
用独立小程序（不烧真实固件）验证「能烧、能跑」：

```
loadfile blink.hex             ; HEX 自带地址
loadfile blink.bin 0x00000000  ; BIN 必须指定起始地址 0
r                              ; 复位
g                              ; 运行（Go）
```
目标板 LED 闪烁 = 烧写/运行链打通。

### A.3 断点 / 单步（验证调试链路）
```
halt            ; 暂停
setbp 0x<addr>  ; 设断点
go              ; 恢复运行，命中即停
st              ; 单步     mem / regs  读内存/寄存器
```

**阶段 A 通过标准**：能 `connect`、能读设备 ID、能备份原 Flash、能烧 blink 且复位后运行、能断点单步。

---

## 2. 串口连接（阶段 B 的第一步）

### 2.1 装驱动 / 识别 COM 口
1. 插上 USB-RS485 转换器。
2. 确认系统识别到它：
   - **图形界面**：右键"此电脑 → 属性 → 设备管理器"→ 展开"端口 (COM 和 LPT)"，应出现 `USB-SERIAL CH340 (COMx)` 或 `USB Serial Port (COMx)`。**记下这个 COMx。**
   - **命令行**：运行
     ```bash
     python tools/w8/w8_serial_detect.py
     ```
     会自动列出所有 COM 口及描述，你就能认出哪个是 RS485。
3. 若"其他设备"里有黄色感叹号 → **没装驱动**，去转换器芯片官网装（常见 CH340：`ch340.com`；FTDI：`ftdichip.com`）。

### 2.2 接线
- USB-RS485 的 **A（也叫 D+/B+）→ 板 UART3 的 A**；**B（D−/B−）→ 板 B**。
- 不确定就**先接一副（A-B 对 A-B），读不到再对调**（RS485 反接是最常见坑）。
- 可选共地：板是 ADM2483 隔离模块，可先共地试，不行再全隔离。

### 2.3 软测：确认能读到
用 `tools/w8/w8_modbus_test.py` 的“枚举”模式（或直接跑），能读到 reg 就 OK。

---

## 3. Modbus 测试（阶段 B）

### 3.1 跑脚本
```bash
cd /d/code/LPC1765FBD100/decompiled
python tools/w8/w8_modbus_test.py              # 默认 COM5 / 9600 / addr=1
python tools/w8/w8_modbus_test.py --port COM8 --baud 9600 --addr 1   # 自定义
```
脚本会依次：
1. 读 reg 40~45（Ug/IA/IB/IC/IF/Uf），未接主回路应≈0 或面板值；
2. 写 reg40=500 再回读 → **验证「读=实测 / 写=注入」不对称**（回读≠500 才正常）；
3. 写 reg61=1 / 0 → 测 P0.20 / RLY3（同时人工用万用表看引脚电平）；
4. 打印每个测试的 PASS/FAIL。

### 3.2 常见报错
| 报错 | 原因 | 解决 |
|---|---|---|
| `No response` | 地址/波特率不对 / A-B 接反 / 板子没上电 | 先查板子通讯菜单地址与主机一致；对调 A-B；确认板子供电 |
| `CRC error` | 数据被干扰 / 波特率不匹配 | 降波特率试 / 缩短线 / 校验位一致 |
| `ValueError` 串口占用 | COM 口被其他程序占用 | 关掉串口助手/其他程序 |

> 若通讯参数（地址/波特率/校验）在面板改了，记得**重启板子生效**（反汇编确证通讯参数需重启）。

---

## 4. 示波器设置（阶段 C：抓触发脉冲；阶段 D：抓输出波形）

> 关键：**先弄清你示波器的探头是 1x 还是 10x**，通道菜单里要选对，否则读数×10/÷10。
> **测 MCU 引脚（≤3.3V）用普通 10x 探头安全；测强电（晶闸管输出）必须差分/隔离探头。**

### 4.1 通用基础设置（抓触发脉冲用）
| 旋钮/菜单 | 设置 | 说明 |
|---|---|---|
| 垂直挡位 V/div | **1V/div**（再看幅值微调） | MCU 引脚脉冲 ~3.3V，1V/div 能看清高/低 |
| 水平时基 s/div | **5ms/div** 或 **2ms/div** | 50Hz 周期 20ms；5ms/div 一屏约 4 个周期，够看 |
| 触发源 | 选你探的那根通道（如 CH1） | 让它稳定不跳动 |
| 触发沿 | **上升沿** | 触发脉冲是上升沿 |
| 触发电平 | 约 **1.5V**（V/2 即可） | 避开噪声抖动 |
| 探头比例 | 看探头是 1x 还是 10x，在 CH 菜单里选对 | 避免读数错误 |
| 耦合 | **DC** | 需要看直流电平 |

### 4.2 怎么读出「脉宽 / 周期 / 相邻组间隔」（阶段 C 核心）
1. 抓到稳定波形后，按示波器的 **CURSORS（光标）**：
   - 测**脉宽**：两条竖光标卡在**一个脉冲的上升沿和下降沿** → 读 Δt（应为 ~667μs=12°）。
   - 测**周期**：两条竖光标卡在**相邻两个同组脉冲**之间 → 读 Δt（应为 ~20ms=50Hz 周期）。
   - 测**相邻组间隔**：两条光标卡在 **G1 与 G2 对应脉冲**之间 → 读 Δt（应为 ~3.333ms=60°）。
2. 手动换算电角度：`电角度 = Δt / 周期 × 360°`。例：3.333ms / 20ms × 360° = 60° ✓
3. **也可存 CSV 给脚本自动判**：见 §5（示波器导出 CSV → `w8_analyze_wave.py`）。

### 4.3 抓软起动渐升（阶段 C5）
- 示波器 **Run/Stop** 先按 **Stop**，再切到 **Single（单次）**，再触发启动动作 → 会定格启动瞬间，能看出**触发角逐渐前移**。

### 4.4 抓强电输出波形（阶段 D）
- **必须差分/隔离探头**，挂晶闸管**输出端**（对中性点/对地）。
- 看到的是「**移相控制下的正弦残缺波**」：每半周一段正弦 + 一段**缺口**（缺口=触发角）。缺口宽度随给定变化。

---

## 5. 波形分析（两种方式）

- **方式1 手动**：用示波器 CURSORS 读 Δt，对照 §4.2 换算，看是否 12°/60°/3.333ms（写进 W8 回填模板）。
- **方式2 交给脚本**：把示波器**导出的 CSV**（两列：时间、电压，如 CH1.CSV）拷到电脑，运行：
  ```bash
  python tools/w8/w8_analyze_wave.py 波形.csv --v-thresh 1.5
  ```
  脚本自动统计：找到多少个脉冲、每个脉宽、主周期、相邻脉冲间隔，换算成电角度的 60°/12°，并打印判定。**可把输出贴回给我进一步分析。**

### 怎么从示波器导出 CSV
- 多数示波器：把波形**保存为 CSV/USB**，或厂商软件里"导出数据"。
- 要求：**至少 2 列**（时间 [s]，电压 [V]）。采样点要多（导出前把时基调大到能覆盖几个完整周期）。

---

## 6. 信号发生器设置（阶段 C1：模拟过零）

1. 波形 **Square / 方波**
2. 频率 **50Hz**（或按你的 60Hz 电网）
3. 幅值 **0 → 3.3V**（板子逻辑电平；若板子类型不同，对照原理图逻辑电源 3.3/5V）
4. 偏移 **0V**（即从 0 到 3.3V 跳变）
5. 接**同步过零输入**（反汇编：EINT1/2/3 = P2.11/12/13，⚠ 以板子实物/原理图为准），**至少接一个**（如 P2.13/TW）。

> 接上过零后，MCU 才会进入"有电网"状态去发触发脉冲。**没接过零 = 没有触发脉冲**（阶段 C 最常见失误）。

---

## 7. AI 怎么帮你（把数据贴回来）

| 你手上有什么 | 给我什么 | 我能做什么 |
|---|---|---|
| Modbus 读数 | `w8_modbus_test.py` 的输出 | 核对 reg40-45 是否合理、判 PASS/FAIL |
| 示波器波形 | 截图 / `w8_analyze_wave.py` 的输出 | 判是否 12°/60°/3.333ms，定位哪组相位差 |
| 强电输出波形 | 截图 + 触发角读数 | 核对移相角 = 180 − 给定趋势 |
| 标定数据 | Modbus 原始值 + 万用表实测值 | 算比例系数，对照固件标定除数 reg51-55 |
| 任何 FAIL | 现象 + 读数 | 结合反汇编逻辑反推原因 |

> **回填模板**见 `W8_HARDWARE_TEST_2026-08-22.md`。复制到记事本边测边填，最后整段贴给我。

---

## 8. 一键准备清单（开测前勾一遍）

- [ ] 环境自检完成（构建命令见 `操作文档.md` §2）
- [ ] Python 依赖已安装（命令见 `操作文档.md` §1）
- [ ] USB-RS485 已识别成 COM 口（`w8_serial_detect.py` 能看到）
- [ ] 示波器探头比例选对、挡位按 §4.1
- [ ] 信号发生器已设 50Hz 0-3.3V 方波
- [ ] 板子已上电（先只给控制电）
- [ ] （L3 强电前）差分/隔离探头就位、持证电工在场

---

## 9. 需要用到的 W8 脚本一览

| 脚本 | 命令 | 依赖 |
|---|---|---|
| `tools/w8/w8_serial_detect.py` | `python tools/w8/w8_serial_detect.py` | pyserial |
| `tools/w8/w8_modbus_test.py` | `python tools/w8/w8_modbus_test.py --port COMx --addr 1` | minimalmodbus |
| `tools/w8/w8_analyze_wave.py` | `python tools/w8/w8_analyze_wave.py 波形.csv --v-thresh 1.5` | 标准库（csv） |
