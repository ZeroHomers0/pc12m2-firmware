# firmware — LPC1765 可编译复刻工程

目标B的 GCC 工程已经完成，不再是“12 个模块 + 07 stub”的早期骨架。07 状态机、08 Modbus
分发、UART3 RX、启动链及数据映像均已进入正式构建；根目录反编译存档仍保留用于追溯。

当前基线（2026-08-23）：`text 61936 / data 3000 / bss 2188`，`firmware.bin` SHA-256 为
`F032EFB70BB3942C4999D7C1F2D0DEBB64125F004C4E19405CAB0DD08F5EAA44`。离线验证通过
`output_stage` 144/144、`state_machine` 115/115 和测试套件 11/11；仍须完成 W8 分级硬件验证。

## 构建

```bash
bash build.sh          # 产出 firmware.elf / firmware.hex / firmware.bin / firmware.map
```

工具链：ARM GNU Toolchain 14.2.Rel1（`arm-none-eabi-gcc`，`-mcpu=cortex-m3 -mthumb`）。

## 目录

| 文件 | 说明 |
|---|---|
| `startup.s` | Cortex-M3 启动：拷贝原 .data 镜像(fw_image)→0x10000000、清零原 .bss(0x1000213C-0x100029C8)、拷贝本 .data/清零 .bss、调 main()；向量表含 8 个真实 IRQ + weak 默认自旋 |
| `lpc1765.ld` | FLASH 0x0/256K、SRAM0 0x10000000/32K（原固件 .data/.bss 布局）、SRAM1 0x2007C000/16K（globals）。**_estack=0x10006768**（复刻原 iar_init_core 最终 SP，避免栈覆盖 .bss） |
| `data_image.s` | 原固件 SRAM .data 初始镜像（`assets/ram_data_image.bin`，8508B） |
| `globals.c/h` | 847 个 DAT_/PTR_ 符号定义/声明（`tools/generation/generate_globals.py` 生成） |
| `src/strpool.c` | **W7a 字符串表**（`tools/generation/generate_string_pool.py` 生成） |
| `stub.c` | 收尾子例程：`freq_adjust_sync(0xAB48)` + 已按原指令恢复的 UART3 RX 组帧 `func_0x0000aed0` |
| `src/` | 13 个可编译模块 + `strpool.c`（01_startup 移除 IAR runtime；07_state_machine.c / 08_modbus_dispatch.c 已 W1a/W1b 完整还原） |
| `inc/reg.h` | LPC1765 外设寄存器宏（FIO/TIMER/UART3/ADC/SCB/PINSEL/NVIC/WDT） |
| `inc/types.h` | Ghidra 类型映射（undefined1/4/8、byte、uint…） |

## 历史构建基准（2026-08-21，已被当前基线取代）

- 零错误零警告；text 35904 / data 3388 / bss 2188；bin 39284B < 256K（阶段1-3 骨架基准）
- 向量表：SP=0x10006768、Reset=0xD4；8 个 IRQ handler 全部绑定（nm 核实）
- SRAM0 布局与原始固件一致：.data 0x10000000-0x1000213C(8508B)、.bss 0x1000213C-0x100029C8(2188B)
- globals 放 SRAM1 隔离，不覆盖 DAT_ 指针指向的 SRAM0 变量区

## W1b + W7 追加（2026-08-21，本会话）

- **W1a/W1b**：`08_modbus_dispatch.c`、`07_state_machine.c` 已从 stub 迁移为完整还原（18/18 case），text 56516 零警告
- **W7a 字符串表（strpool）**：disp_string 直传原 flash 字符串地址，GCC 重链接后乱码。新增 `src/strpool.c`（GBK blob 2507B / 20 簇 / `strpool_map`），`disp_string` 入口映射。**每簇末尾含 NUL**（gen_strpool `str_end` 含尾 NUL，防 disp_string 越簇读到下一簇）。**端到端验证 PASS=20/20 簇**；`map(0x47dc)`="故障"、`map(0x86e0)`="%"、RAM 地址原样返回；20 簇末字节均 NUL
- **W7b 单位字符修正**：`07_state_machine.c` sm4_draw_value 5 处误译 ASCII（`0x56/0x41/0x25`）→ 真实地址（`0x7974 'V'`/`0x7980 'A'`/`0x86e0 '%'`，经原始 ADR 指令字节实锤）
- **case1E 行为等价核对（0xA2C8-0xAB44 运行主界面）**：对照 `_sm_case1E_0xA2C8_0xAB46.txt` 全文 784 行核对，key 分支 / IDLE 刷新 / CTRL_MODE 1·2 / 复位斜坡 / RESET_MODE / 急停 / 块A·块B 启停 / 手动调节 / 超时尾全部精确匹配。修正 2 处：
  - **差异#1**：CTRL_MODE==0 段 `disp_string(0x47dc+0x20)` 曾置于 `if(*CTRL_MODE==0){}` 外（无条件）→ 移入 `if(*DISP_MODE!=1){...}` 内（反汇编 0xA3B0 cbnz/0xA3B8 beq 证实仅在 CTRL_MODE==0 && DISP_MODE!=1 时执行）
  - **差异#2**：块A停机缺失（0xA85E：`FAULT==0 && STOP_PEND==0 && s17==0 && DISP_SEL==0` → `*STOP_PEND=1;*STOP_REQ=0;*RUN=0;disp_string(0x4804-0x1c)`）→ 已在 s17 清理行前插入
- **case5A/14/8/B/9/C 核对（2026-08-22）**：case14/9/C 无差异；case5A(#A STATUS!=1)、case8(#B STATUS!=1、#E 0x6474+0x78 高亮第4参1)、caseB(#D 0x6474+0x8c 高亮第4参1) 各修正。**共性**：运行状态行条件实为`STATUS!=1`（非`==0`）；菜单返回行第4参=光标高亮位（r3 0/1）
- **case3 核对（2026-08-22）**：16 项读宽(+1/−1/+5/−5)全对、导航4页/Key切换/尾刷新全匹配；修正 #C3（key==4 保存红绘应仅行0 inv=1，行1-3 inv=0）
- **case4 核对（2026-08-22）**：修正 #C4a（key==4 红绘仅 call2 inv=1）、#C4b（⚠结构性：word-plus/minus 原无 key 门控相互抵消→重构为 key2/0x16 增 + key3/0x21 减 两互斥分支）、#C4c（word-5 下限0 语义 `(v<6?5:v)-5`）。显示读宽/导航/尾刷新全匹配
- **case7 核对（2026-08-23）**：18 个 case 中最复杂（PID 三路闭环，0x910C-0x9A84）。对照 `_sm_case7_0x910C_0x9A84.txt` 全文 + 52 个 literal 槽值 dump 逐段核对。key1 / key4 / key2·3·0x16·0x21 编辑·导航 / TIMEOUT3 刷新 / 超时清行 / 超时尾(0xc350) 全部精确匹配。修正 2 处：
  - **#C7a**（结构性）：key==4（回主菜单）块尾 `return;` 错误——反汇编 0x9202 后**不返回**，落到 0x9294→0x931c→key 非 2/0x16/3/0x21→0x93a4→0x9614 刷新区。删 return（脱落到 `(*TIMEOUT3)++` 刷新+TIMEOUT 尾）。
  - **#C7b**（多余块）：`*MENU2<4` 内"**D 值 row3**"块删去——反汇编只有 mode(row0)/P(row1)/I(row2) 三段，`cmp *MENU2,#4 / blt 0x994e` 把 MENU2<4(含 0/1/2/3)全送 `<8→0x9984` 从不画 row3；实为 **PI 控制无 D 槽**（每模式仅 P/I 两槽 0x10001711/12、13/14、15/16、17/18）。
  - **读宽**：PID_MODE(0x10001710)及 PID 槽全程 ldrb，用 SM7B 字节宏（避免 word 污染相邻槽）。
- **case7 补充**：编辑增减方向——MENU2==0 时 down(key3/0x21)使 PID_MODE++(clamp 4)/up(key2/0x16)使 PID_MODE--；P(0x10001717)/I(0x10001718) 仅 PID_MODE==4 可编辑;增益槽 0x10001722/23/24/25 级联下限（23≤22、25≤24、26≤25,各自 clamp 0xfa/0x80）；导航 MENU2 0-8,子页标题 0x6aa4(0-3)/0x9400(4-7)/0x9450+0x5ba4(8-c)。
- **12_closed_loop 核对（2026-08-22）**：PID 核心 0x108b0(closed_loop_integral)+0x10f0a(closed_loop_wrapper)。对照 `_disasm/000108b0_FUN_000108b0.txt` 全文逐段核对：死区三档判断 / 分段除数表 4 套(通道1×2+通道2×2,各10段) / 控制方式2固定除数0x46 / PID公式(mla/mls/sdiv,P/I/D系数槽0x10002100-130读写顺序) / 上下限钳位(0x116520/0x5cc60) / wrapper节流 全部等价。修正 1 处：
  - **读宽差异（5 槽）**：死区槽 0x10001722/23/24/25/26 反汇编全程 `ldrb`（byte），globals 原为 `volatile uint32_t*`（word 读，非4对齐地址读相邻4字节）。改 `volatile uint8_t*`（globals.h 5 extern + globals.c 5 定义）；`DAT_00010cdc`(0x1000211c 增益输出槽)保持 word（asm `str`）。
  - **等价槽**：0x10cf4==0x10f40==0x10002128(除数)、0x10cfc==0x10f44==0x10001638(A_RANGE)、0x10cf8==0x10f48==0x10002024、0x10cf0==0x10f4c==0x1000163c(V_RANGE)——IAR 同地址多 literal 槽，C 混用等价。
- **06_param_system 核对（2026-08-22）**：load_config(0x25dc)+param_sync_live_to_eeprom(0x35f2)。对照 `_disasm/000025dc`/`000035f2` 全文核对魔数检查/银行A·B读入/默认回写/shadow→live 拷贝/增益对选择/16 位参数分高低字节全部等价。修正 2 处：
  - **#P1a（系统性读宽错误，279 符号）**：参数 byte 槽（0x10001634/4c/4d/54-5b/64/7c-8b/94/95/c4/cc/d4/dc-e4/ec/f4/fc-ff/1704-06/0c-0f/10-21/22/24-2b）在 globals 全被定义为 `volatile uint32_t*`（word），而反汇编全程 `ldrb/strb`。word 写污染相邻 byte 槽（尤其 PID 槽 0x10001710-21 连续排列）。`tools/fix_readwidth.py`（幂等，findall 处理 gen_globals 挤在同一行的多符号）批量改 279 符号 uint32_t*→uint8_t*（globals.c 定义 + inc/globals.h extern）。
  - **#P1b（局部指针类型 2 处）**：load_config 的 `puVar1`、param_sync 的 `pcVar1` 原声明 `volatile uint32_t*`，实际只用于 byte 参数（`i2c_write_reg(*pcVar1, reg)` 单字节写），改 `volatile uint8_t*`。
  - **读宽判定依据**：param_sync 反汇编 0x35f4 `ldr r0,[0x3988]`+`ldrb r0,[r0]`（byte）vs 0x361e `ldr r0,[r0]`（word）——byte 参数 ldrb、word 参数 ldr；16 位参数（如 reg 0xb/0xc）用 `*(int*)DAT` 强转分高低字节。line 348-350 重复写 `[0x10001722]=[0x10001727]` 经反汇编 0x3512-0x351e 证实为原固件 IAR 冗余写（非伪影），C 忠实复现。
- **04_i2c 核对（2026-08-22）**：I2C GPIO 位带模拟 9 函数（0x1c40-0x1ebc）。对照 `_disasm/00001c40`…`00001ebc` 全文核对：FIO 偏移（[0]=FIO0DIR/[5]=+0x14 PIN/[6]=+0x18 SET/[7]=+0x1C CLR）、延时循环（5/param×10000）、START/STOP 时序、write_byte 移位 `(data<<1)&0xFF`=`lsls #0x19+lsrs #0x18`、ACK 读 FIO0PIN bit10、read_byte 收位、write_reg/read_reg 的 [0xA6][reg][val] 序列 全部等价。修正 2 处：
  - **#I1（读宽）**：`DAT_00001f00`（ACK 记录）值实为 `0x1000158c`（非头注释写的 0x10001F00），反汇编 `strb`（byte 写），globals 原 `uint32_t*`→改 `uint8_t*`（globals.c 定义 + inc/globals.h extern）。
  - **#I2（头注释）**：`DAT_00001f00=0x10001F00` 更正为 `0x1000158C`。
- **08_modbus_dispatch 核对（2026-08-22）**：modbus_dispatch(0xB642)。对照 `_disasm/0000b642` 抽查核对（W1a 已精读 5161 条还原，本次验证等价性，**无新差异**）：
  - 帧态门控（状态==1 写 0x100017B8 后落到 !=5 检查，语义同 C 的 return）、从站匹配（FIO4CLR|=0x20000000 P4.29 + 清状态 + IER|=1）、功能码分发（0x03/0x06/0x10）全等价。
  - **读宽**：53 写分支用显式 `(volatile uint8_t*)`/`(volatile uint32_t*)` 强转，不依赖 globals 类型。特殊地址抽查全对：0x100015F8/FC、0x10001624、0x10001788=`str`(word)、0x10001785=`strb`(byte)。
  - case 0x01（byte 参数 0x10001634，`cmp #3;bcs`=v>=3）、case 0x3E（word 0x10001660，`cmp #0xb5;bcs`=v>=181）完整流程（范围校验→store→param_sync→响应 [addr,06,10,reg,hi,lo,CRC]）等价。
  - 0x03 读边界：`cbz reg==0` + `cmp #0x3f;bls`=reg<=0x3f 等价 `reg==0||reg>0x3f`。
- **08_uart3_modbus 核对（2026-08-22）**：uart3_init(0xAC24)/uart3_tx_byte(0xAE0C)/uart3_rx_timeout_monitor(0xAE50)/UART3_IRQHandler(0xAF08)/crc16(0xAF64)/modbus_read_reg(0xAF94)/modbus_write_multi(0xB2E0) 共 7 函数。对照 `_disasm/0000ac24`…`0000b2e0` + 新 dump `af08_uart3_isr.txt`/`aed0_rx_framing.txt` 全文核对。LCR 数据位映射/波特率系数表/DLL·DLM/FCR/NVIC·IER、IIR 判因(==4 调 RX 组帧/==2 THRE 发下一字节)、63-case switch 读宽全部等价。修正 3 处：
  - **#U1（crc16 循环 off-by-one，Ghidra 反编译错误）**：反汇编 `sub.w r6,r4,#1; uxtb r4,r6; bne` 的 `bne` 测**递减后**值（Thumb `uxtb` 置 Z 标志），循环体执行 (length-1) 次；Ghidra 反编译成 `while(n--)`（length 次）。改 `while ((param_2=(param_2-1)&0xff)!=0)`，删 `bool bVar4`。
  - **#U2（读宽 4 符号，globals）**：DAT_b020(0x4009C000 UART3 基址)、DAT_b038(0x10001793 发送长度)、DAT_b03c(0x1000236C 发送缓冲)、DAT_b530(0x10001785 byte 寄存器) 反汇编全程 `ldrb/strb`（byte），globals 原 `volatile uint32_t*`→改 `volatile uint8_t*`。UART3 寄存器必须按字节访问：`[0xc]` 才对应字节偏移 0xC（`uint32_t*` 会变成字偏移 0x30），word 写污染相邻字节。
  - **#U3（局部指针）**：uart3_init 的 `puVar3` 原 `volatile uint32_t*`，实为 UART3 寄存器字节访问（`*puVar3=(char)param_1`、`puVar3[0xc]=7`），改 `volatile uint8_t*`。
- **09_output_stage 核对（2026-08-22）**：output_stage(0xE9AC，2932 条) + 5 个 ISR（EINT1/2/3 0xF9E8/0xFA0A/0xFA2C、TIMER2/1 0xFF48/0xFF6C）。对照 `_disasm/0000e9ac`/`f9e8_eint1`/`fa0a_eint2`/`fa2c_eint3`/`ff48_timer2`/`ff6c_timer1` 全文逐段核对，**全部等价**。修正 1 处 + 读宽 5 符号：
  - **#O1（取址伪影）**：行 951 `(int)&DAT_00003904` → `0x3904`。`DAT_00003904` 是真实 SRAM1 全局（值 0x10001718），`&` 取到的是变量地址而非 0x3904；反汇编 0xFE02 `movw r1,#0x3904` 实锤为字面量常量（flash 表地址，非变量地址）。
  - **读宽 5 符号**：edc4(0x10002080)/ee3c(0x10002077)/ee58·f26c·f708(0x1000203d) 反汇编全程 `strb`（byte），globals 原 `uint32_t*`→改 `uint8_t*`。
  - **软起停状态机**：0x1000EE24/0x1000F268/0x1000F780 取 0=停/4=运行/5=稳定；两路闭环（f270..f2c4 / f70c..f760）+ 恒压源（f770==2）+ 停机斜坡（fba4/fbc0 联锁）逐分支核对 bcc/bcs/bls/bhi 方向与 C 的 `</<=` 全对。
- **05_adc 核对（2026-08-23）**：adc_init(0x1F04)/adc0_start(0x1F80)/adc0_wait_done(0x1FA6)/adc0_scan_channels(0x1FBC)。对照 `_disasm/00001f04`…`00001fbc` 全文核对：AD0CR/AD0GDR 位操作、`ubfx r0,r1,#4,#0xc`=`(GDR&0xffff)>>4`、ch2/ch1/ch0/ch5/ch3/ch4 六通道平均逻辑 全部等价。**无差异**。
- **11_auth 核对（2026-08-23）**：auth_set_timeout(0x10696)/auth_challenge(0x106A0)/auth_retry(0x10820)。对照 `_disasm/00010696`/`000106a0`/`00010820` 全文核对：24 位挑战（字节 A/B/C→bit0-7/8-15/16-23）+ 16 位应答（高字节比期望 A、低字节比期望 B）、失败重试 5 次锁机、超时递减 全部等价；auth_challenge 的 r4 累加（uVar3<8）为死代码（C 省略行为等价）。修正 1 处：
  - **#A1（读宽）**：`DAT_00010894`(0x10001644) 反汇编 0x10700 `ldrb`（byte），globals 原 `volatile uint32_t*`→改 `volatile uint8_t*`。
- **03_input_debounce 核对（2026-08-23）**：gpio_inputs_dir_init(0x1578)/input_scan_state(0x15FE)/scan_run_stop(0x19C6)/debounce_p09(0x1AB8)/debounce_p116(0x1AE6)/debounce_p117(0x1B3E)/debounce_p06(0x1B96)/chk_p02_p03(0x1BEE)/gpio0_input_init(0x10F8C)/read_input_p02(0x10FAE) 共 10 函数。对照 `_disasm/00001578`…`00010fae` 全文核对：旋转编码器 A/B 相 6 向锁存（1..6）、RUN/STOP 单次+保持双模式、各去抖阈值（0xF/0xFA/0x32）、联锁检查、P0 方向配置 全部等价；DAT_00001978(0x10001588) 确认 **word**（`ldr/str`，非 byte，保持 uint32_t*）。修正 3 处：
  - **#D1（读宽）**：`DAT_0000197c`(0x10001570) 反汇编 0x164a/0x1652 `ldrb/strb`（byte），globals 原 `uint32_t*`→改 `uint8_t*`（旋转方向锁存槽）。
  - **#D2（读宽）**：`DAT_00001c20`(0x1000157A) 反汇编 0x1a7a/0x1aa8/0x1ab4 `strb/ldrb`（byte），globals 原 `uint32_t*`→改 `uint8_t*`（RUN/STOP 保持模式锁存，非 4 对齐地址 word 写污染相邻 0x1000157B/7C）。
  - **#D3（读宽）**：`DAT_00010fd0`(0x10002138) 反汇编 0x10fbc/0x10fc4 `strb`（byte），globals 原 `uint32_t*`→改 `uint8_t*`（P0.2 状态记录）。
- **02_lcd_display + 10_relay_led + 13_gpio_init + 01_startup 核对（2026-08-23，任务 #58）**：02_lcd 22 函数 + 10_relay 5 函数 + 13_gpio 2 函数全部对照反汇编核对，**零逻辑 bug**（仅 gpio2_init 1 处注释 P2.1→P2.2）；01_startup 发现 **8 处 bug**（#S1-#S8）全部实锤修复：
  - **#S1（wdt_init）**：缺 `WDFEED=0xAA`（反汇编 0x22e-0x230 `movs r0,#0xaa;strb r0,[r2,#8]`；喂狗须 0xAA 后 0x55）→ 补。
  - **#S2（wd_feed）**：缺 `WDFEED=0xAA`（反汇编 0x238-0x23c）→ 补。
  - **#S3（timer0_init）**：缺 `TIMER0->TCR=2`（反汇编 0x256-0x25e，复位 TC/PC，PCONP 后 PR 前）→ 补。
  - **#S4（main 认证分支反）**：`if (*DAT_00000750 != 0)` 锁机 → `== 0`（反汇编 0x668 `cbnz r0,0x68e`：非零跳正常路径、零走锁机屏）。头注释 750 语义同步反修。
  - **#S5（读宽）**：`DAT_00000748`(0x10001734) 反汇编 0x624/0x650 `ldr/str`（word）→ 改 uint32_t*（认证重试计数，别名 DAT_0000397c/04368 均 word）。
  - **#S6（读宽）**：`DAT_00000750`(0x1000172C) 反汇编 0x63a/0x646/0x666 `str/ldr`（word）→ 改 uint32_t*（锁机标志，别名 DAT_0000396c/04358/108a4 均 word）。
  - **#S7（读宽，reg.h）**：WDMOD 反汇编 `ldrb/strb`（byte）→ REG32 改 REG8。
  - **#S8（读宽，reg.h）**：WDFEED 反汇编 `strb`（byte）→ REG32 改 REG8（WDTC 保持 REG32=word `str`）。
  - **关键语义澄清**：750（0x1000172C）=锁机标志，**1=放行 / 0=锁机**（11_auth.c `*DAT_000108a4` 成功=1/失败=0、`*DAT_0001089c` 成功=0/失败=1）；原 C 头注释「750=0 放行」与条件双双写反。
- 现 text 59788 / data 3388 / bss 2188（含 strpool）

## 遗留（W1/W8）

- DAT_0000078c 等少数 SRAM 变量宽度未定（可能 byte/word 偏差）
- W8 硬件实测未做（新板未定型：示波器抓 12° 触发脉冲、reg44/45 标定）
