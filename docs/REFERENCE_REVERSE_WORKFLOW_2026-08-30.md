# 参考项目 PC6M 逆向全流程（从原始固件到可编译成品）

> 本文档是参考项目 **PC6M-10（六相 SCR 控制板，LPC1765FBD100）** 逆向工程的**完整流程总结**：
> 从拿到原始固件 BIN 开始，经过 Ghidra 反编译、人工精读还原、GCC 工程重建、执行级 A/B 差分
> 验证、Codex 独立审计，到最后烧写入板实机验证的全过程。所有事实与工具名均来自参考项目
> 文档（`D:\code\LPC1765FBD100\decompiled`），本文仅作流程串联与索引。
>
> **用途**：PC12M-2（12 相孪生板）后续做 12 相差异分析、搭建可编译工程时，按本文档的
> 阶段顺序与工具链复现。两板 MCU 相同、参考项目已全流程跑通，本文是可直接照抄的方法论。

---

## 0. 一图流

```
LPC1765.bin(256KiB 金标准)
   │  ① Ghidra 12.1.3 + Ghidra MCP 导入（补 SRAM 段）
   ▼
函数清单 list_functions → 105 个函数
   │  ② 103 个中小函数 decompile_function → C；2 个大函数 disassemble_function → 人工精读
   ▼
13 个 .c 反编译模块（01_startup..13_gpio_init）
   │  ③ 目标 A：架构确证 + 数据段清单 + 菜单映射 + 硬件印证
   ▼
目标 B：GCC 工程重建
   │  ④ 数据层生成（RAM 镜像/全局符号/字符串池）→ 搭工程（startup.s/ld/reg.h/types.h）
   │  ⑤ 逐函数对照反汇编核对（读宽/结构/常量修正）
   ▼
firmware.bin 可编译、可修改
   │  ⑥ 执行级 A/B 差分（Unicorn 直接跑原 BIN vs 新 ELF）+ Codex 独立审计
   ▼
等价性通过 → ⑦ 烧写入板（SWD/ISP）→ W8 分级实机验证
```

---

## 1. 项目目标与金标准

| 项 | 值 |
|---|---|
| 固件 | `LPC1765.bin`（NXP LPC1765 / Cortex-M3，256KB flash @0x0，262144B） |
| 设备 | PC6M-10 三相晶闸管（SCR）移相触发功率控制板（恒压/恒流/开环三模式） |
| 金标准 | `LPC1765.bin`（SHA-256 `DD629EAC…3F65`）——**一切等价验证的基准** |
| 目标 | ① 完全理解固件 ② 反编译全部函数 ③ 还原为可编译、可修改的等价源码 |
| 等价标准 | **功能等价**（非逐字节）：编译器版本/优化/链接布局不同，可接受标准是行为等价，最终靠硬件验证 |

**金标准地位**：原始 BIN 是唯一不可再生资产。所有后续验证（A/B 差分、函数覆盖审计、读宽
核对）都以它为对照；任何"我看测试过了""文档写了已核对"的结论都不采信，须以 BIN 反汇编
重新验证（Codex 审计即秉持此原则）。

---

## 2. 工具链清单

| 工具 | 版本/说明 | 用途 |
|---|---|---|
| **Ghidra** | 12.1.3 | 反编译主工具，工程 `LPC1765FBD100.gpr` |
| **Ghidra MCP** | 本地服务，读取超时 **5s**、单线程 | 调用 `list_functions` / `decompile_function` / `disassemble_function` |
| **Ghidra 辅助脚本** | `AddSramAndVars.java`、`CreateIsrFunctions.java`、`AnalyzeStateMachine.java`、`TypeLiteralPointers.java`、`create_isr_functions.py` | 补 SRAM 段、创建 ISR、状态机流程导出、字面量指针类型化 |
| **Arm GNU Toolchain** | 14.2.Rel1（`arm-none-eabi-gcc`，`-mcpu=cortex-m3 -mthumb`） | GCC 工程编译器 |
| **Python** | 3.12 + minimalmodbus/pyserial/unicorn | 生成脚本、验证脚本、测试框架 |
| **Unicorn** | Cortex-M3/Thumb 仿真 | 执行级 A/B 差分（直接执行原始 BIN 与 ELF 比较） |
| **J-Link** | V9.70，打包 `tools/jlink/JLink.exe` | SWD 烧写/调试 |
| **Flash Magic / fm.exe** | 13.65 | ISP 备份/解困通道 |

> **MCP 5s 超时是核心约束**：Ghidra MCP 对超大函数的 `decompile_function` 结果会超 5s 传输
> 上限连续失败 → 大函数改走 `disassemble_function` 全量落盘 + 人工精读还原。

---

## 3. 逆向过程（阶段 0–7）

### 阶段 0：工具准备与工程建立

1. 建 Ghidra 工程、导入 `LPC1765.bin`、**补 SRAM 段**（原固件用 IAR 压缩 `.data`，需还原）。
2. 编写辅助脚本（迁入 `tools/`）：
   - `AddSramAndVars.java`：补 SRAM 段与变量标号；
   - `CreateIsrFunctions.java` / `create_isr_functions.py`：从向量表创建并命名 8 个 Cortex-M3 ISR；
   - `AnalyzeStateMachine.java`：状态机流程导出；
   - `TypeLiteralPointers.java`：字面量指针类型化。
3. 早期专题分析产出：`state_machine_analysis.md`（状态机拆解）、`uart3_protocol.md`
   （Modbus-RTU 协议）、`i2c_param_sync.md`（I2C 参数系统）。
4. **防丢失机制**：会话曾因死机丢失，此后每次重大进度固化进 `PROGRESS_2026-08-20.md`
   （防丢失主文档：完整寄存器表、同步映射全图、方法论），会话恢复先读它 + `PLAN.md`。

### 阶段 1：核心架构确证（2026-08-20）

逐层破解固件的功能架构，按"寄存器→参数→链路→外设"的顺序：

- **63 个 Modbus 寄存器**全量解析：范围校验（逐条从写分派 `FUN_0000b642` 提取）、读写不对称、
  别名。关键不对称：reg24/25（写→组4增益槽/读→活动增益对）、reg40（写→src_value 注入值/
  读→ADC ch5 实测）、reg61=远程输出使能、reg62=输出下限系数。
- **61 组参数 live↔shadow↔EEPROM 同步映射全表**（`FUN_000035f2`）：word 型写 reg,reg+1（高字
  节在前）、byte 型写单 reg；load_config 双银行 magic 校验。
- **ADC0 6 通道测量链路**（`FUN_00001fbc`）：6 通道采样 + 5 点移动平均 + 标定除数，产出
  reg40-45；5 个"输出设定字"实为 ADC 标定除数（3500..4500）。
- **闭环链路**：`FUN_00010f0a` → `FUN_000108b0`（误差分区选增益 + 积分滤波，非线性非纯 PID）。
- **输出级链路** `FUN_0000e9ac`：软起动 phase 0→4→5 + 闭环 + 报警积分 + 限幅 + 停机链。
- **重大发现（推翻旧结论）**：LPC1765 无本地 PWM/DAC，但用 **TIMER2(0x40090000) 编程 SCR 触发
  角**（旧扫描只搜 0x40018000/0x40038000，漏了 0x40090000）；TIMER1 扫描 240 步生成 6 窗口
  触发脉冲。I2C 是 **GPIO 位带模拟**（SDA=P0.10/SCL=P0.11）。

### 阶段 2：硬件印证（2026-08-20）

用户提供 `doc/` 硬件文档（BOM `PC6M-10-BOM-更新版.xlsx`、原理图、U38 接线表、分析报告、
面板手册）→ 交叉印证：

- 设备定位**三相晶闸管移相触发板**（非最初以为的"变频输出正弦"）。
- 芯片清单全确认：AT24C02C（EEPROM@0x53）、HEF40106×2、FR120N×6、ULN2003A、ADuM1201（认证
  隔离）、KMB419-301S×6（主桥）、继电器×3、RS8552XM、LM2904×3、LM2901、CJ431、电源树。
- 触发 GPIO：G1-G6 + 12 脉波 P2.8/7/6/5/P0.8/7（12° 脉宽/60° 双脉冲）。
- 认证链路：P2.1-2.4 经 ADuM1201 隔离 1-Wire 挑战-应答（防克隆，失败锁机）。
- 显示 = 12864 图形 LCD（P1 口）；UART3 = RS485（ADM2483 隔离模块 U5）。

### 阶段 3：菜单映射（2026-08-20）

面板菜单树 → 固件参数**全量确证**：基本参数实为 **16 屏**（手册仅 12，多出 13急停/14反馈/
15输入/16起始相位）、保护 10 屏、通讯 4 屏、PID 9 屏、相位校准、恢复出厂（密码门控）、版本
信息。**旧标修正**：gain_a/b=电压/电流量程、out_fine=主从偏移、sub_state=控制方式、reg12=启动
方式、reg13-20=保护参数、reg62=起始相位。

### 阶段 4：全函数反编译导出（2026-08-20 晚 ~ 08-21）

- Ghidra `list_functions` 确认 **105 个函数**。
- **103 个中小函数**逐一 `decompile_function` → C 反编译，写入 13 个 `.c` 模块（01_startup
  到 13_gpio_init），每函数标注真实入口地址 `/* 0x0000XXXX —— 名称 */`，保留 Ghidra 符号。
- **2 个超大函数**（`state_machine` 0x458C、`modbus_dispatch` 0xB642）C 反编译结果超 MCP 5s
  上限连续超时 → 改走 `disassemble_function` 全量落盘 + 人工精读还原：附录 `07_state_machine
  _asm.txt`（10061 条）、`08_modbus_dispatch_asm.txt`（5161 条），.c 中给出流程还原伪代码 +
  关键代码段注释。

### 阶段 5：评估与工作指导（2026-08-21）

回答：反编译是否走完/代码是否完整/能否编译/未完成项/优化方向。产出 `WORK_GUIDE`（W1-W8 工作
清单 + 目标 A/B 路线）。评估结论：**函数指令层完整，源码层不完整**——缺失 5 大类（数据段、
全局符号声明、函数原型、外设寄存器定义、工程文件）。

### 阶段 6：目录整理与项目档案（2026-08-21）

原始 BIN 迁入项目根；关键信息/文件/流程迁移进 `docs/`、`tools/`；撰写项目全史（PROJECT_SUMMARY）
+ 自包含索引（CLAUDE.md），任何 AI 可快速接手。

### 阶段 7：目标 A 收尾（2026-08-21）

目标 A = **可读的逆向文档**，三项收尾：
1. 两大函数注释补完：state_machine 事件码→菜单页分发链、modbus_dispatch 51 写分支结构；
2. 数据段清单导出 → `DATA_SEGMENT_2026-08-21.md`（内存布局/向量表/字符串池 GBK 全表/查表/
   关键魔数/UART3 指针表全映射/SRAM 全局清单）——**同时是目标 B 的 W2 地基**；
3. 交叉引用：13 个 .c 模块头注加交叉引用段指向 docs，发现 3 处引脚/编号差异（EINT1/2/3 实为
   P2.11/12/13、LCD CS/RS/E、继电器编号）已标注。

---

## 4. 目标 B：可编译、可修改的等价固件

> 从"反编译转储"走向"可编译源码"的阶段，这是**整个流程最复杂、工作量最大**的部分。
> 参考项目实际转成了 **GCC 工程**（不是计划中的 IAR）。

### 4.1 工作清单 W1-W8

| # | 工作项 | 状态 |
|---|---|---|
| W1 | 两大函数 C 级还原（state_machine 18 case / modbus_dispatch 51 分支） | ✅ |
| W2 | 数据段全量导出（清单已出；RAM 初始镜像/位域/全部 DAT 落地） | ✅ |
| W3 | 全局变量符号化（120+ 个 DAT_ 重命名 + 类型 + 初始值） | ✅ |
| W4 | 函数原型修正 | ✅ |
| W5 | 结构体还原（参数银行/状态标志/显示缓冲/Modbus 帧） | ✅ |
| W6 | 外设头文件 + 工程（LPC176x 寄存器定义、链接脚本、向量表、启动文件） | ✅ |
| W7 | 构建 + 逐函数对照反汇编验证等价 | ✅ |
| W8 | 硬件实测印证（分级上机） | ✅（后续 W8 系列） |

### 4.2 数据层生成（从原 BIN 生成源码，W2/W3 的地基）

关键点：**数据段（字符串、查表、RAM 初始值）完全缺失时链接不过**。用生成脚本从原 BIN 提取：

| 脚本 | 功能 | 产物 |
|---|---|---|
| `tools/generation/locate_sram_mirrors.py` | 定位 SRAM `.data` 初始镜像 + 字符串池补扫 | 结论并入下条 |
| `tools/generation/extract_ram_data_image.py` | **IAR 压缩 `.data` 解压** | `firmware/assets/ram_data_image.bin`（8508B）+ 校验 |
| `tools/generation/extract_data_segments.py` | 提取数据段常量（字符串/查表/指针） | `evidence/reverse/reports/_data_extract.txt` |
| `tools/generation/generate_globals.py` | 提取 DAT_/PTR_ 符号 → `globals.h/.c` | **847 个符号定义/声明** |
| `tools/generation/generate_string_pool.py` | 生成字符串池 | `firmware/src/strpool.c`（GBK blob + 簇表 + strpool_map） |

> **字符串池关键机制**：`disp_string` 直传原 flash 字符串地址，GCC 重链接后乱码 → 新增
> `strpool.c`（GBK blob 2507B/20 簇/`strpool_map`），`disp_string` 入口映射。每簇末尾含 NUL
> 防越簇读取。20 簇端到端验证 PASS。

### 4.3 GCC 工程骨架

| 文件 | 说明 |
|---|---|
| `startup.s` | Cortex-M3 启动：拷贝原 `.data` 镜像(fw_image)→0x10000000、清零原 `.bss`、拷贝本 `.data`/清零 `.bss`、调 main()；向量表含 8 个真实 IRQ + weak 默认自旋 |
| `lpc1765.ld` | FLASH 0x0/256K、SRAM0 0x10000000/32K（原固件 .data/.bss 布局）、SRAM1 0x2007C000/16K（globals）；**_estack 须匹配原复位 SP** |
| `data_image.s` | 原固件 SRAM `.data` 初始镜像（`assets/ram_data_image.bin`） |
| `globals.c/h` | 847 个 DAT_/PTR_ 符号定义/声明（`generate_globals.py` 生成） |
| `inc/reg.h` | LPC1765 外设寄存器宏（FIO/TIMER/UART3/ADC/SCB/PINSEL/NVIC/WDT） |
| `inc/types.h` | Ghidra 类型映射（undefined1/4/8、byte、uint…） |
| `stub.c` | 收尾子例程：`freq_adjust_sync(0xAB48)` + UART3 RX 组帧 `func_0x0000aed0` |
| `src/` | 13 个可编译模块 + `strpool.c` + `crc16_table.c` |

> **globals 放 SRAM1 隔离**：不覆盖 DAT_ 指针指向的 SRAM0 变量区（原固件变量区 0x10000000
> 起），避免复刻固件的全局符号与原固件布局冲突。

### 4.4 逐函数对照反汇编核对（W7，工作量最大）

每个模块/函数对照 `evidence/reverse/disassembly/` 下金标准反汇编逐段核对。发现的问题
**两类为主**：

**① 读宽错误（byte vs word）——系统性、批量发生**
- 反汇编全程 `ldrb/strb`（byte）的槽，globals 常被定义为 `volatile uint32_t*`（word）。
- word 写污染相邻 byte 槽（尤其连续排列的参数区 0x10001710-21 PID 槽）。
- 修复：`tools/maintenance/fix_readwidth.py`（幂等批量改 `uint32_t*`→`uint8_t*`，globals.c
  定义 + inc/globals.h extern），一次改 **279 个符号**。
- 验证：`tools/verification/check_readwidth.py`、`verify_readwidth_all.py`（反汇编自动提取
  SRAM 访问宽度 vs globals.c 类型对比）。
- **判定依据**：byte 参数 `ldrb`、word 参数 `ldr`；16 位参数用 `*(int*)DAT` 强转分高低字节。

**② 结构/逻辑 bug（Ghidra 反编译错误或人工还原错误）**
- 例：`crc16` 循环 off-by-one（Thumb `uxtb` 置 Z 标志，`bne` 测递减后值 → 循环体执行
  (length-1) 次；Ghidra 反编译成 `while(n--)`）→ 改 `while ((param_2=(param_2-1)&0xff)!=0)`。
- 例：`state_machine` 各 case 的 key 门控、高亮 attr 参数、超时块缺失等（逐 case 核对修正）。
- 例：取址伪影 `(int)&DAT_00003904` → `0x3904`（DAT 是真实变量地址，反汇编 `movw r1,#0x3904`
  实锤是字面量常量）。
- 例：局部指针类型错误（`puVar3` 应为 `volatile uint8_t*` 访问 UART3 寄存器字节）。

**核对方法**：每个模块标题下列出"对照 `_disasm/0000xxxx` 全文核对 + 修正 #X"。逐段比对
`bcc/bcs/bls/bhi` 方向与 C 的 `</<=`、读宽、结构。典型修正集中在：
`02_lcd`(22函数)/`10_relay`(5)/`13_gpio`(2) 零逻辑 bug；`01_startup` 8 处 bug（#S1-S8：WDT 喂狗
缺 0xAA、TCR=2 缺、main 认证分支反、读宽等）；`04_i2c`、`05_adc`、`06_param_system`（279 符号
读宽）、`08_uart3_modbus`（crc16 off-by-one + 读宽 4 符号）、`09_output_stage`、`11_auth`、
`12_closed_loop`（5 槽读宽）、`03_input_debounce`（3 处读宽）、`07_state_machine`（各 case）。

---

## 5. Codex 全量独立审查与修复（2026-08-23）

**原则**：仅以 `LPC1765.bin`、原始指令转储和 LPC1765 硬件定义为金标准，**不采信既有测试通过
结果或文档中"已核对"结论**。审查分支 `codex/decompiler-review`，验证工具 `tools/codex_verify_repairs.py`（不调用 test/ 下的既有测试）。

### 5.1 确认的缺陷（修复进度 2026-08-23）

| 编号 | 缺陷 | 影响 | 修复 |
|---|---|---|---|
| **P0-01** | UART3 RX 组帧 0xAED0 是空实现 | 真实设备不能接收 Modbus 请求 | 恢复为原指令 |
| **P0-02** | LPC 向量校验和无效（槽7写0） | 芯片复位后可能留在 Boot ROM 不运行 | 恢复向量表 + 校验和 |
| **P0-03** | 关键延时循环被 GCC `-Os` 完全优化删除（`Delay`/`i2c_delay`/`long_delay`/认证位延时全 `bx lr`） | LCD/EEPROM/认证时序不再等价 | 改纯 C `volatile` 计数循环 |
| **P0-04** | GPIO/SCR 外设访问丢失 `volatile`，被合并重排 | 改变上电 GPIO 电平转换和 SCR 门极写入顺序（**安全关键**） | 显式补回 `volatile` |
| **P1-01** | IRQ23–34 向量名称/外设顺序错误，且凭空增加 IRQ35/36 | 后半段中断触发会进错误处理 | 按 UM10360 表 50 修正 |
| **P1-02** | 字符串映射漏 3 个菜单文本（0x65F8/0x70C0/0x7E74） | 菜单行乱码/越界 | 修正 gen_strpool 缺陷重生成 |
| **P2-01** | 链接脚本声称复刻原栈顶但地址误读（0x10006768≠实际） | 栈使用区间与原固件不同 | 栈顶改回原复位向量值 |
| **P2-02** | `reg.h` FIO DIR 寄存器名称整体错位 | 后续修改会操作错误 GPIO 端口 | 恢复正确偏移 |

### 5.2 复审结果（修复后独立再验证）

`tools/codex_verify_repairs.py` 在 Unicorn 中直接对比原 BIN 与新 ELF：

| 验证项 | 用例数 | 结果 |
|---|---:|---|
| LPC 向量 | 1 | PASS |
| UART3 RX 0xAED0 | 2 | PASS |
| pin_config / gpio2_init / auth_challenge | 各 1（FIO 写轨迹 48/7/85 次） | PASS |
| EINT1/2/3 ISR、TIMER2 ISR | 各 1 | PASS |
| TIMER1 ISR 矩阵 | 126 | PASS |
| CRC16 | 5 | PASS |
| Modbus 读/写 | 65 / 320 | PASS |
| 闭环积分 | 6 | PASS |
| **合计** | **532 组独立差分** | **全部 PASS** |

同时复核的静态数据：707 个 DAT/PTR 初值 0 差异、CRC 双表 512B 0 差异、字符串池 20 簇 0 差异、
335 个常量 `disp_string` 调用未映射数 0、97 个原函数入口 87 个业务函数存在于新 ELF（余 10 个为
IAR 运行时）。

**结论**：支持进入无门极、无功率负载的上机冒烟测试，不支持跳过波形验证直接带载。

---

## 6. 离线验证体系（测试）

### 6.1 执行级 A/B 差分（核心验证手段）

`tools/verification/verify_firmware_equivalence.py` 主入口：**用 Unicorn 直接执行原始 BIN 与
新 ELF 的关键函数**，比较返回值、SRAM 末态及关键 MMIO 写序。

- 覆盖矩阵：输出级 144、状态机 115、TIMER1 126、Modbus 读 65/写 320、CRC 5、闭环 6。
- 过程：首轮状态机差异定位到输入去抖状态 `0x1000157F/0x10001581`，修复 case3/4/5/7 的
  `TIMER3` 公共尾部；这些路径转为回归用例。
- 测试支持库：`test/support/unicorn_harness.py`（ELF/BIN 加载、符号查询、内存种子、A/B 差分
  公共支持）。

### 6.2 测试模块（25 个，`python test/run_tests.py`）

分两类：
- **static/**（不执行机器码）：CRC 表逐字节 vs 原 BIN、Modbus 读写映射对称、参数同步结构。
- **emulation/**（Unicorn 执行级）：A/B 差分或真执行编译产物——ADC 扫描（1296 例）、
  adc_wait_done 回归护栏、case3 编辑、闭环 PID、控制多拍、CRC 真执行、EEPROM 加载/同步矩阵、
  input_scan 回归护栏、中断序列、Modbus 分发/读写、外设叶轨迹、继电器状态机、状态机调用轨迹/
  多拍矩阵（222 例）、UART RX 序列。

**规则**：修改源码后必须运行对应回归；A/B 差分是"原始 vs 编译"执行级等价护栏。

### 6.3 审计与验证脚本（tools/verification + tools/audit）

| 脚本 | 功能 |
|---|---|
| `verify_firmware_equivalence.py` | 主入口：直接执行原固件 vs 新 ELF |
| `check_readwidth.py` / `verify_readwidth_all.py` | 反汇编 strb/str vs globals.c 类型，找 byte 槽被定义 word |
| `verify_mem_xref.py` | 模块 .c SRAM 访问 vs 金标准 ref，双向判漏 |
| `verify_modbus_c.py` | 08_modbus_dispatch.c 写分支 vs asm 分支表 |
| `verify_periph_xref.py` | 外设地址交叉引用验证 |
| `verify_sm_addresses.py` | 07_state_machine.c SRAM 地址 vs 金标准 |
| `verify_startup.py` | host 模拟 Reset_Handler 启动链路 |
| `verify_strpool.py` | strpool_map 字符串映射正确性 |
| `codex_audit_crc_tables.py` / `codex_audit_function_coverage.py` / `codex_audit_globals.py` / `codex_audit_strpool.py` | 独立只读审计（不调用测试套件） |

---

## 7. 烧写入板与实机验证（W8）

### 7.1 烧写关键点（详见本项目 `操作文档.md`）

- **CRP 地址布局修复（烧写前硬门槛）**：自编译固件 0x2FC 曾落在 `wd_feed()` 指令中 →
  `startup.s` 加 `.crp` 段发 0xFFFFFFFF、`lpc1765.ld` 固定到 0x2FC、`build.sh` objcopy
  `--gap-fill 0xFF`。四道硬门槛：BIN/HEX 0x2FC=0xFFFFFFFF、向量校验和、全量离线测试、A/B 验证。
- **SWD 连不上是设计行为**：固件复用 P1.30(SWDIO)→AD0.4、P1.29(SWCLK)→RS485 DE → 需
  connect-under-reset 或走 ISP。
- **MEMMAP 向量重映射坑**：reset & halt 停在 Boot ROM 时地址 0 映射 Boot ROM → 读真实 Flash
  前先 `w4 0x400FC040,1`；`verifybin` 走 flash loader 直读不受影响。

### 7.2 W8 分级实机验证（阶段 0→4）

- **阶段 0**：环境搭建、离线 A/B 全绿、原固件备份（只读 + CRP 检查 + 双份）。
- **阶段 1**：控制电冒烟——SRAM 哨兵测栈水位、继电器误吸合监测。
- **阶段 2**：三相同步/空载触发（EINT1/2/3 + G1-G6 波形、触发角单调性、保护撤销）。
- **阶段 3**：Modbus/ADC 标定。
- **阶段 4**：低压限流、带载（须差分/隔离探头 + 持证电工）。
- **严格不跨级**：任一必检项失败立即停止。

### 7.3 实机抓出的固件 bug（反编译回归，上机才发现）

- ADC wait_done 指针算术坑（`g_adc + 4` 编译成 +16 字节 → 读 AD0DR0 而非 AD0GDR → WDT 复位
  循环）；input_scan FIO 指针元素偏移；case1 三分支 FAULT 门控；case3 枚举项环绕→钳位；
  Modbus 大端序；编辑态不闪烁；恢复出厂"M"；复位坐标错位。→ 全部已修复并入本项目
  `操作文档.md` 附录（详见该文档）。

---

## 8. 关键坑与经验（逆向方法论）

1. **防丢失第一**：重大进度立即固化到防丢失主文档，会话崩溃可恢复。
2. **金标准不可再生**：原始 BIN 是一切验证基准；所有"已核对"结论须能重新验证。
3. **MCP 超时 ≠ 无法反编译**：超大函数走全量反汇编落盘 + 人工精读，比 C 反编译更可靠。
4. **读宽是系统性错误源**：反汇编 `ldrb/strb` vs `ldr/str` 决定 byte/word，globals 定义错会
   污染相邻槽。批量核对 + 工具自动提取。
5. **volatile 是安全关键**：`-Os` 会删除无副作用的延时循环、合并重排 GPIO 写序——对 SCR 门极
   写入属安全关键，必须显式 `volatile`。
6. **数据段是编译地基**：字符串/查表/RAM 初始镜像缺失则链接不过，须从 BIN 提取生成（RAM 镜像
   用 IAR 解压器）。
7. **等价 = 功能等价**：编译器/布局不同无法逐字节；以 A/B 差分 + 硬件验证为准。
8. **审计独立于测试**：Codex 审查不调用既有测试，只用金标准重新验证，抓出测试盲区（延时被删、
   向量校验和、volatile 丢失）。

---

## 9. 对 PC12M-2（12 相板）的借鉴

| 参考项目动作 | 本项目（PC12M-2）对应动作 |
|---|---|
| 原始 BIN 备份（金标准） | ✅ 已完成：`backup/pc12m2_orig.bin`（SHA `2BC60868…`） |
| 与参考固件差异分析 | 待做：函数级/反汇编级 diff（触发序列/引脚/寄存器映射） |
| Ghidra 反编译 | 复用参考项目 13 模块 + `evidence/reverse/disassembly/` 金标准 |
| GCC 工程重建 | ✅ 基座已搬入（`firmware/` 30 文件，复现构建 SHA 一致）；12 相改动直接在此进行 |
| 读宽/volatile/结构核对 | 12 相改动后逐函数对照参考反汇编复核 |
| A/B 差分验证 | ✅ 基座已搬入（`test/` 29 文件）；触发相关断言待 12 相适配 |
| W8 分级实机 | 触发波形验证须按 12 相窗口预期调整 |

---

## 附：参考项目文档导航（完整流程的证据源）

| 文档 | 覆盖 |
|---|---|
| `docs/history/PROJECT_SUMMARY_2026-08-21.md` | 项目全史（开始·过程·结果） |
| `docs/history/PROGRESS_2026-08-20.md` | 防丢失主文档（寄存器表/同步图/方法论） |
| `docs/history/PLAN.md` | 早期任务拆解 + 关键符号速查表 |
| `docs/project/PROJECT_STATUS.md`（= WORK_GUIDE） | 目标 A/B 路线、W1-W8 清单 |
| `docs/history/CODEX_FULL_AUDIT_2026-08-23.md` | Codex 独立审查（缺陷清单） |
| `docs/history/CODEX_REVALIDATION_2026-08-23.md` | 修复后 532 例复审 |
| `firmware/README.md` | GCC 工程说明 + 各模块核对记录（读宽/结构修正全表） |
| `tools/README.md` | 全部工具分类与功能 |
| `test/README.md` | 25 个测试模块清单 |
| `evidence/reverse/` | decompiled（13 模块）/ disassembly（逐函数）/ reports / state_machine |
| `docs/analysis/` | HARDWARE_VERIFICATION / I2C / UART3 / STATE_MACHINE / MENU_PARAMETER_MAPPING |
| `docs/w8/` | W8 实机验证全记录 |
| `操作文档.md` | 构建 + SWD + ISP 烧写操作（含 PC6M 踩坑与 bug 附录） |
