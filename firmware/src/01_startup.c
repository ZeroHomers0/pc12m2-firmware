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


/* =============================================================================
 * src/01_startup.c — 反编译模块 01（启动/初始化/main/系统时钟）可编译副本
 * 目标B 阶段4：补 include + 移除 IAR runtime 函数（startup.s/vectors.c/
 *   lpc1765.ld 等价替代）+ MMIO 符号换 reg.h 宏 + 跨模块函数前向声明。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"
#include <stdbool.h>

/* ---------- 跨模块函数前向声明（签名与定义模块核实一致） ---------- */
void pin_config(void);
void gpio1_init(void);
void gpio0_input_init(void);
void read_input_p02(void);
void gpio_inputs_dir_init(void);
void i2c_gpio_init(void);
void adc_init(void);
void adc0_scan_channels(void);
void load_config(void);
void gpio2_init(void);
void timer1_init(void);
void timer2_init(void);
void eint1_init(void);
void eint2_init(void);
void eint3_init(void);
void uart3_init(uint divisor);
void auth_challenge(void);
void auth_retry(void);
void param_sync_live_to_eeprom(void);
void disp_splash_screen(void);
void disp_clear(void);
void disp_string(int str_addr, undefined4 row, uint col, undefined4 invert);
void disp_offset(uint offset, undefined4 row, int col, undefined4 invert);
undefined4 chk_p02_p03(void);
undefined1 input_scan_state(void);
void state_machine(int event);
void output_stage(void);
void uart3_rx_timeout_monitor(void);
void modbus_dispatch(int param);
void freq_adjust_sync(int event);
void run_stop_preset(void);

/* ==================== IAR EWARM 运行时（已移除，见文件头） ==================== */



/* ==================== 看门狗 ==================== */

/* 0x000001E4 —— 看门狗超时 ISR：清超时标志 + 计数+1（超时监控用） */
void WDT_IRQHandler(void)
{
  WDMOD = WDMOD & 0xfb;
  *(volatile int *)PTR_wdt_timeout_count_00000244 = *(volatile int *)PTR_wdt_timeout_count_00000244 + 1;
  return;
}


/* 0x00000200 —— 看门狗初始化（timeout_cnt=超时计数值，main 里传 200） */
void wdt_init(uint timeout_cnt)
{
  *(volatile undefined4 *)PTR_wdt_timeout_count_00000244 = 0;
  NVIC_ISER0 = 1;                       /* 使能 WDT IRQ（IRQ0） */
  WDTC = (timeout_cnt & 0x1ffff) << 0xd;    /* WDTC：看门狗定时值 */
  WDMOD = 3;                            /* WDEN+WDRESET */
  WDFEED = 0xAA;                        /* 喂狗序列第 1 字节 */
  WDFEED = 0x55;                        /* 喂狗序列第 2 字节 */
  return;
}


/* 0x00000238 —— 喂狗（WDFEED=0x55） */
void wd_feed(void)
{
  WDFEED = 0xAA;
  WDFEED = 0x55;
  return;
}


/* ==================== 系统节拍定时器 TIMER0 ==================== */

/* 0x00000248 —— TIMER0 初始化：MR0=1999（节拍周期），匹配中断 IRQ0 */
void timer0_init(void)
{
  *(volatile uint *)(DAT_000002d8 + 0xc4) = *g_pconp | 2;  /* PCONP |= 2：TIMER0 上电 */
  TIMER0->TCR = 2;            /* 复位 TC/PC（先复位再配置） */
  TIMER0->PR = 0x18;          /* 预分频 24 */
  TIMER0->MR0 = 1999;         /* MR0 = 1999（节拍周期） */
  TIMER0->IR = 0xff;          /* 清全部中断标志 */
  TIMER0->MCR = 3;            /* MR0 匹配→中断 + 复位 */
  TIMER0->TCR = 1;            /* 计数使能 */
  NVIC_ISER0 = 2;             /* 使能 TIMER0 IRQ（IRQ1） */
  return;
}


/* 0x0000029A —— TIMER0 节拍 ISR（主循环节拍源）：
 *   tick_ready=1（主循环等待它）；phase_cnt++（钳位 200）；tick_countdown--（若非0） */
void TIMER0_IRQHandler(void)
{
  volatile uint8_t *phase_cnt;

  TIMER0->IR = 0xff;
  *PTR_tick_ready_000002dc = 1;
  phase_cnt = g_phase_cnt;
  *g_phase_cnt = *g_phase_cnt + '\x01';
  if (200 < (byte)*phase_cnt) {
    *phase_cnt = 200;
  }
  if (*PTR_tick_countdown_000002e4 != '\0') {
    *PTR_tick_countdown_000002e4 = *PTR_tick_countdown_000002e4 + -1;
  }
  return;
}


/* ==================== 芯片时钟/电源初始化 ==================== */

/* 0x00000440 —— 系统初始化：内部 RC → PLL0/PLL1 → 外设时钟树
 * 时钟/电源寄存器块 0x400FC000（SCB 区，对照 globals.c）：
 *   DAT_00000564=0x400FC1A0（PLL1 锁存等待）、DAT_00000558=0x400FC104、
 *   DAT_00000548=0x400FC088（PLL0STAT）、DAT_0000056c=0x400FC084（PLL0CFG）、
 *   DAT_00000570=0x400FC08C（PLL0FEED，pll_feed）、DAT_00000574=0x400FC080（PLL0CON）、
 *   DAT_00000578=0x400FC0A4（PLL1CFG）、DAT_0000057c=0x400FC0AC（PLL1FEED）、
 *   DAT_00000580=0x400FC0A0（PLL1CON）、DAT_00000584=0x400FC0A8（PLL1STAT）、
 *   DAT_00000588=0x400FC1A8（PCLKSEL0）、g_pconp=0x400FC0C4（PCONP）、
 *   DAT_00000594=0x400FC1C8（PCLKSEL1）。
 * PLL 配置要点：改 PLLxCON 后须向 PLLxFEED 写 0xAA→0x55 序列锁存；随后
 *   do{}while 轮询 PLLxSTAT 位直到锁相完成。pll_feed 即 PLL0FEED 寄存器指针。 */
void SystemInit(void)
{
  volatile uint32_t *pll_feed;

  *DAT_00000564 = 0x20;
  do {
  } while ((*DAT_00000564 & 0x40) == 0);
  *DAT_00000558 = 3;
  DAT_00000554[0x43] = 1;
  *DAT_0000056c = DAT_00000568;
  pll_feed = DAT_00000570;
  *DAT_00000570 = 0xaa;
  *pll_feed = 0x55;
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
  *g_pconp = DAT_0000058c;
  *DAT_00000594 = 0;
  *DAT_00000554 = 0x303a;
  return;
}


/* 0x00000598 —— 长延时（6000×1000 空循环，用于等待电源稳定） */
void long_delay(void)
{
  volatile uint32_t i;
  volatile uint32_t j;

  for (i = 0; i < 6000; i++) {
    for (j = 0; j < 1000; j++) {
      /* 原固件空转延时；volatile 防止 -Os 删除。 */
    }
  }
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
 *     0x1000074C==0（成功）→ 0x10000750=1（放行）
 *     0x1000074C==1（失败）→ 0x10000750=0（锁机）；0x10000748++（重试计数）
 *     成功后 param_sync_live_to_eeprom()（参数同步）
 *
 *   2026-08-27 决定（抄板）：永久强制 *DAT_00000750=1，防抄板认证不再启用，
 *   不因 EEPROM 认证状态锁机。0x1000072C 与状态机 SYNC_2C（07_state_machine.c）
 *   同址：=1 时跳过 RLY3/2/1 安全强制驱动，继电器走正常输出逻辑。
 *   安全强制驱动，继电器走正常输出逻辑。
 *
 * 分支：
 *   · 0x10000750==1（认证通过/强制放行）：chk_p02_p03()<1（P0.2/P0.3 安全联锁未触发）
 *     → 主循环：adc0_scan_channels→input_scan_state→adc0_scan_channels→
 *       state_machine→output_stage→wd_feed→uart3_rx_timeout_monitor→modbus_dispatch(0)
 *   · 0x10000750==1 但联锁触发 → 显示"急停/联锁错误"屏（disp_string 0x76C 区）死循环
 *     + freq_adjust_sync + run_stop_preset
 *   · 0x10000750==0（认证失败，正常不可达）→ 显示错误屏（0x754 区）死循环锁机 */
void main(void)
{
  volatile uint8_t *p_input_code;
  undefined1 input_code;
  int interlock;

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
  uart3_init(0);
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
  /* —— 2026-08-27 决定（抄板）：防抄板认证永久放行，不再启用。
   *    忽略 auth_challenge/auth_retry 结果；1=放行、0=锁机（0x1000172C 与状态机 SYNC_2C 同址）。 */
  *DAT_00000750 = 1;
  if (*DAT_00000750 == 0) {
    /* —— 认证失败：锁机屏（认证已永久放行，正常不可达） —— */
    disp_clear();
    disp_string(0x754,0,4,0);
    disp_string(0x760,2,4,0);
    do {
      wd_feed();
    } while( true );
  }
  /* —— 认证通过 —— */
  interlock = chk_p02_p03();
  if (interlock < 1) {
    /* —— 主循环（正常运行态）—— */
    do {
      do {
      } while (*DAT_000007a0 != '\x01');   /* 等 tick_ready */
      *DAT_000007a0 = '\0';
      stub_ret();
      adc0_scan_channels();
      input_code = input_scan_state();
      *DAT_000007a4 = input_code;
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
  disp_string(0x76c,0,4,0);
  disp_string(0x778,1,2,0);
  disp_string(0x784,2,2,0);
  disp_offset(*DAT_0000078c,2,7,1);
  disp_string(0x790,3,0,0);
  do {
    do {
    } while (*DAT_000007a0 != '\x01');
    *DAT_000007a0 = '\0';
    input_code = input_scan_state();
    p_input_code = DAT_000007a4;
    *DAT_000007a4 = input_code;
    freq_adjust_sync(*p_input_code);
    run_stop_preset();
    wd_feed();
  } while( true );
}


/* 0x000007A8 —— 简单延时（loops×50 空循环） */
void Delay(int loops)
{
  int count = loops * 50;

  while (count != 0) {
    /* 空编译屏障：保留原固件的软件延时循环，同时避免volatile栈读写改变时序。 */
    __asm volatile ("" ::: "memory");
    count--;
  }
}
