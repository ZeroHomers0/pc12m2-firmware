# 12p state_machine 0x4464 解码简报（task #23 子 agent 共享知识）

## 目标
把 12 相板 PC12M-2 的 state_machine 反汇编 dump 逐 case 转成可读 C 片段。
你负责指定地址区间的 case。产出可粘贴进 `firmware/src/07_state_machine.c` 的 C 片段。

## 输入文件
- **12p 金标准 dump**（唯一权威）：`D:\code\PC12M-2\evidence\reverse\disassembly\functions\00004464_FUN_00004464.txt`
  16728 行。每指令一行：`00004464  push {r4,lr}`（前缀是地址，4 位十六进制，不是时间戳）。
  其后可能跟注释行 `; ref 池槽 -> 0xSRAM`（说明该指令引用的池槽→SRAM 地址）。
  **行号 ≠ 地址−0x4464+1**（有注释/空行）。按地址定位：搜索行首 `^0000xxxx  `。
- **语义蓝图（6p 同构版）**：`D:\code\PC12M-2\firmware\src\07_state_machine.c`（当前是 6p 版）。
  函数/结构/注释风格可参考；**6p 地址一律不要抄**，只用它理解每个 case 的键处理模式与绘制逻辑形状。

## 权威 case 边界（每个 case 起点=cmp r0,#N 后的 bne 目标；终点=下一 case 起点）
| MENU | 地址区间 | 6p 蓝图行 |
|---|---|---|
| case1 | 0x48a0-0x514c | 296-615 |
| caseA | 0x514c-0x532a | ~615-776 |
| case62 | 0x532a-0x5472 | ~776 |
| case63 | 0x5472-0x5e18 | ~776-895 |
| case2 | 0x5e18-0x6840 | 776-886 |
| case3 | 0x6840-0x7962 | 895-1056 |
| case4 | 0x7962-0x84a2 | 1058-1181 |
| case5 | 0x84a2-0x8948 | 1183-1253 |
| case6 | 0x8948-0x8e18 | 1480-1577 |
| case7 | 0x8e18-0x97a8 | 1579-1745 |
| case8 | 0x97a8-0x99da | 1255-1316 |
| caseB | 0x99da-0x9ab6 | 1318-1347 |
| case9 | 0x9ab6-0x9b44 | 1349-1367 |
| case5A | 0x9b44-0x9d2a | 1369-1424 |
| caseC | 0x9d2a-0x9dc0 | 1426-1443 |
| case14 | 0x9dc0-0x9fa6 | 1445-1478 |
| case1E | 0x9fa6-0xa058 | 1747-1898 |

注意：12p 的 case 顺序（二进制内 if 级联顺序）是 1,A,62,63,2,3,4,5,6,7,8,B,9,5A,C,14,1E，
与 6p 顺序不同（6p 是 1,A,62,63,2,3,4,5,8,B,9,5A,C,14,6,7,1E）。按地址区间解码即可。

## r4=key 语义（input_scan_state 返回值，12p 与 6p 相同）
1=确认、2=DOWN/减、3=UP/加、4=SET/退出、5=启动、6=停机、0x16=快加、0x21=快减、
0x17=统计清零、0xe=初始参数密码、数字键 0-9 输密码。key<=0 走超时尾。

## 调用映射（12p 地址，已确认；形参语义见 6p 前向声明）
| 12p 地址 | 函数 | 签名 |
|---|---|---|
| 0x238 | wd_feed | void(void) |
| 0x766 | lcd_ctrl_line | void(int on) |
| 0x942 | disp_clear | void(void) |
| 0xcec | disp_string | void(int str_addr, uint32_t row, uint col, uint32_t attr) |
| 0xaf4 | disp_render_char8 | void(uint ch, char row, uint col, uint32_t attr) |
| 0xdf2 | disp_number3 | void(int value, uint32_t row, int col, uint32_t attr) |
| 0xe80 | disp_uint4 | void(uint value, uint32_t row, int col, uint32_t attr) |
| 0xf3c | disp_uint5 | void(uint value, uint32_t row, int col, uint32_t attr) |
| 0x1398 | disp_uint2 | void(uint value, uint32_t row, int col, uint32_t attr) |
| 0x13ec | disp_fixed_1dec | void(uint value, uint32_t row, int col, uint32_t attr) |
| 0x1260 | disp_offset | void(uint offset, uint32_t row, int col, uint32_t attr) |
| 0x1e38 | disp_by_index | 按索引显示（少见） |
| 0x1a68 | 同步检查 | 返回 1 时同步 *0x1000172c |
| 0x1a96 | 同步 | 写 *0x10001760 |
| 0x1aee / 0x1b46 | debounce_read | 结果写 *0x1000175c |
| 0x1976 | scan_run_stop | 结果写 *0x10001727 |
| 0x3534 | param_sync_live_to_eeprom | void(void) |
| 0x40b0 | disp_splash_screen | void(void) |
| 0x41b4 | disp_screen_static | void(void) |
| 0x41ec | disp_screen_calib | void(void) |
| 0x107e0 | out_relay_p021 | void(int on) |
| 0x10800 | fio1_pin20_ctrl | void(int on) |
| 0x10820 | fio1_pin21_ctrl | void(int on) |
| 0x10840 | fio1_pin23_ctrl | void(int on) |
| 0xe6c6 | fio0_pin22_ctrl | void(int on) |
| 0xe6a6 | fio1_pin22_ctrl | void(int on) |
| 0xe4fa | 输出级 | void(void) |
| 0xf70a | run_stop_preset | void(void) |

## 变量语义（12p SRAM；**读宽铁律**：dump 用 `ldrb/strb` 读→byte（volatile uint8_t*）、`ldr/str` 读→word（volatile uint32_t*），不可混用）
- 0x10001724=MENU（byte，case 门控）；0x10001725=光标/子项（byte，≡6p MENU2）；0x10001726=锁标志（byte，case63 key==1 切换 0/1）
- 0x10001740=TIMEOUT2（word，0x3c 初值，倒计到 0 回主屏）；0x10001744=TIMEOUT（word，菜单超时 0x1388）
- 0x10001748=IDLE（word，++到阈值触发 TIMEOUT2--）
- 0x10001754=disp_calc（word）
- 0x10001758=TIMEOUT3（word，0xfa/0x1f4 初值，++==0xfb 触发整页重绘、>0x1f4 回绕）
- 0x1000175c=DB_117（debounce 结果 byte）；0x1000175d=RUN_REQ（byte）；0x1000175e=RUN（byte）；0x1000175f=STOP（byte）
- 0x10001761=状态显示标志（byte）；0x10001762=RESET2 标志（byte）；0x10001765=run 使能标志（byte）
- 0x10001768=首页值源（word，TARGET 源=频率/给定）
- 0x1000162c=CTRL_MODE（case1 用它分支 pin 控制）；0x10001630/0x10001634=增益（word）
- 0x1000164d=DISP_SEL（byte，0/1/2 首页显示源）；0x1000164e=配对标志（byte，==0 直接启停、==1 需 DISP_SEL!=0）
- 0x1000164f=ESTOP（byte，clamp 2）；0x10001650=RESET_MODE（byte，clamp 2，case1 复位流程门控）
- 0x10001651/52/53=?（byte，clamp 1）；0x10001658=?（byte，clamp 0xb4）
- 0x10001690/98/a0/a8/b0=参数 word（++clamp 0x1194、--门控 >0xdac）
- 0x100015e0=PWD_A、0x100015ec=PWD_C、0x100015f2=PWD_BUF（byte 数组，密码）
- 0x100015a8=TARGET（word）；0x100015b4/0x100015d0=V_AMP2/V_AMP（word 显示副本）；0x100015d8=MANUAL（word）；0x100015d4=?（word，MANUAL 派生显示）
- 0x100015f8=HOUR_NOW、0x100015fc=MIN_NOW、0x10001604=HOUR_TOTAL、0x10001608=MIN_TOTAL（word）
- 0x10001620=cfg_word（word）；0x10001618/0x10001614=延时计数（word，密码错延时）
- 0x1000172c=SYNC_2C（byte）；0x10001727=scan_run_stop 结果（byte）
- 0x1000161c=LATCH_OUT、0x10001618=LATCH_IN（word，延时循环用）
- 其余 0x100016xx/0x100017xx 未列者：以 dump 实际访问宽度为准命名（如 disp_calc、menu_tick 等），注释标地址。

## 返回点约定
- 0x4932 = `pop {r4,pc}` = 函数返回点。所有 case 尾 `movs r0,#0; b 0x4932` ≡ return。
- case 内部提前返回用 `b 0x4e04` 等（同样 return）。C 里写 return 即可。
- 函数签名：`int state_machine(int key)`（值返回 0；`(void)key` 无妨）。各 case 是顺序 `if (*MENU == N) { ... return; }` 级联。

## 输出格式（铁律）
1. 只输出你负责 case 的 C 代码片段（不含 include/函数头/其他 case/前向声明/宏定义——这些主会话统一做）。
2. 用宏风格 `#define X ((volatile uint8_t*)0x1000xxxx)` 不行——主会话统一定义。片段内用
   `*(volatile uint8_t*)0x10001725` 或局部 `volatile uint8_t *m2=(volatile uint8_t*)0x10001725;` 均可，
   但**每个地址必须带真实 12p 地址字面量**，注释标语义。
3. 字符串地址（disp_string 第 1 参）直传 dump 中的 flash 地址字面量（如 0x4340），不臆造。
4. 中文注释，标明你解码的地址段（如 `/* case3 @0x6840-0x7962 */`）与关键动作来源地址。
5. 读宽严格按 dump：ldrb/strb→byte、ldr/str→word。有歧义处注释说明。
6. 逐段对照 dump，不臆造。若某段无法确证，宁可写注释 `/* 未确证 */` 也不猜。

## 解码模式提示（各 case 通用尾部）
- 菜单/编辑屏：`(*TIMEOUT3)++` → key 分支（1 切换编辑态/2,3 导航/4 保存退出/2,0x16,3,0x21 改值）
  → `if (*TIMEOUT3 == 0xfb) 重绘页` → `if (*TIMEOUT3 > 0x1f4) { *TIMEOUT3=0; if(查看态)return; 按项擦除 }`
  → `(*TIMEOUT)++; if (*TIMEOUT >= 0x1388) { *TIMEOUT=0; *MENU=1; disp_splash_screen(); }` → return。
- 只读/查询屏：key 分支 → `(*TIMEOUT)++; if (*TIMEOUT >= N) {超时回主屏}` → return。
- 校准/运行屏（case8/5A/1E）：加 `*((volatile uint8_t*)0x100015ce)=1;`（屏内置位）与 key==5/6 启停、run_stop_preset()。
