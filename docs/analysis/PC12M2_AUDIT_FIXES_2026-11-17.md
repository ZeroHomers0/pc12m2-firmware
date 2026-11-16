# PC12M-2 第三轮逐模块精读：源码与原 BIN 差异清单与修复（2026-11-17）

> 目标 BIN：`pc12m2_orig.bin`（SHA-256 `2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`）。
> 所有结论均回查目标 BIN 指令（`objdump -D -b binary -m arm -M force-thumb`，12p 为 Thumb-2）。
> 定制项（菜单文字、字库、20 秒背光、产品信息、认证放行）不在本次排查范围。

## 一、方法

1. 五个只读子代理按文件分工逐分支比对：`07_state_machine.c`（分两段）、
   `09_output_stage.c` + `12_closed_loop.c` + `11_auth.c`、`06_param_system.c` + `08_uart3_modbus.c`
   + `08_modbus_dispatch.c`、`02_lcd_display.c` + `05_adc.c` + `04_i2c.c` + `01_startup.c` +
   `03_input_debounce.c` + `10_relay_led.c` + `13_gpio_init.c`。
2. 自建差分探针（均在 `tmp/`，不进仓库）：
   - `probe_disp12.py` / `probe_disp12_all.py`：显示函数调用序列（13 个 `disp_*` 的
     (名, 参数) 序列），覆盖 菜单 1..0xC/0x14/0x1E/0x5A/0x62/0x63 × 项 × 10 键 × view。
   - `probe_sm12_state.py`：状态机单拍终态（SRAM0 0..0x2200 + 已映射外设区）。
   - `probe_eeprom12.py`：假 EEPROM 下的 `load_config` / `param_sync_live_to_eeprom`。
   - `probe_modbus12_edge.py` / `probe_fuzz_12.py`：Modbus 异常与随机帧。
   - `probe_out_12.py`：`output_stage` 随机状态差分（白名单随机化，避开指针变量）。
   - `probe_save12.py`：菜单保存路径的 EEPROM 写序列。
3. 关键地址纠错（此前脚本多有用错）：12p 屏号在 **0x10001724**（不是 6p 的 0x10001744），
   `0x10001744` 是 12p 的菜单超时 `TIMEOUT`；`0x10001758` 是页刷新计数 `TIMEOUT3`。

## 二、已确认差异与修复

| # | 位置 | 原 BIN 依据 | 差异 | 级别 |
|---|---|---|---|---|
| 1 | `08_modbus_dispatch.c` 两处异常应答 | 0xB42C / 0xB4C2 `adds r0,#0x80` | 源码 `frame[1] \| 0x80`，功能码 ≥0x80 时原机按 8 位回绕（0x83→0x03、0xFF→0x7F），新固件回原功能码 | 功能 |
| 2 | `08_uart3_modbus.c:185` | 0xABC4 `ldrb; cmp #0; ble` | 源码 `(int8_t)` 有符号比较，标志 ≥0x80 时判断相反 | 功能 |
| 3 | `07_state_machine.c` 分派尾部 | 0x9FAA `cmp #0x1e; bne 0xa058` → 0xA058 `b 0xa828` | 共享超时尾只写进了 `if (*MENU == 0x1e)` 内并带 `return`，非法屏号不落尾（原机落尾：`TIMEOUT++`，≥0x1388 回主屏） | 状态 |
| 4 | `01_startup.c:292` 联锁屏 | 0x68A `r1=3, r2=0` | `disp_string(0x740,0,3,0)` 行列写反，会压在「校准模式」行 | 显示 |
| 5 | `07_state_machine.c` case4 第 3 页 | 0x7AC6 `cmp #8; blt` / 0x7ACE `cmp #0xc; bge` | 源码用 `else` 兜底，项号 ≥0xc 时会画第 3 页；原机只画 8..0xb | 显示 |
| 6 | `07_state_machine.c` 8 处 `else` | 见下 | 枚举越界时多画/多算 | 显示 |
| 7 | `09_output_stage.c` EINT3 复位顺序 | 0xF860/0xF866、0xF8CC/0xF8D6 | 源码先 fio1 后 fio0，原机先 fio0 后 fio1 | 结构 |

第 6 项逐条：
- case3 item0 `CTRL_MODE`（原 0x71FE `cmp #2; bne`），越界时原机不画、不动 P1.20/21；
- case3 item10 `DISP_SEL`（0x7506）、item12 `ESTOP`（0x75E6）；
- 校准屏 item5（0x5A7E/0x5A90/0x5AA8）、item6（0x5B54/0x5B66/0x5B7C）；
- case5 item2 `PARITY`（0x87CC，原机 `cmp #3; bne`，≥4 不画 '1 ST0P'）；
- case5 item3 `COM_CHK`（0x8840/0x8852）与菜单 2 入口 `COM_CHK`（0x632C/0x6340），原机按 `==1` 判。

## 三、核对后判定「不需改」的反向结论

- `09_output_stage.c:438/450/461` 的 `*adc_conv_fb = *adc_conv_fb;` 与 `:586/598/610` 的
  `*adc_conv_aux1 = *adc_conv_aux1;` **是原 BIN 真实的同址自写**（0xEC20/0xEC5C/0xECAE、
  0xF0B8/0xF0F4/0xF146，字面量池两处指向同一地址）。12p 不得改成 6p 那种跨槽复制。
- `0x03` 读循环的 "N+2" 行为（`while (byte_count < 2N+3)` 步进 2）属原厂怪癖，源码已忠实复现。
- `param_sync` 的 `cl_thresh_lo` 掉电不恢复（目标集合无 0x1000171B、源集合无 0x10001720）属原厂行为。
- `12_closed_loop.c` 用无符号比较而原机 `blt/bge`：实参恒 <0x80000000，当前不可达。

## 四、验证

- 构建：`text 65120 / data 6312 / bss 2192`，`firmware.bin` 71992 B，
  SHA-256 `16D02C7268FFAAD0F93545721852293F63D67E44890EA1C1CD614954FFABFCBB`。
- `python test/run_tests.py` → 5/5。
- `python tools/verification/verify_firmware_equivalence_12.py` → 全 PASS。
- 差分探针：显示调用序列 152/152 与 全屏 3860 例、状态终态 232/232、EEPROM 34/34、
  Modbus 边界 36/36 与随机 600/600、输出级随机 250/250。
- 未证明：真实中断交错、电气时序、写坏的 EEPROM 值以外的异常源、12p 未覆盖的长按/连击组合。

## 五、遗留（未修）

- 源码注释里的原 BIN 地址整体偏移（`02_lcd_display.c`、`05_adc.c`、`03_input_debounce.c` 等），
  以及 `03_input_debounce.c:12-33` 的引脚标注错误（+0x60/+0x74 实为 FIO3 → P3.25/P3.26；
  0x4/0x8 是 P0.2/P0.3 对调）。属文档问题，不影响行为，未在本次改动。
- `05_adc.c` 七处按 32 位读 uint8 配置字（原机 `ldrb`）；当前高 3 字节恒 0，行为相同。
- 12p 无 `adc0_stop`（原 0x1F48 死代码）。
