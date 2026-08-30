/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 07：状态机 / 频率调节
 *
 * freq_adjust_sync：联锁/错误屏下的频率调节（main 中 0x10000750==0 但
 *   联锁触发分支调用）。0x1000AC0C=当前频率(0.1Hz，上限 0x2B0=688≈68.8Hz，
 *   下限 0x45=69≈6.9Hz)；0x1000AC10=EEPROM 缓存副本（P0.9 信号有效且值变化
 *   时写回芯片 reg 0xC9/0xCA，16 位）；0x1000AC14=显示模式标志。
 *   param_1 事件码：2/0x16=快进(+1)、3/0x21=快退(-1)、5=高档、6=低档。
 * state_machine（0x458c，最大函数，约 10049 行反汇编）：
 *   主菜单/参数编辑状态机，见 MENU_PARAMETER_MAPPING.md。
 *   反编译超时，以分段反汇编方式补全（见下文）。
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · 菜单树→参数全量映射（16+10+4+9 屏）→ docs/MENU_PARAMETER_MAPPING.md
 *   · 状态机拆解 / 事件码 / 字符串表 → docs/state_machine_analysis.md
 *   · 状态标志→事件码、运行计时（120s/300s 同步）→ APPLICATION_GUIDE_2026-08-21.md §三
 * ========================================================================== */

/* 0x0000AB48 —— 联锁屏频率调节 + EEPROM 写回 */
void freq_adjust_sync(int param_1)
{
  uint *puVar1;
  int iVar2;

  iVar2 = debounce_p09();
  if ((iVar2 == 1) && (*DAT_0000ac0c != *DAT_0000ac10)) {
    *DAT_0000ac10 = *DAT_0000ac0c;
    i2c_write_reg((ushort)*DAT_0000ac10 >> 8,0xc9);
    i2c_write_reg((char)*DAT_0000ac10,0xca);
  }
  puVar1 = DAT_0000ac0c;
  if ((param_1 == 2) || (param_1 == 0x16)) {
    *DAT_0000ac0c = *DAT_0000ac0c + 1;
    if (0x2b0 < *puVar1) {
      *puVar1 = 0x2b0;
    }
    disp_offset(*DAT_0000ac0c,2,7,1);
  }
  if ((param_1 == 3) || (param_1 == 0x21)) {
    if (*DAT_0000ac0c < 0x45) {
      *DAT_0000ac0c = 0x45;
    }
    *DAT_0000ac0c = *DAT_0000ac0c - 1;
    disp_offset(*DAT_0000ac0c,2,7,1);
  }
  if (param_1 == 5) {
    *DAT_0000ac14 = 1;
    disp_string(DAT_0000ac18,3,0xb,0);
  }
  if (param_1 == 6) {
    *DAT_0000ac14 = 0;
    disp_string(&DAT_0000ac1c,3,0xb,0);
  }
  return;
}

/* =============================================================================
 * state_machine（0x0000458C，函数体 0x458C-0xAB48 之前，10061 条指令 / 约 702KB）
 * —— 主菜单/状态机（main 主循环调用，主界面导航 + 运行/停机 + 参数编辑）
 *
 * ★ 说明：C 反编译结果超过 MCP 5s 传输上限（连续超时），改为「反汇编精读还原」。
 *   完整反汇编已另存为 decompiled/07_state_machine_asm.txt（10061 条指令）。
 *   菜单各屏的参数映射见 MENU_PARAMETER_MAPPING.md（基本 16 屏/保护 10 屏/
 *   通讯 4 屏/PID 9 屏全量确证）。
 *
 * 调用关系（去重目标，×N 为次数）：
 *   disp_string(0xD3C)×405、disp_uint4(0xED0)×88、disp_clear(0x992)×28、
 *   disp_splash_screen(0x427C)×25、fio1_pin20_ctrl(0x105C8)×23、
 *   fio1_pin21_ctrl(0x105E8)×23、i2c_write_reg(0x1E88)×17、wd_feed(0x238)×14、
 *   param_sync_live_to_eeprom(0x35F2)×11、gpio0_input_init(0xF8C)×11、
 *   disp_uint2(0x13E8)×8、disp_number3(0xE42)×8、disp_fixed_1dec(0x143C)×7、
 *   disp_signed_angle(0x11BC)×5、gpio_outputs_set(0xE79A)×4、run_stop_preset(0xF9AA)×4、
 *   fio0_pin22_ctrl(0xE966)×3、out_relay_p021(0x105A8)×3、disp_render_char8(0xB44)×3、
 *   disp_number(0x1092)×3、disp_offset(0x12B0)×3、lcd_ctrl_line(0x7B6)×2、
 *   fio1_pin22_ctrl(0xE946)×2、fio1_pin23_ctrl(0x10608)×2、debounce_p117(0x1B3E)×2、
 *   debounce_p06(0x1B96)×2、scan_run_stop(0x19C6)×2、debounce_p09(0x1AB8)×1、
 *   out_relay_p020(0x10588)×1、debounce_p116(0x1AE6)×1、disp_screen_static(0x448A)×1、
 *   disp_screen_calib(0x44C2)×1
 *
 * 关键数据区（0x1000 段）：
 *   0x100048B8 tick 计数（>0x1388=5000 复位并刷 LCD 控制线；param>0 亦复位）
 *   0x100048BC..0xD0 运行参数（变化即写 EEPROM：reg 0x97/98、0x99/9A、0x9B/9C）
 *   0x100047CC→0x100048D4 保护参数变化 → 写 reg 0x1D/0x1E
 *   0x100047D8 状态/故障标志字（bit0..15 → 事件码 1..0xE，见下）
 *   0x100048D8 事件码（分发给各菜单页处理）
 *   0x10004CFC 运行状态标志（>0 → 开触发 fio0/1_pin22）
 *   0x10004D00..D14 运行计时器链（tick→秒→十秒→累计分钟）
 *   0x10004D18/0x1C 分时 EEPROM 同步标志（120s/300s 触发 param_sync）
 *   0x10009CC8 菜单字符串表基址；0x10009CA8 菜单页状态
 *   0x10009CCC 菜单停留计数；0x10009CD0 显示模式；0x10009CE8 运行/停机标志
 *   0x10009CD8→0x10009CEC 运行频率（变化写 reg 0xC9/0xCA）
 * 导出：2026-08-21
 * ========================================================================== */

/* ---------------------------------------------------------------------------
 * 一、状态标志位 → 事件码映射（0x100047D8 → 0x100048D8）
 *   bit0 | bit1 | bit2          → 事件 1
 *   bit3                        → 事件 2
 *   bit9                        → 事件 3
 *   bit6                        → 事件 4   （启动）
 *   bit10                       → 事件 5
 *   bit4                        → 事件 6   （停机）
 *   bit5                        → 事件 7
 *   bit8                        → 事件 8
 *   bit7                        → 事件 9
 *   bit14                       → 事件 0xA
 *   bit15                       → 事件 0xB
 *   bit11                       → 事件 0xC
 *   bit13                       → 事件 0xD
 *   bit12                       → 事件 0xE
 *   （bit 判序：0x1,0x4,0x2→1; 0x8→2; 0x200→3; 0x40→4; 0x400→5;
 *     0x10→6; 0x20→7; 0x100→8; 0x80→9; 0x4000→A; 0x8000→B;
 *     0x800→C; 0x2000→D; 0x1000→E）
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * 二、流程还原（伪代码）
 * ---------------------------------------------------------------------------
 * state_machine(param) {
 *   // ── 节拍与 LCD 控制线刷新 ──
 *   *0x48B8 += 1;
 *   if (param > 0)        { *0x48B8 = 0; lcd_ctrl_line(); }        // 主动刷新
 *   if (*0x48B8 > 0x1388) { *0x48B8 = 0; lcd_ctrl_line(); }        // 5000 tick 超时刷新
 *
 *   // ── P0.9 输入消抖 → 运行参数变化写 EEPROM ──
 *   if (debounce_p09() == 1) {
 *     if (*0x48BC != *0x48C0) { *0x48C0 = *0x48BC;
 *       i2c_write_reg(值>>8, 0x97); i2c_write_reg(值&0xFF, 0x98); }
 *     if (*0x48C4 != *0x48C8) { *0x48C8 = *0x48C4;
 *       i2c_write_reg(值>>8, 0x99); i2c_write_reg(值&0xFF, 0x9A); }
 *     if (*0x48CC != *0x48D0) { *0x48D0 = *0x48CC;
 *       i2c_write_reg(值>>8, 0x9B); i2c_write_reg(值&0xFF, 0x9C); }
 *     if (*0x47CC != *0x48D4) { *0x48D4 = *0x47CC;
 *       i2c_write_reg(值>>8, 0x1D); i2c_write_reg(值&0xFF, 0x1E); }
 *   }
 *
 *   // ── 状态标志解码 → 事件码 ──
 *   if (*0x47D8 != 0) {
 *     // 按 bit 判序写入 *0x48D8 = 1..0xE（见映射表）
 *     if (bit0|bit1|bit2 置位) *0x48D8 = 1;
 *     if (bit3) *0x48D8 = 2;  ...  if (bit12) *0x48D8 = 0xE;
 *   }
 *   if (*0x48D8 == 0) {
 *     // 无事件：关报警继电器
 *     out_relay_p021(0); fio1_pin23_ctrl(0); *0x4CF8 = 0;
 *   } else {
 *     // 有事件：关触发、开报警，初始化故障显示
 *     fio0_pin22_ctrl(0); fio1_pin22_ctrl(0);
 *     out_relay_p021(1);  fio1_pin23_ctrl(1);
 *     *0x48DC = 0; *0x48E0 = 1; *0x48E4 = 0; *0x47E4 = 0; *0x48E8 = 0;
 *     gpio_outputs_set();
 *   }
 *
 *   // ── 运行状态处理：计时 + 分时 EEPROM 同步 ──
 *   if (*0x4CFC > 0) {                    // 运行中
 *     fio0_pin22_ctrl(1); fio1_pin22_ctrl(1);   // 开触发使能
 *     *0x4D00 += 1;                       // tick
 *     if (*0x4D00 > 0x7530) { *0x4D00 = 0; *0x4D04 += 1; }   // 30000 tick → 秒
 *     *0x4D08 += 1;
 *     if (*0x4D04 >= 0x3C) { *0x4D04 = 0; *0x4D0C += 1; }    // 60s → 分钟
 *     if (*0x4D08 >= 0x3C) { *0x4D08 = 0; *0x4D10 += 1; *0x4D14 += 1; }
 *     if (*0x4D14 >= 0x140) { *0x4D14 = 0; }
 *     if (*0x4D14 == 0x78  && *0x4D18 == 0)  { *0x4D18 = 1; *0x4D1C = 0; param_sync(); } // 120s
 *     if (*0x4D14 == 0x12C && *0x4D18 == 2 && *0x4D1C == 0) { *0x4D18 = 0; *0x4D1C = 1; param_sync(); } // 300s
 *   }
 *   // ── 事件码分发 → 菜单系统 ──
 *   // *0x48D8 事件码（1..0xE）决定进入哪个处理段：
 *   //   事件 4=启动、6=停机 → 运行/停机菜单页（见下）；
 *   //   其余事件码 → 菜单导航/参数编辑段，操作菜单系统寄存器：
 *   //     一级菜单选择 0x10001744、二级屏索引 0x10001745、编辑门控 0x10001746(==1 可编辑)
 *   //   编辑状态分发（state_machine 内 r4）：
 *   //     r4==2 单步 +1；r4==0x16 快进 +5（阈值类）
 *   //     r4==3 单步 -1；r4==0x21 快退 -5
 *   //     r4==1 UP 键（菜单导航/密码输入）；r4==4 SET 确认/退出
 *   //   每次编辑先写 0x10001764=0（清编辑标志），改完置 0x10001778=0xFA(250) 倒计时返回
 *   //   编辑落点 = 各参数 RAM（0x10001634..0x10001726），随 param_sync 写回 EEPROM
 *   // 完整菜单树→参数映射（16+10+4+9 屏）见 MENU_PARAMETER_MAPPING.md §0-§4
 * }

 *
 * 运行/停机菜单页（事件码 4=启动、6=停机）：
 *   *0x9CE8 = 1; *0x9CCC = 0;
 *   if (event == 6) { *0x9CE8 = 0; *0x9CCC = 0; gpio_outputs_set(); }   // 停机
 *   run_stop_preset();
 *   if (event == 4) {                         // 启动
 *     *0x9CCC = 0; *0x9CD0 = 2; *0x9CA8 = 5;
 *     disp_clear();
 *     disp_string(表+0x64, 0,0,0); disp_string(表+0x78, 1,0,0);   // 4 行状态显示
 *     disp_string(表+0x8C, 2,0,0); disp_string(表+0xA0, 3,0,0);
 *     if (*0x9CD8 != *0x9CEC) { *0x9CEC = *0x9CD8;   // 频率变化写 EEPROM
 *       i2c_write_reg(值>>8, 0xC9); i2c_write_reg(值&0xFF, 0xCA); }
 *     gpio_outputs_set(); *0x9CE8 = 0; run_stop_preset();
 *     *0x9CD4 = 0; *0x9CE0 = 0;
 *   } else {                                  // 其他事件：停留超时回主屏
 *     *0x9CCC += 1;
 *     if (*0x9CCC > 0x3A98) { *0x9CCC = 0; *0x9CE8 = 0;
 *       gpio_outputs_set(); *0x9CD0 = 1; disp_splash_screen(); }
 *   }
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * 三、关键代码段精读（真实反汇编）
 * ---------------------------------------------------------------------------
 * 入口（0x458C）：
 *   push {r4,lr}; mov r4,r0
 *   ldr r0,[0x48B8]; ldr r0,[r0,#0]; adds r0,#1; str r0,[0x48B8]   ; tick++
 *   cmp r4,#0; ble 0x45A8
 *   movs r0,#0; str r0,[r1,#0]; movs r0,#1; bl lcd_ctrl_line       ; param>0 刷新
 *   ldr r0,[0x48B8]; ldr r0,[r0,#0]; movw r1,#0x1388; cmp r0,r1; bls 0x45BE
 *   movs r0,#0; str r0,[0x48B8]; bl lcd_ctrl_line                  ; 5000tick 刷新
 *
 * P0.9 输入 → EEPROM（0x45BE）：
 *   bl debounce_p09; cmp r0,#1; bne 0x4676
 *   ldr r0,[0x48BC]; ldr r1,[0x48C0]; cmp r0,r1; beq 0x45F2       ; 值未变跳过
 *   ldr r0,[0x48BC]; str r0,[0x48C0]                              ; 更新缓存
 *   movs r1,#0x97; ldrh r0,[0x48C0]; ubfx r0,r0,#8,#8; bl i2c_write_reg  ; 高字节→0x97
 *   ldrb r0,[0x48C0]; movs r1,#0x98; bl i2c_write_reg             ; 低字节→0x98
 *   ; 0x48C4→0x48C8（reg 0x99/0x9A）、0x48CC→0x48D0（reg 0x9B/0x9C）
 *   ; 0x47CC→0x48D4（reg 0x1D/0x1E）同模式
 *
 * 状态标志解码（0x4676 起）：
 *   ldr r0,[0x47D8]; ldrb r0,[r0,#0]; tst r0,#4; beq 0x468E       ; bit2
 *   movs r0,#1; str r0,[0x48D8]
 *   tst r0,#2; beq 0x469E                                          ; bit1
 *   movs r0,#1; str r0,[0x48D8]
 *   tst r0,#1; beq 0x46AE                                          ; bit0
 *   movs r0,#1; str r0,[0x48D8]
 *   tst r0,#8; beq 0x46BE                                          ; bit3 → 2
 *   tst r0,#0x200; beq 0x46CE                                      ; bit9 → 3
 *   tst r0,#0x40; beq 0x46DE                                        ; bit6 → 4
 *   tst r0,#0x400; beq 0x46EE                                       ; bit10 → 5
 *   tst r0,#0x10; beq 0x46FE                                        ; bit4 → 6
 *   tst r0,#0x20; beq 0x470E                                        ; bit5 → 7
 *   tst r0,#0x100; beq 0x471E                                       ; bit8 → 8
 *   tst r0,#0x80; beq 0x472E                                        ; bit7 → 9
 *   tst r0,#0x4000; beq 0x473E                                      ; bit14 → A
 *   tst r0,#0x8000; beq 0x474E                                      ; bit15 → B
 *   tst r0,#0x800; beq 0x475E                                       ; bit11 → C
 *   tst r0,#0x2000; beq 0x476E                                      ; bit13 → D
 *   tst r0,#0x1000; beq 0x4782                                      ; bit12 → E
 *
 * 无事件分支（0x4904）：
 *   movs r0,#0; bl out_relay_p021; bl fio1_pin23_ctrl              ; 关报警
 *   movs r0,#0; str r0,[0x4CF8]
 * 有事件分支（0x4782）：
 *   movs r0,#0; bl fio0_pin22_ctrl; bl fio1_pin22_ctrl             ; 关触发
 *   movs r0,#1; bl out_relay_p021; bl fio1_pin23_ctrl              ; 开报警继电器
 *   strb r0,[0x48DC]; strb r0,[0x48E0]... ; bl gpio_outputs_set    ; 输出复位
 *
 * 运行计时（0x4916，运行中开触发 + 分时 param_sync）：
 *   ldr r0,[0x4CFC]; ldrb r0,[r0,#0]; cmp r0,#0; ble 0x49EA
 *   movs r0,#1; bl fio0_pin22_ctrl; bl fio1_pin22_ctrl             ; 开触发使能
 *   ldr r0,[0x4D00]; adds r0,#1; str r0,[0x4D00]                   ; tick++
 *   cmp r0,#0x7530; bls 0x49A4                                     ; 30000tick
 *   movs r0,#0; str r0,[0x4D00]; ldr r0,[0x4D04]; adds r0,#1; str r0,[0x4D04]  ; 秒++
 *   ...0x4D08++、0x4D04/0x4D08 到 0x3C 进位 0x4D0C/0x4D10、0x4D14 到 0x140 清零
 *   ldr r0,[0x4D14]; cmp r0,#0x78; bne 0x49C2                      ; ==120s
 *   ldr r0,[0x4D18]; cbnz r0,0x49C2                                ; 未触发过
 *   movs r0,#1; str r0,[0x4D18]; movs r0,#0; str r0,[0x4D1C]; bl param_sync
 *   ldr r0,[0x4D14]; cmp r0,#0x12C; bne 0x49EA                     ; ==300s
 *   ldr r0,[0x4D18]; cmp r0,#2; bne 0x49EA; ldr r0,[0x4D1C]; cbnz r0,0x49EA
 *   movs r0,#0; str r0,[0x4D18]; movs r0,#1; str r0,[0x4D1C]; bl param_sync
 *
 * 运行/停机菜单页（0x9B60）：
 *   movs r0,#1; strb r0,[0x9CE8]; movs r0,#0; str r0,[0x9CCC]
 *   cmp r4,#6; bne 0x9B7E                                          ; 事件码==6 停机
 *   movs r0,#0; strb r0,[0x9CE8]; str r0,[0x9CCC]; bl gpio_outputs_set
 *   bl run_stop_preset                                             ; 停机预设
 *   cmp r4,#4; bne 0x9C1E                                          ; 事件码==4 启动
 *   movs r0,#0; str r0,[0x9CCC]; movs r0,#2; strb r0,[0x9CD0]
 *   movs r0,#5; strb r0,[0x9CA8]; bl disp_clear
 *   movs r3,#0; mov r2,r3; mov r1,r3; ldr r0,[0x9CC8]; adds r0,#0x64; bl disp_string  ; 第1行
 *   movs r3,#1; movs r2,#0; movs r1,#1; ldr r0,[0x9CC8]; adds r0,#0x78; bl disp_string ; 第2行
 *   ... 0x8C/0xA0 第3/4 行
 *   ldr r0,[0x9CD8]; ldr r1,[0x9CEC]; cmp r0,r1; beq 0x9C04       ; 频率变化
 *   ldr r0,[0x9CD8]; str r0,[0x9CEC]
 *   movs r1,#0xC9; ldrh r0,[0x9CEC]; ubfx r0,r0,#8,#8; bl i2c_write_reg  ; reg 0xC9
 *   ldrb r0,[0x9CEC]; movs r1,#0xCA; bl i2c_write_reg             ; reg 0xCA
 *   bl gpio_outputs_set; movs r0,#0; strb r0,[0x9CE8]; bl run_stop_preset
 *   movs r0,#0; strb r0,[0x9CD4]; strb r0,[0x9CE0]; b 0x999C
 *   ; 其他事件（0x9C1E）：*0x9CCC += 1；>0x3A98(15000) → 停机、回主屏
 *   ldr r0,[0x9CCC]; adds r0,#1; str r0,[0x9CCC]
 *   movw r1,#0x3A98; cmp r0,r1; bcc 0x9C58
 *   movs r0,#0; str r0,[0x9CCC]; strb r0,[0x9CE8]; bl gpio_outputs_set
 *   movs r0,#1; strb r0,[0x9CD0]; bl disp_splash_screen           ; 回主屏
 * --------------------------------------------------------------------------- */
