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
 *   触发角：180°(0xB4) - 当前角；周期基量 0x2C88、角度系数 0x18BD(6333)/100
 *   软起/软停斜坡（状态机 0x1000EE24/0x1000F268/0x1000F780：0=停、4=运行、5=稳定）
 *   闭环：closed_loop_wrapper(PID) 计算输出角；上下限钳位 0x1000F2C8/0x1000F2D0
 *   保护：过压(0x1000EDE4..) / 过流(0x1000EDF8..) / 缺相(0x1000EE04..)，
 *   故障标志 0x1000EDF4（只置不清锁存，需断电复位；2026-08-21 反编译确证）：
 *     bit4=过压、bit5=过流、bit3=缺相(判据×0.15/1.5/2 且延时×50/20/10)、
 *     bit9=缺相严重级(判据×2.5/3/3.5 且延时×5/2/1)
 *   停机斜坡（cfg_word=0 且联锁非 0 时）：0x1000FBB0/6C/70 逐拍降频
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 触发 GPIO（G1-G6 / P12-G1~G6）与驱动链 → docs/HARDWARE_VERIFICATION_2026-08-20.md §二.2、§六.B
 *   · SCR 触发时序（TIMER2.MR0 / TIMER1 扫描 240 步 / 12° 脉宽）→ docs/PROGRESS_2026-08-20.md §4j、§4k
 *   · 保护标志 / 停机链 → APPLICATION_GUIDE_2026-08-21.md §四.1
 *   · 菜单映射（软起/相位限制/主从偏移/起始相位）→ docs/MENU_PARAMETER_MAPPING.md §1
 *   ✅ EINT 引脚差异复核完成（2026-08-21）：eint1/2/3_init 反编译确证 PINSEL4
 *     配置 P2.11/12/13=EINT1/2/3（硬件印证正确）；P0.22=fio0_pin22_ctrl=运行继电器 RLY1。
 * ========================================================================== */

/* 0x0000E5A8 —— 引脚功能/方向配置（PINSEL+FIO DIR，详见 HARDWARE_VERIFICATION）
 *   PTR_DAT_0000E988 = FIO 池基址 0x2009C000
 *   FIO0DIR(P0.20/21/22、P0.15..19、P0.7/8、P0.4/5)、FIO1/FIO2/FIO3/FIO4 方向 */
void pin_config(void)
{
  undefined *puVar1;

  puVar1 = PTR_DAT_0000e988;
  *(uint *)PTR_DAT_0000e988 = *(uint *)PTR_DAT_0000e988 | 0x100000;   /* FIO0DIR P0.20 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x200000;                       /* P0.21 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x400000;                       /* P0.22 */
  *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x100000;     /* FIO0CLR P0.20 */
  *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x200000;     /* P0.21 */
  *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x400000;     /* P0.22 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x20000;                        /* P0.17 触发组 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x40000;                        /* P0.18 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x80000;                        /* P0.19 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 0x200;        /* FIO1DIR P1.9 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x10000;                        /* P0.16 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x8000;                         /* P0.15 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 0x100;        /* P1.8 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 0x80;         /* P1.7 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 0x40;         /* P1.6 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 0x20;         /* P1.5 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x100;                          /* P0.8 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x80;                           /* P0.7 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x20000;      /* FIO0SET P0.17 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x40000;      /* P0.18 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80000;      /* P0.19 */
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x200;        /* FIO2DIR P2.9 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x10000;      /* FIO0SET P0.16 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x8000;       /* P0.15 */
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x100;        /* P2.8 */
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x80;         /* P2.7 */
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x40;         /* P2.6 */
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x20;         /* P2.5 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x100;        /* FIO0SET P0.8 */
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80;         /* P0.7 */
  *(uint *)(puVar1 + 0x20) = *(uint *)(puVar1 + 0x20) | 0x100000;     /* FIO2DIR P2.20 */
  *(uint *)(puVar1 + 0x20) = *(uint *)(puVar1 + 0x20) | 0x200000;     /* P2.21 */
  *(uint *)(puVar1 + 0x20) = *(uint *)(puVar1 + 0x20) | 0x400000;     /* P2.22 */
  *(uint *)(puVar1 + 0x20) = *(uint *)(puVar1 + 0x20) | 0x800000;     /* P2.23 */
  *(uint *)(puVar1 + 0x3c) = *(uint *)(puVar1 + 0x3c) | 0x100000;     /* FIO3DIR P3.20 */
  *(uint *)(puVar1 + 0x3c) = *(uint *)(puVar1 + 0x3c) | 0x200000;     /* P3.21 */
  *(uint *)(puVar1 + 0x3c) = *(uint *)(puVar1 + 0x3c) | 0x400000;     /* P3.22 */
  *(uint *)(puVar1 + 0x3c) = *(uint *)(puVar1 + 0x3c) | 0x800000;     /* P3.23 */
  *(uint *)(puVar1 + 0x80) = *DAT_0000e98c | 0x10000000;              /* FIO3DIR P3.28 */
  *(uint *)(puVar1 + 0x80) = *(uint *)(puVar1 + 0x80) | 0x20000000;   /* P3.29 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x10;                           /* P0.4 */
  *(uint *)puVar1 = *(uint *)puVar1 | 0x20;                           /* P0.5 */
  *(uint *)(puVar1 + 0x40) = *(uint *)(puVar1 + 0x40) | 1;            /* FIO1DIR P1.0 */
  *(uint *)(puVar1 + 0x9c) = *(uint *)(puVar1 + 0x9c) | 0x10000000;   /* FIO4CLR P4.28 */
  *(uint *)(puVar1 + 0x9c) = *(uint *)(puVar1 + 0x9c) | 0x20000000;   /* P4.29 */
  *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x10;         /* FIO0CLR P0.4 */
  *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x20;         /* P0.5 */
  *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 1;            /* FIO2CLR P2.0 */
  return;
}

/* 0x0000E79A —— 输出使能（关断时所有触发/使能线复位） */
void gpio_outputs_set(void)
{
  undefined *puVar1;

  puVar1 = PTR_DAT_0000e988;
  *(uint *)(PTR_DAT_0000e988 + 0x18) = *(uint *)(PTR_DAT_0000e988 + 0x18) | 0x20000;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x40000;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80000;
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x200;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x10000;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x8000;
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x100;
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x80;
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x40;
  *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x20;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x100;
  *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80;
  return;
}

/* 0x0000E816 —— TIMER1 初始化（IRQ2）：MR0=999、匹配中断+复位、预分频 2 */
void timer1_init(void)
{
  undefined4 *puVar1;

  puVar1 = DAT_0000e990;                              /* TIMER1 0x40008000 */
  DAT_0000e990[1] = 2;                                /* PR=2 */
  puVar1[3] = 0x18;                                   /* MCR：MR0 中断+复位 */
  puVar1[6] = 999;                                    /* MR0=999 */
  *puVar1 = 0xff;                                     /* IR 清中断 */
  puVar1[5] = 3;                                      /* TCR：使能+复位 */
  nvic_enable_irq(2);                                 /* NVIC TIMER1_IRQ */
  return;
}

/* 0x0000E838 —— TIMER2 初始化（IRQ3）：PCONP TMR2、MR0=999、匹配中断+复位 */
void timer2_init(void)
{
  undefined4 *puVar1;

  *(uint *)(DAT_0000e998 + 0xc4) = *DAT_0000e994 | 0x400000;   /* PCONP bit22 TMR2 */
  puVar1 = DAT_0000e99c;                              /* TIMER2 0x40090000 */
  DAT_0000e99c[1] = 2;
  puVar1[3] = 0x18;
  puVar1[6] = 999;
  *puVar1 = 0xff;
  puVar1[5] = 3;
  nvic_enable_irq();                                  /* NVIC TIMER2_IRQ */
  return;
}

/* 0x0000E86E —— EINT1 外部中断初始化（P2.11，边沿触发，清挂起，NVIC 0x13=19） */
void eint1_init(void)
{
  int iVar1;

  *(uint *)(DAT_0000e998 + 0x140) = *DAT_0000e9a0 | 2;      /* EXTINT 清 EINT1 */
  iVar1 = DAT_0000e9a4;                                     /* PINSEL 0x4002C000 */
  *(uint *)(DAT_0000e9a4 + 0x10) = *(uint *)(DAT_0000e9a4 + 0x10) | 0x400000;  /* PINSEL4 P2.11=EINT1（bit22=1、bit23=0） */
  *(uint *)(iVar1 + 0x10) = *(uint *)(iVar1 + 0x10) & 0xff7fffff;
  iVar1 = DAT_0000e998;                                     /* SCB 0x400FC000 */
  *(uint *)(DAT_0000e998 + 0x148) = *(uint *)(DAT_0000e998 + 0x148) | 2;      /* EXTMODE 边沿 */
  *(uint *)(iVar1 + 0x14c) = *(uint *)(iVar1 + 0x14c) & 0xfffffffd;          /* EXTPOLAR 下降沿 */
  *(uint *)(iVar1 + 0x140) = *(uint *)(iVar1 + 0x140) | 2;  /* EXTINT 清 */
  nvic_enable_irq(0x13);                                    /* NVIC EINT1_IRQ=19 */
  return;
}

/* 0x0000E8CA —— EINT2 外部中断初始化（P2.12，边沿触发，NVIC 0x14=20） */
void eint2_init(void)
{
  int iVar1;

  iVar1 = DAT_0000e9a4;
  *(uint *)(DAT_0000e9a4 + 0x10) = *(uint *)(DAT_0000e9a4 + 0x10) | 0x1000000; /* PINSEL4 P2.12=EINT2（bit24=1、bit25=0） */
  *(uint *)(iVar1 + 0x10) = *(uint *)(iVar1 + 0x10) & 0xfdffffff;
  iVar1 = DAT_0000e998;
  *(uint *)(DAT_0000e998 + 0x148) = *DAT_0000e9a8 | 4;      /* EXTMODE 边沿 */
  *(uint *)(iVar1 + 0x14c) = *(uint *)(iVar1 + 0x14c) & 0xfffffffb;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x14);                                    /* NVIC EINT2_IRQ=20 */
  return;
}

/* 0x0000E908 —— EINT3 外部中断初始化（P2.13，边沿触发，NVIC 0x15=21） */
void eint3_init(void)
{
  int iVar1;

  iVar1 = DAT_0000e9a4;
  *(uint *)(DAT_0000e9a4 + 0x10) = *(uint *)(DAT_0000e9a4 + 0x10) | 0x4000000; /* PINSEL4 P2.13=EINT3（bit26=1、bit27=0） */
  *(uint *)(iVar1 + 0x10) = *(uint *)(iVar1 + 0x10) & 0xf7ffffff;
  iVar1 = DAT_0000e998;
  *(uint *)(DAT_0000e998 + 0x148) = *DAT_0000e9a8 | 8;      /* EXTMODE 边沿 */
  *(uint *)(iVar1 + 0x14c) = *(uint *)(iVar1 + 0x14c) & 0xfffffff7;          /* EXTPOLAR 下降沿 */
  nvic_enable_irq(0x15);                                    /* NVIC EINT3_IRQ=21 */
  return;
}

/* 0x0000E946 —— P1.22 电平控制（param_1>=1 置位、<1 清零；触发/运行指示） */
void fio1_pin22_ctrl(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_0000e988 + 0x3c) = *(uint *)(PTR_DAT_0000e988 + 0x3c) | 0x400000;
  }
  else {
    *(uint *)(PTR_DAT_0000e988 + 0x38) = *(uint *)(PTR_DAT_0000e988 + 0x38) | 0x400000;
  }
  return;
}

/* 0x0000E966 —— P0.22 电平控制（param_1>=1 置位、<1 清零；运行继电器 RLY1，2026-08-21 复核） */
void fio0_pin22_ctrl(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_0000e988 + 0x1c) = *(uint *)(PTR_DAT_0000e988 + 0x1c) | 0x400000;
  }
  else {
    *(uint *)(PTR_DAT_0000e988 + 0x18) = *(uint *)(PTR_DAT_0000e988 + 0x18) | 0x400000;
  }
  return;
}

/* 0x0000E9AC —— 输出级主处理（SCR 移相触发角计算 + 保护 + 软起停 + PID）
 *   详见文件头说明。 */
void output_stage(void)
{
  byte *pbVar1;
  int *piVar2;
  uint *puVar3;
  uint *puVar4;
  undefined *puVar5;
  uint uVar6;

  pbVar1 = DAT_0000edac;
  if ((*DAT_0000eda8 != 1) && (*DAT_0000edac = *DAT_0000edac + 1, 9 < *pbVar1)) {
    *pbVar1 = 0;
    piVar2 = DAT_0000edb4;
    *DAT_0000edb4 = 0xb4 - *DAT_0000edb0;            /* 触发角 = 180° - 当前角 */
    if (*piVar2 == 0) {
      *piVar2 = 1;
    }
    *DAT_0000edbc = 0xb4 - *DAT_0000edb8;
    if (*DAT_0000edc0 == '\0') {
      *DAT_0000edc4 = 0;
      if (*DAT_0000edc8 == '\0') {
        *DAT_0000edd0 = *DAT_0000edcc / 0xf;         /* 周期/15 换算 */
      }
      if (*DAT_0000edc8 == '\x01') {
        *DAT_0000edd0 = *DAT_0000edd4 / 0xf;
      }
      if (*DAT_0000edc8 == '\x02') {
        *DAT_0000eddc = (*DAT_0000edd8 * 1000) / *DAT_0000edcc;
        *DAT_0000edd0 = *DAT_0000edcc / 0xf;
      }
    }
    if ((*DAT_0000edc0 == '\x01') && (9 < *DAT_0000ede0)) {
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
      puVar3 = DAT_0000ee14;
      *DAT_0000ee14 = *DAT_0000ee14 + 1;
      if (0x5dc < *puVar3) {                          /* 1500 拍 → 欠压/锁定标志 */
        *DAT_0000ee14 = 0;
        *DAT_0000edc4 = 1;
      }
      if (*DAT_0000edc8 == '\0') {
        /* —— 软起动斜坡状态机（0x1000EE24：0=停 4=运行 5=稳定）—— */
        *DAT_0000ee1c = *DAT_0000ee18;
        *DAT_0000ee20 = 0;
        if (*DAT_0000ee24 == 0) {
          *DAT_0000ee24 = 4;
          piVar2 = DAT_0000ee28;
          *DAT_0000ee28 = 0;
          *DAT_0000ee2c = *piVar2;
          *DAT_0000ee30 = 0;
          *DAT_0000ee34 = 0;
          *DAT_0000ee38 = 0;
          *DAT_0000ee3c = 0;
          *DAT_0000ee40 = 0x1771;                     /* 软起累加初始 6001 */
        }
        if (*DAT_0000ee24 == 4) {
          *DAT_0000ee1c = *DAT_0000ede0;
          piVar2 = DAT_0000ee48;
          *DAT_0000ee48 =                             /* 触发步进 = (0x2C88-角*6333/100)/50/除数 */
               (int)(((ulonglong)(0x2c88 - (uint)(*DAT_0000edb4 * 0x18bd) / 100) / 0x32) /
                    (ulonglong)*DAT_0000ee44);
          *DAT_0000ee4c = *DAT_0000ee4c + *piVar2;
          if (*DAT_0000ee50 == '\0') {
            *DAT_0000ee54 = (uint)(*DAT_0000edb4 * 0x18bd) / 100 + *DAT_0000ee4c;
            *DAT_0000ee58 = 1;
          }
          if (*DAT_0000ee50 == '\x01') {
            *DAT_0000ee54 = *DAT_0000ee4c;
            *DAT_0000ee58 = 0;
          }
          piVar2 = DAT_0000ee28;
          *DAT_0000ee28 = *DAT_0000ee54 * 100;
          *DAT_0000ee5c = *piVar2;
          *DAT_0000ee2c = *DAT_0000ee28;
          *DAT_0000ee30 = 0;
          *DAT_0000ee34 = 0;
          if ((*DAT_0000ee1c * 0x2f8) / 100 + 0xedd < *DAT_0000ee54) {  /* 达目标 → 5 稳定 */
            *DAT_0000ee24 = 5;
          }
          if ((*DAT_0000edd8 <= *DAT_0000ee0c) && (*DAT_0000edd8 <= *DAT_0000edcc)) {
            *DAT_0000ee24 = 5;
          }
          if (*DAT_0000ee60 < *DAT_0000ee64) {
            *DAT_0000f268 = 5;
          }
        }
        if (*DAT_0000f268 == 5) {
          /* —— 第一路闭环运行（0x1000F270..F2A4 反馈/给定比较 + PID）—— */
          *DAT_0000f26c = 0;
          puVar3 = DAT_0000f284;
          if ((*DAT_0000f270 < *DAT_0000f274) || (*DAT_0000f27c <= *DAT_0000f278)) {
            *DAT_0000f284 = *DAT_0000f284 + 1;
            if (0x32 < *puVar3) {
              *puVar3 = 0;
              *DAT_0000f280 = 2;
              *DAT_0000f288 = *DAT_0000f288;
              *DAT_0000f28c = '\0';
            }
          }
          else {
            *DAT_0000f280 = 1;
            *DAT_0000f284 = 0;
          }
          puVar3 = DAT_0000f294;
          if (*DAT_0000f27c < *DAT_0000f290) {
            *DAT_0000f294 = *DAT_0000f294 + 1;
            if (10 < *puVar3) {
              *puVar3 = 0;
              *DAT_0000f280 = 0;
              *DAT_0000f288 = *DAT_0000f288;
              *DAT_0000f28c = '\0';
            }
          }
          else {
            *DAT_0000f294 = 0;
          }
          if (((*DAT_0000f290 <= *DAT_0000f27c) && (*DAT_0000f274 <= *DAT_0000f298)) &&
             (*DAT_0000f280 == 2)) {
            *DAT_0000f27c = *DAT_0000f290;
            puVar3 = DAT_0000f290;
            *DAT_0000f290 = *DAT_0000f290 + 1;
            if (6000 < *puVar3) {
              *DAT_0000f290 = 6000;
            }
            *DAT_0000f288 = *DAT_0000f288;
            *DAT_0000f28c = '\0';
          }
          if (((*DAT_0000f274 <= *DAT_0000f298) && (*DAT_0000f280 == 1)) &&
             (*DAT_0000f29c < *DAT_0000f298)) {
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
          if (((*DAT_0000f288 <= *DAT_0000f27c) && (*DAT_0000f274 <= *DAT_0000f298)) &&
             ((*DAT_0000f280 == 1 && (*DAT_0000f298 <= *DAT_0000f29c)))) {
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
          if (((*DAT_0000f278 < 2) && (*DAT_0000f274 <= *DAT_0000f298)) && (*DAT_0000f280 == 1)) {
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
          puVar3 = DAT_0000f2b4;
          if (((*DAT_0000f2a8 < 5) && (*DAT_0000f2ac == '\x01')) && (*DAT_0000f2b0 == '\x01')) {
            *DAT_0000f2b4 = *DAT_0000f2b4 + 1;
            if (100 < *puVar3) {
              *DAT_0000f2b8 = *DAT_0000f2b8 | 0x800;
            }
          }
          else {
            *DAT_0000f2b4 = 0;
          }
          uVar6 = closed_loop_wrapper(*DAT_0000f27c,*DAT_0000f288,*DAT_0000f2c0,*DAT_0000f2bc);
          *DAT_0000f2c4 = uVar6;
        }
        if (((uint)(*DAT_0000f2c8 * 0x18bd) < *DAT_0000f2c4 ||
             *DAT_0000f2c8 * 0x18bd - *DAT_0000f2c4 == 0) && (*DAT_0000f2cc == '\0')) {
          *DAT_0000f2c4 = *DAT_0000f2c8 * 0x18bd;     /* 输出上限钳位 */
        }
        if ((*DAT_0000f2c4 <= (uint)(*DAT_0000f2d0 * 0x18bd)) && (*DAT_0000f2cc == '\0')) {
          *DAT_0000f2c4 = *DAT_0000f2d0 * 0x18bd;     /* 输出下限钳位 */
        }
      }
      if (*DAT_0000f2d4 == '\x01') {
        /* —— 第二路闭环（0x1000F2E8.. 软起 + PID + 钳位）—— */
        *DAT_0000f2d8 = *DAT_0000f27c;
        *DAT_0000f2a0 = 0;
        if (*DAT_0000f268 == 0) {
          *DAT_0000f268 = 4;
          puVar3 = DAT_0000f2c4;
          *DAT_0000f2c4 = 0;
          *DAT_0000f2dc = *puVar3;
          *DAT_0000f2e0 = 0;
          *DAT_0000f2e4 = 0;
          *DAT_0000f2b4 = 0;
          *DAT_0000f28c = '\0';
          *DAT_0000f290 = 0x1771;
        }
        if (*DAT_0000f268 == 4) {
          *DAT_0000f2d8 = *DAT_0000f2e8;
          piVar2 = DAT_0000f2f0;
          *DAT_0000f2f0 =
               (int)(((ulonglong)(0x2c88 - (uint)(*DAT_0000f2d0 * 0x18bd) / 100) / 0x32) /
                    (ulonglong)*DAT_0000f2ec);
          *DAT_0000f2f4 = *DAT_0000f2f4 + *piVar2;
          if (*DAT_0000f2f8 == '\0') {
            *DAT_0000f2fc = (uint)(*DAT_0000f2d0 * 0x18bd) / 100 + *DAT_0000f2f4;
            *DAT_0000f26c = 1;
          }
          if (*DAT_0000f2f8 == '\x01') {
            *DAT_0000f2fc = *DAT_0000f2f4;
            *DAT_0000f26c = 0;
          }
          puVar3 = DAT_0000f2c4;
          *DAT_0000f2c4 = *DAT_0000f2fc * 100;
          *DAT_0000f300 = *puVar3;
          *DAT_0000f2dc = *DAT_0000f2c4;
          *DAT_0000f2e0 = 0;
          *DAT_0000f2e4 = 0;
          if ((*DAT_0000f2d8 * 0x2f8) / 100 + 0xedd < *DAT_0000f2fc) {
            *DAT_0000f268 = 5;
          }
          if ((*DAT_0000f304 <= *DAT_0000f278) && (*DAT_0000f304 <= *DAT_0000f29c)) {
            *DAT_0000f268 = 5;
          }
          if (*DAT_0000f308 < *DAT_0000f2a4) {
            *DAT_0000f268 = 5;
          }
        }
        if (*DAT_0000f268 == 5) {
          /* —— 第二路闭环运行（0x1000F70C..F740）—— */
          *DAT_0000f708 = 0;
          puVar3 = DAT_0000f720;
          if ((*DAT_0000f70c < *DAT_0000f710) || (*DAT_0000f718 <= *DAT_0000f714)) {
            *DAT_0000f720 = *DAT_0000f720 + 1;
            if (0x32 < *puVar3) {
              *puVar3 = 0;
              *DAT_0000f71c = 2;
              *DAT_0000f724 = *DAT_0000f724;
              *DAT_0000f728 = '\0';
            }
          }
          else {
            *DAT_0000f71c = 1;
            *DAT_0000f720 = 0;
          }
          puVar3 = DAT_0000f730;
          if (*DAT_0000f718 < *DAT_0000f72c) {
            *DAT_0000f730 = *DAT_0000f730 + 1;
            if (10 < *puVar3) {
              *puVar3 = 0;
              *DAT_0000f71c = 0;
              *DAT_0000f724 = *DAT_0000f724;
              *DAT_0000f728 = '\0';
            }
          }
          else {
            *DAT_0000f730 = 0;
          }
          if (((*DAT_0000f72c <= *DAT_0000f718) && (*DAT_0000f710 <= *DAT_0000f734)) &&
             (*DAT_0000f71c == 2)) {
            *DAT_0000f718 = *DAT_0000f72c;
            puVar3 = DAT_0000f72c;
            *DAT_0000f72c = *DAT_0000f72c + 1;
            if (6000 < *puVar3) {
              *DAT_0000f72c = 6000;
            }
            *DAT_0000f724 = *DAT_0000f724;
            *DAT_0000f728 = '\0';
          }
          if (((*DAT_0000f710 <= *DAT_0000f734) && (*DAT_0000f71c == 1)) &&
             (*DAT_0000f738 < *DAT_0000f734)) {
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
          if (((*DAT_0000f724 <= *DAT_0000f718) && (*DAT_0000f710 <= *DAT_0000f734)) &&
             ((*DAT_0000f71c == 1 && (*DAT_0000f734 <= *DAT_0000f738)))) {
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
          if (((*DAT_0000f714 < 2) && (*DAT_0000f710 <= *DAT_0000f734)) && (*DAT_0000f71c == 1)) {
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
          puVar3 = DAT_0000f750;
          if (((*DAT_0000f744 < 5) && (*DAT_0000f748 == '\x01')) && (*DAT_0000f74c == '\x01')) {
            *DAT_0000f750 = *DAT_0000f750 + 1;
            if (100 < *puVar3) {
              *DAT_0000f754 = *DAT_0000f754 | 0x800;
            }
          }
          else {
            *DAT_0000f750 = 0;
          }
          uVar6 = closed_loop_wrapper(*DAT_0000f718,*DAT_0000f724,*DAT_0000f75c,*DAT_0000f758);
          *DAT_0000f760 = uVar6;
        }
        if (((uint)(*DAT_0000f764 * 0x18bd) < *DAT_0000f760 ||
             *DAT_0000f764 * 0x18bd - *DAT_0000f760 == 0) && (*DAT_0000f768 == '\0')) {
          *DAT_0000f760 = *DAT_0000f764 * 0x18bd;
        }
        if ((*DAT_0000f760 <= (uint)(*DAT_0000f76c * 0x18bd)) && (*DAT_0000f768 == '\0')) {
          *DAT_0000f760 = *DAT_0000f76c * 0x18bd;
        }
      }
      puVar3 = DAT_0000f778;
      if (*DAT_0000f770 == '\x02') {
        /* —— 恒压源模式（0x1000F774..F794）—— */
        *DAT_0000f778 = *DAT_0000f774;
        puVar4 = DAT_0000f77c;
        *DAT_0000f77c = *puVar3 % 10000;
        if (1000 < *puVar4) {
          *puVar4 = 1000;
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
          piVar2 = DAT_0000f788;
          *DAT_0000f788 =
               (int)(((ulonglong)(0x2c88 - (uint)(*DAT_0000f76c * 0x18bd) / 100) / 0x32) /
                    (ulonglong)*DAT_0000f784);
          *DAT_0000f78c = *DAT_0000f78c + *piVar2;
          if (*DAT_0000f790 == '\0') {
            *DAT_0000f794 = (uint)(*DAT_0000f76c * 0x18bd) / 100 + *DAT_0000f78c;
            *DAT_0000f708 = 1;
          }
          if (*DAT_0000f790 == '\x01') {
            *DAT_0000f794 = *DAT_0000f78c;
            *DAT_0000f708 = 0;
          }
          puVar3 = DAT_0000f760;
          *DAT_0000f760 = *DAT_0000f794 * 100;
          if (*puVar3 < 10) {
            *puVar3 = 10;
          }
          if ((*DAT_0000f77c * 0x2f8) / 100 + 0xedd < *DAT_0000f794) {
            *DAT_0000f780 = 5;
          }
        }
        if (*DAT_0000f780 == 5) {
          *DAT_0000f708 = 0;
          puVar3 = DAT_0000f794;
          *DAT_0000f794 = (*DAT_0000f77c * 0x2f8) / 100 + 0xedd;
          *DAT_0000f760 = *puVar3 * 100;
        }
        if (((uint)(*DAT_0000f764 * 0x18bd) < *DAT_0000f760 ||
             *DAT_0000f764 * 0x18bd - *DAT_0000f760 == 0) && (*DAT_0000f768 == '\0')) {
          *(int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb94 * 0x18bd;
        }
        if ((*(uint *)PTR_out_setpoint_0000fb98 <= (uint)(*DAT_0000fb9c * 0x18bd)) &&
           (*DAT_0000fba0 == '\0')) {
          *(int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb9c * 0x18bd;
        }
      }
    }
    if (((*PTR_cfg_word_0000fba4 == '\0') && (*(int *)PTR_input_locked_0000fba8 != 0)) &&
       (*DAT_0000fbac == '\x01')) {
      /* —— 运行联锁解除：全部输出复位 —— */
      gpio_outputs_set();
      *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl();
      fio1_pin22_ctrl(0);
      *(undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
    }
    if (((*PTR_cfg_word_0000fba4 == '\0') && (*(int *)PTR_input_locked_0000fba8 != 0)) ||
       (*DAT_0000fbc0 < 10)) {
      /* —— 停机斜坡（逐拍降频）—— */
      if (*(int *)PTR_input_locked_0000fba8 == 5) {
        *(undefined4 *)PTR_input_locked_0000fba8 = 4;
        *(uint *)PTR_flag_6c_0000fbb4 = *(uint *)PTR_out_setpoint_0000fb98 / 100;
      }
      puVar5 = PTR_flag_68_0000fbb0;
      if (*DAT_0000fbc4 == 0) {
        gpio_outputs_set();
        *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
        *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
        *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
        *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
        if (*PTR_cfg_word_0000fba4 == '\0') {
          fio0_pin22_ctrl(0);
          fio1_pin22_ctrl(0);
        }
        *(undefined4 *)PTR_input_locked_0000fba8 = 0;
        *PTR_flag_3c_0000fbbc = 0;
      }
      else {
        *(int *)PTR_flag_68_0000fbb0 =
             (int)(((ulonglong)(0x2c88 - (uint)(*DAT_0000fb9c * 0x18bd) / 100) / 0x32) /
                  (ulonglong)*DAT_0000fbc4);
        if (*(uint *)puVar5 < *(uint *)PTR_flag_6c_0000fbb4) {
          *(undefined4 *)PTR_input_locked_0000fba8 = 4;
          if (*DAT_0000fbc8 == '\0') {
            *PTR_flag_3c_0000fbbc = 1;
          }
          *(int *)PTR_flag_6c_0000fbb4 = *(int *)PTR_flag_6c_0000fbb4 - *(int *)PTR_flag_68_0000fbb0;
          if (*DAT_0000fbc8 == '\x01') {
            *(uint *)PTR_flag_70_0000fbb8 =
                 (uint)(*DAT_0000fb9c * 0x18bd) / 100 + *(int *)PTR_flag_6c_0000fbb4;
          }
          if (*DAT_0000fbc8 == '\0') {
            *(undefined4 *)PTR_flag_70_0000fbb8 = *(undefined4 *)PTR_flag_6c_0000fbb4;
          }
          *(int *)PTR_out_setpoint_0000fb98 = *(int *)PTR_flag_70_0000fbb8 * 100;
          if (((uint)(*DAT_0000fb94 * 0x18bd) < *(uint *)PTR_out_setpoint_0000fb98 ||
               *DAT_0000fb94 * 0x18bd - *(uint *)PTR_out_setpoint_0000fb98 == 0) &&
             (*DAT_0000fba0 == '\0')) {
            *(int *)PTR_out_setpoint_0000fb98 = *DAT_0000fb94 * 0x18bd;
          }
          if ((*(uint *)PTR_out_setpoint_0000fb98 <= (uint)(*DAT_0000fb9c * 0x18bd)) &&
             (*DAT_0000fba0 == '\0')) {
            gpio_outputs_set();
            *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
            *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
            *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
            *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
            if (*PTR_cfg_word_0000fba4 == '\0') {
              fio0_pin22_ctrl(0);
              fio1_pin22_ctrl(0);
            }
            *(undefined4 *)PTR_input_locked_0000fba8 = 0;
            *PTR_flag_3c_0000fbbc = 0;
          }
        }
        else {
          gpio_outputs_set();
          *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
          *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
          *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
          *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
          if (*PTR_cfg_word_0000fba4 == '\0') {
            fio0_pin22_ctrl(0);
            fio1_pin22_ctrl(0);
          }
          *(undefined4 *)PTR_input_locked_0000fba8 = 0;
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
  if (*PTR_cfg_word_0000fba4 == '\x01') {
    *(int *)PTR_out_setpoint_0000fb98 = DAT_0000fbcc[0xfa] * 100;
    *(undefined4 *)PTR_input_locked_0000fba8 = 5;
  }
  if (*PTR_cfg_word_0000fba4 == '\0') {
    *(undefined4 *)PTR_out_setpoint_0000fb98 = *DAT_0000fbcc;
    *(undefined4 *)PTR_input_locked_0000fba8 = 0;
    fio0_pin22_ctrl();
    fio1_pin22_ctrl(0);
  }
  return;
}

/* 0x0000F9E8 —— EINT1 ISR：清中断，置 input_state=2（正转），eint1_flag=1 */
void EINT1_IRQHandler(void)
{
  *(uint *)(PTR_DAT_0000fbd4 + 0x140) = *(uint *)PTR_DAT_0000fbd0 | 2;   /* EXTINT 清 EINT1 */
  if (*PTR_input_state_0000fbd8 == '\0') {
    *PTR_input_state_0000fbd8 = 2;
  }
  *PTR_eint1_flag_0000fbdc = 1;
  return;
}

/* 0x0000FA0A —— EINT2 ISR：清中断，置 input_state=1（反转），eint2_flag=1 */
void EINT2_IRQHandler(void)
{
  *(uint *)(PTR_DAT_0000fbd4 + 0x140) = *(uint *)PTR_DAT_0000fbd0 | 4;   /* EXTINT 清 EINT2 */
  if (*PTR_input_state_0000fbd8 == '\0') {
    *PTR_input_state_0000fbd8 = 1;
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
 *   计算 out_scale/out_div/MR0（0x1000D0C+0x18）与触发使能 */
void EINT3_IRQHandler(void)
{
  undefined *puVar1;
  undefined *puVar2;

  *(uint *)(PTR_DAT_0000fbd4 + 0x140) = *(uint *)PTR_DAT_0000fbd0 | 8;   /* EXTINT 清 EINT3 */
  if (*(int *)PTR_input_locked_0000fba8 == 0) {
    if (*PTR_input_state_0000fbd8 == '\x01') {
      *PTR_mode_byte_0000fbe4 = 1;
    }
    if (*PTR_input_state_0000fbd8 == '\x02') {
      *PTR_mode_byte_0000fbe4 = 2;
    }
    *PTR_input_state_0000fbd8 = 0;
  }
  *PTR_eint3_flag_0000fbe8 = 1;
  puVar1 = PTR_debounce_count_0000fbec;
  *PTR_debounce_count_0000fbec = *PTR_debounce_count_0000fbec + '\x01';
  if (9 < (byte)*puVar1) {
    *puVar1 = 0;
    if ((0x60 < (byte)*PTR_phase_cnt_0000fbf0) && ((byte)*PTR_phase_cnt_0000fbf0 < 0x68)) {
      *PTR_freq_hz_0000fbf4 = 0x32;                 /* '2' = 50Hz 档 */
      *(undefined4 *)PTR_hold_count_0000fbf8 = 0;
    }
    if ((0x51 < (byte)*PTR_phase_cnt_0000fbf0) && ((byte)*PTR_phase_cnt_0000fbf0 < 0x56)) {
      *PTR_freq_hz_0000fbf4 = 0x3c;                 /* '<' = 60Hz 档 */
      *(undefined4 *)PTR_hold_count_0000fbf8 = 0;
    }
    puVar1 = PTR_hold_count_0000fbf8;
    if (((((byte)*PTR_phase_cnt_0000fbf0 < 0x61) || (0x67 < (byte)*PTR_phase_cnt_0000fbf0)) &&
        (*PTR_freq_hz_0000fbf4 == '2')) &&
       (*(int *)PTR_hold_count_0000fbf8 = *(int *)PTR_hold_count_0000fbf8 + 1, 4 < *(uint *)puVar1))
    {
      /* —— 50Hz 相位保持超时 → 停机复位 —— */
      *(undefined4 *)puVar1 = 0;
      *PTR_freq_hz_0000fbf4 = 0;
      *PTR_cfg_word_0000fba4 = 0;
      gpio_outputs_set();
      *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl();
      fio1_pin22_ctrl(0);
      *(undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
      *(uint *)PTR_out_param_0000fbfc = *(uint *)PTR_out_param_0000fbfc | 0x2000;
    }
    puVar1 = PTR_hold_count_0000fbf8;
    if ((((byte)*PTR_phase_cnt_0000fbf0 < 0x52) || (0x55 < (byte)*PTR_phase_cnt_0000fbf0)) &&
       ((*PTR_freq_hz_0000fbf4 == '<' &&
        (*(int *)PTR_hold_count_0000fbf8 = *(int *)PTR_hold_count_0000fbf8 + 1, 4 < *(uint *)puVar1)
        ))) {
      /* —— 60Hz 相位保持超时 → 停机复位 —— */
      *(undefined4 *)puVar1 = 0;
      *PTR_freq_hz_0000fbf4 = 0;
      *PTR_cfg_word_0000fba4 = 0;
      gpio_outputs_set();
      *(undefined4 *)PTR_flag_68_0000fbb0 = 0;
      *(undefined4 *)PTR_flag_6c_0000fbb4 = 0;
      *(undefined4 *)PTR_flag_70_0000fbb8 = 0;
      *(undefined4 *)PTR_out_setpoint_0000fb98 = 0;
      fio0_pin22_ctrl();
      fio1_pin22_ctrl(0);
      *(undefined4 *)PTR_input_locked_0000fba8 = 0;
      *PTR_flag_3c_0000fbbc = 0;
      *(uint *)PTR_out_param_0000fbfc = *(uint *)PTR_out_param_0000fbfc | 0x2000;
    }
    *PTR_phase_cnt_0000fffc = 0;
  }
  puVar1 = PTR_DAT_0001000c;
  if (((*(uint *)PTR_input_locked_00010000 < 2) || (7 < *(uint *)PTR_input_locked_00010000)) ||
     ((*(int *)PTR_out_param_00010004 != 0 || ((byte)*PTR_freq_hz_00010008 < 0x32)))) {
    gpio_outputs_set();                               /* 非法态/停机 → 复位输出 */
  }
  else {
    /* —— 输出预置：根据 freq_hz + out_phase 计算触发参数 —— */
    *(undefined4 *)(PTR_DAT_0001000c + 4) = 2;
    *(undefined4 *)puVar1 = 0xff;
    puVar1 = PTR_out_scale_00010018;
    if (*PTR_out_phase_00010010 == '\0') {
      if (*PTR_freq_hz_00010008 == '2') {
        /* 50Hz、单相出 */
        *(undefined4 *)PTR_out_scale_00010018 = *(undefined4 *)PTR_out_setpoint_00010014;
        puVar2 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = (uint)(*(int *)puVar1 * 0x58) / 100;
        puVar1 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = *(uint *)puVar2 / 100;
        if (0x2730 < *(uint *)puVar1) {
          *(undefined4 *)PTR_out_scale_00010018 = 0x2730;
        }
        *(uint *)PTR_out_div_0001001c =
             (uint)((0x2731 - *(int *)PTR_out_scale_00010018) * 10) / 0x22d;
        if (*PTR_mode_byte_00010020 == '\x01') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x1800 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x38) - *(int *)PTR_out_scale_00010018;
        }
        if (*PTR_mode_byte_00010020 == '\x02') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x1814 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x38) - *(int *)PTR_out_scale_00010018;
        }
      }
      puVar1 = PTR_out_scale_00010018;
      if (*PTR_freq_hz_00010008 == '<') {
        /* 60Hz、单相出 */
        *(undefined4 *)PTR_out_scale_00010018 = *(undefined4 *)PTR_out_setpoint_00010014;
        puVar2 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = (uint)(*(int *)puVar1 * 0x50) / 100;
        puVar1 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = *(uint *)puVar2 / 100;
        if (0x23a0 < *(uint *)puVar1) {
          *(undefined4 *)PTR_out_scale_00010018 = 0x23a0;
        }
        *(uint *)PTR_out_div_0001001c =
             (uint)((0x23a1 - *(int *)PTR_out_scale_00010018) * 10) / 0x1fb;
        if (*PTR_mode_byte_00010020 == '\x01') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x11d7 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x33) - *(int *)PTR_out_scale_00010018;
        }
        if (*PTR_mode_byte_00010020 == '\x02') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x11eb +
               (uint)(byte)*PTR_out_fine_00010028 * 0x33) - *(int *)PTR_out_scale_00010018;
        }
      }
    }
    puVar1 = PTR_out_scale_00010018;
    if (*PTR_out_phase_00010010 == '\x01') {
      if (*PTR_freq_hz_00010008 == '2') {
        /* 50Hz、三相出 */
        *(undefined4 *)PTR_out_scale_00010018 = *(undefined4 *)PTR_out_setpoint_00010014;
        puVar2 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = (uint)(*(int *)puVar1 << 7) / 100;
        puVar1 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = *(uint *)puVar2 / 100;
        if (0x3903 < *(uint *)puVar1) {
          *(undefined4 *)PTR_out_scale_00010018 = 0x3903;
        }
        *(uint *)PTR_out_div_0001001c =
             (uint)(((int)&DAT_00003904 - *(int *)PTR_out_scale_00010018) * 10) / 0x32b;
        if (*PTR_mode_byte_00010020 == '\x01') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x2ab5 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x38) - *(int *)PTR_out_scale_00010018;
        }
        if (*PTR_mode_byte_00010020 == '\x02') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x2ac9 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x38) - *(int *)PTR_out_scale_00010018;
        }
      }
      puVar1 = PTR_out_scale_00010018;
      if (*PTR_freq_hz_00010008 == '<') {
        /* 60Hz、三相出 */
        *(undefined4 *)PTR_out_scale_00010018 = *(undefined4 *)PTR_out_setpoint_00010014;
        puVar2 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = (uint)(*(int *)puVar1 * 0x70) / 100;
        puVar1 = PTR_out_scale_00010018;
        *(uint *)PTR_out_scale_00010018 = *(uint *)puVar2 / 100;
        if (0x31e0 < *(uint *)puVar1) {
          *(undefined4 *)PTR_out_scale_00010018 = 0x31e0;
        }
        *(uint *)PTR_out_div_0001001c =
             (uint)((0x31e1 - *(int *)PTR_out_scale_00010018) * 10) / 0x2c5;
        if (*PTR_mode_byte_00010020 == '\x01') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x20af +
               (uint)(byte)*PTR_out_fine_00010028 * 0x33) - *(int *)PTR_out_scale_00010018;
        }
        if (*PTR_mode_byte_00010020 == '\x02') {
          *(uint *)(PTR_DAT_0001000c + 0x18) =
               (*(int *)PTR_out_freq_adj_00010024 * 10 + 0x20b9 +
               (uint)(byte)*PTR_out_fine_00010028 * 0x33) - *(int *)PTR_out_scale_00010018;
        }
      }
    }
    *(undefined4 *)(PTR_DAT_0001000c + 4) = 1;
  }
  return;
}

/* 0x0000FF48 —— TIMER2 ISR：清中断、disp_scan 复位、TIMER1 周期重载 */
void TIMER2_IRQHandler(void)
{
  undefined *puVar1;

  puVar1 = PTR_DAT_0001000c;
  *(undefined4 *)PTR_DAT_0001000c = 0xff;             /* TIMER1 IR 清中断 */
  *(undefined4 *)(puVar1 + 4) = 2;
  *PTR_disp_scan_0001002c = 0;
  puVar1 = PTR_DAT_00010030;
  *(undefined4 *)(PTR_DAT_00010030 + 4) = 2;
  *(undefined4 *)puVar1 = 0xff;
  *(undefined4 *)(puVar1 + 0x18) = 0x36;              /* MR0=0x36 触发周期 */
  *(undefined4 *)(puVar1 + 4) = 1;
  return;
}

/* 0x0000FF6C —— TIMER1 ISR：LCD 12864 动态扫描
 *   disp_scan(0x1000102C/0x10001058/0x10001064) 行计数（0..0xF0，每 0x28 行一组，
 *   共 4 个区域/页），按 mode_byte 与奇偶行控制 FIO1/FIO2 各 COM/SEG 位；
 *   PTR_DAT_00010030=TIMER1 基址，MR0 按 freq_hz('2'=0x488/'<'=0x261) 或 0x36 逐行 */
void TIMER1_IRQHandler(void)
{
  undefined *puVar1;
  undefined *puVar2;

  puVar1 = PTR_DAT_00010030;
  *(undefined4 *)PTR_DAT_00010030 = 0xff;
  *(undefined4 *)(puVar1 + 4) = 2;
  puVar1 = PTR_disp_scan_0001002c;
  *PTR_disp_scan_0001002c = *PTR_disp_scan_0001002c + '\x01';
  if ((uint)(byte)*puVar1 == ((byte)*puVar1 / 0x28) * 0x28) {
    if (*PTR_freq_hz_00010008 == '2') {
      *(undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x488;
    }
    if (*PTR_freq_hz_00010008 == '<') {
      *(undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x261;
    }
  }
  else {
    *(undefined4 *)(PTR_DAT_00010030 + 0x18) = 0x36;
  }
  if ((byte)*PTR_disp_scan_0001002c < 0xf1) {
    *(undefined4 *)(PTR_DAT_00010030 + 4) = 1;
  }
  puVar2 = PTR_DAT_00010454;
  puVar1 = PTR_DAT_00010034;
  if ((*PTR_disp_scan_0001002c != '\0') && ((byte)*PTR_disp_scan_0001002c < 0x29)) {
    /* —— 区域 0（行 1..0x28）—— */
    if (*PTR_mode_byte_00010020 == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_0001002c == ((int)(uint)(byte)*PTR_disp_scan_0001002c >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010454 + 0x58) = *(uint *)(PTR_DAT_00010454 + 0x58) | 0x200;
        *(uint *)(puVar2 + 0x18) = *(uint *)(puVar2 + 0x18) | 0x80000;
        *(uint *)(puVar2 + 0x58) = *(uint *)(puVar2 + 0x58) | 0x20;
        *(uint *)(puVar2 + 0x58) = *(uint *)(puVar2 + 0x58) | 0x40;
      }
      else {
        *(uint *)(PTR_DAT_00010034 + 0x5c) = *(uint *)(PTR_DAT_00010034 + 0x5c) | 0x200;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80000;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x20;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x40;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x20000;
      *(uint *)(puVar2 + 0x18) = *(uint *)(puVar2 + 0x18) | 0x10000;
      *(uint *)(puVar2 + 0x58) = *(uint *)(puVar2 + 0x58) | 0x80;
      *(uint *)(puVar2 + 0x18) = *(uint *)(puVar2 + 0x18) | 0x100;
    }
    else {
      *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x20000;
      *(uint *)(puVar2 + 0x1c) = *(uint *)(puVar2 + 0x1c) | 0x10000;
      *(uint *)(puVar2 + 0x5c) = *(uint *)(puVar2 + 0x5c) | 0x80;
      *(uint *)(puVar2 + 0x1c) = *(uint *)(puVar2 + 0x1c) | 0x100;
    }
  }
  puVar1 = PTR_DAT_00010454;
  if ((0x28 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0x51)) {
    /* —— 区域 1（行 0x29..0x50）—— */
    if (*PTR_mode_byte_0001045c == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x10000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x100;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x40;
      }
      else {
        *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x10000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x100;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x40;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x80000;
      *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x10000;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x40;
      *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x100;
    }
    else {
      *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x80000;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x10000;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x40;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x100;
    }
  }
  puVar1 = PTR_DAT_00010454;
  if ((0x50 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0x79)) {
    /* —— 区域 2（行 0x51..0x78）—— */
    if (*PTR_mode_byte_0001045c == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x10000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x20000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x100;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x80;
      }
      else {
        *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x10000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x20000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x100;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x80;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x80000;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x200;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x40;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x20;
    }
    else {
      *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x80000;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x200;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x40;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x20;
    }
  }
  puVar1 = PTR_DAT_00010454;
  if ((0x78 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0xa1)) {
    /* —— 区域 3（行 0x79..0xA0）—— */
    if (*PTR_mode_byte_0001045c == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x8000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x20000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x80;
      }
      else {
        *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x8000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x20000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x80;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x40000;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x200;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x100;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x20;
    }
    else {
      *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x40000;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x200;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x100;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x20;
    }
  }
  puVar2 = PTR_DAT_00010640;
  puVar1 = PTR_DAT_00010454;
  if ((0xa0 < (byte)*PTR_disp_scan_00010458) && ((byte)*PTR_disp_scan_00010458 < 0xc9)) {
    /* —— 区域 4（行 0xA1..0xC8）—— */
    if (*PTR_mode_byte_0001045c == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010458 == ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010454 + 0x18) = *(uint *)(PTR_DAT_00010454 + 0x18) | 0x8000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x40000;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x100;
      }
      else {
        *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x8000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x40000;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x100;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010458 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010458 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010640 + 0x18) = *(uint *)(PTR_DAT_00010640 + 0x18) | 0x40000;
      *(uint *)(puVar2 + 0x18) = *(uint *)(puVar2 + 0x18) | 0x8000;
      *(uint *)(puVar2 + 0x58) = *(uint *)(puVar2 + 0x58) | 0x100;
      *(uint *)(puVar2 + 0x18) = *(uint *)(puVar2 + 0x18) | 0x80;
    }
    else {
      *(uint *)(PTR_DAT_00010454 + 0x1c) = *(uint *)(PTR_DAT_00010454 + 0x1c) | 0x40000;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x8000;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x100;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80;
    }
  }
  puVar1 = PTR_DAT_00010640;
  if ((200 < (byte)*PTR_disp_scan_00010644) && ((byte)*PTR_disp_scan_00010644 < 0xf1)) {
    /* —— 区域 5（行 0xC9..0xF0）—— */
    if (*PTR_mode_byte_00010648 == '\x01') {
      if ((uint)(byte)*PTR_disp_scan_00010644 == ((int)(uint)(byte)*PTR_disp_scan_00010644 >> 1) * 2
         ) {
        *(uint *)(PTR_DAT_00010640 + 0x58) = *(uint *)(PTR_DAT_00010640 + 0x58) | 0x200;
        *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x40000;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x20;
        *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x100;
      }
      else {
        *(uint *)(PTR_DAT_00010640 + 0x5c) = *(uint *)(PTR_DAT_00010640 + 0x5c) | 0x200;
        *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x40000;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x20;
        *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x100;
      }
    }
    else if ((uint)(byte)*PTR_disp_scan_00010644 ==
             ((int)(uint)(byte)*PTR_disp_scan_00010644 >> 1) * 2) {
      *(uint *)(PTR_DAT_00010640 + 0x18) = *(uint *)(PTR_DAT_00010640 + 0x18) | 0x20000;
      *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x8000;
      *(uint *)(puVar1 + 0x58) = *(uint *)(puVar1 + 0x58) | 0x80;
      *(uint *)(puVar1 + 0x18) = *(uint *)(puVar1 + 0x18) | 0x80;
    }
    else {
      *(uint *)(PTR_DAT_00010640 + 0x1c) = *(uint *)(PTR_DAT_00010640 + 0x1c) | 0x20000;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x8000;
      *(uint *)(puVar1 + 0x5c) = *(uint *)(puVar1 + 0x5c) | 0x80;
      *(uint *)(puVar1 + 0x1c) = *(uint *)(puVar1 + 0x1c) | 0x80;
    }
  }
  if (0xf0 < (byte)*PTR_disp_scan_00010644) {
    *PTR_disp_scan_00010644 = 0;
  }
  return;
}
