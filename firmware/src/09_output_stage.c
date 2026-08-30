/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 09：输出级（SCR 触发角计算）/ 引脚配置 /
 *                         定时器 / 外部中断（急停/锁存）
 *
 * 关键硬件（对照 HARDWARE_VERIFICATION_2026-08-20.md）：
 *   · EINT1/2/3 = P2.11/12/13（PINSEL4 0x4002C000+0x10，值=01；三相同步过零 U10/U11/U12）
 *     反编译确证 2026-08-21；P0.22/23/24 为 LPC176x 默认复用但本固件未启用
 *     SCB 0x400FC000：EXTINT=+0x140、EXTMODE=+0x148、EXTPOLAR=+0x14C
 *   · P0.22 = 运行继电器 RLY1（fio0_pin22_ctrl）；P1.22 = 触发/运行指示（fio1_pin22_ctrl）
 *   · TIMER1(IRQ2)、TIMER2(IRQ3) 兼作显示/触发时序；TIMER1 ISR 为 LCD 动态扫描
 *
 * output_stage（主循环每节拍调用）——SCR 移相触发的核心：
 *   运行状态 0x1000EDA8（==1 跳过处理）；每 9 拍执行一次
 *   触发角：180°(0xB4) - 当前角；周期基量 TRIG_PERIOD、角度系数 ANGLE_SCALE(6333)/100
 *   软起/软停斜坡（状态机 0x1000EE24/0x1000F268/0x1000F780：0=停、4=运行、5=稳定）
 *   闭环：closed_loop_wrapper(PID) 计算输出角；上下限钳位 0x1000F2C8/0x1000F2D0
 *   保护：过压(0x1000EDE4..) / 过流(0x1000EDF8..) / 缺相(0x1000EE04..)，
 *   故障标志 0x1000EDF4（只置不清锁存，需断电复位；2026-08-21 反编译确证）：
 *     bit4=过压、bit5=过流、bit3=缺相(判据×0.15/1.5/2 且延时×50/20/10)、
 *     bit9=缺相严重级(判据×2.5/3/3.5 且延时×5/2/1)
 *   停机斜坡（cfg_word=0 且联锁非 0 时）：0x1000FBB0/6C/70 逐拍降频
 * 导出：2026-08-21（L0 语义化：局部 puVar→fio/timer1/timer2/base、param_1→level、
 *   output_stage/EINT3/TIMER1 HANDLER 各局部已按角色命名；函数名保持语义化原样）
 *
 * 交叉引用：
 *   · 触发 GPIO（G1-G6 / P12-G1~G6）与驱动链 → docs/HARDWARE_VERIFICATION_2026-08-20.md §二.2、§六.B
 *   · SCR 触发时序（TIMER2.MR0 / TIMER1 扫描 240 步 / 12° 脉宽）→ docs/PROGRESS_2026-08-20.md §4j、§4k
 *   · 保护标志 / 停机链 → APPLICATION_GUIDE_2026-08-21.md §四.1
 *   · 菜单映射（软起/相位限制/主从偏移/起始相位）→ docs/MENU_PARAMETER_MAPPING.md §1
 *   ✅ EINT 引脚差异复核完成（2026-08-21）：eint1/2/3_init 反编译确证 PINSEL4
 *     配置 P2.11/12/13=EINT1/2/3（硬件印证正确）；P0.22=fio0_pin22_ctrl=运行继电器 RLY1。
 * ========================================================================== */

/* =============================================================================
 * src/09_output_stage.c — 反编译模块 09（输出级 SCR 触发角/引脚/定时器/EINT）可编译副本
 * 目标B 阶段4 修正：
 *   1) 补 include（types.h/reg.h/globals.h）。
 *   2) PTR_DAT_0000e988 = FIO 池基址 0x2009C000，原反编译作 undefined* 字节偏移基址
 *      （puVar+off = +off 字节：+0x1c FIO0CLR、+0x18 FIO0SET、+0x40 FIO2DIR、+0x58 FIO2SET）。
 *      globals 已强制 volatile uint8_t*；局部 puVar 同步改 volatile uint8_t*。
 *   3) &DAT_00003904 指针算术 → 数字地址 0x3904（flash 表地址，非变量地址）。
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

/* 0x0000E5A8 —— 引脚功能/方向配置（PINSEL+FIO DIR，详见 HARDWARE_VERIFICATION）
 *   PTR_DAT_0000E988 = FIO 池基址 0x2009C000
 *   FIO0DIR(P0.20/21/22、P0.15..19、P0.7/8、P0.4/5)、FIO1/FIO2/FIO3/FIO4 方向
 *   局部：fio = PTR_DAT_0000e988（FIO 池基址，puVar+off = +off 字节） */
void pin_config(void)
{
  volatile uint8_t *fio;

  fio = PTR_DAT_0000e988;
  *(volatile uint *)PTR_DAT_0000e988 = *(volatile uint *)PTR_DAT_0000e988 | 0x100000;   /* FIO0DIR P0.20 */
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
  *(volatile uint *)(fio + 0x80) = *DAT_0000e98c | 0x10000000;              /* FIO4DIR P4.28 */
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

/* 0x0000E79A —— 输出使能（关断时所有触发/使能线复位） */
void gpio_outputs_set(void)
{
  volatile uint8_t *fio;

  fio = PTR_DAT_0000e988;
  *(volatile uint *)(PTR_DAT_0000e988 + 0x18) = *(volatile uint *)(PTR_DAT_0000e988 + 0x18) | 0x20000;
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

/* 0x0000E816 —— TIMER1 初始化（IRQ2）：TCR 复位、PR=0x18 预分频、MR0=999、
 *   匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移（TIMER 寄存器 4 字节对齐，
 *   与原厂 0xE816 反汇编逐地址一致）：[0]=IR、[1]=TCR、[2]=TC、[3]=PR、[5]=MCR、[6]=MR0。
 *   局部：timer1 = DAT_0000e990 → 指向 TIMER1（0x40008000） */
void timer1_init(void)
{
  volatile uint32_t *timer1;

  timer1 = DAT_0000e990;                              /* TIMER1 0x40008000 */
  DAT_0000e990[1] = 2;                                /* TCR=2：TC 复位（原厂 0xE81C） */
  timer1[3] = 0x18;                                   /* PR=0x18：预分频（原厂 0xE820） */
  timer1[6] = 999;                                    /* MR0=999 匹配值（原厂 0xE826） */
  *timer1 = 0xff;                                     /* IR 清中断（原厂 0xE82A） */
  timer1[5] = 3;                                      /* MCR=3：MR0 匹配中断+复位（原厂 0xE82E） */
  nvic_enable_irq(2);                                 /* NVIC TIMER1_IRQ */
  return;
}

/* 0x0000E838 —— TIMER2 初始化（IRQ3）：PCONP TMR2、TCR 复位、PR=0x18 预分频、
 *   MR0=999、匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移（TIMER 寄存器
 *   4 字节对齐，与原厂 0xE838 反汇编逐地址一致）。
 *   局部：timer2 = DAT_0000e99c → 指向 TIMER2（0x40090000） */
void timer2_init(void)
{
  volatile uint32_t *timer2;

  *(volatile uint *)(DAT_0000e998 + 0xc4) = *g_pconp | 0x400000;   /* PCONP bit22 TMR2 */
  timer2 = DAT_0000e99c;                              /* TIMER2 0x40090000 */
  DAT_0000e99c[1] = 2;                                /* TCR=2：TC 复位（原厂 0xE84E） */
  timer2[3] = 0x18;                                   /* PR=0x18：预分频（原厂 0xE854） */
  timer2[6] = 999;                                    /* MR0=999 匹配值（原厂 0xE85A） */
  *timer2 = 0xff;                                     /* IR 清中断（原厂 0xE860） */
  timer2[5] = 3;                                      /* MCR=3：MR0 匹配中断+复位（原厂 0xE866） */
  nvic_enable_irq(3);                                 /* NVIC TIMER2_IRQ（反汇编 0xE868 核 r0=3） */
  return;
}

/* 0x0000E86E —— EINT1 外部中断初始化（P2.11，边沿触发，清挂起，NVIC 0x13=19）
 *   局部：base = 先 PINSEL(0x4002C000,+0x10 PINSEL4) 后 SCB(0x400FC000,EXTINT/EXTMODE/EXTPOLAR) 复用基址 */
void eint1_init(void)
{
  int base;

  *(volatile uint *)(DAT_0000e998 + 0x140) = *DAT_0000e9a0 | 2;      /* EXTINT 清 EINT1 */
  base = DAT_0000e9a4;                                     /* PINSEL 0x4002C000 */
  *(volatile uint *)(DAT_0000e9a4 + 0x10) = *(volatile uint *)(DAT_0000e9a4 + 0x10) | 0x400000;  /* PINSEL4 P2.11=EINT1（bit22=1、bit23=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xff7fffff;
  base = DAT_0000e998;                                     /* SCB 0x400FC000 */
  *(volatile uint *)(DAT_0000e998 + 0x148) = *(volatile uint *)(DAT_0000e998 + 0x148) | 2;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffffd;          /* EXTPOLAR 下降沿 */
  *(volatile uint *)(base + 0x140) = *(volatile uint *)(base + 0x140) | 2;  /* EXTINT 清 */
  nvic_enable_irq(0x13);                                    /* NVIC EINT1_IRQ=19 */
  return;
}

/* 0x0000E8CA —— EINT2 外部中断初始化（P2.12，边沿触发，NVIC 0x14=20）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint2_init(void)
{
  int base;

  base = DAT_0000e9a4;
  *(volatile uint *)(DAT_0000e9a4 + 0x10) = *(volatile uint *)(DAT_0000e9a4 + 0x10) | 0x1000000; /* PINSEL4 P2.12=EINT2（bit24=1、bit25=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xfdffffff;
  base = DAT_0000e998;
  *(volatile uint *)(DAT_0000e998 + 0x148) = *DAT_0000e9a8 | 4;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffffb;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x14);                                    /* NVIC EINT2_IRQ=20 */
  return;
}

/* 0x0000E908 —— EINT3 外部中断初始化（P2.13，边沿触发，NVIC 0x15=21）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint3_init(void)
{
  int base;

  base = DAT_0000e9a4;
  *(volatile uint *)(DAT_0000e9a4 + 0x10) = *(volatile uint *)(DAT_0000e9a4 + 0x10) | 0x4000000; /* PINSEL4 P2.13=EINT3（bit26=1、bit27=0） */
  *(volatile uint *)(base + 0x10) = *(volatile uint *)(base + 0x10) & 0xf7ffffff;
  base = DAT_0000e998;
  *(volatile uint *)(DAT_0000e998 + 0x148) = *DAT_0000e9a8 | 8;      /* EXTMODE 边沿 */
  *(volatile uint *)(base + 0x14c) = *(volatile uint *)(base + 0x14c) & 0xfffffff7;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x15);                                    /* NVIC EINT3_IRQ=21 */
  return;
}

/* 0x0000E946 —— P1.22 电平控制（level>=1 置位、<1 清零；触发/运行指示 LED）
 *   level>=1 → FIO1SET；否则 FIO1CLR */
void fio1_pin22_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_0000e988 + 0x3c) = *(volatile uint *)(PTR_DAT_0000e988 + 0x3c) | 0x400000;
  }
  else {
    *(volatile uint *)(PTR_DAT_0000e988 + 0x38) = *(volatile uint *)(PTR_DAT_0000e988 + 0x38) | 0x400000;
  }
  return;
}

/* 0x0000E966 —— P0.22 电平控制（level>=1 置位、<1 清零；运行继电器 RLY1，2026-08-21 复核）
 *   level>=1 → FIO0SET；否则 FIO0CLR */
void fio0_pin22_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_0000e988 + 0x1c) = *(volatile uint *)(PTR_DAT_0000e988 + 0x1c) | 0x400000;
  }
  else {
    *(volatile uint *)(PTR_DAT_0000e988 + 0x18) = *(volatile uint *)(PTR_DAT_0000e988 + 0x18) | 0x400000;
  }
  return;
}

/* 0x0000E9AC —— 输出级主处理（SCR 移相触发角计算 + 保护 + 软起停 + PID）
 *   详见文件头说明。每节拍调用；*0x1000EDA8==1 跳过处理；每 9 拍执行一次。
 * 局部变量角色（反编译寄存器复用，跨段复用）：
 *   tick_cnt    — 9 拍节拍计数（0x1000EDAC）
 *   word_ptr    — 通用字指针（触发角/软起累加/步进等 0x1000EDB4/EE28/EE48/F2F0/F788）
 *   scratch_ptr — 通用临时指针（闭环看门狗计数/输出值/软起多处）
 *   cv_out      — 恒压源模式输出值指针（0x1000F77C）
 *   ramp_cnt    — 停机斜坡当前值指针（PTR_flag_68_0000fbb0）
 *   pid_out     — closed_loop_wrapper(PID) 返回的闭环输出角 */
void output_stage(void)
{
  volatile uint8_t *tick_cnt;
  volatile uint32_t *word_ptr;
  volatile uint32_t *scratch_ptr;
  volatile uint32_t *cv_out;
  volatile uint8_t *ramp_cnt;
  uint pid_out;

  tick_cnt = DAT_0000edac;
  if ((*DAT_0000eda8 != 1) && (*DAT_0000edac = *DAT_0000edac + 1, 9 < *tick_cnt)) {
    *tick_cnt = 0;
    word_ptr = DAT_0000edb4;
    *DAT_0000edb4 = 0xb4 - *g_reg62_start_phase;            /* 触发角 = 180° - 当前角 */
    if (*word_ptr == 0) {
      *word_ptr = 1;
    }
    *DAT_0000edbc = 0xb4 - *DAT_0000edb8;
    if (*g_cfg_word == '\0') {
      *DAT_0000edc4 = 0;
      if (*g_gain_sel == '\0') {
        *DAT_0000edd0 = *g_gain_b / 0xf;         /* 周期/15 换算 */
      }
      if (*g_gain_sel == '\x01') {
        *DAT_0000edd0 = *g_gain_a / 0xf;
      }
      if (*g_gain_sel == '\x02') {
        *DAT_0000eddc = (*DAT_0000edd8 * 1000) / *g_gain_b;
        *DAT_0000edd0 = *g_gain_b / 0xf;
      }
    }
    if ((*g_cfg_word == '\x01') && (9 < *DAT_0000ede0)) {
      /* —— 过压保护（0x1000EDE4 阈值 / 0x1000EDE8 延时上限）—— */
      if ((*DAT_0000ede4 == 0) || (*DAT_0000ede8 <= *DAT_0000ede4)) {
        *DAT_0000edec = 0;
      }
      else {
        *DAT_0000edec = *DAT_0000edec + 1;
        if ((uint)*DAT_0000edf0 * 0x32 < *DAT_0000edec) {
          *DAT_0000edec = 0;
          *DAT_0000edf4 = *DAT_0000edf4 | 0x10;      /* 过压故障标志 */
        }
      }
      /* —— 过流保护（0x1000EDF8 阈值）—— */
      if ((*DAT_0000edf8 == 0) || (*DAT_0000edf8 <= *DAT_0000ede8)) {
        *DAT_0000edfc = 0;
      }
      else {
        *DAT_0000edfc = *DAT_0000edfc + 1;
        if ((uint)*DAT_0000ee00 * 0x32 < *DAT_0000edfc) {
          *DAT_0000edfc = 0;
          *DAT_0000edf4 = *DAT_0000edf4 | 0x20;      /* 过流故障标志 */
        }
      }
      /* —— 缺相/不平衡保护（0x1000EE04 判据，多级延时）—— */
      if (*DAT_0000ee04 != 0) {
        *DAT_0000ee08 = *DAT_0000ee08 + 1;
        if (*DAT_0000ee0c < *DAT_0000ee04) {
          *DAT_0000ee08 = 0;
        }
        if ((*DAT_0000ee04 <= *DAT_0000ee0c) && ((uint)*DAT_0000ee10 * 0x32 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 8;
        }
        if (((*DAT_0000ee04 * 0xf) / 10 < *DAT_0000ee0c) &&
           ((uint)*DAT_0000ee10 * 0x14 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 8;
        }
        if ((*DAT_0000ee04 * 2 < *DAT_0000ee0c) && ((uint)*DAT_0000ee10 * 10 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 8;
        }
        if (((*DAT_0000ee04 * 0x19) / 10 < *DAT_0000ee0c) &&
           ((uint)*DAT_0000ee10 * 5 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 0x200;
        }
        if ((*DAT_0000ee04 * 3 < *DAT_0000ee0c) && ((uint)*DAT_0000ee10 * 2 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 0x200;
        }
        if (((*DAT_0000ee04 * 0x23) / 10 < *DAT_0000ee0c) && (1 < *DAT_0000ee08)) {
          *DAT_0000edf4 = *DAT_0000edf4 | 0x200;
        }
      }
      scratch_ptr = DAT_0000ee14;
      *DAT_0000ee14 = *DAT_0000ee14 + 1;
      if (0x5dc < *scratch_ptr) {                          /* 1500 拍 → 欠压/锁定标志 */
        *DAT_0000ee14 = 0;
        *DAT_0000edc4 = 1;
      }
      if (*g_gain_sel == '\0') {
        /* —— 软起动斜坡状态机（0x1000EE24：0=停 4=运行 5=稳定）—— */
        *DAT_0000ee1c = *DAT_0000ee18;
        *DAT_0000ee20 = 0;
        if (*DAT_0000ee24 == 0) {
          *DAT_0000ee24 = 4;
          word_ptr = DAT_0000ee28;
          *DAT_0000ee28 = 0;
          *DAT_0000ee2c = *word_ptr;
          *DAT_0000ee30 = 0;
          *DAT_0000ee34 = 0;
          *DAT_0000ee38 = 0;
          *DAT_0000ee3c = 0;
          *DAT_0000ee40 = SOFT_START_INIT;                     /* 软起累加初始 6001 */
        }
        if (*DAT_0000ee24 == 4) {
          *DAT_0000ee1c = *DAT_0000ede0;
          word_ptr = DAT_0000ee48;
          *DAT_0000ee48 =                             /* 触发步进 = (TRIG_PERIOD-角*6333/100)/50/除数 */
               (int)(((ulonglong)(TRIG_PERIOD - (uint)(*DAT_0000edb4 * ANGLE_SCALE) / 100) / 0x32) /
                    (ulonglong)*DAT_0000ee44);
          *DAT_0000ee4c = *DAT_0000ee4c + *word_ptr;
          if (*DAT_0000ee50 == '\0') {
            *DAT_0000ee54 = (uint)(*DAT_0000edb4 * ANGLE_SCALE) / 100 + *DAT_0000ee4c;
            *DAT_0000ee58 = 1;
          }
          if (*DAT_0000ee50 == '\x01') {
            *DAT_0000ee54 = *DAT_0000ee4c;
            *DAT_0000ee58 = 0;
          }
          word_ptr = DAT_0000ee28;
          *DAT_0000ee28 = *DAT_0000ee54 * 100;
          *g_cl_cached_out = *word_ptr;
          *DAT_0000ee2c = *DAT_0000ee28;
          *DAT_0000ee30 = 0;
          *DAT_0000ee34 = 0;
          if ((*DAT_0000ee1c * 0x2f8) / 100 + 0xedd < *DAT_0000ee54) {  /* 达目标 → 5 稳定 */
            *DAT_0000ee24 = 5;
          }
          if ((*DAT_0000edd8 <= *DAT_0000ee0c) && (*DAT_0000edd8 <= *g_gain_b)) {
            *DAT_0000ee24 = 5;
          }
          if (*DAT_0000ee60 < *DAT_0000ee64) {
            *DAT_0000f268 = 5;
          }
        }
        if (*DAT_0000f268 == 5) {
          /* —— 第一路闭环运行（0x1000F270..F2A4 反馈/给定比较 + PID）—— */
          *DAT_0000f26c = 0;
          scratch_ptr = DAT_0000f284;
          if ((*DAT_0000f270 < *DAT_0000f274) || (*DAT_0000f27c <= *DAT_0000f278)) {
            *DAT_0000f284 = *DAT_0000f284 + 1;
            if (0x32 < *scratch_ptr) {
              *scratch_ptr = 0;
              *DAT_0000f280 = 2;
              *DAT_0000f288 = *DAT_0000f288;
              *DAT_0000f28c = '\0';
            }
          }
          else {
            *DAT_0000f280 = 1;
            *DAT_0000f284 = 0;
          }
          scratch_ptr = DAT_0000f294;
          if (*DAT_0000f27c < *DAT_0000f290) {
            *DAT_0000f294 = *DAT_0000f294 + 1;
            if (10 < *scratch_ptr) {
              *scratch_ptr = 0;
              *DAT_0000f280 = 0;
              *DAT_0000f288 = *DAT_0000f288;
              *DAT_0000f28c = '\0';
            }
          }
          else {
            *DAT_0000f294 = 0;
          }
          if (((*DAT_0000f290 <= *DAT_0000f27c) && (*DAT_0000f274 <= *g_gain_b)) &&
             (*DAT_0000f280 == 2)) {
            *DAT_0000f27c = *DAT_0000f290;
            scratch_ptr = DAT_0000f290;
            *DAT_0000f290 = *DAT_0000f290 + 1;
            if (6000 < *scratch_ptr) {
              *DAT_0000f290 = 6000;
            }
            *DAT_0000f288 = *DAT_0000f288;
            *DAT_0000f28c = '\0';
          }
          if (((*DAT_0000f274 <= *g_gain_b) && (*DAT_0000f280 == 1)) &&
             (*g_gain_a < *g_gain_b)) {
            *DAT_0000f2a0 = 1;
            if (*DAT_0000f28c != '\x01') {
              *DAT_0000f28c = '\x01';
              *DAT_0000f290 = *DAT_0000f288;
            }
            if (*DAT_0000f290 < 2) {
              *DAT_0000f290 = 1;
            }
            *DAT_0000f27c = *DAT_0000f274;
            *DAT_0000f288 = *DAT_0000f2a4;
          }
          if (((*DAT_0000f288 <= *DAT_0000f27c) && (*DAT_0000f274 <= *g_gain_b)) &&
             ((*DAT_0000f280 == 1 && (*g_gain_b <= *g_gain_a)))) {
            *DAT_0000f2a0 = 1;
            if (*DAT_0000f28c != '\x01') {
              *DAT_0000f28c = '\x01';
              *DAT_0000f290 = *DAT_0000f288;
            }
            if (*DAT_0000f290 < 2) {
              *DAT_0000f290 = 1;
            }
            *DAT_0000f27c = *DAT_0000f274;
            *DAT_0000f288 = *DAT_0000f2a4;
          }
          if (((*DAT_0000f278 < 2) && (*DAT_0000f274 <= *g_gain_b)) && (*DAT_0000f280 == 1)) {
            *DAT_0000f2a0 = 1;
            if (*DAT_0000f28c != '\x01') {
              *DAT_0000f28c = '\x01';
              *DAT_0000f290 = *DAT_0000f288;
            }
            if (*DAT_0000f290 < 2) {
              *DAT_0000f290 = 1;
            }
            *DAT_0000f27c = *DAT_0000f274;
            *DAT_0000f288 = *DAT_0000f2a4;
          }
          scratch_ptr = DAT_0000f2b4;
          if (((*DAT_0000f2a8 < 5) && (*DAT_0000f2ac == '\x01')) && (*DAT_0000f2b0 == '\x01')) {
            *DAT_0000f2b4 = *DAT_0000f2b4 + 1;
            if (100 < *scratch_ptr) {
              *DAT_0000f2b8 = *DAT_0000f2b8 | 0x800;
            }
          }
          else {
            *DAT_0000f2b4 = 0;
          }
          pid_out = closed_loop_wrapper(*DAT_0000f27c,*DAT_0000f288,*g_act_gain_a,*g_act_gain_b);
          *DAT_0000f2c4 = pid_out;
        }
        if (((uint)(*DAT_0000f2c8 * ANGLE_SCALE) < *DAT_0000f2c4 ||
             *DAT_0000f2c8 * ANGLE_SCALE - *DAT_0000f2c4 == 0) && (*DAT_0000f2cc == '\0')) {
          *DAT_0000f2c4 = *DAT_0000f2c8 * ANGLE_SCALE;     /* 输出上限钳位 */
        }
        if ((*DAT_0000f2c4 <= (uint)(*DAT_0000f2d0 * ANGLE_SCALE)) && (*DAT_0000f2cc == '\0')) {
          *DAT_0000f2c4 = *DAT_0000f2d0 * ANGLE_SCALE;     /* 输出下限钳位 */
        }
      }
      if (*g_gain_sel == '\x01') {
        /* —— 第二路闭环（0x1000F2E8.. 软起 + PID + 钳位）—— */
        *DAT_0000f2d8 = *DAT_0000f27c;
        *DAT_0000f2a0 = 0;
        if (*DAT_0000f268 == 0) {
          *DAT_0000f268 = 4;
          scratch_ptr = DAT_0000f2c4;
          *DAT_0000f2c4 = 0;
          *DAT_0000f2dc = *scratch_ptr;
          *DAT_0000f2e0 = 0;
          *DAT_0000f2e4 = 0;
          *DAT_0000f2b4 = 0;
          *DAT_0000f28c = '\0';
          *DAT_0000f290 = SOFT_START_INIT;
        }
        if (*DAT_0000f268 == 4) {
          *DAT_0000f2d8 = *DAT_0000f2e8;
          word_ptr = DAT_0000f2f0;
          *DAT_0000f2f0 =
               (int)(((ulonglong)(TRIG_PERIOD - (uint)(*DAT_0000f2d0 * ANGLE_SCALE) / 100) / 0x32) /
                    (ulonglong)*DAT_0000f2ec);
          *DAT_0000f2f4 = *DAT_0000f2f4 + *word_ptr;
          if (*DAT_0000f2f8 == '\0') {
            *DAT_0000f2fc = (uint)(*DAT_0000f2d0 * ANGLE_SCALE) / 100 + *DAT_0000f2f4;
            *DAT_0000f26c = 1;
          }
          if (*DAT_0000f2f8 == '\x01') {
            *DAT_0000f2fc = *DAT_0000f2f4;
            *DAT_0000f26c = 0;
          }
          scratch_ptr = DAT_0000f2c4;
          *DAT_0000f2c4 = *DAT_0000f2fc * 100;
          *g_cl_cached_out = *scratch_ptr;
          *DAT_0000f2dc = *DAT_0000f2c4;
          *DAT_0000f2e0 = 0;
          *DAT_0000f2e4 = 0;
          if ((*DAT_0000f2d8 * 0x2f8) / 100 + 0xedd < *DAT_0000f2fc) {
            *DAT_0000f268 = 5;
          }
          if ((*DAT_0000f304 <= *DAT_0000f278) && (*DAT_0000f304 <= *g_gain_a)) {
            *DAT_0000f268 = 5;
          }
          if (*DAT_0000f308 < *DAT_0000f2a4) {
            *DAT_0000f268 = 5;
          }
        }
        if (*DAT_0000f268 == 5) {
          /* —— 第二路闭环运行（0x1000F70C..F740）—— */
          *DAT_0000f708 = 0;
          scratch_ptr = DAT_0000f720;
          if ((*DAT_0000f70c < *DAT_0000f710) || (*DAT_0000f718 <= *DAT_0000f714)) {
            *DAT_0000f720 = *DAT_0000f720 + 1;
            if (0x32 < *scratch_ptr) {
              *scratch_ptr = 0;
              *DAT_0000f71c = 2;
              *DAT_0000f724 = *DAT_0000f724;
              *DAT_0000f728 = '\0';
            }
          }
          else {
            *DAT_0000f71c = 1;
            *DAT_0000f720 = 0;
          }
          scratch_ptr = DAT_0000f730;
          if (*DAT_0000f718 < *DAT_0000f72c) {
            *DAT_0000f730 = *DAT_0000f730 + 1;
            if (10 < *scratch_ptr) {
              *scratch_ptr = 0;
              *DAT_0000f71c = 0;
              *DAT_0000f724 = *DAT_0000f724;
              *DAT_0000f728 = '\0';
            }
          }
          else {
            *DAT_0000f730 = 0;
          }
          if (((*DAT_0000f72c <= *DAT_0000f718) && (*DAT_0000f710 <= *g_gain_a)) &&
             (*DAT_0000f71c == 2)) {
            *DAT_0000f718 = *DAT_0000f72c;
            scratch_ptr = DAT_0000f72c;
            *DAT_0000f72c = *DAT_0000f72c + 1;
            if (6000 < *scratch_ptr) {
              *DAT_0000f72c = 6000;
            }
            *DAT_0000f724 = *DAT_0000f724;
            *DAT_0000f728 = '\0';
          }
          if (((*DAT_0000f710 <= *g_gain_a) && (*DAT_0000f71c == 1)) &&
             (*g_gain_b < *g_gain_a)) {
            *DAT_0000f73c = 1;
            if (*DAT_0000f728 != '\x01') {
              *DAT_0000f728 = '\x01';
              *DAT_0000f72c = *DAT_0000f724;
            }
            if (*DAT_0000f72c < 2) {
              *DAT_0000f72c = 1;
            }
            *DAT_0000f718 = *DAT_0000f710;
            *DAT_0000f724 = *DAT_0000f740;
          }
          if (((*DAT_0000f724 <= *DAT_0000f718) && (*DAT_0000f710 <= *g_gain_a)) &&
             ((*DAT_0000f71c == 1 && (*g_gain_a <= *g_gain_b)))) {
            *DAT_0000f73c = 1;
            if (*DAT_0000f728 != '\x01') {
              *DAT_0000f728 = '\x01';
              *DAT_0000f72c = *DAT_0000f724;
            }
            if (*DAT_0000f72c < 2) {
              *DAT_0000f72c = 1;
            }
            *DAT_0000f718 = *DAT_0000f710;
            *DAT_0000f724 = *DAT_0000f740;
          }
          if (((*DAT_0000f714 < 2) && (*DAT_0000f710 <= *g_gain_a)) && (*DAT_0000f71c == 1)) {
            *DAT_0000f73c = 1;
            if (*DAT_0000f728 != '\x01') {
              *DAT_0000f728 = '\x01';
              *DAT_0000f72c = *DAT_0000f724;
            }
            if (*DAT_0000f72c < 2) {
              *DAT_0000f72c = 1;
            }
            *DAT_0000f718 = *DAT_0000f710;
            *DAT_0000f724 = *DAT_0000f740;
          }
          scratch_ptr = DAT_0000f750;
          if (((*DAT_0000f744 < 5) && (*DAT_0000f748 == '\x01')) && (*DAT_0000f74c == '\x01')) {
            *DAT_0000f750 = *DAT_0000f750 + 1;
            if (100 < *scratch_ptr) {
              *DAT_0000f754 = *DAT_0000f754 | 0x800;
            }
          }
          else {
            *DAT_0000f750 = 0;
          }
          pid_out = closed_loop_wrapper(*DAT_0000f718,*DAT_0000f724,*g_act_gain_a,*g_act_gain_b);
          *DAT_0000f760 = pid_out;
        }
        if (((uint)(*DAT_0000f764 * ANGLE_SCALE) < *DAT_0000f760 ||
             *DAT_0000f764 * ANGLE_SCALE - *DAT_0000f760 == 0) && (*DAT_0000f768 == '\0')) {
          *DAT_0000f760 = *DAT_0000f764 * ANGLE_SCALE;
        }
        if ((*DAT_0000f760 <= (uint)(*DAT_0000f76c * ANGLE_SCALE)) && (*DAT_0000f768 == '\0')) {
          *DAT_0000f760 = *DAT_0000f76c * ANGLE_SCALE;
        }
      }
      scratch_ptr = DAT_0000f778;
      if (*g_gain_sel == '\x02') {
        /* —— 恒压源模式（0x1000F774..F794）—— */
        *DAT_0000f778 = *DAT_0000f774;
        cv_out = DAT_0000f77c;
        *DAT_0000f77c = *scratch_ptr % 10000;
        if (1000 < *cv_out) {
          *cv_out = 1000;
        }
        if (*DAT_0000f77c < 10) {
          *DAT_0000f77c = 10;
        }
        if (*DAT_0000f780 == 0) {
          *DAT_0000f780 = 4;
        }
        if (*DAT_0000f780 == 4) {
          if (*DAT_0000f784 == 0) {
            *DAT_0000f780 = 5;
          }
          word_ptr = DAT_0000f788;
          *DAT_0000f788 =
               (int)(((ulonglong)(TRIG_PERIOD - (uint)(*DAT_0000f76c * ANGLE_SCALE) / 100) / 0x32) /
                    (ulonglong)*DAT_0000f784);
          *DAT_0000f78c = *DAT_0000f78c + *word_ptr;
          if (*DAT_0000f790 == '\0') {
            *DAT_0000f794 = (uint)(*DAT_0000f76c * ANGLE_SCALE) / 100 + *DAT_0000f78c;
            *DAT_0000f708 = 1;
          }
          if (*DAT_0000f790 == '\x01') {
            *DAT_0000f794 = *DAT_0000f78c;
            *DAT_0000f708 = 0;
          }
          scratch_ptr = DAT_0000f760;
          *DAT_0000f760 = *DAT_0000f794 * 100;
          if (*scratch_ptr < 10) {
            *scratch_ptr = 10;
          }
          if ((*DAT_0000f77c * 0x2f8) / 100 + 0xedd < *DAT_0000f794) {
            *DAT_0000f780 = 5;
          }
        }
        if (*DAT_0000f780 == 5) {
          *DAT_0000f708 = 0;
          scratch_ptr = DAT_0000f794;
          *DAT_0000f794 = (*DAT_0000f77c * 0x2f8) / 100 + 0xedd;
          *DAT_0000f760 = *scratch_ptr * 100;
        }
        if (((uint)(*DAT_0000f764 * ANGLE_SCALE) < *DAT_0000f760 ||
             *DAT_0000f764 * ANGLE_SCALE - *DAT_0000f760 == 0) && (*DAT_0000f768 == '\0')) {
          *(volatile int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb94 * ANGLE_SCALE;
        }
        if ((*(volatile uint *)PTR_out_setpoint_0000fb98 <= (uint)(*DAT_0000fb9c * ANGLE_SCALE)) &&
           (*DAT_0000fba0 == '\0')) {
          *(volatile int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb9c * ANGLE_SCALE;
        }
      }
    }
    if (((*g_cfg_word == '\0') && (*(volatile int *)PTR_input_locked_0000fba8 != 0)) &&
       (*DAT_0000fbac == '\x01')) {
      /* —— 运行联锁解除：全部输出复位 —— */
      gpio_outputs_set();
      *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
    }
    if (((*g_cfg_word == '\0') && (*(volatile int *)PTR_input_locked_0000fba8 != 0)) ||
       (*DAT_0000fbc0 < 10)) {
      /* —— 停机斜坡（逐拍降频）—— */
      if (*(volatile int *)PTR_input_locked_0000fba8 == 5) {
        *(volatile undefined4 *)PTR_input_locked_0000fba8 = 4;
        *(volatile uint *)PTR_flag_6c_0000fbb4 = *(volatile uint *)PTR_out_setpoint_0000fb98 / 100;
      }
      ramp_cnt = PTR_flag_68_0000fbb0;
      if (*DAT_0000fbc4 == 0) {
        gpio_outputs_set();
        *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
        *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
        *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
        *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
        if (*g_cfg_word == '\0') {
          fio0_pin22_ctrl(0);
          fio1_pin22_ctrl(0);
        }
        *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
        *PTR_flag_3c_0000fbbc = 0;
      }
      else {
        *(volatile int *)PTR_flag_68_0000fbb0 =
             (int)(((ulonglong)(TRIG_PERIOD - (uint)(*DAT_0000fb9c * ANGLE_SCALE) / 100) / 0x32) /
                  (ulonglong)*DAT_0000fbc4);
        if (*(volatile uint *)ramp_cnt < *(volatile uint *)PTR_flag_6c_0000fbb4) {
          *(volatile undefined4 *)PTR_input_locked_0000fba8 = 4;
          if (*DAT_0000fbc8 == '\0') {
            *PTR_flag_3c_0000fbbc = 1;
          }
          *(volatile int *)PTR_flag_6c_0000fbb4 = *(volatile int *)PTR_flag_6c_0000fbb4 - *(volatile int *)PTR_flag_68_0000fbb0;
          if (*DAT_0000fbc8 == '\x01') {
            *(volatile uint *)PTR_flag_70_0000fbb8 =
                 (uint)(*DAT_0000fb9c * ANGLE_SCALE) / 100 + *(volatile int *)PTR_flag_6c_0000fbb4;
          }
          if (*DAT_0000fbc8 == '\0') {
            *(volatile undefined4 *)PTR_flag_70_0000fbb8 = *(volatile undefined4 *)PTR_flag_6c_0000fbb4;
          }
          *(volatile int *)PTR_out_setpoint_0000fb98 = *(volatile int *)PTR_flag_70_0000fbb8 * 100;
          if (((uint)(*DAT_0000fb94 * ANGLE_SCALE) < *(volatile uint *)PTR_out_setpoint_0000fb98 ||
               *DAT_0000fb94 * ANGLE_SCALE - *(volatile uint *)PTR_out_setpoint_0000fb98 == 0) &&
             (*DAT_0000fba0 == '\0')) {
            *(volatile int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb94 * ANGLE_SCALE;
          }
          if ((*(volatile uint *)PTR_out_setpoint_0000fb98 <= (uint)(*DAT_0000fb9c * ANGLE_SCALE)) &&
             (*DAT_0000fba0 == '\0')) {
            gpio_outputs_set();
            *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
            *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
            *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
            *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
            if (*g_cfg_word == '\0') {
              fio0_pin22_ctrl(0);
              fio1_pin22_ctrl(0);
            }
            *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
            *PTR_flag_3c_0000fbbc = 0;
          }
        }
        else {
          gpio_outputs_set();
          *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
          *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
          *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
          *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
          if (*g_cfg_word == '\0') {
            fio0_pin22_ctrl(0);
            fio1_pin22_ctrl(0);
          }
          *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
          *PTR_flag_3c_0000fbbc = 0;
        }
      }
    }
  }
  return;
}

/* 0x0000F9AA —— 运行/停机预设（cfg_word=1 → 启动；=0 → 停机复位输出） */
void run_stop_preset(void)
{
  if (*g_cfg_word == '\x01') {
    *(volatile int *)PTR_out_setpoint_0000fb98 = DAT_0000fbcc[0xfa] * 100;
    *(volatile undefined4 *)PTR_input_locked_0000fba8 = 5;
  }
  if (*g_cfg_word == '\0') {
    *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = *DAT_0000fbcc;
    *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
  }
  return;
}

/* 0x0000F9E8 —— EINT1 ISR：清中断，置 input_state=2（正转），eint1_flag=1 */
void EINT1_IRQHandler(void)
{
  *(volatile uint *)(PTR_DAT_0000fbd4 + 0x140) = *(volatile uint *)PTR_DAT_0000fbd0 | 2;   /* EXTINT 清 EINT1 */
  if (*g_input_state == '\0') {
    *g_input_state = 2;
  }
  *PTR_eint1_flag_0000fbdc = 1;
  return;
}

/* 0x0000FA0A —— EINT2 ISR：清中断，置 input_state=1（反转），eint2_flag=1 */
void EINT2_IRQHandler(void)
{
  *(volatile uint *)(PTR_DAT_0000fbd4 + 0x140) = *(volatile uint *)PTR_DAT_0000fbd0 | 4;   /* EXTINT 清 EINT2 */
  if (*g_input_state == '\0') {
    *g_input_state = 1;
  }
  *PTR_eint2_flag_0000fbe0 = 1;
  return;
}

/* 0x0000FA2C —— EINT3 ISR：急停/外部控制锁存
 *   0x1000FBD8=input_state、0x1000FBE4=mode_byte（正/反转模式）、
 *   0x1000FBA8=input_locked、0x1000FBEC 消抖计数、
 *   0x1000FBF0=phase_cnt（相位窗口判断）、0x1000FBF4=freq_hz、
 *   0x1000FBF8=hold_count（保持计数超时 → 复位运行）、
 *   后半段为输出预置：按 freq_hz（'2'=50Hz/'<'=60Hz）与 out_phase(0/1) 查表
 *   计算 out_scale/out_div/MR0（0x1000D0C+0x18）与触发使能
 * 局部：ptr=消抖计数/保持计数/输出区复用指针；out_scale=输出比例(0x1000 输出区)指针 */
void EINT3_IRQHandler(void)
{
  volatile uint8_t *ptr;
  volatile uint8_t *out_scale;

  *(volatile uint *)(PTR_DAT_0000fbd4 + 0x140) = *(volatile uint *)PTR_DAT_0000fbd0 | 8;   /* EXTINT 清 EINT3 */
  if (*(volatile int *)PTR_input_locked_0000fba8 == 0) {
    if (*g_input_state == '\x01') {
      *g_mode_byte = 1;
    }
    if (*g_input_state == '\x02') {
      *g_mode_byte = 2;
    }
    *g_input_state = 0;
  }
  *PTR_eint3_flag_0000fbe8 = 1;
  ptr = PTR_debounce_count_0000fbec;
  *PTR_debounce_count_0000fbec = *PTR_debounce_count_0000fbec + '\x01';
  if (9 < (byte)*ptr) {
    *ptr = 0;
    if ((0x60 < (byte)*g_phase_cnt) && ((byte)*g_phase_cnt < 0x68)) {
      *g_freq_hz = 0x32;                 /* '2' = 50Hz 档 */
      *(volatile undefined4 *)PTR_hold_count_0000fbf8 = 0;
    }
    if ((0x51 < (byte)*g_phase_cnt) && ((byte)*g_phase_cnt < 0x56)) {
      *g_freq_hz = 0x3c;                 /* '<' = 60Hz 档 */
      *(volatile undefined4 *)PTR_hold_count_0000fbf8 = 0;
    }
    ptr = PTR_hold_count_0000fbf8;
    if (((((byte)*g_phase_cnt < 0x61) || (0x67 < (byte)*g_phase_cnt)) &&
        (*g_freq_hz == '2')) &&
       (*(volatile int *)PTR_hold_count_0000fbf8 = *(volatile int *)PTR_hold_count_0000fbf8 + 1, 4 < *(volatile uint *)ptr))
    {
      /* —— 50Hz 相位保持超时 → 停机复位 —— */
      *(volatile undefined4 *)ptr = 0;
      *g_freq_hz = 0;
      *g_cfg_word = 0;
      gpio_outputs_set();
      *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
      *(volatile uint *)PTR_out_param_0000fbfc = *(volatile uint *)PTR_out_param_0000fbfc | 0x2000;
    }
    ptr = PTR_hold_count_0000fbf8;
    if ((((byte)*g_phase_cnt < 0x52) || (0x55 < (byte)*g_phase_cnt)) &&
       ((*g_freq_hz == '<' &&
        (*(volatile int *)PTR_hold_count_0000fbf8 = *(volatile int *)PTR_hold_count_0000fbf8 + 1, 4 < *(volatile uint *)ptr)
        ))) {
      /* —— 60Hz 相位保持超时 → 停机复位 —— */
      *(volatile undefined4 *)ptr = 0;
      *g_freq_hz = 0;
      *g_cfg_word = 0;
      gpio_outputs_set();
      *(volatile undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(volatile undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(volatile undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(volatile undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
      *(volatile uint *)PTR_out_param_0000fbfc = *(volatile uint *)PTR_out_param_0000fbfc | 0x2000;
    }
    *g_phase_cnt = 0;
  }
  ptr = PTR_DAT_0001000c;
  if (((*(volatile uint *)PTR_input_locked_00010000 < 2) || (7 < *(volatile uint *)PTR_input_locked_00010000)) ||
     ((*(volatile int *)PTR_out_param_00010004 != 0 || ((byte)*g_freq_hz < 0x32)))) {
    gpio_outputs_set();                               /* 非法态/停机 → 复位输出 */
  }
  else {
    /* —— 输出预置：根据 freq_hz + out_phase 计算触发参数 —— */
    *(volatile undefined4 *)(PTR_DAT_0001000c + 4) = 2;
    *(volatile undefined4 *)ptr = 0xff;
    ptr = g_out_scale;
    if (*g_out_phase == '\0') {
      if (*g_freq_hz == '2') {
        /* 50Hz、单相出 */
        *(volatile undefined4 *)g_out_scale = *(volatile undefined4 *)PTR_out_setpoint_00010014;
        out_scale = g_out_scale;
        *(volatile uint *)g_out_scale = (uint)(*(volatile int *)ptr * 0x58) / 100;
        ptr = g_out_scale;
        *(volatile uint *)g_out_scale = *(volatile uint *)out_scale / 100;
        if (0x2730 < *(volatile uint *)ptr) {
          *(volatile undefined4 *)g_out_scale = 0x2730;
        }
        *(volatile uint *)PTR_out_div_0001001c =
             (uint)((0x2731 - *(volatile int *)g_out_scale) * 10) / 0x22d;
        if (*g_mode_byte == '\x01') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x1800 +
               (uint)(byte)*g_out_fine * 0x38) - *(volatile int *)g_out_scale;
        }
        if (*g_mode_byte == '\x02') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x1814 +
               (uint)(byte)*g_out_fine * 0x38) - *(volatile int *)g_out_scale;
        }
      }
      ptr = g_out_scale;
      if (*g_freq_hz == '<') {
        /* 60Hz、单相出 */
        *(volatile undefined4 *)g_out_scale = *(volatile undefined4 *)PTR_out_setpoint_00010014;
        out_scale = g_out_scale;
        *(volatile uint *)g_out_scale = (uint)(*(volatile int *)ptr * 0x50) / 100;
        ptr = g_out_scale;
        *(volatile uint *)g_out_scale = *(volatile uint *)out_scale / 100;
        if (0x23a0 < *(volatile uint *)ptr) {
          *(volatile undefined4 *)g_out_scale = 0x23a0;
        }
        *(volatile uint *)PTR_out_div_0001001c =
             (uint)((0x23a1 - *(volatile int *)g_out_scale) * 10) / 0x1fb;
        if (*g_mode_byte == '\x01') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x11d7 +
               (uint)(byte)*g_out_fine * 0x33) - *(volatile int *)g_out_scale;
        }
        if (*g_mode_byte == '\x02') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x11eb +
               (uint)(byte)*g_out_fine * 0x33) - *(volatile int *)g_out_scale;
        }
      }
    }
    ptr = g_out_scale;
    if (*g_out_phase == '\x01') {
      if (*g_freq_hz == '2') {
        /* 50Hz、三相出 */
        *(volatile undefined4 *)g_out_scale = *(volatile undefined4 *)PTR_out_setpoint_00010014;
        out_scale = g_out_scale;
        *(volatile uint *)g_out_scale = (uint)(*(volatile int *)ptr << 7) / 100;
        ptr = g_out_scale;
        *(volatile uint *)g_out_scale = *(volatile uint *)out_scale / 100;
        if (0x3903 < *(volatile uint *)ptr) {
          *(volatile undefined4 *)g_out_scale = 0x3903;
        }
        *(volatile uint *)PTR_out_div_0001001c =
             (uint)((0x3904 - *(volatile int *)g_out_scale) * 10) / 0x32b;
        if (*g_mode_byte == '\x01') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x2ab5 +
               (uint)(byte)*g_out_fine * 0x38) - *(volatile int *)g_out_scale;
        }
        if (*g_mode_byte == '\x02') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x2ac9 +
               (uint)(byte)*g_out_fine * 0x38) - *(volatile int *)g_out_scale;
        }
      }
      ptr = g_out_scale;
      if (*g_freq_hz == '<') {
        /* 60Hz、三相出 */
        *(volatile undefined4 *)g_out_scale = *(volatile undefined4 *)PTR_out_setpoint_00010014;
        out_scale = g_out_scale;
        *(volatile uint *)g_out_scale = (uint)(*(volatile int *)ptr * 0x70) / 100;
        ptr = g_out_scale;
        *(volatile uint *)g_out_scale = *(volatile uint *)out_scale / 100;
        if (0x31e0 < *(volatile uint *)ptr) {
          *(volatile undefined4 *)g_out_scale = 0x31e0;
        }
        *(volatile uint *)PTR_out_div_0001001c =
             (uint)((0x31e1 - *(volatile int *)g_out_scale) * 10) / 0x2c5;
        if (*g_mode_byte == '\x01') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x20af +
               (uint)(byte)*g_out_fine * 0x33) - *(volatile int *)g_out_scale;
        }
        if (*g_mode_byte == '\x02') {
          *(volatile uint *)(PTR_DAT_0001000c + 0x18) =
               (*(volatile int *)PTR_out_freq_adj_00010024 * 10 + 0x20b9 +
               (uint)(byte)*g_out_fine * 0x33) - *(volatile int *)g_out_scale;
        }
      }
    }
    *(volatile undefined4 *)(PTR_DAT_0001000c + 4) = 1;
  }
  return;
}

/* 0x0000FF48 —— TIMER2 ISR：清中断、disp_scan 复位、TIMER1 周期重载
 *   局部：timer_base = TIMER 寄存器基址（先 PTR_DAT_0001000c 后 00010030） */
void TIMER2_IRQHandler(void)
{
  volatile uint8_t *timer_base;

  timer_base = PTR_DAT_0001000c;
  *(volatile undefined4 *)PTR_DAT_0001000c = 0xff;             /* TIMER1 IR 清中断 */
  *(volatile undefined4 *)(timer_base + 4) = 2;
  *PTR_disp_scan_0001002c = 0;
  timer_base = PTR_DAT_00010030;
  *(volatile undefined4 *)(PTR_DAT_00010030 + 4) = 2;
  *(volatile undefined4 *)timer_base = 0xff;
  *(volatile undefined4 *)(timer_base + 0x18) = 0x36;              /* MR0=0x36 触发周期 */
  *(volatile undefined4 *)(timer_base + 4) = 1;
  return;
}

/* 0x0000FF6C —— TIMER1 ISR：LCD 12864 动态扫描
 *   disp_scan(0x1000102C/0x10001058/0x10001064) 行计数（0..0xF0，每 0x28 行一组，
 *   共 4 个区域/页），按 mode_byte 与奇偶行控制 FIO1/FIO2 各 COM/SEG 位；
 *   PTR_DAT_00010030=TIMER1 基址，MR0 按 freq_hz('2'=0x488/'<'=0x261) 或 0x36 逐行
 * 局部：fio1/fio2 = FIO 池各区域基址（PTR_DAT_00010034/00010454/00010640，字节偏移） */
void TIMER1_IRQHandler(void)
{
  volatile uint8_t *fio1;
  volatile uint8_t *fio2;

  fio1 = PTR_DAT_00010030;
  *(volatile undefined4 *)PTR_DAT_00010030 = 0xff;
  *(volatile undefined4 *)(fio1 + 4) = 2;
  fio1 = PTR_disp_scan_0001002c;
  *PTR_disp_scan_0001002c = *PTR_disp_scan_0001002c + '\x01';
  if ((uint)(byte)*fio1 == ((byte)*fio1 / 0x28) * 0x28) {
    if (*g_freq_hz == '2') {
      *(volatile undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x488;
    }
    if (*g_freq_hz == '<') {
      *(volatile undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x261;
    }
  }
  else {
    *(volatile undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x36;
  }
  if ((byte)*PTR_disp_scan_0001002c < 0xf1) {
    *(volatile undefined4 *)(PTR_DAT_00010030 + 4) = 1;
  }
  fio2 = PTR_DAT_00010454;
  fio1 = PTR_DAT_00010034;
  if ((*PTR_disp_scan_0001002c != '\0') && ((byte)*PTR_disp_scan_0001002c < 0x29)) {
    /* —— 区域 0（行 1..0x28）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_0001002c == ((int)(uint)(byte)*PTR_disp_scan_0001002c >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010454 + 0x58) = *(volatile uint *)(PTR_DAT_00010454 + 0x58) | 0x200;
        *(volatile uint *)(fio2 + 0x18) = *(volatile uint *)(fio2 + 0x18) | 0x80000;
        *(volatile uint *)(fio2 + 0x58) = *(volatile uint *)(fio2 + 0x58) | 0x20;
        *(volatile uint *)(fio2 + 0x58) = *(volatile uint *)(fio2 + 0x58) | 0x40;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010034 + 0x5c) = *(volatile uint *)(PTR_DAT_00010034 + 0x5c) | 0x200;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80000;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x20;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x40;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x20000;
      *(volatile uint *)(fio2 + 0x18) = *(volatile uint *)(fio2 + 0x18) | 0x10000;
      *(volatile uint *)(fio2 + 0x58) = *(volatile uint *)(fio2 + 0x58) | 0x80;
      *(volatile uint *)(fio2 + 0x18) = *(volatile uint *)(fio2 + 0x18) | 0x100;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x20000;
      *(volatile uint *)(fio2 + 0x1c) = *(volatile uint *)(fio2 + 0x1c) | 0x10000;
      *(volatile uint *)(fio2 + 0x5c) = *(volatile uint *)(fio2 + 0x5c) | 0x80;
      *(volatile uint *)(fio2 + 0x1c) = *(volatile uint *)(fio2 + 0x1c) | 0x100;
    }
  }
  fio1 = PTR_DAT_00010454;
  if ((0x28 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0x51)) {
    /* —— 区域 1（行 0x29..0x50）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x10000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x80000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x100;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x40;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x10000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x100;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x40;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x80000;
      *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x10000;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x40;
      *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x100;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x80000;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x10000;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x40;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x100;
    }
  }
  fio1 = PTR_DAT_00010454;
  if ((0x50 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0x79)) {
    /* —— 区域 2（行 0x51..0x78）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x10000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x20000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x100;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x80;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x10000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x20000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x100;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x80;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x80000;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x200;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x40;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x20;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x80000;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x200;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x40;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x20;
    }
  }
  fio1 = PTR_DAT_00010454;
  if ((0x78 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0xa1)) {
    /* —— 区域 3（行 0x79..0xA0）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x8000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x20000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x80;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x80;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x8000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x20000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x80;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x40000;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x200;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x100;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x20;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x40000;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x200;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x100;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x20;
    }
  }
  fio2 = PTR_DAT_00010640;
  fio1 = PTR_DAT_00010454;
  if ((0xa0 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0xc9)) {
    /* —— 区域 4（行 0xA1..0xC8）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010454 + 0x18) = *(volatile uint *)(PTR_DAT_00010454 + 0x18) | 0x8000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x40000;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x80;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x100;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x8000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x40000;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x100;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010640 + 0x18) = *(volatile uint *)(PTR_DAT_00010640 + 0x18) | 0x40000;
      *(volatile uint *)(fio2 + 0x18) = *(volatile uint *)(fio2 + 0x18) | 0x8000;
      *(volatile uint *)(fio2 + 0x58) = *(volatile uint *)(fio2 + 0x58) | 0x100;
      *(volatile uint *)(fio2 + 0x18) = *(volatile uint *)(fio2 + 0x18) | 0x80;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010454 + 0x1c) = *(volatile uint *)(PTR_DAT_00010454 + 0x1c) | 0x40000;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x8000;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x100;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80;
    }
  }
  fio1 = PTR_DAT_00010640;
  if ((200 < (byte)*PTR_disp_scan_00010644) && ((byte)*PTR_disp_scan_00010644 < 0xf1)) {
    /* —— 区域 5（行 0xC9..0xF0）—— */
    if (*g_mode_byte == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010644 == ((int)(uint)(byte)*PTR_disp_scan_00010644 >> 1) * 2
         ) {
        *(volatile uint *)(PTR_DAT_00010640 + 0x58) = *(volatile uint *)(PTR_DAT_00010640 + 0x58) | 0x200;
        *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x40000;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x20;
        *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x100;
      }
      else {
        *(volatile uint *)(PTR_DAT_00010640 + 0x5c) = *(volatile uint *)(PTR_DAT_00010640 + 0x5c) | 0x200;
        *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x40000;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x20;
        *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x100;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010644 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010644 >> 1) * 2) {
      *(volatile uint *)(PTR_DAT_00010640 + 0x18) = *(volatile uint *)(PTR_DAT_00010640 + 0x18) | 0x20000;
      *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x8000;
      *(volatile uint *)(fio1 + 0x58) = *(volatile uint *)(fio1 + 0x58) | 0x80;
      *(volatile uint *)(fio1 + 0x18) = *(volatile uint *)(fio1 + 0x18) | 0x80;
    }
    else {
      *(volatile uint *)(PTR_DAT_00010640 + 0x1c) = *(volatile uint *)(PTR_DAT_00010640 + 0x1c) | 0x20000;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x8000;
      *(volatile uint *)(fio1 + 0x5c) = *(volatile uint *)(fio1 + 0x5c) | 0x80;
      *(volatile uint *)(fio1 + 0x1c) = *(volatile uint *)(fio1 + 0x1c) | 0x80;
    }
  }
  if (0xf0 < (byte)*PTR_disp_scan_00010644) {
    *PTR_disp_scan_00010644 = 0;
  }
  return;
}
