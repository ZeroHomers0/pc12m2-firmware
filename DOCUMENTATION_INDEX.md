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
| 快速复核：立即数差分与覆盖门禁（2026-11-17） | `docs/analysis/PC12M2_FAST_REVIEW_2026-11-17.md` |

- **2026-11-17 通信校验越界绘制**：按原 BIN 0x8840/0x8852、0x632C/0x6340 把通信校验的 if/else 改为相等判断（`test/emulation/test_enum_range_12.py` 22/22）；同时修正显示探针 TIMEOUT3 预置值导致的空跑比较，按 0xFA 重跑逐屏显示矩阵。构建 text 65156、SHA `EA2920F1…9E728D2`；未烧写。

- **2026-11-17 快速复核**：比较类立即数差集 54 条全部定性为等价改写或数据区误判；覆盖门禁发现原机 6 处跨槽反馈复制原有矩阵仅命中 2 处，已新增 `test/emulation/test_feedback_copy_12.py` 补齐至 6/6（回归 8/8→9/9）。详见 `docs/analysis/PC12M2_FAST_REVIEW_2026-11-17.md`。未改业务源码、未烧写。

- **2026-11-17 状态机全矩阵与擦除帧项号修复**：把六相式「屏×项号×视图×按键×TIMEOUT3 档」全矩阵移植到十二相，修正探针自身五处缺陷（`BASE` 32 位覆盖 MENU 三字节、只比 SRAM 且跳过显示体、`MEM_WRITE` 钩子致新机函数体不执行、`disp_splash_screen` 误用 `0x1486`、pin22 地址写错）后得 **14688 例 bad=0**，并用两次注入缺陷验证检出能力（166/816、14/816）。矩阵检出 1 处真实差异：`MENU==3` 擦除帧沿用入口缓存的 `it`，而原机 15 处各自重读 `0x10001725`（`0x771a/0x772e/…`），故 `key==4`（已清 MENU2）那一帧应擦第 0 行。已改 `switch (*MENU2)`（`firmware/src/07_state_machine.c`），新增 `test/emulation/test_state_machine_erase_12.py`（112 例 + 15 处覆盖门禁，回退缺陷时 13 项失败）。构建 text 65148、BIN 72020 B、SHA `3383114779…B335FAB6`；回归 9/9→10/10，原 BIN 完整 A/B 全 PASS。未烧写。

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

- [2026-11-17 最新提交联合复核与执行完成门禁](docs/analysis/JOINT_VALIDATION_2026-11-17.md)

- [2026-11-17 十二相初始化与底层 I/O 覆盖补齐](docs/analysis/PC12M2_INIT_TRACE_2026-11-17.md)：为 21 个此前无执行级 A/B 的函数补对照，抓到并修复 `freq_adjust_sync` 的 `0xa98c`「停止」串未进字符串池导致渲染指令字节的缺陷。

## 差异消除实施方案（2026-11-17）

[PC6M／PC12M 固件逻辑差异消除与缺陷修复实施方案](docs/project/FIRMWARE_EQUIVALENCE_REMEDIATION_PLAN_2026-11-17.md)：以证据修正行为和验证失真，保留认证放行与已授权定制，不以删除代码或证据完成任务。该文件为实施设计，仅做代码/模拟验证，不包含实机验证或烧写，不代表已完成全量验收。

- [双项目近期修复独立复核](docs/analysis/INDEPENDENT_REPAIR_REVIEW_2026-11-17.md)

> **2026-11-17 软件复核第二批**：[启动顺序修复、连续软件链与未关闭分支缺口](docs/analysis/SOFTWARE_REVALIDATION_2026-11-17.md)。原BIN A/B与定制专项通过，未烧写/实机；不代表实施方案全部验收。
