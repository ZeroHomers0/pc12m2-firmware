# P5 Unicorn A/B 等价性验证 — 进度记录

状态：**已完成**（2026-08-31 全部 PASS）
验证脚本：`tools/verification/verify_firmware_equivalence_12.py`
复跑：`cd PC12M-2 && python tools/verification/verify_firmware_equivalence_12.py`
（改源码后先 `cd firmware && bash build.sh` 重建）

## 全部 PASS（2026-08-31 最终）

```
VECTOR: PASS (SP=0x100029a0, checksum=0, CRP=0xFFFFFFFF)
PIN_CONFIG: PASS writes=48
GPIO2_INIT: PASS writes=7
AUTH_CHALLENGE: PASS writes=90
GPIO_OUTPUTS_SET: PASS writes=12
FIO0_P22: PASS writes=1
FIO1_P22: PASS writes=1
RELAY_DIRECT: PASS funcs=4 levels=2
TIMER0_IRQHandler: PASS writes=1
TIMER2_IRQHandler: PASS writes=6
WDT_IRQHandler: PASS writes=0
UART3_IRQHandler: PASS writes=0
UART3_IRQHandler: PASS writes=1
TIMER1_MATRIX: PASS cases=972
CRC_MATRIX: PASS cases=5
MODBUS_REGS: PASS read=65 write=320
CLOSED_LOOP: PASS integral=6 wrapper=12
OUTPUT_STAGE_MATRIX: PASS cases=36
RUN_STOP_PRESET: PASS cfg=2
STATE_MACHINE_MATRIX: PASS cases=130
DISPLAY_MATRIX: PASS cases=106
DISPLAY_FULL_EXEC: PASS cases=4
DEBOUNCE: PASS cases=9 scan_run_stop=4
AUTH: PASS funcs=3
DISPLAY_SCREEN_STATIC: PASS writes=32298
DISPLAY_SCREEN_CALIB: PASS writes=30505
MISC: PASS funcs=6

==== 12p A/B 等价性验证：全部 PASS ====
```

## 本次会话修复记录（自上一暂停点）

### 1. state_machine debounce 计数器槽错位（已修复）
- 症状：`state_machine SRAM mismatch (1,0,0,1)`：0x1000157D old=1 new=0、
  0x1000157F old=0 new=1（两字节相邻）。
- 根因：`firmware/src/03_input_debounce.c` 三个 debounce 函数计数器 DAT_ 符号
  **系统性错位 2 字节**（OLD 反汇编金标准 `evidence/reverse/disassembly/functions/00001a96..3.txt`）。
- 修复对照（OLD 高沿槽 / 低沿槽）：

  | NEW 函数 | OLD 入口 | 修复后高沿槽 | 修复后低沿槽 |
  |---|---|---|---|
  | debounce_p116 | 0x1a96 | 0x157C `DAT_00001bd8` | 0x157D `DAT_00001bdc` |
  | debounce_p117 | 0x1aee | 0x157E `DAT_00001be0` | 0x157F `DAT_00001be4` |
  | debounce_p06 | 0x1b46 | 0x1580 `DAT_00001be8` | 0x1581 `DAT_00001bec` |

  debounce_p09（0x1a68）用 0x157B `DAT_00001bd4`，本就正确，未改。
  globals.c 地址映射本身正确（globals.c:58-64），无需改。
- 验证：修复后 `STATE_MACHINE_MATRIX: PASS cases=130`、`DEBOUNCE: PASS cases=9 scan_run_stop=4`。

### 2. 三个认证函数 R0 残留不一致（源码返回 OLD 残留值）
- `auth_set_timeout`（0x108D2）：OLD `movw r0,#0xc350` 退出 R0=50000；NEW GCC 用 r2/r3。
  修复：函数返回该值（`return *(volatile uint*)0x100020C4 = 50000;`）。
- `auth_challenge`（0x108DC）：OLD 退出 R0=0x5500（challenge_byte 组C 0x55 连续左移末态）；
  NEW 残留 0x100020C0。修复：返回 challenge_byte。
- `auth_verify_loop`（0x10A38）：OLD 退出 R0=末次循环检查读的 cnt；NEW 残留 auth_challenge 返回。
  修复：返回 `*cnt`。
- 另：NEW 延时循环每迭代指令数多于 OLD，auth_verify_loop 5×24bit 挑战需 ~3.5M 条，
  2M 会在第 3 次挑战中断（假差异），`verify_auth` 放宽至 `max_insn=8_000_000`。

### 3. 显示串池 / disp_screen_static 地址（已修复）
- strpool 重新生成：21 clusters / 2699 B，清除无用簇（0x4814 是 literal pool 的
  SRAM 指针 0x10001608，非字符串）。
- `disp_screen_static`：注释 0x448A→0x41B4；四个串地址 0x4814/0x4824/0x4834/0x4844
  （literal pool 指针，错误）→ 0x436c/0x437c/0x438c/0x439c（真实 GBK 串，ADR 计算目标）。
- 修复后 `DISPLAY_FULL_EXEC: PASS cases=4`、`DISPLAY_MATRIX: PASS cases=106`。

### 4. void 屏渲染函数 R0 非 ABI 语义（改为行为校验）
- 症状：`disp_screen_calib` 8M 下 OLD R0=0x08、NEW R0=0x00（RAM same=True）。
- 根因：OLD R0=0x08 是 char8（0xaf4）内层字体循环 `adds r0,r4,#1` 的编译器寄存器残留
  （7+1=8）；NEW char8（0xb4c）残留的是 disp_data 返回值。纯编译器代码生成差异，
  C 源码无法合理复刻，追平需复刻 IAR 代码生成。
- 结论：对 void LCD 渲染函数，R0 非 ABI 语义；正确做法是校验行为等价。
- 修复：`verify_misc` 对 `disp_screen_static`/`disp_screen_calib` 改为
  「GPIO 写迹（真实渲染像素位，FIO 池 0x2009C000）+ SRAM 末态」校验，
  保留 R0+SRAM 校验给 uart3_tx_byte/uart3_rx_timeout_monitor/load_config/
  param_sync_live_to_eeprom。8M 指令配额（全屏渲染 4 行 GBK 串，500k 会中途截断）。
- 验证：OLD/NEW 写迹完全一致（32298 / 30505 条），`DISPLAY_SCREEN_STATIC/CALIB: PASS`。

## 更早修复记录（此前会话，保留）

### closed_loop 位置式输出槽地址（已修复并 PASS）
- 症状：`CLOSED_LOOP` (100,90,2,3) RAM mismatch，0x10002108 old=0x0140 new=0x0000
- 根因：globals.c `pid_integral` 误指 0x100020F8（累加器 DAT_00011164 地址）。
  OLD 反汇编 0x110C0 铁证：位置式输出写 **0x10002108**（DAT_00011160），
  累加器 0x100020F8（DAT_00011164）。
- 修复：
  - `firmware/globals.c`：`pid_integral`、`g_pid_integral` → 0x10002108
  - `firmware/src/09_output_stage.c`：4 处 `*pid_integral = *out_setpoint;` → `*DAT_00011164 = ...`
    （OLD output_stage 0xE9F2/0xEAA8/0xEEA4/0xEF5A 写累加器 0x100020F8）
  - `firmware/src/12_closed_loop.c`：注释 0x10002130/0x10002120 → 0x10002108/0x100020F8

### run_stop_preset R0 残留（验证脚本放宽，已 PASS）
- 症状：cfg=1 时 R0 OLD=0x1、NEW=0x64（编译器残留，非契约）
- 根因：OLD（IAR）在 cfg==1 路径末尾重读 cfg_word 致 R0=1；NEW（GCC）乘法常数
  0x64 残留 R0。调用点 OLD main 0x6B6 `bl 0xF70A` 后立即 `bl 0x238` 覆盖 R0，不消费。
- 修复：`verify_run_stop_preset` 只断言 SRAM 末态（注释已说明缘由）。
  6p 参考脚本对 run_stop_preset 亦无 R0 断言。

## 2026-08-31 清理 6p 遗留 + 静态测试修复（期间新发现）

### 1. 删除 6p 时代工具（用户批准「删 A + B 类」，用户持两份独立工程）
- A 类 6 个（`tools/generation/_scan_funcs6.py` + `tools/archive/` 5 个）
- B 类 tools 6p 脚本 16 个（6p 版 `verify_firmware_equivalence.py`、`scan_state_machine_audit.py`、
  `verify_sm_addresses.py`、`verify_periph_xref.py`、`verify_readwidth_all.py`、`verify_modbus_c.py`、
  `extract_ram_data_image.py`/`generate_globals.py`/`generate_string_pool.py`（6p 版）、
  `codex_audit_*`×4、`apply_consts_08/09.py`、`w8_relay_ab.py`）
- P3 一次性辅助 6 个（`_scan_src_headers6.py`、`_tmp_loc13.py`、`_audit*_sym_coverage.py`、
  `_check_funcmap_dir.py`、`extract_data_segments.py`）
- **test/emulation 整体 22 个**（6p 基座：引用 6p 地址 0x458C/0x25DC/0x35F2/0x108B0…，
  7 个依赖 6p 版 `verify_firmware_equivalence.py`，全部读不存在的根 `LPC1765.bin`）
- **当时保留**：`test/support/unicorn_harness.py` 与 `w8_stack_watermark.py`；后续结构审计确认
  两者仍使用六相栈顶与 SRAM 预置，已于 2026-08-31 删除，避免十二相假 PASS。
  当前保留 `test/static/*`（12p 源码静态检查）、`run_tests.py`/README、`apply_consts_12.py`、
  `verify_startup.py`/`verify_strpool.py`/`verify_mem_xref.py`/`check_readwidth.py`、`w8_combine_hex.py` 等。
- `tools/README.md` 重写为 12p 现状。

### 2. 修复 test/static 三个静态检查（12p 可用，6p 标注残留）
- `test_crc16_tables_and_semantics.py`：BIN 路径 `LPC1765.bin`→`backup/pc12m2_orig.bin`，
  表地址 0x11034/0x11134 → **0x111D8/0x112D8** → **8/8 PASS**。
- `test_modbus_register_map.py`：函数签名/正则按 12p 十六进制 case + cast 形式重写，
  INV1 豁免 reg 0x17/0x18 → **5/5 PASS**。
- `test_parameter_sync_structure.py`：已 6/6 PASS（无需改）。

### 3. 【新发现】12p CRC16 表真实地址 = 0x111D8/0x112D8（≠6p 的 0x11034/0x11134）
- 证据：OLD crc16 反汇编 `0000acd4` 池槽 `0xADCC→0x000111D8`、`0xADD0→0x000112D8`；
  BIN @0x111D8 表用验证帧 `[01 03 00 00 00 0A]` 算出 CRC=0xCDC5（= crc16_table.c 标注）。
- 修正：`crc16_table.c`/`08_uart3_modbus.c` 注释 0x11034/0x11134 → 0x111D8/0x112D8（表内容本身正确）。
- 注意：BIN @0x11034/0x11134 是代码非表；0x508D 处另有一份有效 CRC 表（布局不同，非 modbus 用）。

### 4. 【新发现】modbus reg 0x17/0x18 读写不对称（原固件设计，非移植 bug）
- read_reg(0xAD04) TBB idx0x17/0x18 读活动槽 **0x10001706/07**（pid_kp2/ki2）；
  write_multi(0xB050) 写银行4 **0x1000170F/10**。
- 源码与 OLD 反汇编一致：PID 前导把所选银行 KP/KI（0x1709-0x1710）复制到活动槽（0x1706/07），
  read 读生效值、write 写源参数。测试豁免记录为 `INV1_KNOWN_ASYMMETRIC={0x17,0x18}`。

## 6p 参考对照（地址差异提醒）
6p 参考 globals：`DAT_00001c2c=0x157D`、`c30=0x157E`、`c34=0x157F`、
`c38=0x1580`、`c3c=0x1581`（5 个计数器，12p 为 7 个从 0x157B 起）。
12p 布局比 6p 少一个、多两个——移植时计数器符号需逐一按 OLD 反汇编核对，
勿直接沿用 6p 的 DAT 符号（`c2c`↔`bd8` 等地址不同）。

## 2026-08-31 后续：额外覆盖测试（任务 #4）+ 认证放行（任务 #6）

### 1. 任务 #4：9 个未覆盖测试组 113/113 PASS（脚本保留）
参考 6p `test/emulation` 全部测试，为 12p 补 9 组未覆盖 A/B 差分测试
（`test/emulation/test_extra_coverage_12.py`，644 行）：

```
adc_wait_done            4 PASS  0 FAIL    ← AD0GDR/AD0DR0 播不同结果，抓错读 +16
adc_scan_channels       13 PASS  0 FAIL    ← 连续多拍 + cfg_word/gain_sel/标定除数边界
input_scan_state         6 PASS  0 FAIL    ← 全 6 位引脚组合 × 计数初值差分(512 例)
uart_rx_sequence         5 PASS  0 FAIL    ← 多字节组帧状态转移 / 索引回绕 255→0
modbus_dispatch         20 PASS  0 FAIL    ← 读/写/异常/CRC/站址 + 13 帧异常矩阵
eeprom_sync_matrix       1 PASS  0 FAIL    ← param_sync 逐字节(0x110)/批量(8)扰动
interrupt_sequence       5 PASS  0 FAIL    ← EINT1/2/3+TIMER0/1/2+UART3 顺序执行
control_multitick        3 PASS  0 FAIL    ← 状态机+输出级持久 RAM 连续多拍
case3_edit              56 PASS  0 FAIL    ← menu=3 case3 编辑键矩阵(menu2=2..15)
─────────────────────────────────────
TOTAL 113 PASS / 0 FAIL
```

差分暴露并修复 **2 个真实移植 bug**（详见 6p 侧
`PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md`）：
- **adc0_scan_channels 增益全局对调**（`05_adc.c`）：`gain_a`(0x10001630)/`gain_b`(0x10001634)
  四个使用点全部对调（OLD 反汇编 `00001f6c` 铁证），零值样本掩盖、非零样本暴露。
  修复后 adc_scan_channels 13/13 PASS。
- **modbus_dispatch 0x10 异常路径漏写 menu_param_4**（`08_modbus_dispatch.c`）：
  OLD 0xDFB4 先写 `menu_param_4 = frame[6]` 再比较 `frame[6] != Q*2`；源码先比较、
  异常 return 时漏写。修复后 modbus_dispatch 20/20 PASS。

测试脚本调试要点（供复现）：hook 注册地址 bug（12p OLD/NEW 布局不同，只注册本侧地址）；
`UC_HOOK_MEM_WRITE` 未导入；`_isr_seq_run` 对虚拟名 `"RX"` KeyError 需先映射为
`"UART3_IRQHandler"`。

### 2. 任务 #6：认证永久放行（`01_startup.c` main()，A/B 仍全 PASS）
2026-08-31 决定（抄板，同 6p）：防抄板认证永久放行，不再启用。
- 12p 反逻辑：`auth_pass_flag`@0x100020C0 **0=通过、非 0=未通过**（与 6p 0x10000750 相反）。
- `main()` 认证段 `auth_verify_loop()` 之后强制 `*lock = 0`（`lock = auth_pass_flag`），
  锁机分支（"报警忙碌"/"CPU 忙碌" 死循环）正常不可达；`auth_verify_loop()` 调用保留
  （保持原厂语义与 A/B 等价）。
- 验证脚本不覆盖 main()/启动序列（`verify_auth` 直接以三个认证函数为入口、`verify_startup`
  只验 data_image 嵌入与 SRAM0 初值），故改 main() 不破坏 A/B 等价：
  重建固件（text 63768/data 6312/bss 2192）后 `verify_firmware_equivalence_12.py` 全 PASS
  （含 `AUTH: PASS funcs=3`、`STATE_MACHINE_MATRIX: PASS cases=130`），
  `test_extra_coverage_12.py` 113/113 PASS 无回归。
- 认证放行前，上板运行会在无认证链路（ADuM1201 隔离 1-Wire 总线）时锁机，已永久消除。

## 2026-09-01 产品信息定制（菜单 8.8 产品版本信息屏）

用户要求把 case9 产品版本信息屏（`0x9ab6`，`MENU==9`）4 行文本覆写为：
型号:PC12M-2 / 版本:V2.0 / 厂商:XIANPOWER / 电话:02984205750（原厂 ST36C/V2.0.2016/SINEP0WER/18938061832）。

- **实现**：改 `tools/generation/generate_string_pool_12.py` 新增 `PRODUCT_INFO_OVERRIDES`
  （4 个 12p flash 地址 → GBK 定制串），重新生成 `firmware/src/strpool.c` 得到
  `strpool_override` 段，`strpool_map` 前置查表返回定制串。
- **为什么这样做**：strpool blob 是原厂 bin 逐字节复制，0x6acc 位于簇中部，直接改内容会
  破坏其后 0x6ad8/0x6ae8/0x6af8 的相对偏移；`strpool_map` 是 `disp_string` 的唯一入口
  （`02_lcd_display.c` 首行调用），前置查表可保持 **flash 地址与执行路径完全不变**，
  仅替换显示内容。原厂串仍保留于 blob 未使用。
- **验证**：重建固件 text 63768→63920（+152 = override 段），`test/run_tests.py` 4/4、
  `verify_firmware_equivalence_12.py` 全 PASS（含 `DISPLAY_MATRIX: cases=106`、
  `DISPLAY_FULL_EXEC: cases=4`——两套显示验证只覆盖 case3/case4，不碰 case9）、
  `test_extra_coverage_12.py` 113/113 无回归。新固件 bin 0x00ceec 起含 4 个定制串
  （override_blob），原厂串仍可搜到（blob 保留，未显示）。
## 2026-09-01 厂商 X/O 显示修复（问题2，分支 w8-debug-2026-09-01）

产品信息屏"厂商:XIANPOWER"的 **X/O 不显示**（6p 同款问题，见 6p `W8_ISSUE_FIX_2026-08-28.md` 问题2）。
原厂串是 `SINEP0WER`（数字 0），故原厂从未暴露。

- **根因**：原 BIN 8×8 ASCII 字库仅 36 字符（map @0x10001546 = `0123456789%:. +IVUAWSBP-DCRc,M*FHTEN`，
  无 X(0x58)/O(0x4F)）。`disp_render_char8`（OLD 入口 0xAF4 / NEW 0xB50）查表未命中直接 return，
  X/O 渲染空白。
- **方案（方案C，选它因 A/B 最安全）**：不动 ram_data_image / 字库 / 链接布局。
  在 `02_lcd_display.c` 新增 `ext_char8_xo[0x20]`（X/O 各 0x10 字节，与原字库同格式：
  8 列 × 2 页、列高字节 bit7=行首），`disp_render_char8` 对 `ch==0x58/'X'`、`ch==0x4F/'O'` 提前匹配，
  其余字符仍查原字库（字库外字符保持空白，如 'Z'）。
- **为什么不在 map 加字符**：map 槽在 0x2000/0x2110 区全是被引用变量，扩展无空位（早前验证）。
- **验证**（新增 `XO_FONT` 至 A/B 工具，PAIRS 新增 `disp_render_char8` OLD=0xAF4）：
  - OLD 'X' → **0 次** GPIO 写（原厂确实空白，基线证明）
  - NEW 'X' → 387 次 GPIO 写、NEW 'O' → 387 次（渲染生效）
  - NEW 'Z'(0x5A 字库外) → 0 次（空白保留，回归护栏）
  - A/B 全套 PASS（含 DISPLAY_SCREEN_STATIC 32298 写 / CALIB 30505 写）、
    `test_extra_coverage_12.py` 113/113、`test/run_tests.py` 4 模块全 PASS。
- **性质**：对本固件的**有意定制偏离**（原 BIN 渲染 X/O 为空白）；A/B 快照区无差异，
  仅"渲染 X/O 字符"的 GPIO 写迹与 OLD 不同（OLD 不写）。产品信息屏 case9 本就不在 A/B 矩阵，
  故矩阵结果不变。
- 提交：`5a5d575`（firmware/src/02_lcd_display.c + tools/verification/verify_firmware_equivalence_12.py）。

## 2026-09-01 修正 X/O 字形风格（问题2 续，分支 w8-debug-2026-09-01，提交 `09b286c`）

烧写后用户实测：**X/O 显示了，但字体/大小/粗细与其他字符不一致**（我首版画的 X/O 又大又粗）。

- **根因**：首版 `ext_char8_xo` 用满 8 列 × 粗笔画连续 16 行；而原字库真实格式（2026-09-01 从
  `firmware/assets/ram_data_image.bin` 提取全部 36 字形核实）是：**字符 7 列（col0-6）× 16 行、
  笔画 1px**；`disp_render_char8` 每字写 2 页（行×2-0x48 / 行×2-0x47），**上页 = 字符上半部
  （行0-4），下页 = 字符下半部（行10-15），行5-9 留空**。字库统一如此（如 V 上页上窄下宽、
  下页收窄到行15 顶点；N 用 1px 斜线做对角线）。
- **修正**：
  - `'O'` 直接复用原字库**数字 '0' 的字形**（`f0 08 08 08 08 08 f0 00` / `1f 20 20 20 20 20 1f 00`）
    ——原厂 SINEP0WER 本来就用 0 代替 O，保证与整行一致；
  - `'X'` 按 N/M/V 的 1px 斜线风格重绘：`80 40 20 18 20 40 80 00`（上页两斜线收敛）/
    `07 08 10 20 10 08 07 00`（下页两斜线发散），交叉点在中间空白区。
- **验证**：新字形字节已核验写入 BIN @0xcd10(X)/0xcd20(O)（相邻同表）；
  A/B 全套 PASS（`XO_FONT: old_X=0写 / new_X=387写 / new_O=387写 / new_Z=0写`）、
  `test/run_tests.py` 4/4。commit `09b286c`。

## 2026-09-02 X 字形二次修正与 O 路径收敛

用户实机照片显示厂商首字符仍呈菱形而非 X。复核 `ram_data_image.bin` 中原厂 `N/A/V/0`
的逐行点阵后确认：原厂 8×16 ASCII 的有效像素连续位于行3-13，并不存在“行5-9留空”。
此前把 LCD 的上下两个 8 行页误解成两个分离的字形半部，导致四段斜线围成菱形。

- `X` 改为 7×11、1px 点阵，两条斜线在行8/col3 真实相交；高度与原字库一致。
- `O` 不再保存重复字模，直接使用原字库索引0的数字 `0`；显示与原厂 `SINEP0WER`
  中的 `0` 完全一致，同时扩展表由 32 B 减为 16 B。
- `XO_FONT` 增加 X 的 16 字节精确断言，并逐项比较 `O` 与 `0` 的 GPIO 写迹，避免
  “有输出但字形错误”的回归。
- 验证：`test/run_tests.py` 通过，额外覆盖 113/113 通过，十二相完整 A/B 验证全 PASS；
  `XO_FONT` 为 OLD X=0写、NEW X=387写、NEW O=387写、NEW Z=0写，且 O/0 写迹相同。

## 2026-09-02 产品信息电话行宽修正

实机菜单只能显示电话值的前 11 个 ASCII 字符，`029-84205750` 共 12 字符，末尾 `0`
被截断。按用户要求去掉连字符，改为 `02984205750`（正好 11 字符），重新生成字符串池
并构建固件；十二相完整 A/B 验证全 PASS。

## 2026-09-02 实机测试通过 + W8 分级流程废弃

- 当前基线（最终）：`text 63964 / data 6312 / bss 2192`，`firmware.bin` SHA-256
  `449A13DB053C5A81632E9C66C6B467723BC995BE56E7BEC79425804BF38663E2`（bin 70836 B）。
- **实机测试通过**：自编译固件已烧写入板并经用户直接实机测试通过（含产品信息定制、
  X/O 字形修复与电话行宽修正后的最终固件）。阶段 0/1 之前所列 A/B 与覆盖矩阵均保持 PASS。
- **W8 分级实测流程废弃**：用户未按 W8 分级流程测试，该分级流程不再执行；
  `docs/w8/` 文档保留为历史流程档案，进度以 `AGENTS.md` 为准。实机通过不代表分级波形/
  带载 100% 量化等价；如需补充分级量化（十二路空载触发、reg44/45 标定、低压/带载），
  按 `docs/w8/W8_HARDWARE_TEST_2026-08-31.md` 规范另行安排。
