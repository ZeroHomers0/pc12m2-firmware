# AGENTS.md — PC12M-2 十二相 SCR 控制板（LPC1765FBD100）

> 本文件是 AI 接手项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目与当前状态

目标设备为 **PC12M-2 十二相 SCR 移相触发功率控制板**，MCU 为 NXP **LPC1765FBD100**。
该板是参考项目 **PC6M-10（六相）** 的孪生板：MCU 与外围电路几乎完全一致，唯一实质性差异是
**触发相数 6 → 12**（触发脉冲序列、通道数、部分寄存器映射随之变化），因此**控制程序只有细微差别**。

参考项目（已逆向完成、可编译、实机验证）位于 `D:\code\LPC1765FBD100\decompiled`，
其中 `AGENTS.md`、`firmware/`、`docs/analysis/`、`evidence/` 是理解 6 相位固件的金标准。
本项目的核心方法论：**先备份 12 相位原固件 → 与 6 相位参考固件做差异对比 → 定位 12 相位改动点 →
据此搭建可编译工程**。

### 当前进度（2026-08-30）

- 项目结构已按参考项目搭建；`tools/jlink` 已复制免安装 J-Link 最小集。
- SWD 不可用（本机无 J-Link 探针枚举），改走 **ISP 通道（Flash Magic / fm.exe）**。
- **原固件备份已完成（2026-08-30）**：ISP 9600 波特分块读取整片 256 KiB，合并为
  `backup/pc12m2_orig.bin`，SHA-256 `2BC60868…BD271BD1`，见 `backup/BACKUP_RECORD.md`。
  - 已确认：Signature `0x26013733`=LPC1765、**CRP @0x2FC = 0xFFFFFFFF（无读保护）**、
    向量表有效（SP=0x100029A0、Reset=0x000001A9）、抽样重读 0 差异。
  - 与 6 相位参考固件逐字节差异 70629 处（重新编译所致），后续差异分析须在函数级进行。
- **可复用资产已从 PC6M 参考项目整体搬入（2026-08-30）**：`firmware/` 源码基座、
  `test/` 基座、`tools/` 通用工具、`docs/analysis|project|w8` 逆向结论与参考经验、
  `evidence/reverse|hardware` 全部证据；参考固件对照置于 `evidence/reverse/reference/`
  （`PC6M10_LPC1765.bin` SHA `dd629eac…`、`PC6M10_selfbuild.bin` SHA `80e21528…`）。
  搬运边界：不搬参考项目 `docs/history/`、进度文档（PROJECT_STATUS/W8_TEST_MASTER）、
  `backup/` 自身备份区。全部 342 个文件哈希核对一致（详见 `DOCUMENTATION_INDEX.md`）。

## 必须遵守

- 全程中文交流。
- 本板固件与参考板固件的差异分析，**必须基于原始 BIN 对比**，不能凭空假设相数。
- 反编译、反汇编和硬件原始资料属于证据，不得因为当前未引用而删除。
- **备份铁律**：未核对备份哈希一致前，绝不擦写 Flash；CRP 若非 0xFFFFFFFF 绝不 unlock/recover/mass erase。
- J-Link 一律调用仓库打包版 `tools/jlink/JLink.exe`；本机未枚举到 J-Link 时走 ISP 通道。
- 上板接电只接控制电，断开市电/门极/功率负载。

## 目录职责

```text
backup/                     固件备份（part*.hex 分块、pc12m2_orig.bin 成品、日志）——已 gitignore，勿提交
firmware/                   可编译工程源码基座（自参考项目搬入，12 相改动直接在此进行）
docs/project/               应用指南、数据段（自参考项目搬入；PROJECT_STATUS 为本项目自建）
docs/analysis/              逆向结论：硬件印证/I2C/Modbus/状态机/菜单参数（自参考项目搬入）
docs/w8/                    实机验证：参考项目 W8 经验文档（非本项目进度；TEST_MASTER 本项目自建）
docs/history/               历史计划、进度与审计（本项目自建，参考项目 history 未搬入）
evidence/hardware/          BOM、接线表、手册等原始硬件证据（12 相位板资料待收集）
evidence/reverse/           原始反编译、反汇编（含全量 asm_full.txt）和过程报告
evidence/reverse/reference/ 参考固件对照（PC6M10_LPC1765.bin / PC6M10_selfbuild.bin）
test/                       静态与 Unicorn 执行级测试基座（自参考项目搬入，触发断言待 12 相适配）
tools/                      审计/生成/Ghidra/验证/W8 实机工具（自参考项目搬入）；jlink/ 为打包 J-Link
```

## 常用入口

- 备份原固件（只读）：`python tools/w8/w8_backup_orig.py`（SWD 版，需 J-Link）
  或按 `backup/read_full_9600.cmd` 走 ISP（fm.exe）。
- 合并分块备份：`python tools/w8/w8_combine_hex.py`
- 构建固件：`cd firmware && bash build.sh`（源码基座已就绪，12 相改动前先验证可复现编译）
- 等价性验证：`python tools/verification/verify_firmware_equivalence.py`
- 全部测试：`python test/run_tests.py`
- SWD/ISP 操作命令见 `操作文档.md`。

## 关键限制与下一步

- **下一步 1**：把成品 `backup/pc12m2_orig.bin` 复制到第二处独立物理位置（U 盘/另一台机）。
- **下一步 2**：与参考项目 `LPC1765.bin` 做**函数级/反汇编级**差异对比，定位 12 相位改动点
  （触发序列、引脚、寄存器映射、参数表）。字节级差异 70629 处为重新编译所致，无直接意义。
- **下一步 3**：基于差异分析搭建/调整 `firmware/` 可编译工程。
