/* =============================================================================
 * LPC1765FBD100 (PC12M-2 / 12 相晶闸管移相触发板，LPC1765 / Cortex-M3)
 * 反编译源码导出 — 模块 09：输出级（SCR 触发角计算）/ 引脚配置 /
 *                         定时器 / 外部中断（急停/锁存）
 *
 * 12p（PC12M-2）标识：固件 backup/pc12m2_orig.bin（SHA 2bc60868…bd271bd1）
 *   本模块（output_stage 0xE70C / run_stop_preset 0xF70A）为 9 个真实差异
 *   函数之一（六相参考入口为 0xE9AC/0xF9AA），
 *   按 12p 反汇编 dump 逐字节重写（池槽→SRAM 映射经 BIN 直读 + globals.c 双验证）。
 *
 * 关键硬件：
 *   · EINT1/2/3 = P2.11/12/13（PINSEL4 0x4002C000+0x10；三相同步过零 U10/U11/U12）
 *   · P0.22 = 运行继电器 RLY1（fio0_pin22_ctrl 0xE6C6）；P1.22 = 触发/运行指示
 *     （fio1_pin22_ctrl 0xE6A6）
 *   · TIMER1(IRQ2) MR0=999、TIMER2(IRQ3) MR0=999；EINT/TIMER 向量见
 *     memory：仅 EINT2/3 + ADC/TIMER0 使能，TIMER1/2 向量死条目，触发靠轮询。
 *
 * output_stage（主循环每节拍调用）——SCR 移相触发的核心（12p）：
 *   入口守卫 0xE70C：*DAT_0000eb08(0x100015CE)==1 跳过；startup_count(0x10002051)
 *     字节计数 <10 跳过；软起角=0xB4-ulim、软停角=0xB4-llim
 *   保护（cfg_word==1 且 ch3>=10 才进主体）：
 *     th1(0xE7C0)/th2(0xE80E) 都用 **ch5(0x10001590)**（12p 与 6p 差异），
 *     th3(0xE85A) 六级用 ch4(0x10001594)：判据 ×1.5/×2/×2.5/×3/×3.5
 *     且延时计数 ×50/×20/×10/×5/×2/×1，故障位 bit3(0x8)/bit4(0x10)/bit5(0x20)/
 *     bit9(0x200)/bit11(0x800 watchdog)
 *   dispatch（顺序 if 链，非 else-if）：gain_sel==0→path1、==1→path2、==2→path3，
 *     然后无条件落底停机段1（0xF52E）
 *   path1(0xE9CC)/path2(0xEE76)：入口==0 初始化、==4 软起斜坡（trigger_step 累加
 *     角、门 1/2/3 进稳定）、==5 稳定（pid_state 0/1/2 状态机 + ①③④⑤⑥⑦ +
 *     watchdog + closed_loop PID）。path2 阈值用 DAT_0000f470(0x10001640)、
 *     ④⑤⑥⑦ 操作数为 **gain_b**（非 path1 的 gain_a）、PID feedback=aux1。
 *     path1 的 ⑦ 用 ch5<2（非 ch4）。
 *   path3(0xF316)：mode2_target(0x1000200C)=ch3 % 10000（钳 10..1000），
 *     startup_div 除零→input_locked=5，斜坡/稳定输出。
 *   softstop：path1 严格 `<`；path2/path3/ramp `<=`。softstart 全 `>=`。
 *   停机段1(0xF52E)：cfg_word==0 && input_locked!=0 && stop_req==1 →
 *     gpio_outputs_set+清零+继电器断开+复位
 *   停机段2(0xF56C)：cfg_word!=0||input_locked==0 且 ch3>=10 → return
 *   sd2_body(0xF580)→ ramp(0xF5D6) / full_reset_1(0xF5A2)，full_reset_2(0xF6A2)/
 *     full_reset_3(0xF6D6) 同构。
 * run_stop_preset(0xF70A)：cfg_word==1 → out_setpoint=protocol_work_3[0xfa]*100、
 *   input_locked=5；cfg_word==0 → out_setpoint=*protocol_work_3、input_locked=0、
 *   继电器断开。cfg_word 为 **ldrb 字节读**（6p 是 word 读）。
 *
 * 交叉引用：
 *   · 主循环调用序（output_stage bl 0xE70C / run_stop_preset bl 0xF70A）
 *     → src/01_startup.c（main 0x5CC）
 *   · closed_loop_wrapper（PID）→ src/12_closed_loop.c
 *   · nvic_enable_irq → src/13_gpio_init.c:19
 * 导出：2026-08-31（12p 重写；9 个小函数 body 逐字节验证匹配 12p dumps）
 * ========================================================================== */

/* =============================================================================
 * src/09_output_stage.c — 反编译模块 09（输出级 SCR 触发角/引脚/定时器/EINT）
 * 目标B（12p）阶段3 修正：
 *   1) 补 include（types.h/reg.h/globals.h/consts.h）。
 *   2) PTR_DAT_0000e6e8 = FIO 池基址 0x2009C000，globals.c 中为 uint32_t 标量
 *      （非指针），局部 fio 需强转 volatile uint8_t*（+off 字节偏移：+0x18
 *      FIO0SET、+0x1c FIO0CLR、+0x20 FIO1DIR、+0x38 FIO1SET、+0x3c FIO1CLR、
 *      +0x40 FIO2DIR、+0x58 FIO2SET、+0x5c FIO2CLR、+0x80 FIO4DIR、+0x9c FIO4CLR）。
 *   3) udiv_safe：Cortex-M3 UDIV 除零返回 0 不 fault；startup_div/stop_div/
 *      gain_a 可为 0，06 处除法用 udiv_safe 防呆（语义=硬件除零→0）。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"
#include "inc/consts.h"

/* 跨模块前向声明：nvic_enable_irq 定义在 13_gpio_init.c:19；
   closed_loop_wrapper 定义在 12_closed_loop.c（undefined4, 4 参） */
void nvic_enable_irq(uint irq_num);
undefined4 closed_loop_wrapper(undefined4 setpoint,undefined4 feedback,
                                 undefined4 coef_a,undefined4 coef_b);

/* Cortex-M3 UDIV 除零返回 0（不 fault）；startup_div/stop_div/gain_a 可为 0 */
static inline uint32_t udiv_safe(uint32_t n, uint32_t d)
{
  return d ? n / d : 0;
}

/* ==================== 引脚配置 ==================== */

/* 0x0000E308 —— 引脚功能/方向配置（PINSEL+FIO DIR，12p；6p 0xE5A8）
 *   PTR_DAT_0000e6e8 = FIO 池基址 0x2009C000
 *   FIO0DIR(P0.20/21/22、P0.15..19、P0.7/8、P0.4/5)、FIO1/FIO2/FIO3/FIO4 方向
 *   局部：fio = PTR_DAT_0000e6e8（FIO 池基址，puVar+off = +off 字节） */
void pin_config(void)
{
  volatile uint8_t *fio;

  fio = (volatile uint8_t *)DAT_0000e6e8;
  *(volatile uint *)DAT_0000e6e8 = *(volatile uint *)DAT_0000e6e8 | 0x100000;   /* FIO0DIR P0.20 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x200000;                       /* P0.21 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x400000;                       /* P0.22 */
  *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100000;     /* FIO0CLR P0.20 */
  *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x200000;     /* P0.21 */
  *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x400000;     /* P0.22 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x20000;                        /* P0.17 触发组 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x40000;                        /* P0.18 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x80000;                        /* P0.19 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 0x200;        /* FIO2DIR P2.9 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x10000;                        /* P0.16 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x8000;                         /* P0.15 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 0x100;        /* P2.8 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 0x80;         /* P2.7 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 0x40;         /* P2.6 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 0x20;         /* P2.5 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x100;                          /* P0.8 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x80;                           /* P0.7 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x20000;      /* FIO0SET P0.17 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;      /* P0.18 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;      /* P0.19 */
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;        /* FIO2SET P2.9 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;      /* FIO0SET P0.16 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;       /* P0.15 */
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;        /* P2.8 */
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;         /* P2.7 */
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;         /* P2.6 */
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;         /* P2.5 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;        /* FIO0SET P0.8 */
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;         /* P0.7 */
  *(volatile uint *)(fio + 0x20) = *(volatile uint *)(fio + 0x20) | 0x100000;     /* FIO1DIR P1.20 */
  *(volatile uint *)(fio + 0x20) = *(volatile uint *)(fio + 0x20) | 0x200000;     /* P1.21 */
  *(volatile uint *)(fio + 0x20) = *(volatile uint *)(fio + 0x20) | 0x400000;     /* P1.22 */
  *(volatile uint *)(fio + 0x20) = *(volatile uint *)(fio + 0x20) | 0x800000;     /* P1.23 */
  *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x100000;     /* FIO1CLR P1.20 */
  *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x200000;     /* P1.21 */
  *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x400000;     /* P1.22 */
  *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x800000;     /* P1.23 */
  *(volatile uint *)(fio + 0x80) = *DAT_0000e6ec | 0x10000000;              /* FIO4DIR P4.28 */
  *(volatile uint *)(fio + 0x80) = *(volatile uint *)(fio + 0x80) | 0x20000000;   /* P4.29 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x10;                           /* P0.4 */
  *(volatile uint *)fio = *(volatile uint *)fio | 0x20;                           /* P0.5 */
  *(volatile uint *)(fio + 0x40) = *(volatile uint *)(fio + 0x40) | 1;            /* FIO2DIR P2.0 */
  *(volatile uint *)(fio + 0x9c) = *(volatile uint *)(fio + 0x9c) | 0x10000000;   /* FIO4CLR P4.28 */
  *(volatile uint *)(fio + 0x9c) = *(volatile uint *)(fio + 0x9c) | 0x20000000;   /* P4.29 */
  *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x10;         /* FIO0CLR P0.4 */
  *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x20;         /* P0.5 */
  *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 1;            /* FIO2CLR P2.0 */
  return;
}

/* 0x0000E4FA —— 输出使能（关断时所有触发/使能线复位） */
void gpio_outputs_set(void)
{
  volatile uint8_t *fio;

  fio = (volatile uint8_t *)DAT_0000e6e8;
  *(volatile uint *)(DAT_0000e6e8 + 0x18) = *(volatile uint *)(DAT_0000e6e8 + 0x18) | 0x20000;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
  *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
  *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
  return;
}

/* ==================== 定时器 ==================== */

/* 0x0000E576 —— TIMER1 初始化（IRQ2）：TCR 复位、PR=0x18 预分频、MR0=999、
 *   匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移（TIMER 寄存器 4 字节对齐，
 *   与原厂 0xE576 反汇编逐地址一致）：[0]=IR、[1]=TCR、[2]=TC、[3]=PR、[5]=MCR、[6]=MR0。
 *   局部：timer1 = DAT_0000e6f0 → 指向 TIMER1（0x40008000） */
void timer1_init(void)
{
  volatile uint32_t *timer1;

  timer1 = DAT_0000e6f0;                              /* TIMER1 0x40008000 */
  DAT_0000e6f0[1] = 2;                                /* TCR=2：TC 复位（原厂 0xE57C） */
  timer1[3] = 0x18;                                   /* PR=0x18：预分频（原厂 0xE580） */
  timer1[6] = 999;                                    /* MR0=999 匹配值（原厂 0xE586） */
  *timer1 = 0xff;                                     /* IR 清中断（原厂 0xE58A） */
  timer1[5] = 3;                                      /* MCR=3：MR0 匹配中断+复位（原厂 0xE58E） */
  nvic_enable_irq(2);                                 /* NVIC TIMER1_IRQ */
  return;
}

/* 0x0000E598 —— TIMER2 初始化（IRQ3）：PCONP TMR2、TCR 复位、PR=0x18 预分频、
 *   MR0=999、匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移。
 *   局部：timer2 = DAT_0000e6fc → 指向 TIMER2（0x40090000） */
void timer2_init(void)
{
  volatile uint32_t *timer2;

  *(volatile uint *)(DAT_0000e6f8 + 0xc4) = *g_pconp | 0x400000;   /* PCONP bit22 TMR2 */
  timer2 = DAT_0000e6fc;                              /* TIMER2 0x40090000 */
  DAT_0000e6fc[1] = 2;                                /* TCR=2：TC 复位（原厂 0xE5AE） */
  timer2[3] = 0x18;                                   /* PR=0x18：预分频（原厂 0xE5B4） */
  timer2[6] = 999;                                    /* MR0=999 匹配值（原厂 0xE5BA） */
  *timer2 = 0xff;                                     /* IR 清中断（原厂 0xE5C0） */
  timer2[5] = 3;                                      /* MCR=3：MR0 匹配中断+复位（原厂 0xE5C6） */
  nvic_enable_irq(3);                                 /* NVIC TIMER2_IRQ（反汇编 0xE5C8 核 r0=3） */
  return;
}

/* ==================== 外部中断 ==================== */

/* 0x0000E5CE —— EINT1 外部中断初始化（P2.11，边沿触发，清挂起，NVIC 0x13=19）
 *   局部：base = 先 PINSEL(0x4002C000,+0x10 PINSEL4) 后 SCB(0x400FC000,EXTINT/EXTMODE/EXTPOLAR) 复用基址 */
void eint1_init(void)
{
  int base;

  *(volatile uint *)(DAT_0000e6f8 + 0x140) = *DAT_0000e700 | 2;      /* EXTINT 清 EINT1 */
  base = DAT_0000e704;                                     /* PINSEL 0x4002C000 */
  *(volatile uint *)(DAT_0000e704 + 0x10) = *(volatile uint *)(DAT_0000e704 + 0x10) | 0x400000;  /* PINSEL4 P2.11=EINT1（bit22=1、bit23=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xff7fffff;
  base = DAT_0000e6f8;                                     /* SCB 0x400FC000 */
  *(volatile uint *)(DAT_0000e6f8 + 0x148) = *(volatile uint *)(DAT_0000e6f8 + 0x148) | 2;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffffd;          /* EXTPOLAR 下降沿 */
  *(volatile uint *)(base + 0x140) = *(volatile uint *)(base + 0x140) | 2;  /* EXTINT 清 */
  nvic_enable_irq(0x13);                                    /* NVIC EINT1_IRQ=19 */
  return;
}

/* 0x0000E62A —— EINT2 外部中断初始化（P2.12，边沿触发，NVIC 0x14=20）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint2_init(void)
{
  int base;

  base = DAT_0000e704;
  *(volatile uint *)(DAT_0000e704 + 0x10) = *(volatile uint *)(DAT_0000e704 + 0x10) | 0x1000000; /* PINSEL4 P2.12=EINT2（bit24=1、bit25=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xfdffffff;
  base = DAT_0000e6f8;
  *(volatile uint *)(DAT_0000e6f8 + 0x148) = *DAT_0000e708 | 4;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffffb;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x14);                                    /* NVIC EINT2_IRQ=20 */
  return;
}

/* 0x0000E668 —— EINT3 外部中断初始化（P2.13，边沿触发，NVIC 0x15=21）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint3_init(void)
{
  int base;

  base = DAT_0000e704;
  *(volatile uint *)(DAT_0000e704 + 0x10) = *(volatile uint *)(DAT_0000e704 + 0x10) | 0x4000000; /* PINSEL4 P2.13=EINT3（bit26=1、bit27=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xf7ffffff;
  base = DAT_0000e6f8;
  *(volatile uint *)(DAT_0000e6f8 + 0x148) = *DAT_0000e708 | 8;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffff7;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x15);                                    /* NVIC EINT3_IRQ=21 */
  return;
}

/* ==================== 引脚电平控制 ==================== */

/* 0x0000E6A6 —— P1.22 电平控制（level>=1 置位、<1 清零；触发/运行指示 LED）
 *   level>=1 → FIO1SET；否则 FIO1CLR */
void fio1_pin22_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_0000e6e8 + 0x3c) = *(volatile uint *)(DAT_0000e6e8 + 0x3c) | 0x400000;
  }
  else {
    *(volatile uint *)(DAT_0000e6e8 + 0x38) = *(volatile uint *)(DAT_0000e6e8 + 0x38) | 0x400000;
  }
  return;
}

/* 0x0000E6C6 —— P0.22 电平控制（level>=1 置位、<1 清零；运行继电器 RLY1）
 *   level>=1 → FIO0SET；否则 FIO0CLR */
void fio0_pin22_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_0000e6e8 + 0x1c) = *(volatile uint *)(DAT_0000e6e8 + 0x1c) | 0x400000;
  }
  else {
    *(volatile uint *)(DAT_0000e6e8 + 0x18) = *(volatile uint *)(DAT_0000e6e8 + 0x18) | 0x400000;
  }
  return;
}

/* ==================== 输出级主处理 ==================== */

/* 0x0000E70C —— 输出级主处理（SCR 移相触发角计算 + 保护 + 软起停 + PID）
 *   详见文件头说明。每节拍调用（main bl 0xE70C）。
 *   注意：cfg_word/gain_sel/alarm_flag/startup_count/startup_state/ramp_phase/
 *   ramp_state/menu_flag_5/eeprom_param_3/stop_req/stop_div/pid_kp2/pid_ki2
 *   均为 **ldrb/strb 字节访问**（globals.h 为 uint32_t*，需强转 (volatile uint8_t*)）；
 *   DAT_0000eb50/eb60/eb70（th 延时系数）与 startup_div 已是 volatile uint8_t*。
 *   udiv_safe：startup_div/stop_div/gain_a 可能为 0（硬件 UDIV 除零=0）。 */
void output_stage(void)
{
  /* ---- 入口守卫 0xE70C ---- */
  if (*DAT_0000eb08 == 1) {                 /* 运行状态 0x100015CE==1 → 跳过本拍 */
    return;
  }
  *(volatile uint8_t *)startup_count += 1;  /* 0x10002051 字节计数 */
  if (*(volatile uint8_t *)startup_count < 10) {
    return;                                  /* 每 10 拍执行一次 */
  }
  *(volatile uint8_t *)startup_count = 0;
  *softstart_angle = 0xb4 - *ulim_angle;    /* 软起角 = 180° - 上限角 */
  if (*softstart_angle == 0) {
    *softstart_angle = 1;
  }
  *softstop_angle = 0xb4 - *llim_angle;     /* 软停角 = 180° - 下限角 */

  /* ---- cfg_word==0 参数块 0xE74E ---- */
  if (*(volatile uint8_t *)cfg_word == 0) {
    *(volatile uint8_t *)alarm_flag = 0;    /* 报警标志清零 */
    if (*(volatile uint8_t *)gain_sel == 0) {
      *DAT_0000eb30 = *gain_a / 0xf;        /* 增益档 0：上限=增益A/15 */
    }
    if (*(volatile uint8_t *)gain_sel == 1) {
      *DAT_0000eb30 = *gain_b / 0xf;        /* 增益档 1：上限=增益B/15 */
    }
    if (*(volatile uint8_t *)gain_sel == 2) {
      *mode2_scale = udiv_safe(*gain_c * 1000, *gain_a);  /* 模式2 比例=增益C×1000/增益A */
      *DAT_0000eb30 = *gain_a / 0xf;
    }
  }

  /* ---- 保护门 0xE7B0：cfg_word==1 且 ch3>=10 才进保护/触发主体 ---- */
  if (*(volatile uint8_t *)cfg_word == 1 && *adc_conv_ch3 >= 10) {
    /* ---- th1 过压 0xE7C0（判据 ch5>th1 阈值，延时 ov_count_1>50×th1 系数）---- */
    if (*DAT_0000eb44 != 0 && *adc_conv_ch5 > *DAT_0000eb44) {
      *ov_count_1 += 1;
      if (*ov_count_1 > 50 * *DAT_0000eb50) {
        *ov_count_1 = 0;
        *out_param |= 0x10;                 /* bit4 过压 */
      }
    } else {
      *ov_count_1 = 0;
    }
    /* ---- th2 过流 0xE80E（判据 ch5<th2 阈值，延时 ov_count_2>50×th2 系数）---- */
    if (*DAT_0000eb58 != 0 && *adc_conv_ch5 < *DAT_0000eb58) {
      *ov_count_2 += 1;
      if (*ov_count_2 > 50 * *DAT_0000eb60) {
        *ov_count_2 = 0;
        *out_param |= 0x20;                 /* bit5 过流 */
      }
    } else {
      *ov_count_2 = 0;
    }
    /* ---- th3 缺相 0xE85A（判据 ch4，六级：×1.5/×2/×2.5/×3/×3.5）---- */
    if (*DAT_0000eb64 != 0) {
      *ov_count_3 += 1;
      if (*adc_conv_ch4 < *DAT_0000eb64) {
        *ov_count_3 = 0;
      }
      if (*adc_conv_ch4 >= *DAT_0000eb64 && *ov_count_3 > 50 * *DAT_0000eb70) {
        *out_param |= 0x8;                  /* bit3 缺相（×1.0，延时 50） */
      }
      if (15 * *DAT_0000eb64 / 10 < *adc_conv_ch4 && *ov_count_3 > 20 * *DAT_0000eb70) {
        *out_param |= 0x8;                  /* ×1.5，延时 20 */
      }
      if (2 * *DAT_0000eb64 < *adc_conv_ch4 && *ov_count_3 > 10 * *DAT_0000eb70) {
        *out_param |= 0x8;                  /* ×2，延时 10 */
      }
      if (25 * *DAT_0000eb64 / 10 < *adc_conv_ch4 && *ov_count_3 > 5 * *DAT_0000eb70) {
        *out_param |= 0x200;                /* bit9 缺相严重（×2.5，延时 5） */
      }
      if (3 * *DAT_0000eb64 < *adc_conv_ch4 && *ov_count_3 > 2 * *DAT_0000eb70) {
        *out_param |= 0x200;                /* ×3，延时 2 */
      }
      if (35 * *DAT_0000eb64 / 10 < *adc_conv_ch4 && *ov_count_3 > 1) {
        *out_param |= 0x200;                /* ×3.5，延时 1 */
      }
    }
    /* ---- alarm_timer 0xE9A2：1500 拍超时置报警 ---- */
    *alarm_timer += 1;
    if (*alarm_timer > 0x5dc) {
      *alarm_timer = 0;
      *(volatile uint8_t *)alarm_flag = 1;
    }

    /* ---- dispatch 0xE9C4（顺序 if 链，非 else-if）---- */

    /* ============ path1（增益档 0）0xE9CC ============ */
    if (*(volatile uint8_t *)gain_sel == 0) {
      /* path1 入口 0xE9CC */
      *DAT_0000eb7c = *pid_target_set;      /* 目标暂存 */
      *pid_active = 0;
      if (*input_locked == 0) {             /* 首次进入：初始化状态机 */
        *input_locked = 4;
        *out_setpoint = 0;
        *DAT_00011164 = *out_setpoint;      /* OLD 0xE9F2 写累加器 0x100020F8 */
        *pid_err = 0;
        *pid_err_prev = 0;
        *pid_watchdog = 0;
        *(volatile uint8_t *)startup_state = 0;
        *startup_timer = 0x1771;
      }
      /* path1 ==4 软起斜坡 0xEA0E */
      if (*input_locked == 4) {
        *DAT_0000eb7c = *adc_conv_ch3;      /* 目标=当前 ch3 */
        *trigger_step = udiv_safe((TRIG_PERIOD - *softstart_angle * ANGLE_SCALE / 100) / 50,
                                  *startup_div);
        *trigger_accum += *trigger_step;    /* 相位累加 */
        if (*(volatile uint8_t *)ramp_phase == 0) {
          *trigger_angle = *softstart_angle * ANGLE_SCALE / 100 + *trigger_accum;
          *(volatile uint8_t *)ramp_state = 1;
        }
        if (*(volatile uint8_t *)ramp_phase == 1) {
          *trigger_angle = *trigger_accum;
          *(volatile uint8_t *)ramp_state = 0;
        }
        *out_setpoint = *trigger_angle * 100;
        *DAT_0000ebbc = *out_setpoint;      /* 回显槽 */
        *DAT_00011164 = *out_setpoint;      /* OLD 0xE9F2 写累加器 0x100020F8 */
        *pid_err = 0;
        *pid_err_prev = 0;
        /* 门1：目标角换算 > 触发角 → 进稳定 */
        if (*DAT_0000eb7c * 0x2f8 / 100 + 0xedd < *trigger_angle) {
          *input_locked = 5;
        }
        /* 门2：ch4/ch5 增益条件 */
        if (*adc_conv_ch4 >= *gain_c && *gain_a >= *gain_c) {
          *input_locked = 5;
        }
        /* 门3：aux2 < fb */
        if (*adc_conv_aux2 < *adc_conv_fb) {
          *input_locked = 5;
        }
      }
      /* path1 ==5 稳定 + PID 0xEBD0 */
      if (*input_locked == 5) {
        *(volatile uint8_t *)ramp_state = 0;
        /* ① pid_state 触发 0xEBD8 */
        if (*adc_conv_ch4 >= *gain_c && *adc_conv_ch5 < *pid_target_set) {
          *pid_state = 1;
          *DAT_0000efe4 = 0;
        } else {
          *DAT_0000efe4 += 1;
          if (*DAT_0000efe4 > 50) {
            *DAT_0000efe4 = 0;
            *pid_state = 2;
            *adc_conv_fb = *adc_conv_fb;    /* 原固件自写回（保持忠实） */
            *(volatile uint8_t *)startup_state = 0;
          }
        }
        /* ③ pid_feedback 超时 0xEC1A */
        if (*pid_target_set >= *startup_timer) {
          *pid_feedback = 0;
        } else {
          *pid_feedback += 1;
          if (*pid_feedback > 10) {
            *pid_feedback = 0;
            *pid_state = 0;
            *adc_conv_fb = *adc_conv_fb;
            *(volatile uint8_t *)startup_state = 0;
          }
        }
        /* ④ 目标爬升 0xEC6A */
        if (*pid_target_set >= *startup_timer && *gain_a >= *gain_c && *pid_state == 2) {
          *pid_target_set = *startup_timer;
          *startup_timer += 1;
          if (*startup_timer > 0x1770) {
            *startup_timer = 0x1770;
          }
          *adc_conv_fb = *adc_conv_fb;
          *(volatile uint8_t *)startup_state = 0;
        }
        /* ⑤⑥⑦ 闭环动作（ACTION_path1） */
        if (*gain_a >= *gain_c && *pid_state == 1 && *gain_b < *gain_a) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_fb;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *gain_c;
          *adc_conv_fb = *adc_conv_aux1;
        }
        if (*pid_target_set >= *adc_conv_fb && *gain_a >= *gain_c &&
            *pid_state == 1 && *gain_b < *gain_a) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_fb;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *gain_c;
          *adc_conv_fb = *adc_conv_aux1;
        }
        if (*adc_conv_ch5 < 2 && *gain_a >= *gain_c && *pid_state == 1) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_fb;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *gain_c;
          *adc_conv_fb = *adc_conv_aux1;
        }
        /* watchdog 0xED7E */
        if (*DAT_0000f008 < 5 && *(volatile uint8_t *)alarm_flag == 1 &&
            *(volatile uint8_t *)eeprom_param_3 == 1) {
          *pid_watchdog += 1;
          if (*pid_watchdog > 0x64) {
            *out_param |= 0x800;            /* bit11 闭环看门狗 */
          }
        } else {
          *pid_watchdog = 0;
        }
        /* PID 0xEDDE */
        *out_setpoint = closed_loop_wrapper(*pid_target_set, *adc_conv_fb,
                                            *(volatile uint8_t *)pid_kp2,
                                            *(volatile uint8_t *)pid_ki2);
      }
      /* path1 softstop（严格 <）0xEE2A / softstart（>=）0xEE50 */
      if (*softstop_angle * ANGLE_SCALE < *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstop_angle * ANGLE_SCALE;
      }
      if (*softstart_angle * ANGLE_SCALE >= *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstart_angle * ANGLE_SCALE;
      }
    }

    /* ============ path2（增益档 1）0xEE76 ============ */
    if (*(volatile uint8_t *)gain_sel == 1) {
      *DAT_0000eb7c = *pid_target_set;
      *pid_active = 0;
      if (*input_locked == 0) {
        *input_locked = 4;
        *out_setpoint = 0;
        *DAT_00011164 = *out_setpoint;      /* OLD 0xE9F2 写累加器 0x100020F8 */
        *pid_err = 0;
        *pid_err_prev = 0;
        *pid_watchdog = 0;
        *(volatile uint8_t *)startup_state = 0;
        *startup_timer = 0x1771;
      }
      /* path2 ==4 软起斜坡 0xEEC0 */
      if (*input_locked == 4) {
        *DAT_0000eb7c = *adc_conv_ch3;
        *trigger_step = udiv_safe((TRIG_PERIOD - *softstart_angle * ANGLE_SCALE / 100) / 50,
                                  *startup_div);
        *trigger_accum += *trigger_step;
        if (*(volatile uint8_t *)ramp_phase == 0) {
          *trigger_angle = *softstart_angle * ANGLE_SCALE / 100 + *trigger_accum;
          *(volatile uint8_t *)ramp_state = 1;
        }
        if (*(volatile uint8_t *)ramp_phase == 1) {
          *trigger_angle = *trigger_accum;
          *(volatile uint8_t *)ramp_state = 0;
        }
        *out_setpoint = *trigger_angle * 100;
        *DAT_0000ebbc = *out_setpoint;
        *DAT_00011164 = *out_setpoint;      /* OLD 0xE9F2 写累加器 0x100020F8 */
        *pid_err = 0;
        *pid_err_prev = 0;
        /* 门1 */
        if (*DAT_0000eb7c * 0x2f8 / 100 + 0xedd < *trigger_angle) {
          *input_locked = 5;
        }
        /* 门2（path2 阈值 DAT_0000f470，操作数 gain_b） */
        if (*adc_conv_ch5 >= *DAT_0000f470 && *gain_b >= *DAT_0000f470) {
          *input_locked = 5;
        }
        /* 门3：aux2 < aux1 */
        if (*adc_conv_aux2 < *adc_conv_aux1) {
          *input_locked = 5;
        }
      }
      /* path2 ==5 稳定 + PID 0xEFBC/0xF06C */
      if (*input_locked == 5) {
        *(volatile uint8_t *)ramp_state = 0;
        /* ① pid_state 触发 0xF06E（判据 ch5>=阈值 && ch4<目标） */
        if (*adc_conv_ch5 >= *DAT_0000f470 && *adc_conv_ch4 < *pid_target_set) {
          *pid_state = 1;
          *DAT_0000efe4 = 0;
        } else {
          *DAT_0000efe4 += 1;
          if (*DAT_0000efe4 > 50) {
            *DAT_0000efe4 = 0;
            *pid_state = 2;
            *adc_conv_aux1 = *adc_conv_aux1;
            *(volatile uint8_t *)startup_state = 0;
          }
        }
        /* ③ pid_feedback 超时 0xF0C6 */
        if (*pid_target_set >= *startup_timer) {
          *pid_feedback = 0;
        } else {
          *pid_feedback += 1;
          if (*pid_feedback > 10) {
            *pid_feedback = 0;
            *pid_state = 0;
            *adc_conv_aux1 = *adc_conv_aux1;
            *(volatile uint8_t *)startup_state = 0;
          }
        }
        /* ④ 目标爬升 0xF102（操作数 gain_b） */
        if (*pid_target_set >= *startup_timer && *gain_b >= *DAT_0000f470 &&
            *pid_state == 2) {
          *pid_target_set = *startup_timer;
          *startup_timer += 1;
          if (*startup_timer > 0x1770) {
            *startup_timer = 0x1770;
          }
          *adc_conv_aux1 = *adc_conv_aux1;
          *(volatile uint8_t *)startup_state = 0;
        }
        /* ⑤⑥⑦ 闭环动作（ACTION_path2：pid_target_set=阈值、aux1=fb） */
        if (*gain_b >= *DAT_0000f470 && *pid_state == 1 && *gain_a < *gain_b) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_aux1;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *DAT_0000f470;
          *adc_conv_aux1 = *adc_conv_fb;
        }
        if (*pid_target_set >= *adc_conv_aux1 && *gain_b >= *DAT_0000f470 &&
            *pid_state == 1 && *gain_a < *gain_b) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_aux1;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *DAT_0000f470;
          *adc_conv_aux1 = *adc_conv_fb;
        }
        if (*adc_conv_ch4 < 2 && *gain_b >= *DAT_0000f470 && *pid_state == 1) {
          *pid_active = 1;
          if (*(volatile uint8_t *)startup_state != 1) {
            *(volatile uint8_t *)startup_state = 1;
            *startup_timer = *adc_conv_aux1;
          }
          if (*startup_timer < 2) {
            *startup_timer = 1;
          }
          *pid_target_set = *DAT_0000f470;
          *adc_conv_aux1 = *adc_conv_fb;
        }
        /* watchdog 0xF256（源 DAT_0000f4a4=0x100015AC） */
        if (*DAT_0000f4a4 < 5 && *(volatile uint8_t *)alarm_flag == 1 &&
            *(volatile uint8_t *)eeprom_param_3 == 1) {
          *pid_watchdog += 1;
          if (*pid_watchdog > 0x64) {
            *out_param |= 0x800;
          }
        } else {
          *pid_watchdog = 0;
        }
        /* PID 0xF2AE（feedback=aux1） */
        *out_setpoint = closed_loop_wrapper(*pid_target_set, *adc_conv_aux1,
                                            *(volatile uint8_t *)pid_kp2,
                                            *(volatile uint8_t *)pid_ki2);
      }
      /* path2 softstop（<=）0xF2C2 / softstart（>=）0xF2E8 */
      if (*softstop_angle * ANGLE_SCALE <= *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstop_angle * ANGLE_SCALE;
      }
      if (*softstart_angle * ANGLE_SCALE >= *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstart_angle * ANGLE_SCALE;
      }
    }

    /* ============ path3（增益档 2）0xF316 ============ */
    if (*(volatile uint8_t *)gain_sel == 2) {
      *mode2_target = *adc_conv_ch3;        /* 模式2 目标=ch3 */
      *DAT_0000eb7c = *mode2_target % 10000;
      if (*DAT_0000eb7c > 1000) {
        *DAT_0000eb7c = 1000;               /* 钳位上限 1000 */
      }
      if (*DAT_0000eb7c < 10) {
        *DAT_0000eb7c = 10;                 /* 钳位下限 10 */
      }
      if (*input_locked == 0) {
        *input_locked = 4;
      }
      /* path3 ==4 软起斜坡 0xF35C */
      if (*input_locked == 4) {
        if (*startup_div == 0) {
          *input_locked = 5;                /* startup_div 除零 → 直接稳定 */
        }
        *trigger_step = udiv_safe((TRIG_PERIOD - *softstart_angle * ANGLE_SCALE / 100) / 50,
                                  *startup_div);
        *trigger_accum += *trigger_step;
        if (*(volatile uint8_t *)ramp_phase == 0) {
          *trigger_angle = *softstart_angle * ANGLE_SCALE / 100 + *trigger_accum;
          *(volatile uint8_t *)ramp_state = 1;
        }
        if (*(volatile uint8_t *)ramp_phase == 1) {
          *trigger_angle = *trigger_accum;
          *(volatile uint8_t *)ramp_state = 0;
        }
        *out_setpoint = *trigger_angle * 100;
        if (*out_setpoint < 10) {
          *out_setpoint = 10;
        }
        /* 门1 */
        if (*DAT_0000eb7c * 0x2f8 / 100 + 0xedd < *trigger_angle) {
          *input_locked = 5;
        }
      }
      /* path3 ==5 稳定 0xF41E */
      if (*input_locked == 5) {
        *(volatile uint8_t *)ramp_state = 0;
        *trigger_angle = *DAT_0000eb7c * 0x2f8 / 100 + 0xedd;
        *out_setpoint = *trigger_angle * 100;
      }
      /* path3 softstop（<=）0xF450 / softstart（>=）0xF508 */
      if (*softstop_angle * ANGLE_SCALE <= *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstop_angle * ANGLE_SCALE;
      }
      if (*softstart_angle * ANGLE_SCALE >= *out_setpoint &&
          *(volatile uint8_t *)menu_flag_5 == 0) {
        *out_setpoint = *softstart_angle * ANGLE_SCALE;
      }
    }
  } else {
    /* 保护门失败 0xE8B2 → 0xF52E 停机段1（空 else，落底执行停机段1） */
  }

  /* ---- 停机段1 0xF52E（dispatch 顺序 if 链后无条件落底） ---- */
  if (*(volatile uint8_t *)cfg_word == 0 && *input_locked != 0 &&
      *(volatile uint8_t *)stop_req == 1) {
    gpio_outputs_set();                     /* bl 0xE4FA */
    *trigger_step = 0;
    *trigger_accum = 0;
    *trigger_angle = 0;
    *out_setpoint = 0;
    fio0_pin22_ctrl(0);                     /* bl 0xE6C6：运行继电器断开 */
    fio1_pin22_ctrl(0);                     /* bl 0xE6A6 */
    *input_locked = 0;
    *(volatile uint8_t *)ramp_phase = 0;
  }

  /* ---- 停机段2 门 0xF56C：cfg_word!=0 或 input_locked==0 且 ch3>=10 → 返回 ---- */
  if (*(volatile uint8_t *)cfg_word != 0 || *input_locked == 0) {
    if (*adc_conv_ch3 >= 10) {
      return;
    }
  }

  /* ---- sd2_body 0xF580 ---- */
  if (*input_locked == 5) {
    *input_locked = 4;
    *trigger_accum = *out_setpoint / 100;
  }
  if (*(volatile uint8_t *)stop_div == 0) {
    /* ---- full_reset_1 0xF5A2 ---- */
    gpio_outputs_set();
    *trigger_step = 0;
    *trigger_accum = 0;
    *trigger_angle = 0;
    *out_setpoint = 0;
    if (*(volatile uint8_t *)cfg_word == 0) {
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
    }
    *input_locked = 0;
    *(volatile uint8_t *)ramp_phase = 0;
    return;
  }

  /* ---- ramp 停机斜坡 0xF5D6 ---- */
  *trigger_step = udiv_safe((TRIG_PERIOD - *softstart_angle * ANGLE_SCALE / 100) / 50,
                            *(volatile uint8_t *)stop_div);
  if (*trigger_accum > *trigger_step) {
    *input_locked = 4;
    if (*(volatile uint8_t *)ramp_state == 0) {
      *(volatile uint8_t *)ramp_phase = 1;
    }
    *trigger_accum -= *trigger_step;
    if (*(volatile uint8_t *)ramp_state == 1) {
      *trigger_angle = *softstart_angle * ANGLE_SCALE / 100 + *trigger_accum;
    }
    if (*(volatile uint8_t *)ramp_state == 0) {
      *trigger_angle = *trigger_accum;
    }
    *out_setpoint = *trigger_angle * 100;
    if (*softstop_angle * ANGLE_SCALE <= *out_setpoint &&
        *(volatile uint8_t *)menu_flag_5 == 0) {
      *out_setpoint = *softstop_angle * ANGLE_SCALE;
    }
    if (*softstart_angle * ANGLE_SCALE >= *out_setpoint &&
        *(volatile uint8_t *)menu_flag_5 == 0) {
      /* ---- full_reset_2 0xF6A2 ---- */
      gpio_outputs_set();
      *trigger_step = 0;
      *trigger_accum = 0;
      *trigger_angle = 0;
      *out_setpoint = 0;
      if (*(volatile uint8_t *)cfg_word == 0) {
        fio0_pin22_ctrl(0);
        fio1_pin22_ctrl(0);
      }
      *input_locked = 0;
      *(volatile uint8_t *)ramp_phase = 0;
      return;
    }
    return;
  }

  /* ---- full_reset_3 0xF6D6 ---- */
  gpio_outputs_set();
  *trigger_step = 0;
  *trigger_accum = 0;
  *trigger_angle = 0;
  *out_setpoint = 0;
  if (*(volatile uint8_t *)cfg_word == 0) {
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
  }
  *input_locked = 0;
  *(volatile uint8_t *)ramp_phase = 0;
  return;
}

/* ==================== 运行/停止预置 ==================== */

/* 0x0000F70A —— 运行/停止预置（12p；6p 0xF9AA）
 *   由 main 校准模式死循环每节拍调用（01_startup.c bl 0xF70A）。
 *   cfg_word 为 **ldrb 字节读**（6p 是 word 读）：
 *     cfg_word==1（运行）→ out_setpoint=protocol_work_3[0xfa]*100（[0x3e8]=250*4），
 *       input_locked=5（稳定）
 *     cfg_word==0（停止）→ out_setpoint=*protocol_work_3，input_locked=0，
 *       运行继电器/指示断开（fio0/1_pin22_ctrl(0)）
 *   protocol_work_3=0x100017D8（uint32_t*，[0xfa] 元素经 ldr.w [r0,#0x3e8]） */
void run_stop_preset(void)
{
  if (*(volatile uint8_t *)cfg_word == 1) {
    *out_setpoint = protocol_work_3[0xfa] * 100;
    *input_locked = 5;
  }
  if (*(volatile uint8_t *)cfg_word == 0) {
    *out_setpoint = *protocol_work_3;
    *input_locked = 0;
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
  }
  return;
}

/* 0x0000F748 —— EINT1 ISR：清 EXTINT bit1，置 input_pending=2（正转请求），eint1_flag=1 */
void EINT1_IRQHandler(void)
{
  *(volatile uint *)(0x400FC140) = *(volatile uint *)0x400FC140 | 2;   /* EXTINT 清 EINT1 */
  if (*(volatile uint8_t *)input_pending == '\0') {
    *(volatile uint8_t *)input_pending = 2;
  }
  *(volatile uint8_t *)eint1_flag = 1;
  return;
}

/* 0x0000F76A —— EINT2 ISR：清 EXTINT bit2，置 input_pending=1（反转请求），eint2_flag=1 */
void EINT2_IRQHandler(void)
{
  *(volatile uint *)(0x400FC140) = *(volatile uint *)0x400FC140 | 4;   /* EXTINT 清 EINT2 */
  if (*(volatile uint8_t *)input_pending == '\0') {
    *(volatile uint8_t *)input_pending = 1;
  }
  *(volatile uint8_t *)eint2_flag = 1;
  return;
}

/* 0x0000F78C —— EINT3 ISR：相位窗口频率锁定 + 停机保护 + 输出预置
 *   输入锁存：input_locked==0 时 input_pending（1=正转/2=反转）→ mode_byte；
 *   消抖计数 debounce_count(0x10001fda) 每中断 +1，>=10 处理：
 *     phase_cnt 落 0x61-0x67 → freq_hz='2'(50Hz)；落 0x52-0x55 → freq_hz='<'(60Hz)；
 *     相位不在窗口且保持计数 hold_count(0x1000209c)>=5 → 停机复位。
 *   输出预置：input_locked∈2..7 且 out_param==0 且 freq_hz>=0x32 时按 50/60Hz
 *     计算 out_scale(0x100020a4)/out_div(0x10002034)/MR0（TIMER2=0x40090000+0x18），
 *     否则 gpio_outputs_set() 复位输出。
 *   （12p 与 6p 差异：无 out_phase 单相/三相分支；MR0 公式无 out_fine 项；
 *     60Hz scale=setpoint*75/100/100 而非 6p 的 *0x50；clamp 0x2725/0x21fc） */
void EINT3_IRQHandler(void)
{
  *(volatile uint *)(0x400FC140) = *(volatile uint *)0x400FC140 | 8;   /* EXTINT 清 EINT3 */
  if (*(volatile uint32_t *)input_locked == 0) {
    if (*(volatile uint8_t *)input_pending == 1) {
      *(volatile uint8_t *)mode_byte = 1;
    }
    if (*(volatile uint8_t *)input_pending == 2) {
      *(volatile uint8_t *)mode_byte = 2;
    }
    *(volatile uint8_t *)input_pending = 0;
  }
  *(volatile uint8_t *)eint3_flag = 1;
  *(volatile uint8_t *)debounce_count = *(volatile uint8_t *)debounce_count + 1;
  if (*(volatile uint8_t *)debounce_count >= 10) {
    *(volatile uint8_t *)debounce_count = 0;
    /* 相位窗口 → 频率锁定 */
    if ((0x60 < *(volatile uint8_t *)phase_cnt) && (*(volatile uint8_t *)phase_cnt < 0x68)) {
      *(volatile uint8_t *)freq_hz = 0x32;               /* '2' = 50Hz 档 */
      *(volatile uint32_t *)hold_count = 0;
    }
    if ((0x51 < *(volatile uint8_t *)phase_cnt) && (*(volatile uint8_t *)phase_cnt < 0x56)) {
      *(volatile uint8_t *)freq_hz = 0x3c;               /* '<' = 60Hz 档 */
      *(volatile uint32_t *)hold_count = 0;
    }
    /* 50Hz 相位保持超时 → 停机复位 */
    if (((*(volatile uint8_t *)phase_cnt < 0x61) || (*(volatile uint8_t *)phase_cnt > 0x67)) &&
        (*(volatile uint8_t *)freq_hz == 0x32)) {
      *(volatile uint32_t *)hold_count = *(volatile uint32_t *)hold_count + 1;
      if (*(volatile uint32_t *)hold_count >= 5) {
        /* —— 停机复位（hold_count 超时）—— */
        *(volatile uint32_t *)hold_count = 0;
        *(volatile uint8_t *)freq_hz = 0;
        *(volatile uint8_t *)cfg_word = 0;
        gpio_outputs_set();
        *(volatile uint32_t *)trigger_step = 0;
        *(volatile uint32_t *)trigger_accum = 0;
        *(volatile uint32_t *)trigger_angle = 0;
        *(volatile uint32_t *)out_setpoint = 0;
        fio1_pin22_ctrl(0);
        fio0_pin22_ctrl(0);
        *(volatile uint32_t *)input_locked = 0;
        *(volatile uint8_t *)ramp_phase = 0;
        *(volatile uint32_t *)out_param = *(volatile uint32_t *)out_param | 0x2000;
      }
    }
    /* 60Hz 相位保持超时 → 停机复位 */
    if (((*(volatile uint8_t *)phase_cnt < 0x52) || (*(volatile uint8_t *)phase_cnt > 0x55)) &&
        (*(volatile uint8_t *)freq_hz == 0x3c)) {
      *(volatile uint32_t *)hold_count = *(volatile uint32_t *)hold_count + 1;
      if (*(volatile uint32_t *)hold_count >= 5) {
        /* —— 停机复位（hold_count 超时）—— */
        *(volatile uint32_t *)hold_count = 0;
        *(volatile uint8_t *)freq_hz = 0;
        *(volatile uint8_t *)cfg_word = 0;
        gpio_outputs_set();
        *(volatile uint32_t *)trigger_step = 0;
        *(volatile uint32_t *)trigger_accum = 0;
        *(volatile uint32_t *)trigger_angle = 0;
        *(volatile uint32_t *)out_setpoint = 0;
        fio1_pin22_ctrl(0);
        fio0_pin22_ctrl(0);
        *(volatile uint32_t *)input_locked = 0;
        *(volatile uint8_t *)ramp_phase = 0;
        *(volatile uint32_t *)out_param = *(volatile uint32_t *)out_param | 0x2000;
      }
    }
    *(volatile uint8_t *)phase_cnt = 0;
  }
  /* 输出预置：非法态/停机 → 复位输出 */
  if ((*(volatile uint32_t *)input_locked <= 1) || (*(volatile uint32_t *)input_locked > 7) ||
      (*(volatile uint32_t *)out_param != 0) || (*(volatile uint8_t *)freq_hz < 0x32)) {
    gpio_outputs_set();                               /* 非法态/停机 → 复位输出 */
  }
  else {
    /* —— 有效输出：TIMER2 复位 + 按 50/60Hz 计算触发参数 —— */
    *(volatile uint32_t *)(0x40090000 + 4) = 2;       /* TIMER2 TCR = 2（复位） */
    *(volatile uint32_t *)(0x40090000 + 0) = 0xff;    /* TIMER2 IR 清中断 */
    if (*(volatile uint8_t *)freq_hz == 0x32) {       /* 50Hz */
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_setpoint;
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_scale * 0x58 / 100;
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_scale / 100;
      if (*(volatile uint32_t *)out_scale > 0x2725) {
        *(volatile uint32_t *)out_scale = 0x2725;
      }
      *(volatile uint32_t *)out_div =
           (uint)((0x2726 - *(volatile int *)out_scale) * 10) / 0x22d;
      if (*(volatile uint8_t *)mode_byte == 1) {
        *(volatile uint32_t *)(0x40090000 + 0x18) =
             *(volatile uint32_t *)out_freq_adj * 10 + 0x31c7 - *(volatile uint32_t *)out_scale;
      }
      if (*(volatile uint8_t *)mode_byte == 2) {
        *(volatile uint32_t *)(0x40090000 + 0x18) =
             *(volatile uint32_t *)out_freq_adj * 10 + 0x31db - *(volatile uint32_t *)out_scale;
      }
    }
    if (*(volatile uint8_t *)freq_hz == 0x3c) {       /* 60Hz */
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_setpoint;
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_scale * 75 / 100;
      *(volatile uint32_t *)out_scale = *(volatile uint32_t *)out_scale / 100;
      if (*(volatile uint32_t *)out_scale > 0x21fc) {
        *(volatile uint32_t *)out_scale = 0x21fc;
      }
      *(volatile uint32_t *)out_div =
           (uint)((0x21fd - *(volatile int *)out_scale) * 10) / 0x1e3;
      if (*(volatile uint8_t *)mode_byte == 1) {
        *(volatile uint32_t *)(0x40090000 + 0x18) =
             *(volatile uint32_t *)out_freq_adj * 10 + 0x269f - *(volatile uint32_t *)out_scale;
      }
      if (*(volatile uint8_t *)mode_byte == 2) {
        *(volatile uint32_t *)(0x40090000 + 0x18) =
             *(volatile uint32_t *)out_freq_adj * 10 + 0x26b8 - *(volatile uint32_t *)out_scale;
      }
    }
    *(volatile uint32_t *)(0x40090000 + 4) = 1;       /* TIMER2 TCR = 1（使能） */
  }
  return;
}

/* 0x0000FAE8 —— TIMER2 ISR：TIMER2 复位、step_counter 清零、TIMER1 周期重载
 *   TIMER2=0x40090000（12p 主触发定时器）：清 IR、TCR=2 复位；
 *   step_counter(0x1000204c) 清零；
 *   TIMER1=0x40008000（12p 触发辅助）：TCR=2、清 IR、MR0=0x31、TCR=1 重载 */
void TIMER2_IRQHandler(void)
{
  *(volatile uint32_t *)(0x40090000 + 0) = 0xff;    /* TIMER2 IR 清中断 */
  *(volatile uint32_t *)(0x40090000 + 4) = 2;       /* TIMER2 TCR = 2（复位） */
  *(volatile uint8_t *)step_counter = 0;            /* 步进计数清零 */
  *(volatile uint32_t *)(0x40008000 + 4) = 2;       /* TIMER1 TCR = 2（复位） */
  *(volatile uint32_t *)(0x40008000 + 0) = 0xff;    /* TIMER1 IR 清中断 */
  *(volatile uint32_t *)(0x40008000 + 0x18) = 0x31; /* TIMER1 MR0 = 0x31（触发周期） */
  *(volatile uint32_t *)(0x40008000 + 4) = 1;       /* TIMER1 TCR = 1（使能） */
  return;
}

/* 0x0000FB0C —— TIMER1 ISR：12 相触发方向控制（12p；6p 此处为 LCD 扫描）
 *   TIMER1=0x40008000（触发辅助定时器）。step_counter(0x1000204C) 每中断 +1；
 *   每 20 拍（sc%0x14==0）重载 MR0：50Hz('2') → dir0: 0xc8+tp*4 / dir1: 0xc8+(0xc8-tp)*4，
 *   60Hz('<') → dir0: 0x82+tp*2 / dir1: 0x82+(0xc8-tp)*2；非 20 拍 MR0=0x36；freq 非 '2'/'<' 保持。
 *   sc<=0xf0 → TCR=1（继续）；trig_dir(0x10002050) 每 0x14 拍交替（0/1），sc>0xf0 清 0。
 *   12 段 FIO 位操作：每段 sc 20 拍窗口，按 mode_byte(0x1000204D)==1/!=1 与 sc 奇偶，
 *   对 FIO 池(0x2009C000) +0x18/0x1c(FIO0SET/CLR)、+0x58/0x5c(FIO2SET/CLR) 置位；
 *   段 2/4/6/8/10/12 再按 counter2(0x1000164C)<0x64 / >0x64 双组。sc>0xf0 → step_counter=0。
 * 局部：sc/dir/tp/c2/mb/fh = 各 SRAM 变量字节别名；fio = FIO 池基址。 */
void TIMER1_IRQHandler(void)
{
  volatile uint8_t *sc  = (volatile uint8_t *)step_counter;
  volatile uint8_t *dir = (volatile uint8_t *)trig_dir;
  volatile uint8_t *tp  = (volatile uint8_t *)trig_phase;
  volatile uint8_t *c2  = (volatile uint8_t *)counter2;
  volatile uint8_t *mb  = (volatile uint8_t *)mode_byte;
  volatile uint8_t *fh  = (volatile uint8_t *)freq_hz;
  volatile uint        fio = 0x2009c000;

  *(volatile uint32_t *)(0x40008000 + 0) = 0xff;   /* TIMER1 IR 清中断 */
  *(volatile uint32_t *)(0x40008000 + 4) = 2;      /* TIMER1 TCR = 2（复位） */
  *sc = *sc + 1;                                   /* step_counter++ */

  if (*sc % 0x14 == 0) {                           /* 每 20 拍重载 MR0 */
    if (*fh == 0x32) {                             /* 50Hz '2' */
      if (*dir == 0) {
        *(volatile uint32_t *)(0x40008000 + 0x18) = 0xc8 + *tp * 4;
      }
      if (*dir == 1) {
        *(volatile uint32_t *)(0x40008000 + 0x18) = 0xc8 + (0xc8 - *tp) * 4;
      }
    }
    if (*fh == 0x3c) {                             /* 60Hz '<' */
      if (*dir == 0) {
        *(volatile uint32_t *)(0x40008000 + 0x18) = 0x82 + *tp * 2;
      }
      if (*dir == 1) {
        *(volatile uint32_t *)(0x40008000 + 0x18) = 0x82 + (0xc8 - *tp) * 2;
      }
    }
  } else {
    *(volatile uint32_t *)(0x40008000 + 0x18) = 0x36;  /* 非 20 拍 */
  }

  if (*sc <= 0xf0) {
    *(volatile uint32_t *)(0x40008000 + 4) = 1;    /* TIMER1 TCR = 1（使能） */
  }

  /* trig_dir 每 0x14 拍交替 */
  if (*sc <= 0x14)                          *dir = 0;
  if ((*sc > 0x14) && (*sc <= 0x28))        *dir = 1;
  if ((*sc > 0x28) && (*sc <= 0x3c))        *dir = 0;
  if ((*sc > 0x3c) && (*sc <= 0x50))        *dir = 1;
  if ((*sc > 0x50) && (*sc <= 0x64))        *dir = 0;
  if ((*sc > 0x64) && (*sc <= 0x78))        *dir = 1;
  if ((*sc > 0x78) && (*sc <= 0x8c))        *dir = 0;
  if ((*sc > 0x8c) && (*sc <= 0xa0))        *dir = 1;
  if ((*sc > 0xa0) && (*sc <= 0xb4))        *dir = 0;
  if ((*sc > 0xb4) && (*sc <= 0xc8))        *dir = 1;
  if ((*sc > 0xc8) && (*sc <= 0xdc))        *dir = 0;
  if ((*sc > 0xdc) && (*sc <= 0xf0))        *dir = 1;
  if (*sc > 0xf0)                           *dir = 0;

  /* —— 段 1（sc 0x01..0x14，无 counter2）—— */
  if ((*sc > 0) && (*sc <= 0x14)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x200;      /* FIO2CLR P2.9 */
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80000;    /* FIO0CLR P0.19 */
      } else {
        *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;      /* FIO2SET P2.9 */
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;    /* FIO0SET P0.19 */
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x20000;    /* FIO0CLR P0.17 */
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x10000;    /* FIO0CLR P0.16 */
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x20000;    /* FIO0SET P0.17 */
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;    /* FIO0SET P0.16 */
      }
    }
  }

  /* —— 段 2（sc 0x15..0x28，counter2 双组）—— */
  if ((*sc > 0x14) && (*sc <= 0x28)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
        }
      }
    }
  }

  /* —— 段 3（sc 0x29..0x3c，无 counter2）—— */
  if ((*sc > 0x28) && (*sc <= 0x3c)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x10000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x10000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;
      }
    }
  }

  /* —— 段 4（sc 0x3d..0x50，counter2 双组）—— */
  if ((*sc > 0x3c) && (*sc <= 0x50)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
        }
      }
    }
  }

  /* —— 段 5（sc 0x51..0x64，无 counter2）—— */
  if ((*sc > 0x50) && (*sc <= 0x64)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x10000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x20000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x10000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x20000;
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80000;
        *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x200;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80000;
        *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;
      }
    }
  }

  /* —— 段 6（sc 0x65..0x78，counter2 双组）—— */
  if ((*sc > 0x64) && (*sc <= 0x78)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
        }
      }
    }
  }

  /* —— 段 7（sc 0x79..0x8c，无 counter2）—— */
  if ((*sc > 0x78) && (*sc <= 0x8c)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x8000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x20000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x20000;
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x40000;
        *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x200;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;
        *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;
      }
    }
  }

  /* —— 段 8（sc 0x8d..0xa0，counter2 双组）—— */
  if ((*sc > 0x8c) && (*sc <= 0xa0)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
        }
      }
    }
  }

  /* —— 段 9（sc 0xa1..0xb4，无 counter2）—— */
  if ((*sc > 0xa0) && (*sc <= 0xb4)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x8000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x40000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x40000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x8000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;
      }
    }
  }

  /* —— 段 10（sc 0xb5..0xc8，counter2 双组）—— */
  if ((*sc > 0xb4) && (*sc <= 0xc8)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
        }
      }
    }
  }

  /* —— 段 11（sc 0xc9..0xdc，无 counter2）—— */
  if ((*sc > 0xc8) && (*sc <= 0xdc)) {
    if (*mb == 1) {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x200;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x40000;
      } else {
        *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x200;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x40000;
      }
    } else {
      if (*sc % 2) {
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x20000;
        *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x8000;
      } else {
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x20000;
        *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x8000;
      }
    }
  }

  /* —— 段 12（sc 0xdd..0xf0，counter2 双组）—— */
  if ((*sc > 0xdc) && (*sc <= 0xf0)) {
    if (*c2 < 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x100;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x100;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x100;
        }
      }
    } else if (*c2 > 0x64) {
      if (*mb == 1) {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x20;
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x40;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x20;
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x40;
        }
      } else {
        if (*sc % 2) {
          *(volatile uint *)(fio + 0x5c) = *(volatile uint *)(fio + 0x5c) | 0x80;
          *(volatile uint *)(fio + 0x1c) = *(volatile uint *)(fio + 0x1c) | 0x80;
        } else {
          *(volatile uint *)(fio + 0x58) = *(volatile uint *)(fio + 0x58) | 0x80;
          *(volatile uint *)(fio + 0x18) = *(volatile uint *)(fio + 0x18) | 0x80;
        }
      }
    }
  }

  if (*sc > 0xf0) {
    *sc = 0;
  }
  return;
}
