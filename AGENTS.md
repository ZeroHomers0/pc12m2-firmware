# AGENTS.md — PC12M-2 十二相 SCR 控制板（LPC1765FBD100）

> 本文件是 AI 接手 PC12M-2 项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目定位

本仓库只维护 **PC12M-2 十二相 SCR 移相触发控制板**。MCU 为 NXP LPC1765FBD100。
PC6M-10 六相项目已经完成全部实验并达到成熟状态，只作为只读参考；禁止修改六相仓库，
也不得把六相结论直接当作十二相事实。

十二相判断必须以本项目的原始固件、反汇编和 A/B 执行结果为准。六相源码只能帮助理解
同构结构、命名和验证方法。

## 当前状态（2026-11-13）

- 原固件曾通过 ISP 完整读取，记录 SHA-256 为
  `2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`，
  长度 262144 B，CRP=`0xFFFFFFFF`，详见 `backup/BACKUP_RECORD.md`。
- 原固件 `backup/pc12m2_orig.bin` 已就位并核验 SHA-256
  （`2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`）；
  2026-08-31 按用户决定另存根目录副本 `pc12m2_orig.bin` 并提交入库（覆盖原「绝不入库」铁律，
  详见 `backup/BACKUP_RECORD.md`）。擦写板上 Flash 前仍需制作第二物理副本。
- P0-P4 已完成：十二相函数清单、反汇编、数据镜像、全局映射和可编译 C 工程已建立。
- P5 Unicorn A/B 等价验证**已全部 PASS**（2026-08-31）：向量、GPIO、TIMER1 972 例、
  Modbus 65 读/320 写、闭环、输出级、状态机 130 例、显示 106 例+全执行 4 例、
  去抖、认证、MISC 全过。本次修复：debounce 计数器槽错位、三个认证函数 R0 残留、
  strpool/显示串地址、void 屏渲染函数改行为校验。详见
  `docs/analysis/P5_VERIFICATION_PROGRESS.md`。
- **任务 #4（测试覆盖查漏）已完成**：参考 6p `test/` 全部测试，为 12p 补 9 个未覆盖
  测试组共 **113 例全部 A/B 等价 PASS**，脚本保留于 `test/emulation/test_extra_coverage_12.py`。
  差分暴露并修复 2 个真实移植 bug：adc0_scan_channels 增益全局对调（OLD 0x1F6C 铁证）、
  modbus_dispatch 0x10 异常路径漏写 menu_param_4（OLD 0xDFB4 先写后比）。
  详见 6p 侧 `PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md`。
- **任务 #6（认证放行）已完成**：2026-08-31 决定（抄板，同 6p）——防抄板认证永久放行。
  `01_startup.c` `main()` 认证段后强制 `*lock = 0`（`auth_pass_flag`@0x100020C0，**0=通过**，
  与 6p 的 1=放行相反）；`auth_verify_loop()` 调用保留（保持原厂语义与 A/B 等价），
  锁机分支（"报警忙碌/CPU 忙碌"）正常不可达。重建固件后 `verify_firmware_equivalence_12.py`
  全 PASS（含 `AUTH: PASS funcs=3`）、113/113 PASS 无回归。
- **任务 #5（W8 问题复查）已完成**：逐项复查 6p 实机（`PC6M-10/docs/w8/`）暴露的 19 项问题
  + 6 项流程/清单，**12p 无任何一项需要代码级修复**——6 项移植期已修（ADC 指针、FIO 读址、
  case3 钳位/闪烁、Modbus 大端、恢复出厂串地址、复位坐标）、3 项原厂设计、12p 独有的
  认证放行/case1/case1E 三分支门控/主屏 LED 错译均已完成。详见 6p 侧
  `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md`。
- **2026-09-01/02 后续进度**：产品信息定制（型号/版本/厂商/电话）、厂商 X/O 字形修复与
  电话行宽修正（`029-84205750`→`02984205750`）均已完成并 A/B 全 PASS，详见
  `docs/analysis/P5_VERIFICATION_PROGRESS.md`。2026-09-02 基线 `text 63964 / data 6312 / bss 2192`，
  `firmware.bin` SHA-256 `449A13DB053C5A81632E9C66C6B467723BC995BE56E7BEC79425804BF38663E2`。
- **2026-09-02 实机测试通过（用户直接实机验证）**：自编译固件已烧写入板并经用户实机测试通过
  （含产品信息定制与 X/O 修复后的最终固件）。**W8 分级实测流程未按计划执行、已废弃**——用户
  未按 W8 分级测试；`docs/w8/` 分级文档保留为历史流程档案，进度以本文件与
  `DOCUMENTATION_INDEX.md` 为准。实机通过不代表分级波形/带载 100% 量化等价。

- **2026-11-13 菜单与背光同步**（用户明确授权同时修改六相、十二相）：菜单改为αmin限制/αmax限制（min/max为下标）、主从移相、相位补偿；控制方式选项改为远程/本地/面板。字形集中在 `02_lcd_display.c`，α全高等比例，α+紧凑下标总宽仍32像素。仅背光无操作阈值改为10000拍（约20秒），其它菜单超时不变。
- 十二相地址由本仓原BIN核实；生成入口 `tools/generation/generate_string_pool_12.py` 的 `MENU_TEXT_OVERRIDES`，兼容六相语义字符串头文件及缺省stub文件；产品型号保持PC12M-2。旧型号/误称仅清理活动文本，原BIN和evidence原始证据保留。
- 当前构建：`text 65156 / data 6312 / bss 2192`，`firmware.bin` 72028 B，SHA-256 `EA2920F11856CDF9810E6B3C0D17427DE429FD4630DB3B6762BC94B1E9E728D2`（2026-11-17 第三轮逐模块精读修复后）；本次尚未烧写/实机确认。菜单专项与背光28例、回归4/4（含113例扩展覆盖）、十二相原BIN完整A/B全部通过；日志见 `evidence/reverse/reports/menu_backlight_*_2026-11-13.log`。新增专项入口 `verify_menu_text_12.py`、`verify_backlight_timeout_12.py`，位于 `tools/verification/`。本次显示和背光为用户授权定制偏离。

- **2026-11-14 菜单标题调整**：在 `firmware-menu-display-2026-11-13` 分支将相位补偿改为同步校准，集中字库新增同/步字形。菜单与背光专项、十二相原BIN完整A/B验证全部通过（`evidence/reverse/reports/menu_sync_calibration_ab_2026-11-14.log`）；本次未烧写。

- **2026-11-16 端子事件契约与回归增强**：本仓扫描原本已正确返回7/8，无六相点动5/6混用错误；新增 `firmware/inc/input_events.h` 并在扫描和状态机共用端子事件名。新增 `test/emulation/test_terminal_event_contract.py`：1176例扫描、88例去抖均与本仓原BIN一致；完整原BIN A/B及全量回归5/5（含113例扩展覆盖）通过，日志见 `evidence/reverse/reports/terminal_event_*_2026-11-16.log`；固件哈希保持 `8CF36CDC075CAE353072340106841AC65A7A6E63A3D53752CA8AA31061EA634E`。保留原版本地＋自锁门控，本次未烧写。

- **2026-11-16 菜单文案调整**：共享选项外控→模选、限相→解限；启动方式点动→瞬动、自锁→保持；菜单6相位参数校准→同步参数校准。十二相另将菜单9及详情标题改为手动均流，子菜单电流平衡→参数设置。编号/值列/参数逻辑不变；短标题补空格清除旧字形。新增瞬/解/均/选/持集中点阵；同/步沿用已扩展字形。菜单专项16项正反显通过。显示全执行A/B以原厂文字验证渲染行为，定制文字及字形另由菜单专项实际执行检查。本仓完整原BIN A/B及回归5/5通过，日志见 `evidence/reverse/reports/menu_wording_*_2026-11-16.log`。此次未提交、推送或烧写。

- **2026-11-17 第三轮逐模块精读修复**：按 6p 同一路径复查全部模块（五个只读子代理逐分支
  比对 + 自建差分探针），确认并修复 7 类差异：Modbus 异常应答功能码改 `adds #0x80` 8 位回绕；
  `rx_flag` 标志改无符号判零；非法屏号落共享超时尾（原 0x9FAA→0xA058→0xA828）；联锁屏
  `disp_string(0x740,3,0,0)` 行列纠正；case4 第 3 页限 `8 <= 项号 < 0xc`（原 0x7AC6/0x7ACE）；
  8 处枚举越界 `else` 改独立相等判断；EINT3 停机复位改 fio0 先于 fio1。
  验证：显示调用序列 152/152、全屏显示 3860/3860、状态终态 232/232、EEPROM 34/34、
  Modbus 边界 36/36 与随机 600/600、输出级随机 250/250、回归 6/6（含新增 test/emulation/test_exception_func_code_12.py 13/13）、原 BIN A/B 全 PASS。
  反向结论：`09_output_stage.c` 六处同址自写是原 BIN 真实行为，不得改成 6p 的跨槽复制。
  记录见 `docs/analysis/PC12M2_AUDIT_FIXES_2026-11-17.md`。未烧写、未上机。

- **2026-11-17 通信校验越界绘制修复**：用户实测指出通信校验 2/3 等越界值原版不绘制选项。原 BIN 0x8840 `cbnz`（==0 画关闭）/0x8852 `cmp #1; bne`（==1 画开启）、入口块 0x632C/0x6340 同理；12p 源码两处 `if/else` 已改为相等判断。新增 `test/emulation/test_enum_range_12.py`（22 例）。同时发现并修正了显示探针的空跑缺陷：12p 页面只在 `*TIMEOUT3 == 0xFB` 时整页重绘，旧探针预置 0 导致两版都不绘制，此前 152/152、3860/3860、1970/1970 多为「双方空序列」比较；现按 0xFA 预置重跑：`tmp/probe_disp12_full.py` 覆盖 18 个屏号 × 项 0..16 × 5 键 × view 0/1 × TIMEOUT3 两种预置，共 6120 例，全部一致（其中 3514 例两版都有实际绘制、2606 例双方均为空序列）。验证：枚举越界 22/22、COM_CHK/PARITY 专项 30/30、原 BIN A/B 全 PASS、回归 6/6。提交 `384f251`。未烧写。

## 必须遵守

- 全程中文交流。
- Git 提交说明必须使用中文（提交标题及正文均使用中文描述）。
- 只修改本仓库；六相仓库只读参考。
- 十二相结论必须由 `pc12m2_orig.bin`、十二相反汇编或十二相实测支持。
- 修改源码后必须执行十二相 A/B 验证，不能以六相测试结果替代。
- 未找回并核对原固件备份及第二物理副本前，绝不擦写 Flash。
- CRP 非 `0xFFFFFFFF` 时绝不执行 unlock、recover 或 mass erase。
- 上板时只接控制电，断开市电、门极和功率负载；未完成分级放行不得带载。
- J-Link 只使用 `tools/jlink/JLink.exe`；无探针时走 ISP。
- 十二相原始反汇编与过程证据不得删除；六相重复副本已获用户授权清理。

## 目录职责

```text
backup/                     十二相原固件备份、ISP 分块与日志（敏感、gitignore）
firmware/                   十二相可编译源码工程
docs/project/               十二相项目状态、应用与数据布局
docs/analysis/              十二相模块结论与 P5 验证进度
docs/w8/                    十二相实机验证流程、规范与状态
evidence/hardware/          硬件资料；board/display/reports 为六相参考，十二相事实须另行验证
evidence/reverse/           十二相逆向证据
  disassembly/functions/    带引用注释的逐函数反汇编，当前权威
  disassembly/raw/          十二相原始逐函数导出
  notes/                    十二相状态机人工拆解过程记录
  reports/                  十二相生成报告
test/                       十二相静态检查与公共 Unicorn 支持
tools/                      十二相生成、验证、实机与 J-Link 工具
```

## 常用入口

操作入口（详见 `操作文档.md`；根目录三个 `.bat` 已删除，跨机不通用）：
- 构建 —— `cd firmware && bash build.sh`（产物 `firmware.bin/hex/elf/map`，SHA/尺寸见 操作文档.md §2）
- SWD 烧写（日常主通道，首选）—— 打包版 `tools/jlink/JLink.exe` + `-CommanderScript`，命令见 操作文档.md §3
- ISP 烧写（SWD 连不上时的解困通道）—— Flash Magic + USB-TTL，接线与用法见 操作文档.md §4
- 重启固件一律**物理断电再上电**（J-Link 驱动 nRESET 复位会悬挂 SWD，干扰复用调试脚的固件运行）。

```powershell
# 构建（Git Bash）
cd firmware
bash build.sh

# 静态测试（CRC 测试需原 BIN，已就位）
cd ..
python test/run_tests.py

# 十二相完整 A/B 验证；需要原 BIN 和已构建固件
python tools/verification/verify_firmware_equivalence_12.py
```

## 下一步

**实机测试已通过（2026-09-02），W8 分级实测流程已废弃**，无强制后续步骤。若需发布/量产/
换板复验：按 `操作文档.md` 重建并烧写，实机冒烟核对 LCD/按键/通信即可；如需补充分级波形量化
（十二路空载触发、reg44/45 标定、低压/带载），按 `docs/w8/W8_HARDWARE_TEST_2026-08-31.md` 规范
另行安排，不属于当前收尾基线。上板安全前提（断开市电/门极/功率负载、仅限流控制电）不变。
