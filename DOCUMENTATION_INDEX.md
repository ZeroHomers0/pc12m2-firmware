# 文档索引

> 最后更新：2026-08-30。当前进度优先级：`AGENTS.md` → 本文 → `backup/BACKUP_RECORD.md`。
> 本项目是 6 相参考项目 PC6M-10（`D:\code\LPC1765FBD100\decompiled`，已逆向完成、可编译）的
> 12 相孪生板 PC12M-2 工程。以下文档多数为**从参考项目整体搬入的可复用资产**，
> 其内容描述的硬件/协议/算法对两板一致，可作分析基础；凡标注「参考经验」者属于
> 参考项目的实机记录，**不代表本项目现状**。

## 本项目自有文档（现状/进度权威）

| 主题 | 文档 |
|---|---|
| 项目状态与目录职责 | `AGENTS.md`（唯一权威上下文） |
| 构建与烧写操作 | `操作文档.md`（构建 + SWD + ISP 全流程手册，含**附录：PC6M 踩坑与 Bug 总结**，以参考版为底本已按本项目适配） |
| 原固件备份记录 | `backup/BACKUP_RECORD.md`（备份方法、SHA-256、CRP、铁律） |

## 逆向结论（从 PC6M 参考项目搬入，可作 12 相分析基础）

| 主题 | 文档 |
|---|---|
| 硬件印证 | `docs/analysis/HARDWARE_VERIFICATION_2026-08-20.md`（触发引脚/驱动链/12 脉波扩展） |
| I2C/参数 | `docs/analysis/I2C_PARAM_SYNC.md` |
| UART3/Modbus | `docs/analysis/UART3_PROTOCOL.md` |
| 状态机 | `docs/analysis/STATE_MACHINE_ANALYSIS.md` |
| 菜单参数 | `docs/analysis/MENU_PARAMETER_MAPPING.md` |
| 应用指南 | `docs/project/APPLICATION_GUIDE_2026-08-21.md` |
| 数据段 | `docs/project/DATA_SEGMENT_2026-08-21.md` |

## W8 实机验证（参考经验，非本项目进度）

> 从 PC6M 参考项目搬入。**进度权威只由本项目日后自建**；下表文档是参考项目当时的
> 实机记录与代码级预案，**描述的是 6 相板**，12 相板实机前须按硬件差异调整。

| 文档 | 内容 |
|---|---|
| `W8_STAGE2_CODE_PREPLAN_2026-08-27.md` | ★ 触发引擎代码级预案（TIMER1 240 步/6 窗口、序列表）——12 相差异分析直接基础 |
| `W8_ISSUE_LOG_2026-08-27.md` | ★ 坑/bug 时间线总汇（SWD 失联、CRP、指针算术、FAULT 门控、DISP_SEL 等 11 项） |
| `W8_ISSUE_FIX_2026-08-28.md` | ★ 反编译回归修复（编辑态闪烁、LED 错译） |
| `W8_ISSUE_FIX_2026-08-30.md` | ★ 6 项实机修复（Modbus 大端、枚举钳位、恢复出厂"M"、复位坐标） |
| `W8_DISP_SEL_FIX_2026-08-27.md` | ★ case1 上下键无效根因 1+2、DISP_SEL=控制方式语义 |
| `W8_HARDWARE_TEST_2026-08-22.md` | 硬件接线/时序/安全前提规范 |
| `W8_ONBOARDING_2026-08-22.md` | 实机导航入口与停止线 |
| `W8_SOFTWARE_OPERATION.md` | 软件/示波器/信号源操作手册 |
| `W8_JLINK_DEBUG_2026-08-24.md` | SWD 调试脚复用（connect-under-reset）经验 |
| `W8_ISP_FLASH_2026-08-26.md` | ISP 擦除/CRP 风险/恢复经验 |
| 其余 `W8_*_2026-08-2x.md` | 参考项目当时的实机问题记录（仅追溯） |

## 证据与历史

- 原始硬件证据：`evidence/hardware/`（参考板 BOM/接线/手册 + 12 相位板资料待收集）。
- 原始反编译、反汇编和过程报告：`evidence/reverse/`（`decompiled/`、`disassembly/` 含
  逐函数与全量 `asm_full.txt`、`reports/`、`state_machine/`）。
- 参考固件对照（差异分析基准）：`evidence/reverse/reference/`：
  - `PC6M10_LPC1765.bin`（原始 6 相固件金标准，SHA `dd629eac…f13f65`）
  - `PC6M10_selfbuild.bin`（参考项目自编译固件，SHA `80e21528…cdbf`）
- 历史计划与进度：`docs/history/`（本项目自建；参考项目 `docs/history/` 未搬入）。

## 可复用工程基座（从 PC6M 参考项目搬入）

| 目录 | 说明 |
|---|---|
| `firmware/` | GCC 可编译工程源码基座（MCU+外设一致，12 相改动直接在此进行） |
| `test/` | 离线测试基座（Unicorn 执行级等价性测试，触发相关断言留待 12 相适配） |
| `tools/` | 审计/生成/Ghidra/验证/W8 实机工具（`jlink/` 免安装 J-Link；`w8/` 含备份与实机脚本） |

## 逆向流程参考（参考经验，非本项目进度）

| 文档 | 内容 |
|---|---|
| `docs/REFERENCE_REVERSE_WORKFLOW_2026-08-30.md` | ★ 参考项目 PC6M 逆向**全流程**总结：Ghidra 反编译 → 目标 A 架构确证 → 目标 B GCC 工程重建 → A/B 差分验证 → Codex 独立审计 → 烧写入板实机验证，含全部工具与命令 |
