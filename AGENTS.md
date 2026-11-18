# AGENTS.md — PC12M-2 十二相 SCR 控制板（LPC1765FBD100）

- **2026-11-18 剩余软件复核清单已完成**：本批原候选双出口1980/1983；另1个完整帧契约下不可达出口、2个CRC数据误反汇编，均有证据分类，未解释候选0；原函数/向量102项、测试干预559处全部分类。修复基本参数异常项号标题上界和认证前8拍GPIO读副作用，永久放行与授权显示/背光保留。当前模块集合53/53通过（完整回归+末批增量去重），完整原BIN A/B、定制、真实旧构建反测与负向检出通过，两次构建一致。当前BIN 72048 B，SHA256 `6B9F08E1E0DF0824AD67A5BA040C0E6C262B339801CC46CDEB7EAAB1D7C5E903`，text 65176/data 6312/bss 2192；本批未烧写、未提交/推送。代码/模拟契约验收完成，不是任意RAM/全中断交错或真实硬件量化等价证明。报告 `docs/analysis/FINAL_SOFTWARE_REVIEW_2026-11-18.md`；证据 `evidence/reverse/reports/final_software_review_2026-11-18/`。下方旧批次的“未完成”保留为当时事实，当前以本条和最终manifest为准。

- **2026-11-18 剩余分支第六批定向复核**：通信高地址/PID档位382例、双银行魔数/增益选择/同步重启96例、菜单字段14256例与超时2916例、校准/调节/故障显示/手动给定2962例通过。密码十副本首轮内层真实1000次，GPIO三种读回156组ISR与18例关断叶函数一致；五类注入均检出并恢复。累计双出口1522→1822，新增关闭300，剩余161（单出口144、未命中17），不是等价率或bug数。回归46/46、完整原BIN A/B通过；未发现新业务差异，业务源码/BIN未变。报告docs/analysis/BRANCH_GAP_AUDIT_2026-11-18.md，证据evidence/reverse/reports/branch_gap_audit_2026-11-18/。本批未烧写/实机/提交/推送；其余分支、逐条干预及完整外设/中断契约仍未全部关闭。

- **2026-11-18 状态机/故障复位第五批**：新增108条连续启停链（28728拍）、72例故障复位/禁用（启用者两段3000轮及6000份AA/55喂狗后确切自跳）、64组运行统计/落盘、320组缺相恢复及1248拍模式切换；受控SET/CLR/W1C/NVIC模型下52组真实ISR调度通过。修复公共运行器临时钩子泄漏，旧运行器及缺失喂狗写序注入均被检出。回归40/40、完整原BIN A/B、菜单/背光专项通过；未发现新业务差异，业务源码/BIN未变。报告docs/analysis/STATE_LIFECYCLE_AUDIT_2026-11-18.md，证据evidence/reverse/reports/state_lifecycle_audit_2026-11-18/。全程序分支、逐条干预契约、FIOCLR读行为及更广中断交错仍保留，S0–S7未整体关闭；未烧写/实机。

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
- 当前构建：`text 65176 / data 6312 / bss 2192`，`firmware.bin` 72048 B，SHA-256 `6B9F08E1E0DF0824AD67A5BA040C0E6C262B339801CC46CDEB7EAAB1D7C5E903`（2026-11-18最终软件复核修复后，未烧写）。当前模块集合53/53通过，完整原BIN A/B、菜单/背光专项与证据见本文件顶部最终复核条目；历史构建与旧批结论保留。

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

- **2026-11-17 最新提交联合复核与执行完成门禁修复**：当前提交2093690已含通信校验越界绘制修复。独立验证：功能码256例、通信标志768例、菜单/状态132例与本项目原BIN全部一致；50/60Hz两条EINT3保持超时路径实际GPIO写序一致。验证器增加正常返回断言，修复MISC缺少UART THRE就绪、EEPROM加载含大量纯等待且预算不足、void函数错误比较R0的验证问题；配置加载只跳过原空转写周期延时，I2C读写本体继续执行。回归8/8及完整原BIN A/B全部通过，MISC各路径均实际返回。最新构建text65156/data6312/bss2192，72028字节，SHA256 EA2920F11856CDF9810E6B3C0D17427DE429FD4630DB3B6762BC94B1E9E728D2。详见 `docs/analysis/JOINT_VALIDATION_2026-11-17.md`，日志与哈希在 `evidence/reverse/reports/joint_validation_2026-11-17/`。本轮未烧写。

- **2026-11-17 十二相快速复核（比较类立即数差分 + 覆盖门禁）**：用户要求对十二相做快速复核（不复用六相逐函数逐指令的慢路径）。方法：把六相收敛出的「比较类指令立即数集合差」移植到本仓，按 42 组「原机入口→新 ELF 符号」抽取 `cmp/cmn/tst/teq`（含条件后缀与 `.w`）的 `#imm` 双向差集，仅对未被 `v±1` 改写解释的 54 条回查原机反汇编。结果：42 函数中 21 个有差集，54 条**全部**可归类为位测试改写（`tst.w #bit` → `lsls`+`bmi`）、`>=`/`>` 改写、`switch`→区间/`tbh` 降级、原机数据区被误反汇编（如 `0x6adc` 处 `"V:2016"`、`0x4cf8` 处 `"---"`），以及既有授权定制（X/O 与字形、背光 10000、认证放行），**未发现新的行为差异**。两处工具实现须注意：`PAIRS` 的新 ELF 符号名是 `*_IRQHandler` 而非 `wdt_isr`/`timer1_isr` 等旧名；原机函数边界不能用反汇编证据首行的 `body=N`（不含尾部字面量池），须取「本入口到下一个证据入口」的距离。同时用三个探针检查测试真实性：密码错误延时未被现有矩阵触发（无六相那种预算截断假通过）、`output_stage` 仅 `cfg=1 & locked=0` 的 4 组真正进入 `closed_loop_wrapper`、以及**原机 6 处跨槽反馈复制（`0xED18/0xED7E/0xEDD2` 的 `fb←aux1` 与 `0xF1B0/0xF216/0xF26A` 的 `aux1←fb`）原有矩阵只覆盖 2 处**。据此新增 `test/emulation/test_feedback_copy_12.py`，把 6 条原机复制指令做成覆盖门禁、把 `fb/aux1` 写入序列做成行为契约（6 组用例均令 `fb != aux1`，误写成自赋值即失败），已 6/6 通过并被回归自动发现，模块数 8/8→**9/9**。结论与范围见 `docs/analysis/PC12M2_FAST_REVIEW_2026-11-17.md`（明确未做逐函数逐指令通读、未做状态机全矩阵、未烧写）。本次未改业务源码，构建仍为 text 65156 / 72028 B / `EA2920F1…9E728D2`。

- **2026-11-17 十二相状态机全矩阵（含 1 处真实差异修复）**：把六相收敛出的「屏×项号×视图×按键×TIMEOUT3 档」全矩阵移植到本仓（`tmp/probe_sm12_matrix_v4.py`，不入库）。**关键前提：先证明矩阵有检出能力，否则数字无意义**——初版给出 6732/6732、7344/7344 全绿，但注入缺陷后仍全绿，追查发现探针自身五处缺陷：①`probe_sm12_state.py` 的 `BASE` 含 `0x10001724/25/26`（MENU/MENU2/MENU3），32 位 `mem_write` 把用例参数覆盖成 `0x1E/0x14/0x0A`，实测 menu=0x02/0x04/0x63 指令集合完全相同（442 条）；②只比 SRAM 末态且跳过 `disp_*` 函数体，纯显示缺陷不可见；③`UC_HOOK_MEM_WRITE` 挂 `0x2009C000` 会让新版 `fio1_pin23_ctrl` 函数体一条不执行（Unicorn TCG 缓存问题），改用 `UC_HOOK_CODE`；④`disp_splash_screen` 的 OLD 地址误用 `0x1486`（落在 `disp_fixed_1dec` 体内），正确是 `0x40B0`；⑤`fio0/1_pin22_ctrl` 误用 `0xE966/0xE946`，正确是 `0xE6C6/0xE6A6`，且新版被 GCC 拆出 `.part.0`、`run_stop_preset` 等处直接 `bl ...part.0` 需一并挂钩（`.part.0` 内 R0 是陈旧值，不能当实参）。另修正密码延时：原机是 **10 份**内联副本（`0x5220/0x536C/0x8988/0x8AA4/0x8AEE/0x8B38/0x8BBA/0x8C70/0x8CBA/0x8D04`），不是 3 份，且 `0x1C32` 的 EEPROM 写周期延时是另一回事，不能混同。修正后 **14688 例 bad=0 / err=0**（disp_calls=27531、pin_calls=65516）。**矩阵检出 1 处真实差异**：`MENU==3` 基本参数屏的擦除帧（`TIMEOUT3` 自增到 `0x1F5`）原本 `switch (it)` 用入口缓存的项号，而原机在该块内 **15 处各自重读 `0x10001725`**（`0x771A/0x772E/0x7744/0x775A/0x7770/0x77A2/0x77D4/0x77EA/0x7800/0x781A/0x7830/0x7846/0x785C/0x7872/0x7888`）；`key==4`（保存退回）分支刚把 `*MENU2` 清 0，故该帧原机擦第 0 行、旧实现擦离开时的项号行。已改 `firmware/src/07_state_machine.c` 的 `switch (*MENU2)` 并注明依据；新增 `test/emulation/test_state_machine_erase_12.py`（112 例比较 `disp_string` 调用序列 + 15 处重读地址覆盖门禁；把修复回退后该测试 13 项失败，验证有效）。构建 text **65148** / data 6312 / bss 2192、`firmware.bin` **72020 B**、SHA-256 `33831147794B322B13C2D915ED6A7D239AF6D5F1E432FF34415ECA5CB335FAB6`；回归 9/9→**10/10**，原 BIN 完整 A/B 全 PASS。**注意：本仓此前 `docs/analysis/PC12M2_FAST_REVIEW_2026-11-17.md` 4.1 节「密码延时未被矩阵触发、无预算截断假通过」的结论已因上述探针缺陷而失效，文档已就地更正并保留原结论以示过程。**未烧写、未实机验证。

- **2026-11-17 十二相初始化/底层 I/O 覆盖补齐（含 1 处真实显示缺陷修复）**：针对 `text` 覆盖统计暴露的缺口，为 12p 的 21 个此前无执行级 A/B 的函数补上对照（`uart3_init`/`lcd_data_byte`/`gpio1_init`/`gpio_inputs_dir_init`/`chk_p02_p03`/`i2c_gpio_init`/`i2c_start`/`i2c_stop`/`i2c_delay_short`/`i2c_write_byte`/`i2c_read_byte`/`adc_init`/`SystemInit`/`nvic_enable_irq` 等），新增 `test/emulation/test_init_trace_12.py`：26 组外设函数用「写迹(地址/宽度/值) + 10 个外设页末态」对照，11 组读写 SRAM 的函数（`i2c_read_reg` 2 组、`freq_adjust_sync` 9 组覆盖 event=0/2/0x16/3/0x21/5/6 全部分支边界）用「预置 + SRAM 与页末态」对照。五个 Unicorn 陷阱已写进 docstring：①`0x2009C000` FIO 页不能挂 `UC_HOOK_MEM_WRITE`（TCG 缓存会让新 ELF 的 `fio1_pin23_ctrl` 函数体一条不执行）；②`i2c_delay` 入口是 `0x1C32` 而非 `0x1C46`（后者在循环体内，置 PC 会破坏栈）；③`SystemInit` 死等 OSCSTAT/PLL 锁定位，且 Unicorn MEM_WRITE 钩子在写入生效**之前**触发，须记账到 pending 在下一指令补位；④新 ELF 用全局槽 `0x2007D810` 间接取 FIO 基址，须预置`0x2009C000`；⑤`V.machine()` 未映射 `0x4002C000/0x40034000/0x4003C000`。**本测试抓到 1 处真实缺陷**：`freq_adjust_sync` 的 event=6（置停止）分支调用 `disp_string(0xa98c,3,0xb,0)`，原机 `0xa98c` 是「停止」（GBK `cd a3 d6 b9 00`），但 12p 把该函数从 `src/` 迁到 `firmware/stub.c`，而字符串池生成器 `tools/generation/generate_string_pool_12.py` 的 `SCAN_FILES_12` 只扫 `firmware/src/*.c`，地址从未进 `strpool_clusters`；`strpool_map` 未命中时原样返回地址，新 ELF 在 `0xa98c` 处是**指令字节**，于是把代码当字符串渲染，表现为 FIO 写迹差异（`页0x2009C000: +0x038 OLD=0x13 NEW=0x10, +0x039 OLD=0xC7 NEW=0x47`）。已把 `firmware/stub.c` 并入 `SCAN_FILES_12` 并重新生成 `firmware/src/strpool.c`（簇 21→22，blob 2699→2704 B），新增静态门禁 `test/emulation/test_strpool_coverage_12.py`：断言 158 个 `disp_string` 地址实参全部被簇或 override 覆盖（回退生成器即报 `0xA98C 未被 strpool 覆盖`），并把非字面量实参锁定在 4 处已知基线。检出能力已双向验证：注入 `TIMER0->MR0 1999→1998` 报`+0x018 OLD=0xCF NEW=0xCE`；注入 `freq_adjust_sync` 上限 `0x2b0→0x2af` 报 `+0x1624 OLD=0xB0 NEW=0xAF`（教训：函数级 A/B 只跑「无害入参」等于没测，必须覆盖每个行为分支的边界）。随后补入 `long_delay`（`0x598`，6000×1000 双层空转）：它不写任何外设或 SRAM，只改写内层计数器时两版写迹全空、末态相同，把 `i < 6000` 改成 `< 5999` 仍 PASS，故把外层迭代次数导成伪写迹 `(0xFFFFFFF0,4,轮数)` 参与写序列比较；快进钩子按 `is_new` 区分（原机内层 `0x5AA` 计 `[sp,#0]`、外层 `0x5BE movw #6000`，新 ELF 内层 `0x6FA` 计 `[sp,#4]`、外层 `0x6DE` 计 `[sp,#0]`），且 `is_long` 必须同时认原机入口与 新 ELF 符号。改完后注入 `6000→5999` 即报「写序列 1/1 项不同」。覆盖门禁升至 **22 个唯一原机入口 / 38 组用例**；覆盖率口径修正（原统计脚本只认 `0x798`/`0x00000798`，漏了测试常用的四位补零写法 `0x0798`）：函数字节覆盖 71/102 提升到 **97/102 = 61026/61140 B（99.8%）**，剩余 5 项共 114 B 全为 IAR C 运行时或空函数（`0x188` `__scatterload` 调用链、`0x5ca` 空函数、`0x11178`/`0x111c4`/`0x111cc` 属 `__rt_lib_init`）。构建 `text 65168 / data 6312 / bss 2192`、`firmware.bin` **72040 B**、SHA-256 `C84AE01FDD7B306522D15D38E6275624BA4A0A35AABCA3C53FE7115E53571C6B`；回归 10/10→**12/12**，十二相原 BIN 完整 A/B 全 PASS（含 DISPLAY 106 / DISPLAY_FULL_EXEC 4 / DISPLAY_SCREEN_STATIC 32298 / DISPLAY_SCREEN_CALIB 30505 / XO_FONT）。未烧写、未实机验证。

- **2026-11-17 双项目独立修复复核**：近期提交未发现应撤销的业务修复；扩域又确认两仓PID给定/反馈及死区比较误用无符号（各486例旧版216失败、修复后全过）；十二相相位显示已按字节读取，不照搬六相修改。补齐独立执行入口正常返回门禁。每仓控制/索引扩域5648例、菜单数值扩域6800例与双通道各240拍持久SRAM回放均通过；回归14/14、完整原BIN A/B、菜单/背光专项通过，两次构建BIN一致。认证放行与定制保留；未提交/推送/烧写。不把有限验证写成全部路径已穷举，实施方案S0–S7未整体关闭。详见 `docs/analysis/INDEPENDENT_REPAIR_REVIEW_2026-11-17.md`，证据在 `evidence/reverse/reports/independent_repair_review_2026-11-17/`。

## 必须遵守

- **2026-11-17 用户明确的差异消除范围**：按 `docs/project/FIRMWARE_EQUIVALENCE_REMEDIATION_PLAN_2026-11-17.md` 实施；目标是消除源码与原BIN的非授权逻辑差异及测试假通过，不是删除代码。认证永久放行继续保留；菜单、字形、背光等已授权定制保留，原始证据不得删减。用户进一步限定本方案仅做代码/模拟验证，不包含实机验证或烧写，也不以实机测试作为方案验收条件。当前仅形成实施文档，阶段任务需另按证据验收。

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

- **2026-11-17 软件复核第二批**：真实main调用链确认原BIN 0x600/0x604/0x608为EINT1→EINT2→EINT3，修复源码1→3→2及错误注释，保留修复前有效失败。新增复位链、264拍主循环、持久EEPROM/断电恢复、I2C位级、UART连续帧、轮询等待、TIMER1全字节、失锁恢复、8个ISR抢占窗口、10份密码延时与缺陷注入验证，完整原BIN A/B通过。条件分支候选1983个，双出口908、单出口626、未命中449，不代表全部可达路径等价或方案S0–S7全部验收。详见 `docs/analysis/SOFTWARE_REVALIDATION_2026-11-17.md`；未烧写/实机，本批提交号以Git历史为准。

- **2026-11-17 输出/保护复核第三批**：新增输出级边界11494例（5378例实际进入PID）、284条每条5帧的连续故障封锁/输入恢复/再启动拒绝链。原输出级150个条件分支累计双出口67→150，全部实际命中。未发现新业务差异，业务源码与固件哈希不变；当前回归31/31，完整原BIN A/B证据见 `evidence/reverse/reports/output_protection_audit_2026-11-17/`。报告 `docs/analysis/OUTPUT_PROTECTION_AUDIT_2026-11-17.md` 明确150/150不代表全程序/全参数等价，状态机、通信和外设模型缺口继续保留；未烧写/实机。

- **2026-11-17 菜单/通信复核第四批**：本仓未修改业务源码，固件SHA保持0832CC36…37018B。最终菜单矩阵14688/14688、通信边界5002/5002、非零工作槽与跨帧回放60例、逐键密码流程28例、通信复位2例、回归36/36及完整原BIN A/B通过。同批六相通信共享工作槽偏离只在六相修复；十二相完整SRAM已相同，不照搬修改。短帧残留功能码实测CRC读255字节后正常错误应答，否定65535字节越界推测。报告 `docs/analysis/MENU_MODBUS_AUDIT_2026-11-17.md`，证据 `evidence/reverse/reports/menu_modbus_audit_2026-11-17/`。未烧写/实机；状态机启停、故障清除/恢复全路径及外设/NVIC模型仍待复核。

- **2026-11-18 数字格式化直接复核**：两仓各900例原BIN/ELF字符调用契约通过；本仓候选双出口1844/1983，剩余139（单出口126、未命中13）。未修改业务源码、未烧写；不代表全程序等价。见 `docs/analysis/NUMERIC_LEAF_AUDIT_2026-11-18.md`。
