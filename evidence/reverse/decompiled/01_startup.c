/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 01：IAR 运行时 / 启动 / 系统初始化
 *
 * 工具：Ghidra 反编译（MCP）
 * 固件：LPC1765.bin（NXP LPC1765 / Cortex-M3）
 * 导出：2026-08-20
 * 说明：反编译原样保留；<...> 内为理解注释。
 *
 * 交叉引用：
 *   · 中断/定时器架构（TIMER0 系统节拍/EINT/向量表）→ docs/PROGRESS_2026-08-20.md §4j
 *   · 启动序列与硬件（LPC1765 / 12MHz 晶振）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §一.1
 *   · 认证门控调用 → 11_auth.c（ADuM1201 隔离链路，HARDWARE_VERIFICATION §二.5）
 * ========================================================================== */

/* ---------- 内存布局相关外部符号（按 Ghidra 命名） ---------- */
/* RAM 段 0x1000E000 起；BSS 清零由 iar_init_core 执行 16 字。
 * tick_ready = 0x100007A0（TIMER0 节拍标志，主循环等它=1）
 * input_code = 0x100007A4（input_scan_state 返回值，传给 state_machine）
 * auth 相关：0x10000744=认证使能？0x10000748=重试计数 0x1000074C=认证结果 0x10000750=锁机标志
 */


/* ==================== IAR EWARM 运行时（编译器启动代码） ==================== */

/* 0x000000CC —— RAM 数据段拷贝（ROM→RAM），完成后跳 iar_program_start */
void startup_copy_ram(void)
{
  int iVar1;
  code *UNRECOVERED_JUMPTABLE;
  int iVar2;
  undefined4 *puVar3;

  iVar1 = DAT_000000f8;
  puVar3 = (undefined4 *)(DAT_000000f8 + 0xf8);
  iVar2 = DAT_000000f8 + 0xf7;
  if (puVar3 == (undefined4 *)(DAT_000000fc + 0xf8)) {
    iar_program_start();
  }
  UNRECOVERED_JUMPTABLE = *(code **)(iVar1 + 0x104);
  if (((uint)UNRECOVERED_JUMPTABLE & 1) != 0) {
    UNRECOVERED_JUMPTABLE = (code *)(iVar2 - (int)UNRECOVERED_JUMPTABLE);
  }
                    /* WARNING: Could not recover jumptable at 0x000000f6. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)(*puVar3,*(undefined4 *)(iVar1 + 0xfc),*(undefined4 *)(iVar1 + 0x100));
  return;
}


/* 0x00000180 —— 寄存器初始化 stub（push/pop 全弹，无实际功能） */
undefined8 iar_init_registers(undefined4 param_1,undefined4 param_2)
{
  return CONCAT44(param_2,param_1);
}


/* 0x00000184 —— VFP 初始化 stub（空，Cortex-M3 无硬件 FPU） */
void iar_init_vfp(void)
{
  return;
}


/* 0x00000188 —— IAR 程序入口：init_core → init_registers → main → 收尾 */
void iar_program_start(void)
{
  undefined4 uVar1;
  undefined4 extraout_r2;
  undefined8 uVar2;

  uVar1 = iar_init_core();
  iar_init_registers(uVar1,extraout_r2);
  main();
  uVar2 = delay_wrapper();
  iar_init_vfp();
  abort_trap((int)uVar2,(int)((ulonglong)uVar2 >> 0x20));
  (*DAT_000001d0)();
                    /* WARNING: Could not recover jumptable at 0x000001ae. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*DAT_000001d4)();
  return;
}


/* 0x0000019A —— 延时例程（IAR 运行时；配合 abort_trap 的死循环） */
void delay_runtime(undefined4 param_1,undefined4 param_2)
{
  iar_init_vfp();
  abort_trap(param_1,param_2);
  (*DAT_000001d0)();
                    /* WARNING: Could not recover jumptable at 0x000001ae. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*DAT_000001d4)();
  return;
}


/* 0x000001C4 —— 数据初始化（区域定界常量 0x100027C8 / 0x100029C8） */
undefined8 iar_data_init(void)
{
  return CONCAT44(DAT_000001dc,DAT_000001d8);
}


/* 0x000010FD4 —— 内核初始化：SP←0x10006768（RAM 顶）、数据初始化、清 16 字 RAM */
void iar_init_core(void)
{
  uint uVar1;
  undefined4 *puVar2;
  undefined4 unaff_lr;
  uint *puVar3;

  uVar1 = get_initial_sp();
  *(undefined4 *)((uVar1 & 0xfffffff8) + 0x5c) = unaff_lr;
  puVar3 = (uint *)((uVar1 & 0xfffffff8) + 0x58);
  *puVar3 = uVar1;
  iar_data_init();
  puVar2 = (undefined4 *)*puVar3;
  *puVar2 = 0;
  puVar2[1] = 0;
  puVar2[2] = 0;
  puVar2[3] = 0;
  puVar2[4] = 0;
  puVar2[5] = 0;
  puVar2[6] = 0;
  puVar2[7] = 0;
  puVar2[8] = 0;
  puVar2[9] = 0;
  puVar2[0xa] = 0;
  puVar2[0xb] = 0;
  puVar2[0xc] = 0;
  puVar2[0xd] = 0;
  puVar2[0xe] = 0;
  puVar2[0xf] = 0;
  return;
}


/* 0x000011020 —— 读初始 SP 字面量（0x10006768） */
undefined4 get_initial_sp(void)
{
  return DAT_00011024;
}


/* 0x000011028 —— 异常/中止陷阱：软断点 + 死循环（IAR abort） */
void abort_trap(void)
{
  software_bkpt(0xab);
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


/* ==================== 看门狗 ==================== */

/* 0x000001E4 —— 看门狗超时 ISR：清超时标志 + 计数+1（超时监控用） */
void WDT_IRQHandler(void)
{
  DAT_40000000 = DAT_40000000 & 0xfb;
  *(int *)PTR_wdt_timeout_count_00000244 = *(int *)PTR_wdt_timeout_count_00000244 + 1;
  return;
}


/* 0x00000200 —— 看门狗初始化（param_1=超时计数值，main 里传 200） */
void wdt_init(uint param_1)
{
  *(undefined4 *)PTR_wdt_timeout_count_00000244 = 0;
  uRame000e100 = 1;
  _DAT_40000004 = (param_1 & 0x1ffff) << 0xd;
  DAT_40000000 = 3;
  DAT_40000008 = 0x55;
  return;
}


/* 0x00000238 —— 喂狗（WDFEED=0x55） */
void wd_feed(void)
{
  DAT_40000008 = 0x55;
  return;
}


/* ==================== 系统节拍定时器 TIMER0 ==================== */

/* 0x00000248 —— TIMER0 初始化：MR0=1999（节拍周期），匹配中断 IRQ0 */
void timer0_init(void)
{
  *(uint *)(DAT_000002d8 + 0xc4) = *DAT_000002d4 | 2;
  _DAT_4000400c = 0x18;      /* MCR：MR0 匹配→中断+复位 */
  _DAT_40004018 = 1999;      /* MR0 = 1999 */
  _DAT_40004000 = 0xff;      /* IR 清中断 */
  _DAT_40004014 = 3;         /* TCR：计数使能 */
  _DAT_40004004 = 1;         /* PC 预分频=1 */
  uRame000e100 = 2;
  return;
}


/* 0x0000029A —— TIMER0 节拍 ISR（主循环节拍源）：
 *   tick_ready=1（主循环等待它）；phase_cnt++（钳位 200）；tick_countdown--（若非0） */
void TIMER0_IRQHandler(void)
{
  undefined *puVar1;

  _DAT_40004000 = 0xff;
  *PTR_tick_ready_000002dc = 1;
  puVar1 = PTR_phase_cnt_000002e0;
  *PTR_phase_cnt_000002e0 = *PTR_phase_cnt_000002e0 + '\x01';
  if (200 < (byte)*puVar1) {
    *puVar1 = 200;
  }
  if (*PTR_tick_countdown_000002e4 != '\0') {
    *PTR_tick_countdown_000002e4 = *PTR_tick_countdown_000002e4 + -1;
  }
  return;
}


/* 0x000002EE —— 延时包装（IAR 运行时，配合死循环） */
void delay_wrapper(undefined4 param_1)
{
  delay_runtime(param_1);
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


/* ==================== 芯片时钟/电源初始化 ==================== */

/* 0x00000440 —— 系统初始化：内部 RC → PLL → 外设时钟树 */
void SystemInit(void)
{
  undefined4 *puVar1;

  *DAT_00000564 = 0x20;
  do {
  } while ((*DAT_00000564 & 0x40) == 0);
  *DAT_00000558 = 3;
  DAT_00000554[0x43] = 1;
  *DAT_0000056c = DAT_00000568;
  puVar1 = DAT_00000570;
  *DAT_00000570 = 0xaa;
  *puVar1 = 0x55;
  *DAT_00000574 = 1;
  *DAT_00000570 = 0xaa;
  DAT_00000554[0x23] = 0x55;
  do {
  } while ((*DAT_00000548 & 0x4000000) == 0);
  *DAT_00000574 = 3;
  DAT_00000554[0x23] = 0xaa;
  *DAT_00000570 = 0x55;
  do {
  } while ((*DAT_00000548 & 0x3000000) == 0);
  *DAT_00000578 = 0x23;
  DAT_00000554[0x2b] = 0xaa;
  *DAT_0000057c = 0x55;
  *DAT_00000580 = 1;
  *DAT_0000057c = 0xaa;
  DAT_00000554[0x2b] = 0x55;
  do {
  } while ((*DAT_00000584 & 0x400) == 0);
  *DAT_00000580 = 3;
  DAT_00000554[0x2b] = 0xaa;
  *DAT_0000057c = 0x55;
  do {
  } while ((*DAT_00000584 & 0x300) == 0);
  *DAT_00000588 = 0;
  DAT_00000554[0x6b] = 0;
  *DAT_00000590 = DAT_0000058c;
  *DAT_00000594 = 0;
  *DAT_00000554 = 0x303a;
  return;
}


/* 0x00000598 —— 长延时（6000×1000 空循环，用于等待电源稳定） */
void long_delay(void)
{
  undefined4 local_c;
  undefined4 local_8;

  for (local_8 = 0; local_8 < 6000; local_8 = local_8 + 1) {
    for (local_c = 0; local_c < 1000; local_c = local_c + 1) {
    }
  }
  return;
}


/* 0x000005CA —— 空函数（bx lr） */
void stub_ret(void)
{
  return;
}


/* ==================== 主程序 ==================== */

/* 0x000005CC —— 主程序
 * 启动序列：
 *   SystemInit → pin_config → gpio1_init → long_delay → gpio0_input_init →
 *   read_input_p02 → timer0_init → gpio_inputs_dir_init → i2c_gpio_init →
 *   adc_init → load_config → pin_config → gpio2_init → timer1_init →
 *   timer2_init → eint1_init → eint2_init → eint3_init → uart3_init →
 *   long_delay → 开机认证(auth_challenge ×3) → disp_splash_screen →
 *   auth_retry → wdt_init(200)
 *
 * 认证逻辑：
 *   (0x10000744==0) && (0x10000748<3)：auth_challenge()
 *     0x1000074C==1（成功）→ 0x10000750=0（放行）
 *     0x1000074C==0（失败）→ 0x10000750=1（锁机）；0x10000748++（重试计数）
 *     成功后 param_sync_live_to_eeprom()（参数同步）
 *
 * 分支：
 *   · 0x10000750==0（认证通过）：chk_p02_p03()<1（P0.2/P0.3 安全联锁未触发）
 *     → 主循环：adc0_scan_channels→input_scan_state→adc0_scan_channels→
 *       state_machine→output_stage→wd_feed→uart3_rx_timeout_monitor→modbus_dispatch(0)
 *   · 0x10000750==0 但联锁触发 → 显示"急停/联锁错误"屏（disp_string 0x76C 区）死循环
 *     + freq_adjust_sync + run_stop_preset
 *   · 0x10000750!=0（认证失败）→ 显示错误屏（0x754 区）死循环锁机 */
void main(void)
{
  undefined1 *puVar1;
  undefined1 uVar2;
  int iVar3;

  SystemInit();
  pin_config();
  gpio1_init();
  long_delay();
  gpio0_input_init();
  read_input_p02();
  timer0_init();
  gpio_inputs_dir_init();
  i2c_gpio_init();
  adc_init();
  load_config();
  pin_config();
  gpio2_init();
  timer1_init();
  timer2_init();
  eint1_init();
  eint2_init();
  eint3_init();
  uart3_init();
  long_delay();
  if ((*DAT_00000744 == '\0') && (*DAT_00000748 < 3)) {
    auth_challenge();
    if (*DAT_0000074c == '\x01') {
      *DAT_00000750 = 0;
    }
    if (*DAT_0000074c == '\0') {
      *DAT_00000750 = 1;
    }
    *DAT_00000748 = *DAT_00000748 + 1;
    param_sync_live_to_eeprom();
  }
  disp_splash_screen();
  auth_retry();
  wdt_init(200);
  if (*DAT_00000750 != 0) {
    /* —— 认证失败：锁机屏 —— */
    disp_clear();
    disp_string(0x754,0,4);
    disp_string(0x760,2,4,0);
    do {
      wd_feed();
    } while( true );
  }
  /* —— 认证通过 —— */
  iVar3 = chk_p02_p03();
  if (iVar3 < 1) {
    /* —— 主循环（正常运行态）—— */
    do {
      do {
      } while (*DAT_000007a0 != '\x01');   /* 等 tick_ready */
      *DAT_000007a0 = '\0';
      stub_ret();
      adc0_scan_channels();
      uVar2 = input_scan_state();
      *DAT_000007a4 = uVar2;
      adc0_scan_channels();
      state_machine(*DAT_000007a4);
      output_stage();
      wd_feed();
      uart3_rx_timeout_monitor();
      modbus_dispatch(0);
    } while( true );
  }
  /* —— 安全联锁触发（P0.2/P0.3）：显示联锁错误屏 + 降频 + 停机预设 —— */
  disp_clear();
  disp_string(0x76c,0,4);
  disp_string(0x778,1,2,0);
  disp_string(0x784,2,2,0);
  disp_offset(*DAT_0000078c,2,7,1);
  disp_string(0x790,3,0);
  do {
    do {
    } while (*DAT_000007a0 != '\x01');
    *DAT_000007a0 = '\0';
    uVar2 = input_scan_state();
    puVar1 = DAT_000007a4;
    *DAT_000007a4 = uVar2;
    freq_adjust_sync(*puVar1);
    run_stop_preset();
    wd_feed();
  } while( true );
}


/* 0x000007A8 —— 简单延时（param_1×50 空循环） */
void Delay(int param_1)
{
  for (param_1 = param_1 * 0x32; param_1 != 0; param_1 = param_1 + -1) {
  }
  return;
}
