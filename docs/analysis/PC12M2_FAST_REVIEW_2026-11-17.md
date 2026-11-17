# PC12M-2 快速复核（2026-11-17）

> 目标：按用户要求对十二相做**快速**复核，不重复 PC6M-10 的慢路径
> （6M 曾用：87 函数逐条反汇编人工核对、状态机矩阵数十万例、8 个子代理逐文件精读）。
>
> 方法：把 6M 最终收敛出的**比较类指令立即数集合差**方法移植到 12p，
> 只对「差集未被 ±1 改写解释」的条目回查原机反汇编，再补两项**测试覆盖门禁**探针。
>
> 结论：**未发现新的行为差异**；发现 1 项**测试覆盖缺口**（不影响当前正确性，但属
> 6M 同类缺陷的潜在漏检点），已记录并给出补测建议。

---

## 一、方法与工具

### 1.1 立即数（阈值）差分

工具（均在本仓 `tmp/`，一次性探针，不入库）：

| 脚本 | 作用 |
|---|---|
| `tmp/cmpdiff12.py` | 按 42 组「原机入口 → 新 ELF 符号」抽取 `cmp/cmn/tst/teq`（含条件后缀与 `.w`）的 `#imm` 集合，做双向差集，并过滤「v±1 在对方集合出现过」的等价改写；结果写 `tmp/cmpdiff12.txt` |
| `tmp/one12.py` | `python tmp/one12.py <旧起> <旧止> <新符号> <imm...>`，并排打印两侧 `cmp/tst` 上下文（±3 行） |
| `tmp/ctx12.py` | 在全 `state_machine` 范围内按立即数检索上下文 |

两处关键实现修正（否则结果失真）：

1. **符号名**：`verify_firmware_equivalence_12.py` 的 `PAIRS` 值是**旧名映射**
   （`wdt_isr`/`timer0_isr`/`uart3_isr`/`timer1_isr`/`timer2_svc`），新 ELF 实际符号是
   `WDT_IRQHandler`/`TIMER0_IRQHandler`/`UART3_IRQHandler`/`TIMER1_IRQHandler`/`TIMER2_IRQHandler`。
2. **原机函数边界**：`evidence/reverse/disassembly/functions/*.txt` 首行 `body=N`
   **不含尾部字面量池**（`state_machine` 报 21844，实际代码延续到 0x9B44）。改为取「本入口到
   下一个证据入口」的距离并取 `max(body, diff)`。

结果：42 个函数中 21 个存在差集，未解释条目 54 条，逐条定性后**全部为等价改写或数据区误判**。

### 1.2 覆盖门禁探针（防止「假通过」）

| 脚本 | 作用 |
|---|---|
| `tmp/probe_closed_loop_entry.py` | 统计 `output_stage` 各组合是否真的调用 `closed_loop_wrapper` |
| `tmp/probe_password_delay.py` | 统计状态机用例是否真正走到 RETURN（未被指令预算截断），以及是否命中密码延时循环 |
| `tmp/probe_feedback_copy2.py` | 统计原机 6 处**跨槽反馈复制**指令是否被现有矩阵执行 |

---

## 二、54 条未解释差集的定性（全部等价）

### 2.1 位测试（`tst.w #bit` → `lsls` + `bmi/bpl`）

原机故障字 12 个位测试 `0x4582 tst.w r0,#64` / `0x4592 #1024` / `0x45A2 #16` / `0x45B2 #32` /
`0x45C2 #256` / `0x45D2 #128` / `0x45E2 #16384` / `0x45F2 #32768` / `0x4602 #2048` /
`0x4612 #8192` / `0x4622 #4096`；新 ELF 对应 `0x45FC lsls r6,r3,#29`（bit2）、
`0x460E lsls r0,r2,#30`（bit1）、`0x461C lsls r1,r2,#31`（bit0）、`0x462E lsls r2,r2,#28`（bit3）、
`0x466E lsls r0,r2,#27`（bit4）、`0x467C lsls r1,r2,#26`（bit5），并在 `0x8388–0x840A` 重复一遍。

位序号与 `firmware/src/07_state_machine.c:379-394` 的映射一致
（0x1/0x2/0x4→1、0x8→2、0x200→3、0x40→4、0x400→5、0x10→6、0x20→7、0x100→8、0x80→9、
0x4000→0xa、0x8000→0xb、0x800→0xc、0x2000→0xd、0x1000→0xe）。

同类：`scan_run_stop`（`0x198C tst.w #0x8000000` → `0x1A4A lsls r5,r5,#3`）、
`debounce_p09`(`0x1A6C #512`)、`debounce_p116`(`0x1A9A #65536`)、`debounce_p117`(`0x1AF2 #131072`)、
`debounce_p06`(`0x1B4A #64`)、`uart3_tx_byte`(`0xAB9E #32`、`0xABB4 #1`)。

### 2.2 数据区被误反汇编

`state_machine` 中 `0x4CFC cmp r5,#45` 与 `0x6AE0/0x4CA0/0x4CC8/0x5FD4/0x5FE8/0x564C/0x5660/0x5674/0x5688/0x569C cmp r6,#48..57`
落在字符串数据区（`0x4CF8` 处字节 `ebc2 2d3a`、`0x6ADC` 处 `563a 2e32 2e30 3032 3631`），
不是运行时阈值。

### 2.3 `>=` 与 `>` 的编译改写（逐条已核对）

| 新 ELF | 原机 | 判断 |
|---|---|---|
| `0x7EE2 cmp r2,#68 / itt ls / movls #69` | `0x985C cmp r0,#69 / bcs / movs #69`（池 `0x9C24`=0x10001624 相位偏移） | 等价（`>=0x45` ≡ `>0x44`） |
| `0x7338 cmp r3,#245 / itt hi / movhi #246` | `0x85F8 cmp r0,#246 / blt / movs #246`（从站地址上限） | 等价 |
| `0x7C02`/`0x7C3E `cmp r2,#249 / movhi #250` | `0x921E`/`0x9262`/`0x9DE8 `cmp r0,#250 / blt / movs #250` | 等价 |
| `0x478E`/`0x47A8 cmp r2,#59` | `0x46D4`/`0x46EC cmp r0,#60 / bcc` | 等价（秒进位） |
| `0x8BC2 cmp r3,#62`（read）/ `0x8BFE cmp r3,#61`（write） | `0xAE06 tbb` 前守卫 `idx>=0x3f` / `ofs>=0x3e` | 等价 |
| `0xA3E0 cmp r2,#9 / bls`、`0xB132 cmp r2,#9 / itt ls / movls #10` | `0xE724 cmp r0,#10 / blt` | 等价（`++tick > 9`） |
| `0xC6F0 cmp r3,#4 / bls`（`auth_verify_loop`） | `0x10A6C cmp r0,#5 / blt` | 等价 |
| `0xC5FA cmp.w lr,#7 / bls`（`auth_challenge`） | 挑战表长度检查 | 等价 |
| `0xBC1C`/`0xBC90 cmp r1,#99`（TIMER1） | 原机 `cmp r0,#100` | 等价 |

`0x8174 cmp r2,#199 / bls / movs #199` 对应原机 `0x9B6E cmp r0,#199 / ble / movs #199`
（0x5A 手动平衡页，池 `0x9C60` = `0x1000168C` `BAL_ANG`），**完全一致**；
`firmware/src/07_state_machine.c:1758-1766` 的 `> 0xc7 → 0xc7` 与 `< 2 → 2` 后自减（下限实为 1）也对得上。

### 2.4 显示与定制项

- `disp_clear`：原机 `0x942` 双循环（行 <8、列 <64，起始 `0xb8`）；新 ELF `0x9FE movs r5,#64`
  递减 + `0xA12 cmp r4,#192`，覆盖 `0xb8..0xbf` 共 8 行 × 64 列 = 512 字节，等价。
- `disp_render_char8` 新增 `0x1c/0x24/0x4F('O')/0x58('X')/0x61('a')`、`disp_string` 新增
  `0x61/0x69/0x6D/0x6E/0x78/0xA6/0xC1` 等：12p **已授权的字库/字符定制**（X/O 与 α 字形），非缺陷。
- `load_config` 新增 `0x1/0x2/0x3/0x4`：魔数分支化，等价。
- `TIMER1_IRQHandler`：新 ELF 98 条 `cmp` 覆盖原机 81 条的同一集合
  （50/240/20/40/60/80/100/120/140/160/180/200/220/240 的 240 步扫描表），无缺项。

---

## 三、主验证与回归基线（复核时全部通过）

```
python tools\verification\verify_firmware_equivalence_12.py
  VECTOR / PIN_CONFIG 48 / GPIO2_INIT 7 / AUTH_CHALLENGE 90 / GPIO_OUTPUTS_SET 12 /
  FIO0_P22 / FIO1_P22 / RELAY_DIRECT 4×2 / TIMER0 / TIMER2 6 / WDT / UART3 ×2 /
  TIMER1_MATRIX 972 / CRC_MATRIX 5 / MODBUS_REGS read=65 write=320 /
  CLOSED_LOOP integral=6 wrapper=12 / OUTPUT_STAGE_MATRIX 36 /
  CONSTANT_CURRENT_VOLTAGE_LIMIT 84 / CONSTANT_VOLTAGE_CURRENT_LIMIT 162 /
  RUN_STOP_PRESET cfg=2 / STATE_MACHINE_MATRIX 130 / DISPLAY_MATRIX 106 /
  DISPLAY_FULL_EXEC 4 / DEBOUNCE 9 + scan_run_stop 4 / AUTH 3 / XO_FONT /
  MISC_COMPLETED ×4 / DISPLAY_SCREEN_STATIC 32298 / DISPLAY_SCREEN_CALIB 30505 / MISC 6
  → 「12p A/B 等价性验证：全部 PASS」

python test\run_tests.py     → 模块通过 8/8（MODBUS 5/5、PARAM_SYNC 6/6 等）
```

补齐 4.3 的覆盖门禁后重跑：`python test\run_tests.py` → **模块通过 9/9**
（新增 `test_emulation_test_feedback_copy_12`），12p 主 A/B 仍全部 PASS，构建仍为
`text 65156 / data 6312 / bss 2192`、72028 B、SHA-256 `EA2920F1…9E728D2`（未改业务源码）。

---

## 四、覆盖门禁结果（本轮新增发现）

### 4.1 密码错误延时：原结论已被推翻（探针自身有缺陷）

**⚠ 本节 2026-11-17 晚些时候被推翻，保留原文以示过程。**

原结论：`tmp/probe_password_delay.py` 跑 11 屏 × 10 键 = 110 例得
`loop_old=0 loop_new=0 trunc_old=0 trunc_new=0`，据此认为现有矩阵没走到密码错误路径、
不存在 6M 那种「预算截断导致假通过」。

**推翻原因（两处）**：

1. **探针根本没生效**。`tmp/probe_sm12_state.py` 的 `BASE` 字典里含
   `0x10001724: 30, 0x10001725: 20, 0x10001726: 10`，而这三个地址正是
   `MENU`/`MENU2`(项号)/`MENU3`(编辑态)。`snapshot()` 先写用例参数
   `uc.mem_write(0x10001724, bytes((menu, item, view)))`，随后又用 32 位
   `for address, value in params.items(): uc.mem_write(address, struct.pack("<I", value))`
   把三个字节覆盖成 `0x1E / 0x14 / 0x0A`。实测：`menu=0x02/0x04/0x63` 三者执行的指令集合
   **完全相同**（442 条），起始三字节都是 `('0x1e','0x14','0xa')`。
   即此前跑的所有「屏 × 项 × 键」矩阵，其实都在跑**同一个用例**（认证屏）。
2. **延时有 10 份副本，不是 3 份**。全镜像扫描 `movw rX,#10000 ; cmp ; bcc` 取 `bcc` 目标：

   | movw | 循环体 | | movw | 循环体 |
   |---|---|---|---|---|
   | `0x5256` | `0x5220` | | `0x8be8` | `0x8bba` |
   | `0x539a` | `0x536c` | | `0x8c9e` | `0x8c70` |
   | `0x89b6` | `0x8988` | | `0x8ce8` | `0x8cba` |
   | `0x8ad2` | `0x8aa4` | | `0x8d32` | `0x8d04` |
   | `0x8b1c` | `0x8aee` | | | |
   | `0x8b66` | `0x8b38` | | | |

   只快进前三份时，`menu=0x6 key=0xE` 仍会耗尽预算（停在 `pc=0x8bca`）。
   另有 `0xf4c`/`0x107e`（显示数值用）、`0x1c46`（EEPROM 写周期延时 `FUN_00001c32`）、
   `0xf322` 也是 `movw #10000`，但都不是该延时循环，**不能**一并快进。

3. 官方验证器 `verify_firmware_equivalence_12.py:515` 的 `_seed_state_machine()`
   不受此缺陷影响（它是唯一写这三字节的地方，且其后不再覆盖），其 130 例矩阵有效。

### 4.2 闭环调用：覆盖偏窄

`tmp/probe_closed_loop_entry.py`（24 组 cfg×gain_sel×locked×th）：
只有 `cfg=1` 且 `gain_sel∈{0,1}` 且 `locked=0` 的 4 组真正调用了 `closed_loop_wrapper`（各 1 次），
OLE 与 NEW 次数一致。`locked∈{4,5}` 的组合不进入闭环，属原机门控行为，非缺陷；
但说明**闭环相关断言的有效样本只有 4 组**。

### 4.3 跨槽反馈复制：原有矩阵仅覆盖 2/6 → 已补门禁至 6/6

原机 `output_stage` 的反馈槽为 `fb=0x100015B8`、`aux1=0x100015BC`，共 6 处跨槽复制：

| 原机指令 | 语义 | 源码 | 原有矩阵 | 补齐后 |
|---|---|---|---|---|
| `0xED18` | `fb ← aux1` | `firmware/src/09_output_stage.c:475` | ❌ 未覆盖 | ✅ |
| `0xED7E` | `fb ← aux1` | `:488` | ❌ 未覆盖 | ✅ |
| `0xEDD2` | `fb ← aux1` | `:500` | ❌ 未覆盖 | ✅ |
| `0xF1B0` | `aux1 ← fb` | `:624` | ❌ 未覆盖 | ✅ |
| `0xF216` | `aux1 ← fb` | `:637` | ✅ 覆盖 | ✅ |
| `0xF26A` | `aux1 ← fb` | `:649` | ✅ 覆盖 | ✅ |

另有 6 处**同槽自写**（原机确实是自赋值，属原厂行为，不可改成跨槽）：
`0xEC20`/`0xEC5C`/`0xECAE`（`fb ← fb`，源码 `:438/:450/:461`）与
`0xF0B8`/`0xF0F4`/`0xF146`（`aux1 ← aux1`，源码 `:598/:610`）。

**定性**：源码写法**正确**（跨槽复制指向对方槽，自写处保持自写），已由原机指令逐条确认。
但原有 A/B 矩阵（`OUTPUT_STAGE_MATRIX` 36 例 + 恒流限压 84 例，共 120 例）**只执行了 6 处中的 2 处**
（`0xF216`/`0xF26A`），另 4 处从未被任何用例进入 —— 与 6M 曾出现的真实缺陷
（`09_output_stage.c` 第二路三处被误写成自赋值，而回归恰好未进入该分支、30/30 仍全绿）属**同类风险**。

**已补**：新增 `test/emulation/test_feedback_copy_12.py`，把「6 处原机复制指令必须被执行」做成
**覆盖门禁**，同时把「`fb`/`aux1` 写入序列与原 BIN 逐条一致」做成**行为契约**：

- 用 `UC_HOOK_CODE` 断言每条原机地址 `0xED18/0xED7E/0xEDD2/0xF1B0/0xF216/0xF26A` 都执行过；
- 用 `UC_HOOK_MEM_WRITE`（`0x100015B8..0x100015BF`）记录两侧 `(地址, 值)` 写入序列并比较；
- 6 组定向用例均令 `fb != aux1`，故一旦把跨槽复制误写成自赋值，写入值必然不同而直接失败。

运行结果：`[PASS] 反馈跨槽复制契约：6/6 处已覆盖，原 BIN 与自编译 ELF 写入序列一致`。
该用例已被 `test/run_tests.py` 自动发现，模块数由 8 升至 **9/9**。

---

## 五、结论

1. **发现并修复 1 处真实行为差异**（2026-11-17 状态机全矩阵新增，见第七节）：
   `MENU==3` 基本参数屏的擦除帧用入口缓存的项号 `it` 而非重读 `*MENU2`，
   在 `key==4`（保存退回，已把 MENU2 清 0）那一帧擦错行。原机在擦除块内 15 处
   各自重读 `0x10001725`。已改 `switch (*MENU2)`，并新增
   `test/emulation/test_state_machine_erase_12.py`（112 例 + 15 处覆盖门禁）。
2. **42 个关键函数的比较类立即数差集全部可解释**为位测试改写、`>=`/`>` 改写、
   `switch` 降级、数据区误反汇编或既有授权定制（字库/字形/背光/认证放行）。
3. 12p 主 A/B（含 TIMER1 972、MODBUS read 65/write 320、DISPLAY 106、
   DISPLAY_SCREEN_STATIC 32298、DISPLAY_SCREEN_CALIB 30505）全 PASS；
   `test/run_tests.py` 由 8/8 → 9/9（跨槽反馈复制）→ **10/10**（擦除帧项号重读）。
4. 与 6M 已修的 7 类差异逐项对照，12p 均为**已正确**状态：
   异常功能码 `+0x80` 回绕（`08_modbus_dispatch.c:161/172`）、
   枚举越界不绘制（`07_state_machine.c:185/205/214/324/329`）、
   保护页第 3 页上界 `8..11`（`:752/818/879/1218/1524/1604`）、
   端子事件码 7/8（`inc/input_events.h`）、跨槽反馈复制（见 4.3）。
5. 测试覆盖方面补了两处缺口：跨槽反馈复制 6/6（`test_feedback_copy_12.py`）、
   擦除帧项号重读 15/15（`test_state_machine_erase_12.py`）。

## 六、附：本轮未做的事（明示范围）

- 未做逐函数逐指令人工通读（6M 做过，约 87 函数）。
- 未做子代理并行精读。
- 未烧写、未实机验证。

## 七、状态机全矩阵（2026-11-17 追加，含真实差异修复）

### 7.1 为什么要先证明「矩阵能抓错」

初版矩阵给出 `6732/6732`、`7344/7344` 全绿，但**注入缺陷后依然全绿**
（把 `07_state_machine.c:185` 的 `else if (*b2c == 2)` 改成 `else`，
并追加 `fio1_pin21_ctrl(1)` 副作用）。查下来是探针自身三处缺陷：

| # | 缺陷 | 表现 | 修法 |
|---|---|---|---|
| 1 | `BASE` 字典含 MENU/MENU2/MENU3 三字节，32 位覆盖了用例参数 | 所有屏跑的其实是同一用例（见 4.1） | `BASE_FIXED` 排除三字节，用例参数**最后写** |
| 2 | 只比 SRAM 末态，且跳过 `disp_*` 函数体 | 纯显示缺陷天然不可见 | 记录 `disp_*` 调用序列（名 + r0..r3）与 pin 调用序列 |
| 3 | `UC_HOOK_MEM_WRITE` 挂在 `0x2009C000` 段 | 新版 `fio1_pin23_ctrl` 函数体一条不执行（Unicorn TCG 缓存问题） | 改用 `UC_HOOK_CODE` 记录调用 |

### 7.2 另外三处探针错误（都会造成假差异）

- **`disp_splash_screen` 的 OLD 地址写成 `0x1486`**，正确是 `0x40B0`。
  `0x1486` 落在 `disp_fixed_1dec`（`0x13EC..0x14A2`）体内，于是 OLD 侧不跳过
  splash 体、NEW 侧跳过，`key=4` 的绘制序列凭空不同。
- **`fio0_pin22_ctrl` / `fio1_pin22_ctrl` 的 OLD 地址写成 `0xE966/0xE946`**，
  正确是 `0xE6C6` / `0xE6A6`（官方 `PAIRS` 表）。写错时挂到别的函数上，
  产生「NEW 多调用两次 pin22」的假差异。
- **新版 `fio[01]_pin22_ctrl` 被 GCC 拆成公开符号 + `.part.0`**，
  `run_stop_preset` 等处直接 `bl ...part.0`（level<1 路径尾跳），
  只挂公开符号会漏记；且 `.part.0` 里的 R0 是陈旧值（实测 17216），
  不能当作实参记录。

### 7.3 最终矩阵结果

修正后 `tmp/probe_sm12_matrix_v4.py`：
`python tmp\probe_sm12_matrix_v4.py` → **`cases=14688 bad=0 err=0
disp_calls=27531 pin_calls=65516`**，耗时约 5 分钟。

维度：18 屏 × 17 项号 × 6 键（0/1/2/3/4/5/0x16/0x21 中实际 6 个有效值）
× 2 视图 × 2 个 `TIMEOUT3` 起始档（`0xFA` → 自增到 `0xFB` 触发整页重绘；
`0x1F4` → 自增到 `0x1F5` 触发擦除帧）。

**检出能力已验证两次**（每次都跑完、看完差异、再还原）：

| 注入的缺陷 | 矩阵结果 |
|---|---|
| `case3` 枚举越界仍绘制（`else if (*b2c == 2)` → `else`） | `bad=166/816`（屏 3 单元） |
| `case3` 擦除帧用 `it` 而非 `*MENU2`（本次真实缺陷） | `bad=14/816`（屏 3 单元） |

### 7.4 复现方式

```powershell
cd D:\code\LPC1765FBD100\PC12M-2
$env:PYTHONIOENCODING='utf-8'
python tmp\probe_sm12_matrix_v4.py            # 全 18 屏
python tmp\probe_sm12_matrix_v4.py 3          # 只跑屏 3
```

`tmp/` 下探针不入库（一次性诊断工具）；入库的守门测试是
`test/emulation/test_feedback_copy_12.py` 与
`test/emulation/test_state_machine_erase_12.py`。
