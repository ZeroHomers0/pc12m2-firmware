/* =============================================================================
 * LPC1765FBD100（PC12M-2 十二相晶闸管移相触发板）
 * 反编译源码导出 — 模块 01：IAR 运行时 / 启动 / 系统初始化
 *
 * 工具：Ghidra 反编译（MCP）
 * 固件：backup/pc12m2_orig.bin（NXP LPC1765 / Cortex-M3）
 * 十二相整理：2026-08-31
 * 说明：反编译原样保留；<...> 内为理解注释。
 *
 * 交叉引用：
 *   · 中断/定时器架构 → evidence/reverse/disassembly/functions/
 *   · 启动序列与硬件 → docs/analysis/HARDWARE_VERIFICATION_2026-08-31.md
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
void uart3_init(void);
void auth_verify_loop(void);   /* 0x10A38 认证循环（11_auth.c） */
void auth_set_timeout(void);   /* 0x108D2 认证超时窗口（11_auth.c） */
void auth_challenge(void);     /* 0x108DC 认证挑战（11_auth.c） */
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
void modbus_dispatch(void);
void freq_adjust_sync(int event);
void run_stop_preset(void);

/* ==================== IAR EWARM 运行时（已移除，见文件头） ==================== */



/* ==================== 看门狗 ==================== */

/* 0x000001E4 —— 看门狗超时 ISR：清超时标志 + 计数+1（超时监控用）
 * 12p 超时计数 = 0x1000210c（6p 为 0x244 池槽指向的地址，dump 0x1e4/0x200 均 ref 0x1000210c） */
void WDT_IRQHandler(void)
{
  WDMOD = WDMOD & 0xfb;              /* 清 WDTOF（0x40000000，ldrb/strb） */
  *(volatile int *)0x1000210c += 1;  /* 32 位计数 +1（ldr/str） */
  return;
}


/* 0x00000200 —— 看门狗初始化（timeout_cnt=超时计数值，main 里传 200） */
void wdt_init(uint timeout_cnt)
{
  *(volatile uint *)0x1000210c = 0;    /* 12p 超时计数清零（str，dump 0x200） */
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


/* 0x0000029A —— TIMER0 节拍 ISR（主循环节拍源，12p）：
 *   tick_ready=1（0x10000006，strb；主循环等待它）；phase_cnt++（0x10001fd9，
 *   ldrb/strb，钳位 0xc8=200）；tick_countdown--（0x10001764，字节，若非0） */
void TIMER0_IRQHandler(void)
{
  TIMER0->IR = 0xff;                       /* 清全部中断标志 */
  *(volatile uint8_t *)tick_ready = 1;     /* strb → 0x10000006 */
  *g_phase_cnt = *g_phase_cnt + 1;         /* ldrb/strb → 0x10001fd9 */
  if (0xc8 < *g_phase_cnt) {
    *g_phase_cnt = 0xc8;                   /* 钳位 200 */
  }
  if (0 < *(volatile uint8_t *)tick_countdown) {
    *(volatile uint8_t *)tick_countdown =
      *(volatile uint8_t *)tick_countdown - 1;   /* strb → 0x10001764 */
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
  volatile uint32_t *scb = (volatile uint32_t *)0x400FC000;  /* SCB 基址（DAT_00000554 在 12p globals 为标量） */

  *DAT_00000564 = 0x20;
  do {
  } while ((*DAT_00000564 & 0x40) == 0);
  *DAT_00000558 = 3;
  scb[0x43] = 1;                        /* 0x400FC10C（str.w [r1,#0x10c]） */
  *DAT_0000056c = DAT_00000568;
  pll_feed = DAT_00000570;
  *DAT_00000570 = 0xaa;
  *pll_feed = 0x55;
  *DAT_00000574 = 1;
  *DAT_00000570 = 0xaa;
  scb[0x23] = 0x55;                     /* 0x400FC08C（str.w [r1,#0x8c]） */
  do {
  } while ((*DAT_00000548 & 0x4000000) == 0);
  *DAT_00000574 = 3;
  scb[0x23] = 0xaa;
  *DAT_00000570 = 0x55;
  do {
  } while ((*DAT_00000548 & 0x3000000) == 0);
  *DAT_00000578 = 0x23;
  scb[0x2b] = 0xaa;                     /* 0x400FC0AC（str.w [r1,#0xac]） */
  *DAT_0000057c = 0x55;
  *DAT_00000580 = 1;
  *DAT_0000057c = 0xaa;
  scb[0x2b] = 0x55;
  do {
  } while ((*DAT_00000584 & 0x400) == 0);
  *DAT_00000580 = 3;
  scb[0x2b] = 0xaa;
  *DAT_0000057c = 0x55;
  do {
  } while ((*DAT_00000584 & 0x300) == 0);
  *DAT_00000588 = 0;
  scb[0x6b] = 0;                        /* 0x400FC1AC（str.w [r1,#0x1ac]） */
  *g_pconp = DAT_0000058c;
  *DAT_00000594 = 0;
  *scb = 0x303a;                        /* 0x400FC000（str.w [r1,#0x0]） */
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

/* 0x000005CC —— 主程序（12p；6p 0x5cc 认证流程不同）
 * 启动序列（对应反汇编 0x5CC..0x620 调用序）：
 *   SystemInit → pin_config → gpio1_init → long_delay → timer0_init →
 *   gpio_inputs_dir_init → i2c_gpio_init → adc_init → load_config → pin_config →
 *   gpio2_init → timer1_init → timer2_init → eint1_init → eint3_init → eint2_init →
 *   uart3_init → long_delay → disp_splash_screen → auth_verify_loop → wdt_init(200)
 * 差异（vs 6p 0x5cc）：
 *   · 无 gpio0_input_init / read_input_p02（12p 无对应函数，见 P3 DELETE）
 *   · EINT 使能顺序：eint1 → eint3 → eint2（6p 为 eint1,eint2,eint3）
 *   · 认证改为 auth_verify_loop(0x10A38)：循环调 0x108D2/0x108DC 直至
 *     auth_pass_flag(0x100020C0)==0 通过或重试计数(0x100020C8)>=5；
 *     **0=通过、非 0=未通过**（与 6p 0x10000750 反逻辑）
 *   · tick_ready = 0x10000006（6p 0x100007A0）、input_code = 0x10000007（6p 0x100007A4）
 *
 * 分支：
 *   · auth_pass_flag != 0（认证未通过）→ 显示"报警忙碌"(0x704)+"CPU 忙碌"(0x710) 死循环
 *   · chk_p02_p03()>0（P0.2/P0.3 安全联锁触发）→ 显示"校准模式"屏（0x71C 区）死循环
 *     + freq_adjust_sync + run_stop_preset
 *   · 正常 → 主循环：stub_ret→adc0_scan_channels→input_scan_state→adc0_scan_channels
 *     →state_machine→output_stage→wd_feed→uart3_rx_timeout_monitor→modbus_dispatch(0) */
void main(void)
{
  volatile uint8_t *tick = (volatile uint8_t *)tick_ready;      /* 0x10000006 节拍标志 */
  volatile uint8_t *lock = (volatile uint8_t *)auth_pass_flag; /* 0x100020C0 认证标志(0=通过) */
  volatile uint8_t *inp  = (volatile uint8_t *)0x10000007;     /* input_scan_state 返回值 */
  int interlock;

  SystemInit();                 /* bl 0x440 */
  pin_config();                 /* bl 0xE308 */
  gpio1_init();                 /* bl 0x9E0 */
  long_delay();                 /* bl 0x598 */
  timer0_init();                /* bl 0x248 */
  gpio_inputs_dir_init();       /* bl 0x1528 */
  i2c_gpio_init();              /* bl 0x1BF0 */
  adc_init();                   /* bl 0x1EB4 */
  load_config();                /* bl 0x258C */
  pin_config();                 /* bl 0xE308 */
  gpio2_init();                 /* bl 0x10888 */
  timer1_init();                /* bl 0xE576 */
  timer2_init();                /* bl 0xE598 */
  eint1_init();                 /* bl 0xE5CE */
  eint3_init();                 /* bl 0xE62A */
  eint2_init();                 /* bl 0xE668 */
  uart3_init();                 /* bl 0xA994 */
  long_delay();                 /* bl 0x598 */
  disp_splash_screen();         /* bl 0x40B0 */
  auth_verify_loop();           /* bl 0x10A38 */
  wdt_init(200);                /* movs r0,#0xc8; bl 0x200 */

  if (*lock != 0) {             /* 0x622：认证未通过 → 报警锁机 */
    disp_clear();               /* bl 0x942 */
    disp_string(0x704,0,4,0);   /* "报警忙碌"（第 0 行 col4） */
    disp_string(0x710,2,4,0);   /* "CPU 忙碌"（第 2 行 col4） */
    for (;;) {
      wd_feed();                /* bl 0x238 */
    }
  }

  interlock = chk_p02_p03();    /* bl 0x1B9E */
  if (interlock > 0) {          /* 0x650：联锁触发 → 校准模式屏死循环 */
    disp_clear();
    disp_string(0x71c,0,4,0);   /* "校准模式"（第 0 行 col4） */
    disp_string(0x728,1,2,0);   /* "输出电压"（第 1 行 col2） */
    disp_string(0x734,2,2,0);   /* "参数: "（第 2 行 col2） */
    disp_offset(*out_freq_adj,2,7,1);   /* bl 0x1260：电压值显示 */
    disp_string(0x740,0,3,0);   /* "工作状态"（第 0 行 col3） */
    for (;;) {                  /* 0x698 联锁死循环 */
      if (*tick == 1) {
        *tick = 0;
        *inp = input_scan_state();      /* bl 0x15AE */
        freq_adjust_sync(*inp);         /* bl 0xA856 */
        run_stop_preset();              /* bl 0xF70A */
        wd_feed();
      }
    }
  }

  for (;;) {                    /* 0x6C2 主循环 */
    if (*tick == 1) {
      *tick = 0;
      stub_ret();                       /* bl 0x5CA */
      adc0_scan_channels();             /* bl 0x1F6C */
      *inp = input_scan_state();        /* bl 0x15AE */
      adc0_scan_channels();             /* bl 0x1F6C */
      state_machine(*inp);              /* bl 0x4464 */
      output_stage();                   /* bl 0xE70C */
      wd_feed();                        /* bl 0x238 */
      uart3_rx_timeout_monitor();       /* bl 0xABC0 */
      modbus_dispatch();                /* bl 0xB3B2 */
    }
  }
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
