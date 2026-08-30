# 目标B 可读化改造规范（L0 + L1 + L3）

> 读者：改造子代理 / 后续接手者。目标是把反编译源码从「天书」变「可读」，
> **不改任何运行逻辑**，不破坏已完成的 W7 行为等价验证。

## 一个铁律：行为等价

反编译源码是**逐函数对照反汇编验证过行为等价**的（W7）。本次改造只许做
「重命名 + 注释 + 类型别名」，**禁止改变控制流、禁止改变任何存取地址、
禁止改变任何访问宽度**。任何看起来像「重构」的动作都不做。

## 三种层次的许可范围

### L0 —— 变量/参数语义化（安全，最大可读性增量）
把反编译垃圾名换成语义名：
- `param_1 / param_2 / ...` → 按函数语义命名（如 `len` / `reg_addr` / `tx_byte`）
- `uVar1 / iVar1 / piVar1 / pbVar1 / puVar2 / bVar / cVar` → 语义名
  （如 `crc_hi` / `tbl_idx` / `tick_cnt` / `tx_idx` / `uart3` / `pinsel`）
- 命名规范：小写 + 下划线；指针加语义前缀（`p_` 或直接 `xxx_ptr`）；布尔用 `is_*`。

**关键技巧：不同函数的 `param_1` 含义不同，绝不能全文替换。** 用工具
`tools/rename_locals.py` 按函数边界替换：
```
python tools/rename_locals.py src/xx.c 函数名 param_1=len,param_2=reg_addr,puVar1=uart3
```
该脚本只替换**指定函数体+签名**内的词，保留别处同名变量。可多次调用，逐函数处理。

### L1 —— 类型精确化（谨慎）
- **安全**（直接替换）：`undefined1`→`uint8_t`、`byte`→`uint8_t`、`undefined2`→`uint16_t`、
  `ushort`→`uint16_t`、`undefined4`→`uint32_t`、`undefined`→`uint8_t`、`byte`→`uint8_t`。
  这些是 Ghidra 别名（types.h 里已 typedef 等价），换成标准类型名纯可读性，行为不变。
- **谨慎**（需确认字节宽 + 调用点兼容才改）：`uint`→`uint16_t` 或 `uint32_t`。
  判断依据：函数用 `&0xff`/`&0xffff` 截断 → 16 位；赋值给已知 `uint16_t`/`uint32_t`
  变量 → 跟从；不确定就**保留 `uint`**。
- **绝不要**把 `volatile uint8_t *` 改成非 volatile，不要动 `DAT_` 符号定义的 类型。

### L3 —— 注释升级（安全）
- 函数头注释保留**入口地址**（如 `0x0000AC24`）——这是与反汇编对应的关键锚点。
- 把注释改成语义化描述；修正明显错误。
- **核实硬件/寄存器语义**：对照 `firmware/globals.c` 里该 DAT 符号的地址值（如
  `DAT_0000b00c = 0x2009C000` = FIO 池、`DAT_0000b014 = 0x400FC000` = SCB 基址）。
  别凭空猜；不确定就写「（待核实）」或保留中性描述。

## 绝对禁止（红牌）
1. 改 `globals.c` / `globals.h` / `reg.h` / `types.h` 里的任何定义。
2. 改变控制流（if/while/for/switch 的骨架）、改变存取地址或访问宽度。
3. 删任何 `DAT_0000xxxx` / `func_0x...` 符号引用（**符号保留原样**，只动局部变量/参数/注释）。
4. 做 L2 重构（拆分函数、提取公共子表达式、合并重复分支）。
5. 改动已经语义化的函数名。

## 自验（每改完一个 .c 文件后必须做）
```
cd /d/code/LPC1765FBD100/decompiled/firmware
CC="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin/arm-none-eabi-gcc"
"$CC" -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -Os -ffreestanding -fno-builtin -Wall \
  -I. -Iinc -Wno-unused-but-set-variable -Wno-unused-variable -Wno-pointer-sign \
  -Wno-parentheses -fsyntax-only src/你的文件.c
```
必须**零错误零警告**。只做语法检查，不链接（不会与其他子代理产物冲突）。
最终由主代理统一 `bash build.sh` 链接验收。

## 汇报要求
每个文件完成后汇报：改了多少处、该文件剩余 `param_`/`uVar` 计数（应趋近 0）、
任何「不确定所以保留原名」的项、`-fsyntax-only` 结果。

## 风格范本
读已完成的首个模块 `firmware/src/08_uart3_modbus.c` —— 它的变量/类型/注释已经是
目标形态，照它的风格做。
