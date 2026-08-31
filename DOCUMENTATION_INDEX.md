# PC12M-2 文档索引

> 更新日期：2026-08-31。优先级：`AGENTS.md` → 本文 → `docs/project/PROJECT_STATUS.md`。

## 当前项目

| 主题 | 文档 |
|---|---|
| 权威上下文 | `AGENTS.md` |
| 项目状态 | `docs/project/PROJECT_STATUS.md` |
| 构建、备份、烧写 | `操作文档.md` |
| 逆向与重建流程 | `docs/analysis/REVERSE_WORKFLOW_2026-08-31.md` |
| 应用与模块边界 | `docs/project/APPLICATION_GUIDE_2026-08-31.md` |
| 内存与数据布局 | `docs/project/DATA_SEGMENT_2026-08-31.md` |

## 十二相分析

| 主题 | 文档 |
|---|---|
| P5 A/B 验证进度 | `docs/analysis/P5_VERIFICATION_PROGRESS.md` |
| 硬件/引脚证据状态 | `docs/analysis/HARDWARE_VERIFICATION_2026-08-31.md` |
| 参数与 EEPROM | `docs/analysis/I2C_PARAM_SYNC.md` |
| Modbus | `docs/analysis/UART3_PROTOCOL.md` |
| 菜单状态机 | `docs/analysis/STATE_MACHINE_ANALYSIS.md` |
| 菜单参数 | `docs/analysis/MENU_PARAMETER_MAPPING.md` |

## 十二相实机验证

| 主题 | 文档 |
|---|---|
| 唯一进度总控 | `docs/w8/W8_TEST_MASTER.md` |
| 入口与停止线 | `docs/w8/W8_ONBOARDING_2026-08-31.md` |
| 上板前离线状态 | `docs/w8/W8_PRE_HARDWARE_VALIDATION_2026-08-31.md` |
| 硬件执行规范 | `docs/w8/W8_HARDWARE_TEST_2026-08-31.md` |
| 软件与仪器操作 | `docs/w8/W8_SOFTWARE_OPERATION.md` |

## 证据边界

- `evidence/reverse/disassembly/functions/`：十二相带引用注释的逐函数反汇编，当前权威。
- `evidence/reverse/disassembly/raw/`：十二相原始逐函数导出。
- `evidence/reverse/notes/`：十二相状态机人工拆解记录。
- `evidence/reverse/reports/`：十二相生成报告。
- `evidence/hardware/`：当前仅有显示面板资料；PC12M-2 原理图、BOM 和接线资料待补。

六相实验记录、六相原始资料和六相反汇编不在本仓库维护。
