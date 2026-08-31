# P5 Unicorn A/B 等价性验证 — 进度记录

状态：**进行中**（2026-08-31 暂停点）
验证脚本：`tools/verification/verify_firmware_equivalence_12.py`
复跑：`cd PC12M-2 && python tools/verification/verify_firmware_equivalence_12.py`
（改源码后先 `cd firmware && bash build.sh` 重建）

## 已 PASS 项（截至暂停点，含 2026-08-31 修复后）

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
```

## 本次会话已完成的修复（P5 内）

### 1. closed_loop 位置式输出槽地址（已修复并 PASS）
- 症状：`CLOSED_LOOP` (100,90,2,3) RAM mismatch，0x10002108 old=0x0140 new=0x0000
- 根因：globals.c `pid_integral` 误指 0x100020F8（累加器 DAT_00011164 地址）。
  OLD 反汇编 0x110C0 铁证：位置式输出写 **0x10002108**（DAT_00011160），
  累加器 0x100020F8（DAT_00011164）。
- 修复：
  - `firmware/globals.c`：`pid_integral`、`g_pid_integral` → 0x10002108
  - `firmware/src/09_output_stage.c`：4 处 `*pid_integral = *out_setpoint;` → `*DAT_00011164 = ...`
    （OLD output_stage 0xE9F2/0xEAA8/0xEEA4/0xEF5A 写累加器 0x100020F8）
  - `firmware/src/12_closed_loop.c`：注释 0x10002130/0x10002120 → 0x10002108/0x100020F8
- 修复后 `CLOSED_LOOP: PASS integral=6 wrapper=12`、`OUTPUT_STAGE_MATRIX: PASS cases=36`

### 2. run_stop_preset R0 残留（验证脚本放宽，已 PASS）
- 症状：cfg=1 时 R0 OLD=0x1、NEW=0x64（编译器残留，非契约）
- 根因：OLD（IAR）在 cfg==1 路径末尾重读 cfg_word 致 R0=1；NEW（GCC）乘法常数
  0x64 残留 R0。调用点 OLD main 0x6B6 `bl 0xF70A` 后立即 `bl 0x238` 覆盖 R0，不消费。
- 修复：`verify_run_stop_preset` 只断言 SRAM 末态（注释已说明缘由）。
  6p 参考脚本对 run_stop_preset 亦无 R0 断言。

## 当前卡点（未解决）— state_machine debounce 计数器错位

**错误信息**：
```
AssertionError: state_machine SRAM mismatch (1, 0, 0, 1): [(268440957, 1, 0), (268440959, 0, 1)]
```
即 0x1000157D old=1 new=0、0x1000157F old=0 new=1（两字节相邻）。

**根因定位（已确认）**：`firmware/src/03_input_debounce.c` 中 debounce 计数器
DAT_ 符号**系统性错位 2 字节**。用 mem-hook 抓写：OLD 写 0x157D（PC=0x1AD2，
debounce_p116 低沿），NEW 写 0x157F（PC=0x1A68）。

OLD 反汇编金标准（证据文件 `evidence/reverse/disassembly/functions/00001a96..3.txt`）：

| NEW 函数 | OLD 入口 | 高沿槽（OLD） | 低沿槽（OLD） | NEW 源码现用 | 应改用 |
|---|---|---|---|---|---|
| debounce_p116 | 0x1a96 | 0x157C `DAT_00001bd8` | 0x157D `DAT_00001bdc` | DAT_00001be0/be4 (0x157E/7F) | **DAT_00001bd8/bdc** |
| debounce_p117 | 0x1aee | 0x157E `DAT_00001be0` | 0x157F `DAT_00001be4` | DAT_00001be8/bec (0x1580/81) | **DAT_00001be0/be4** |
| debounce_p06 | 0x1b46 | 0x1580 `DAT_00001be8` | 0x1581 `DAT_00001bec` | DAT_00001bd8/bdc (0x157C/7D) | **DAT_00001be8/bec** |

debounce_p09（0x1a68）用 0x157B `DAT_00001bd4` —— 正确，无需改。

**待改文件**：`firmware/src/03_input_debounce.c`
- debounce_p116 函数（~line 289-309）：`DAT_00001be0`→`DAT_00001bd8`、`DAT_00001be4`→`DAT_00001bdc`
- debounce_p117 函数（~line 319-339）：`DAT_00001be8`→`DAT_00001be0`、`DAT_00001bec`→`DAT_00001be4`
- debounce_p06 函数（~line 349-369）：`DAT_00001bd8`→`DAT_00001be8`、`DAT_00001bdc`→`DAT_00001bec`

globals.c 地址本身无需改（DAT_00001bd8..bec 映射正确，见 globals.c:58-64）。

**验证**：改后重建 + 重跑脚本，应过 `verify_state_machine_matrix`，
并继续 `verify_display_matrix / verify_display_full_exec / verify_debounce /
verify_auth / verify_misc` 至「==== 12p A/B 等价性验证：全部 PASS ====」。

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
- **保留**：`test/support/unicorn_harness.py`（12p `w8_stack_watermark.py` 依赖）、
  `test/static/*`（12p 源码静态检查，已修复）、`run_tests.py`/README、`apply_consts_12.py`、
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
