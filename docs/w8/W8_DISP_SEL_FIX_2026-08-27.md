# W8 case1 首页"输入信号"上下键无效——修复记录（2026-08-27）

> 前置：见 `W8_POST_FLASH_2026-08-26.md`（CRP 布局修复+SWD 烧写）。本文记录
> case1（一级菜单首页）第一行"输入信号"上下键无效的两个根因排查、修复与 DISP_SEL
> 语义破译。总入口见 `W8_ONBOARDING_2026-08-22.md`。

## 0. 一句话结论

- **根因1（已修复烧写）**：case1 三分支曾被 C 复刻错误嵌套进 `FAULT==0` else。W8 断市电
  缺相参数开启 → FAULT 置位 → 上下键永不执行。已移出 FAULT 门控，A/B 编译验证 + 板上
  字节比对确认。
- **根因2（原厂设计，需用户操作）**：`DISP_SEL(0x10001655)` = **控制方式**
  （0=通讯、1=本地、2=定值）。**只有"定值"分支处理 UP/DOWN 调 MANUAL**，本地/通讯分支
  不处理（原厂 0x52FA-0x542C + 操作手册第 77 行双重确认）。当前 EEPROM 控制方式=1（本地），
  需在"基本参数设置→控制方式"切到"定值"后上下键才有效。

## 1. 符号与语义（本会话破译）

| RAM 地址 | 符号 | 语义 | 持久化 |
|---|---|---|---|
| `0x10001655` | DISP_SEL | 控制方式：0 通讯 / 1 本地 / 2 定值 | EEPROM reg 0x1a（param_sync 453-457） |
| `0x10001685` | DISP_SEL 影子 | load_config 恢复源 | 84-85 行 i2c_read_reg(0x1a) |
| `0x10001634` | CTRL_MODE | 运行模式（恒压/恒流/开环），三分支选电压/电流量程 | EEPROM reg 0x0a |
| `0x100015a8` | TARGET | 输入信号显示值（本地模式首页显示此值） | 不持久化 |
| `0x100015d8` | MANUAL | 定值模式给定（首页显示） | EEPROM reg 0x1d/0x1e |
| `0x100015dc` | MANUAL 缓存 | param_sync 比对副本 | 同上 |
| `0x100015d4` | MANUAL×量程/1000 | V_AMP 源 | 不持久化 |

## 2. 根因1：三分支 FAULT 门控（已修复）

**原厂结构**（`0000458c_FUN_0000458c.txt`）：
```
0x52A2: ldrb FAULT; cbnz FAULT, 0x52FA   ; FAULT!=0 → 跳过 0x52A8-0x52F6 启停，直接到三分支
0x52A8-0x52F6: RUN/STOP/复位逻辑（受 FAULT 门控）
0x52FA-0x542C: DISP_SEL 三分支（不受 FAULT 门控）
```

**C 复刻错误**：把三分支嵌套进 `if (*FAULT == 0) { ... } else { 三分支 }`，导致
FAULT=7（缺相）时上下键永不执行。

**修复**（`07_state_machine.c:564-590`）：三分支移出 FAULT 门控。
```c
/* 0x52FA：DISP_SEL 三分支不受 FAULT 门控——原厂 0x52A2 在 FAULT!=0 时
 * cbnz 跳到 0x52FA 仍执行三分支，仅 RUN/STOP 逻辑(0x52A8-0x52F6)被 FAULT 跳过。 */
if (*DISP_SEL == 0) { *TARGET = *FREQ; ... }
else if (*DISP_SEL == 1) { *V_AMP = *V_AMP2; }
else { /* DISP_SEL==2：MANUAL 增减 + TARGET/0x15d4/V_AMP 写入 */ }
```

**A/B 验证**：旧产物 `bne 4b92`（三分支在门控内）→ 新产物 `bne 5152`
（FAULT cbnz 直接跳三分支）。板上字节比对一致：
```
0x510e: cbnz r3(FAULT@0x10001624), 0x5152   ; FAULT!=0 → 三分支
0x5152: ldr r2,=0x10001655; ldrb [r2,#85]    ; DISP_SEL
```

## 3. 根因2：DISP_SEL=控制方式，定值才处理上下键（原厂设计）

**原厂三分支**（0x52FA-0x542C）：
- `DISP_SEL==0`（0x5300-0x5350）：TARGET=FREQ+量程换算，**无按键处理**
- `DISP_SEL==1`（0x5352-0x5360）：仅 `V_AMP=V_AMP2`，**无按键处理**
- `DISP_SEL==2`（0x5362-0x542C）：`key==2/0x16`→MANUAL++、`key==3/0x21`→MANUAL--，
  然后 TARGET=MANUAL、0x15d4=MANUAL×量程/1000、V_AMP=V_AMP2=0x15d4

**操作手册**（`_manual_utf8.txt` 第 77 行）：
> 控制方式：当黄标移到控制方式:本地，按下SET键，本地闪烁，按压DOWN键，切换到通讯，
> 定值，当确认通讯时，按下SET键，系统进入通讯控制模式；**当定值闪烁时，按下SET键，
> 系统给定由显示屏UP键和DOWN键，增大或减小**。

第 20 行确认断电保存：
> 无论在"停机"状态还是"运行"状态，按压"UP"键，输入信号增大...每按一次增大0.1%...
> 当设置到某一个值时，例如：输入信号：50%，下次开机时，此值不变。

## 4. 残留验证数据（修复版，2026-08-27）

`backup/jlink_recheck_disp_sel.jlink` / `jlink_full_disp_zone.jlink`（connect-under-reset +
ClrRESET 自由跑 → SetRESET 停核读残留）：

```text
DISP_SEL(0x10001655) = 0x01  影子(0x10001685) = 0x01   → EEPROM reg 0x1a = 1（本地）
TARGET(0x100015a8)   = 0x00                              → 本地模式显示 0.0%
0x100015d4           = 0x00
V_AMP(0x100015d0)    = 0x00  V_AMP2(0x100015b4) = 0x00
MANUAL(0x100015d8)   = 0x1F4(500)  缓存(0x100015dc)=500  → EEPROM 恢复成功
FAULT(0x10001624)=0  RUN(0x10001628)=0  MENU(0x10001744)=1  CTRL_MODE(0x10001634)=0
V_RANGE(0x1000163c)=525  A_RANGE(0x10001638)=1000
```

**关键推断**：DISP_SEL 写入点只有 case3 菜单（`07_state_machine.c:974/988`）和
Modbus（`08_modbus_dispatch.c:173`），**无启动时强制写 1 的代码**。当前 EEPROM=1（本地）
是测试中经菜单切到本地并保存所致。用户报告"原厂上电直接可调"= 原厂出厂 EEPROM 控制方式=定值(2)。

## 5. 用户操作指引（✅ 已验证 2026-08-27）

1. 首页按 SET → 密码（出厂密码：连续按 UP 3 次 + 连续按 DOWN 3 次）→ SET
2. 基本参数设置 → 黄标移到"控制方式"（第10项）→ SET（值闪烁）
3. 按 UP 切到"定值" → SET 确认 → 退出
4. 回首页：显示"输入信号 50.0%"（MANUAL=500），按 UP/DOWN 每按 ±0.1%、长按连续、
   断电保存（EEPROM reg 0x1d/0x1e）

**实机验证结果（2026-08-27）**：按上述操作切到"定值"后，首页上下键正常工作——
每按一次 ±0.1%、长按连续增减、断电重启后保存值不变，均符合预期。**本根因闭环。**

## 6. 未决/后续

- [x] 用户实测切到定值后上下键是否生效（2026-08-27 已实测：生效，闭环）
- [x] case3 菜单 item10 DISP_SEL 编辑键循环语义——**2026-08-29 A/B + BIN 反汇编闭环**：
      BIN 增量钳位（1→2→2），C 复刻曾误写成环绕（2→0），已修复（见下）。
      手册"DOWN 切通讯/定值"以本地=0 描述，实机行为以 BIN 钳位为准。
- [ ] `MENU==0x1e`（`07_state_machine.c:1790-1815`）的另一份三分支结构正确（不受门控），
      但调用时机（运行状态？）待确认与 case1 无冲突

### 2026-08-29 增量：case3 枚举项环绕→钳位修复（bug #5 控制方式互斥）

**BIN 实测**（`LPC1765.bin` 0x6DF0-0x6EFA 反汇编 + A/B 修正播种后全矩阵）：
- item10 DISP_SEL（0x6E42-0x6E60）：增量 `++ ; cmp #2; ble; movs #2` → **钳位在 2**
  （定值 按 UP 保持定值，不会跳回通讯）
- item11 b56（0x6E60-0x6E7E）：钳位在 1；item12 ESTOP（0x6E7E）：钳位在 2；
  item13 FEEDBACK（0x6EA2）：钳位在 1；item14 INPUT_SEL（0x6EC0）：钳位在 1
- **item0 CTRL_MODE（0x6DAE-0x6DC8）例外：增量 2→0 环绕、减量 0→3→2 环绕**，与上不同
  （原厂对不同枚举项故意用不同语义）
- 减量方向 items 10-14 全部钳位在 0（2→1→0→0），无 0→max 环绕

**C 复刻错误**（`07_state_machine.c` case3 编辑 switch 增量分支）曾把 items 10-14 写成
`if (*X > max) *X = 0` 环绕。**2026-08-29 修复为钳位**（item0 保持环绕以匹配 BIN）：

```c
case 10: (*DISP_SEL)++;  if (*DISP_SEL > 2) *DISP_SEL = 2; break;   /* BIN 钳位 */
case 11: (*b56)++;       if (*b56 > 1) *b56 = 1; break;              /* BIN 钳位 */
case 12: (*ESTOP)++;     if (*ESTOP > 2) *ESTOP = 2; break;          /* BIN 钳位 */
case 13: (*FEEDBACK)++;  if (*FEEDBACK > 1) *FEEDBACK = 1; break;    /* BIN 钳位 */
case 14: (*INPUT_SEL)++; if (*INPUT_SEL > 1) *INPUT_SEL = 1; break;  /* BIN 钳位 */
```

**A/B 验证**：修正播种顺序（`_seed_display_items` 会把 ESTOP/FEEDBACK/INPUT_SEL 播成
1/0/0，必须在标准播种**之后**再覆盖目标项）后全矩阵 48 用例（items 0/10/11/12/13/14 ×
值 0..3 × UP/DOWN）修复前 13 差异 → 修复后 **0 差异**；扩展 case3 全 16 项 × 边界值 ×
4 键（2/3/0x16/0x21）**256 用例 0 差异**。

**与 bug #6 关联**：只有"定值(2)"分支处理上下键调 MANUAL。修复前在定值按 UP 会环绕到
通讯(0)，模式在三分支间跳变、定值给定被跳过；修复后定值保持定值，模式互斥稳定。

## 7. 相关脚本/备份

- `backup/pre_disp_sel_fix_flash.bin`：三分支修复前固件备份（SHA 见 08-27 记录）
- `backup/jlink_recheck_disp_sel.jlink` / `jlink_full_disp_zone.jlink`：残留验证
- `backup/jlink_set_disp_sel_constant.jlink`：在线写 DISP_SEL 尝试（结论：ClrRESET 后
  load_config 用 EEPROM 值覆盖 live，在线改 RAM 无法持久化，须走面板菜单）
