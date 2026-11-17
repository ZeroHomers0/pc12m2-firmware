# PC12M-2 初始化与底层 I/O 覆盖补齐（2026-11-17）

## 1. 起因

2026-11-17 的覆盖率统计暴露：十二相 94 个函数、合计 57082 B 中，有一批只写外设、不参与
业务状态的初始化/底层 I/O 函数从未进入任何执行级 A/B。这些函数单个都不大，但合起来是
`SystemInit`、UART3、LCD 字节通道、GPIO、I2C 位时序、ADC 初始化这一整层地基；它们错了，
上层矩阵再密也测不出来（上层用例都在它们建立的硬件状态下跑）。

本轮把这批函数补上对照，并在过程中抓到 1 处真实显示缺陷。

## 2. 函数身份如何确定（不是猜的）

十二相的反汇编证据文件名是 `%08x_FUN_%08x.txt`，只能给出入口地址与字节数，不给语义。
为避免「猜函数名」，用六相的 `tools/audit/full_review_2026-11-16/function_inventory.json`
（87 项，含 `name`/`source`/`old_bytes`）逐字节比对：

| 12p 入口 | 字节 | 六相同名函数 | 字节 |
|---|---|---|---|
| `0x0440` | 262 | `SystemInit` | 262 |
| `0x0786` | 286 | `lcd_data_byte` | 286 |
| `0x09E0` | 276 | `gpio1_init` | 276 |
| `0x1528` | 134 | `gpio_inputs_dir_init` | 134 |
| `0x1B9E` | 28 | `chk_p02_p03` | 28 |
| `0x1BF0` | 44 | `i2c_gpio_init` | 44 |
| `0x1C1C` | 22 | `i2c_delay_short` | 22 |
| `0x1C5E` | 80 | `i2c_start` | 80 |
| `0x1CAE` | 62 | `i2c_stop` | 62 |
| `0x1CEC` | 190 | `i2c_write_byte` | 190 |
| `0x1DAA` | 142 | `i2c_read_byte` | 142 |
| `0x1EB4` | 124 | `adc_init` | 124 |
| `0xA994` | 488 | `uart3_init` | 488 |

13 项中有 12 项的**函数体加紧随其后的 96 B 字面量池都逐字节相同**；唯一例外是
`chk_p02_p03`，池多 10 字节（12p 引用 `0x1000164E`、六相引用 `0x10001656`，不同全局，
预期差异）。`SystemInit` 两仓逐字节完全一致，0 差异字节。

另两个函数语义已定但不逐字节相同：`0x1E6C i2c_read_reg`（64 B）、`0xA856
freq_adjust_sync`（196 B，与六相 `0xAB48` 有 33 字节差异，分支结构有别）。两者读写 SRAM，
且都不在官方 `PAIRS` 表里，此前从未 A/B。

## 3. 判定方式：写迹 + 页末态，而不是重造业务状态

这批函数的共同特征是**只写外设与 GPIO、不读参数、不依赖 SRAM 内容**，因此不需要播种业务
状态，直接比较：

- **写迹**：`UC_HOOK_MEM_WRITE` 记录 `(地址, 宽度, 值)` 序列，逐项比对；
- **页末态**：10 个外设页整体比对 —— `0x40000000`、`0x40004000`、`0x40008000`、
  `0x4002C000`、`0x40034000`、`0x40090000`、`0x4009C000`、`0x400FC000`、`0xE000E000`、
  `0x2009C000`。

读写 SRAM 的两个函数另测：预置槽位后比较整片 SRAM（`0x10000000..0x100029A0`）加 10 页末态。

## 4. 五个必须绕开的 Unicorn 陷阱

全部实测确认，已写进 `test_init_trace_12.py` 的 docstring，避免后来者重踩：

1. **FIO 页 `0x2009C000` 不能挂 `UC_HOOK_MEM_WRITE`**。挂上后 Unicorn 的 TCG 缓存会让新
   ELF 的 `fio1_pin23_ctrl` 等函数体**一条都不执行**，表现为「新固件少写一次 GPIO」。该页
   因此只比末态，不挂写钩子。
2. **`i2c_delay` 的入口是 `0x1C32`，不是 `0x1C46`**。`0x1C46` 在它体内的内层循环里，把 PC
   设到那里会跳过压栈、破坏栈平衡，随后 `UC_ERR_FETCH_UNMAPPED`。
3. **`SystemInit` 死等晶振/PLL 锁定位**（`0x448 tst #0x40; beq` 等 OSCSTAT、`0x496`、
   `0x4B6`、`0x4F8`、`0x518` 共五处）。而且 **Unicorn 的 MEM_WRITE 钩子在写入生效之前就
   触发**——在钩子里置位会被紧随其后的 store 覆盖（实测 SCS 只剩 `0x20`）。改成把要置的位
   记账到 `pending`，在下一指令的 CODE 钩子里补位。
4. **新 ELF 用全局指针槽 `0x2007D810` 间接取 FIO 基址**（原机用立即数池直取），必须预置成
   `0x2009C000`，否则新机读 0 地址写崩。
5. **`V.machine()` 未映射 `0x4002C000`(PINSEL4) / `0x40034000`(ADC0) / `0x4003C000`**，
   需补映射。

## 5. 抓到的真实缺陷：`0xa98c`「停止」串未进字符串池

### 现象

`freq_adjust_sync(event=6)`（置停止）在 `firmware/stub.c` 里调用
`disp_string((int)0xa98c, 3, 0xb, 0)`。新旧固件的 FIO 页末态不同：

```
页0x2009C000: +0x038 OLD=0x13 NEW=0x10, +0x039 OLD=0xC7 NEW=0x47
```

`+0x38`/`+0x39` 是 FIO1 的 SET/CLR 寄存器，说明两版驱动 LCD 的字节序列不同。

### 根因

原机 `0xa98c` 处是 GBK「停止」：

```
0xa98c: cd a3 d6 b9 00
```

但 `disp_string` 走 `strpool_map` 把原固件 flash 地址映射到内嵌 GBK blob。而
`tools/generation/generate_string_pool_12.py` 里

```python
SCAN_FILES_12 = [str(p) for p in (ROOT / "firmware" / "src").glob("*.c")]
```

只扫 `firmware/src/*.c`。十二相把 `freq_adjust_sync` 从 `src/` 迁到了 `firmware/stub.c`
（六相留在 `src/`），于是这个地址从未被收录。`strpool_map` 未命中时**原样返回地址**——
而新 ELF 在 `0xa98c` 处是**指令字节**，`disp_string` 就把代码当字符串渲染。

### 影响范围

用脚本枚举全仓 158 个 `disp_string` 地址实参，逐个检查是否被簇区间或 override 命中：
修复前只有 `0xa98c` 一个漏网，其余 157 个都命中（六相同项检查也是 0 漏网，说明这个问题
是十二相独有的搬迁副作用）。

### 修复

```python
SCAN_FILES_12 = [str(p) for p in (ROOT / "firmware" / "src").glob("*.c")]
# 12p 把 freq_adjust_sync 从 src 迁到了 firmware/stub.c，其 disp_string(0xa98c)
# 也必须进池，否则 strpool_map 未命中该地址，会把新 ELF 的代码字节当字符串渲染。
SCAN_FILES_12 += [str(ROOT / "firmware" / "stub.c")]
```

重新生成后簇数 21→22、blob 2699→2704 B，新增一个 `{43404, 5, strpool_blob+2699}`
（43404 = 0xa98c）的簇，内容即 `cd a3 d6 b9 00`。

### 防回归

新增 `test/emulation/test_strpool_coverage_12.py`，把「地址实参集合 ⊆ strpool 覆盖集合」
变成断言，并把非字面量实参锁定在 4 处已知基线（`01_startup.c`/`02_lcd_display.c`/
`07_state_machine.c`/`stub.c` 各一处形参下传）。回退生成器的改动即报：

```
[FAIL] 0xA98C 未被 strpool 覆盖（调用点: stub.c）
[FAIL] strpool 覆盖不完整：1 个地址会渲染成指令字节
```

## 6. 检出能力双向验证

补测试最怕「写了但抓不到错」。两次注入均被本测试报出：

| 注入点 | 改动 | 报出的差异 |
|---|---|---|
| `firmware/src/01_startup.c:112` | `TIMER0->MR0 = 1999` → `1998` | `+0x018 OLD=0xCF NEW=0xCE`（TIMER0 页） |
| `firmware/stub.c:59` | 上限 `0x2b0` → `0x2af` | `+0x1624 OLD=0xB0 NEW=0xAF`（freq_adjust 槽） |

第二次注入**首次未被检出**：当时 `freq_adjust_sync` 只跑了 `event=0/1`，压根没进递增分支。
补上 `event=2/0x16/3/0x21/5/6` 全部分支（含上限 `0x2b0`、下限 `0x45` 的边界）后才报出。
**教训：函数级 A/B 只跑「无害入参」等于没测，必须覆盖每个行为分支的边界。**

## 7. 覆盖率变化

| 口径 | 补测前 | 补测后 |
|---|---|---|
| 十二相函数字节覆盖（被测试引用） | 82/94 函数 = 98.9% | 82/94 函数 = 98.9% |

补测的 21 个入口把此前未纳入 A/B 的约 2752 B 全部纳入执行级对照。仍未被引用的 10 个函数
是 IAR C 运行时（`0x00cc` 192 B、`0x0180`/`0x0184` 各 4 B、`0x0188` 26 B、`0x019a` 22 B、
`0x01c4` 10 B、`0x02ee` 14 B、`0x11178` 74 B、`0x111c4`/`0x111cc` 各 4/8 B，共约 332 B），
属编译器运行时启动代码，不承载应用逻辑。

## 8. 验证结果

- 构建：`text 65168 / data 6312 / bss 2192`，`firmware.bin` **72040 B**，SHA-256
  `C84AE01FDD7B306522D15D38E6275624BA4A0A35AABCA3C53FE7115E53571C6B`。
- `test/emulation/test_init_trace_12.py`：**37/37 通过**（26 组外设函数 + 11 组 SRAM 函数）。
- `test/emulation/test_strpool_coverage_12.py`：**通过**（158 个地址全覆盖）。
- `test/run_tests.py`：模块通过 **12/12**（原 10/10 加两个新测试）。
- `tools/verification/verify_firmware_equivalence_12.py`：**全部 PASS**，含 DISPLAY_MATRIX 106、
  DISPLAY_FULL_EXEC 4、DISPLAY_SCREEN_STATIC 32298、DISPLAY_SCREEN_CALIB 30505、XO_FONT。

本轮**未烧写、未实机验证**。
