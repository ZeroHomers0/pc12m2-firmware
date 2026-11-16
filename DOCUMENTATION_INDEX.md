# PC12M-2 文档索引

> 更新日期：2026-11-16。优先级：`AGENTS.md` → 本文 → `docs/project/PROJECT_STATUS.md`。
>
> **2026-09-02 结论**：P5/113 覆盖/认证放行/产品信息定制/X-O 字形修复全部完成，自编译固件已
> 烧写入板并经用户**直接实机测试通过**。**W8 分级实测流程未按计划执行、已废弃**——用户未按
> W8 分级测试；`docs/w8/` 分级文档保留为历史流程档案，进度以 `AGENTS.md` 与本文为准。

> **2026-11-13**：同步菜单文字、全高α及min/max下标、20秒背光；本次未烧写。构建尺寸、SHA及专项入口见 `AGENTS.md`。回归4/4（含113例扩展覆盖）、菜单专项、背光28例及十二相完整A/B全部通过。

> **2026-11-14**：菜单标题改为“同步校准”，补齐同/步字形；当前构建见 `AGENTS.md`，本次尚未烧写。

> **2026-11-16**：端子事件独立命名，新增1176＋88例原BIN回归；十二相原事件码即为正确值7/8，固件哈希不变。

> **2026-11-16 菜单文案**：模选/解限、瞬动/保持、同步参数校准及手动均流/参数设置；字形与专项已更新，尚未烧写。当前构建见 `AGENTS.md`。

## 当前项目

| 主题 | 文档 |
|---|---|
| 权威上下文 | `AGENTS.md` |
| 项目状态 | `docs/project/PROJECT_STATUS.md` |
| 构建、备份、烧写 | `操作文档.md` |
| 逆向与重建流程 | `docs/analysis/REVERSE_WORKFLOW_2026-08-31.md` |
| 应用与模块边界 | `docs/project/APPLICATION_GUIDE_2026-08-31.md` |
| 内存与数据布局 | `docs/project/DATA_SEGMENT_2026-08-31.md` |
| 发布与免环境烧写工具 | `release/flash-tool-pc12m2.zip`（PowerShell 原生烧写 `tools/` 亦含入口） |

## 十二相分析

| 主题 | 文档 |
|---|---|
| P5 A/B 验证 + 测试覆盖/认证放行进度 | `docs/analysis/P5_VERIFICATION_PROGRESS.md` |
| 测试覆盖查漏（任务 #4，113/113） | `test/emulation/test_extra_coverage_12.py` + 6p 侧 `PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md` |
| 6p W8 问题复查（任务 #5） | 6p 侧 `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md` |
| 硬件/引脚证据状态 | `docs/analysis/HARDWARE_VERIFICATION_2026-08-31.md` |
| 参数与 EEPROM | `docs/analysis/I2C_PARAM_SYNC.md` |
| Modbus | `docs/analysis/UART3_PROTOCOL.md` |
| 菜单状态机 | `docs/analysis/STATE_MACHINE_ANALYSIS.md` |
| 菜单参数 | `docs/analysis/MENU_PARAMETER_MAPPING.md` |
| 第三轮逐模块精读差异与修复（2026-11-17） | `docs/analysis/PC12M2_AUDIT_FIXES_2026-11-17.md` |

- **2026-11-17 通信校验越界绘制**：按原 BIN 0x8840/0x8852、0x632C/0x6340 把通信校验的 if/else 改为相等判断（`test/emulation/test_enum_range_12.py` 22/22）；同时修正显示探针 TIMEOUT3 预置值导致的空跑比较，按 0xFA 重跑逐屏显示矩阵。构建 text 65156、SHA `EA2920F1…9E728D2`；未烧写。

## 十二相实机验证

> ⚠️ W8 分级实测流程已废弃（用户未按分级测试，实机已直接通过）；下表文档为历史流程档案。

| 主题 | 文档 |
|---|---|
| 历史流程档案 | `docs/w8/W8_TEST_MASTER.md` |
| 历史入口与停止线 | `docs/w8/W8_ONBOARDING_2026-08-31.md` |
| 上板前离线状态（历史） | `docs/w8/W8_PRE_HARDWARE_VALIDATION_2026-08-31.md` |
| 硬件执行规范 | `docs/w8/W8_HARDWARE_TEST_2026-08-31.md` |
| 软件与仪器操作 | `docs/w8/W8_SOFTWARE_OPERATION.md` |

## 证据边界

- `evidence/reverse/disassembly/functions/`：十二相带引用注释的逐函数反汇编，当前权威。
- `evidence/reverse/disassembly/raw/`：十二相原始逐函数导出。
- `evidence/reverse/notes/`：十二相状态机人工拆解记录。
- `evidence/reverse/reports/`：十二相生成报告。
- `evidence/hardware/`：已按 `board/display/reports` 引入六相参考资料；PC12M-2 自身原理图、BOM 和接线资料仍待补。

六相实验记录和六相反汇编不在本仓库维护；引入的六相硬件参考资料统一放在
`evidence/hardware/`，不得直接当作十二相板级事实。
