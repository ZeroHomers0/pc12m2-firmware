/* =============================================================================
 * src/03_input_debounce.c — 反编译模块 03（输入/去抖）可编译副本
 * 目标B 阶段4：原样复制，仅加 includes。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"

/* 0x00001578 —— 输入引脚方向配置（置输入模式）
 *   DAT_00001974=0x2009C000（FIO 池基址）
 *   [8]  = FIO0DIR（+0x20）；[0x18]=FIO1DIR（+0x60）
 *   配置为输入：P0.28/27（0x10000000/0x8000000）、P1.16/17（0x10000/0x20000）、
 *   P0.9（0x200）、P0.6（0x40）、P0.2/0.3（0x4/0x8） */
void gpio_inputs_dir_init(void)
{
  volatile uint32_t *fio;

  fio = DAT_00001974;
  DAT_00001974[8] = DAT_00001974[8] & 0xfff7ffff;   /* P1.17 输入 */
  fio[8] = fio[8] & 0xfffbffff;               /* P1.16 输入 */
  *fio = *fio & 0xbfffffff;                   /* P0.30 输入 */
  *fio = *fio & 0xdfffffff;                   /* P0.29 输入 */
  fio[0x18] = fio[0x18] & 0xfdffffff;         /* P2.25 输入 */
  fio[0x18] = fio[0x18] & 0xfbffffff;         /* P2.24 输入 */
  *fio = *fio & 0xf7ffffff;                   /* P0.27 输入 */
  *fio = *fio & 0xefffffff;                   /* P0.28 输入 */
  fio[8] = fio[8] & 0xfffeffff;               /* P1.16 输入 */
  fio[8] = fio[8] & 0xfffdffff;               /* P1.17 输入 */
  *fio = *fio & 0xffffffbf;                   /* P0.6 输入 */
  *fio = *fio & 0xfffffffb;                   /* P0.3 输入 */
  *fio = *fio & 0xfffffff7;                   /* P0.2 输入 */
  return;
}

/* 0x000015FE —— 输入扫描状态机（旋转编码器 + RUN/STOP 按钮解码）
 *   首次检测任何输入变化 → 计数 0x10001978 累加；
 *   连续采样 0x19(25) 拍后按 A/B 相组合锁存旋转方向到 0x1000197C（1..6）；
 *   计数超 0xF9 → 0xF5 并产生按键事件码：
 *     0x0B=编码器加（慢，计数>10 触发，含复位 0x10001980 复用）
 *     0x16=编码器加（A=1,B=0，快）
 *     0x21=编码器减（A=1,B=1,P1.16=0）
 *     0x17=编码器减（A=1,B=0,P1.16=0）
 *     0x0E=编码器加（A=0,B=1,P1.16=0，计数>0x1E 触发）
 *   （0x16/0x21 = state_machine 的 r4 快进/快退码；0x0B/0x17/0x0E 为慢/组合键）
 *   全部输入就绪（非 0）→ 返回 0x1000197C 锁存值 */
undefined1 input_scan_state(void)
{
  volatile uint32_t *scan_cnt;
  volatile uint8_t *cnt_p;

  scan_cnt = DAT_00001978;
  /* —— 任一输入未就绪（P1.16/P1.17/P0.28/P0.27/P2.9/P2.8 为 0）→ 进入扫描 —— */
  if ((((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) == 0) ||
        ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) == 0)) ||
       ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) == 0)) ||
      (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) == 0 ||
       ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) == 0)))) ||
     ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) == 0)) {
    *DAT_00001978 = *DAT_00001978 + 1;
    if (*scan_cnt == 0x19) {
      /* 连续 0x19 拍后锁存旋转方向（A/B 相组合，1..6） */
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))))) {
        *DAT_0000197c = 1;
      }
      if (((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) == 0)) &&
          (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)))))) &&
         ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)) {
        *DAT_0000197c = 2;
      }
      if (((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) == 0)) &&
         ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))) {
        *DAT_0000197c = 3;
      }
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) == 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))))) {
        *DAT_0000197c = 4;
      }
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
         ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) == 0)))) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))) {
        *DAT_0000197c = 5;
      }
      if ((((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0)) &&
          (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)))) &&
         ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) == 0)) {
        *DAT_0000197c = 6;
      }
    }
    if (0xf9 < *DAT_00001978) {
      *DAT_00001978 = 0xf5;
      *DAT_0000197c = 0;
      cnt_p = DAT_00001980;
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))))) {
        *DAT_00001980 = *DAT_00001980 + 1;
        if (0xb < *cnt_p) {
          *cnt_p = 0xb;
        }
        if (*DAT_00001980 == 10) {
          return 0xb;          /* 0x0B = 慢加事件（计数 10） */
        }
      }
      cnt_p = DAT_00001980;
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) == 0)) &&
         ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)))) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))) {
        return 0x16;           /* 0x16 = 快加（A=1,B=0） */
      }
      if (((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) == 0)) &&
         ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))) {
        return 0x21;           /* 0x21 = 快减（P0.28=1,P0.27=1,P1.16=0） */
      }
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) == 0)) &&
         (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) == 0 &&
          ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x74) & 0x4000000) != 0)))))) {
        *DAT_00001980 = *DAT_00001980 + 1;
        if (0x1f < *cnt_p) {
          *cnt_p = 0x1f;
        }
        if (*DAT_00001980 == 0x1e) {
          return 0x17;         /* 0x17 = 慢减事件（计数 0x1E=30） */
        }
      }
      cnt_p = DAT_00001c10;
      if ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint *)((uint32_t)DAT_00001974 + 0x34) & 0x40000) != 0)) &&
         ((((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint *)((uint32_t)DAT_00001974 + 0x14) & 0x20000000) == 0 &&
            ((*(volatile uint *)(DAT_00001c0c + 0x74) & 0x2000000) != 0)))) &&
          ((*(volatile uint *)(DAT_00001c0c + 0x74) & 0x4000000) != 0)))) {
        *DAT_00001c10 = *DAT_00001c10 + 1;
        if (0x1f < *cnt_p) {
          *cnt_p = 0x1f;
        }
        if (*DAT_00001c10 == 0x1e) {
          return 0xe;          /* 0x0E = 组合键事件（计数 0x1E） */
        }
      }
    }
  }
  else {
    /* —— 全部输入就绪（稳定）—— */
    if (0x18 < *DAT_00001978) {
      *DAT_00001978 = 0;
      return *DAT_0000197c;    /* 返回锁存方向 */
    }
    *DAT_0000197c = 0;
    *DAT_00001978 = 0;
    *DAT_00001980 = 0;
  }
  return 0;
}

/* 0x000019C6 —— RUN/STOP 按钮扫描（P0.28/P0.27）
 *   P0.28 低且 P0.27 高（RUN 按下，计数 0x32=50）→ 返回 7
 *   P0.28 高且 P0.27 低（STOP 按下，计数 0x32）→ 返回 8
 *   双模式：0x10001C14==0 单次触发；!=0 保持模式（0x10001C20 锁存 7/8）
 *   0x10001C0C = FIO 池基址（0x2009C000） */
undefined1 scan_run_stop(void)
{
  volatile uint8_t *cnt_p;
  undefined1 evt;

  cnt_p = DAT_00001c18;
  if (*DAT_00001c14 == '\0') {
    /* —— 单次触发模式 —— */
    if (((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x10000000) == 0) &&
       ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x8000000) != 0)) {
      *DAT_00001c18 = *DAT_00001c18 + 1;
      if (*cnt_p == 0x32) {
        *cnt_p = 0x32;
        return 7;              /* RUN */
      }
      if (0x32 < *DAT_00001c18) {
        *DAT_00001c18 = 0x32;
      }
    }
    else {
      *DAT_00001c18 = 0;
    }
    cnt_p = DAT_00001c1c;
    if (((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x10000000) == 0) ||
       ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x8000000) != 0)) {
      *DAT_00001c1c = 0;
    }
    else {
      *DAT_00001c1c = *DAT_00001c1c + 1;
      if (*cnt_p == 0x32) {
        *cnt_p = 0x32;
        return 8;              /* STOP */
      }
      if (0x32 < *DAT_00001c1c) {
        *DAT_00001c1c = 0x32;
      }
    }
    evt = 0;
  }
  else {
    /* —— 保持模式：去抖后锁存到 0x10001C20 —— */
    if ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x10000000) == 0) {
      *DAT_00001c18 = *DAT_00001c18 + 1;
      if (0x31 < *cnt_p) {
        *cnt_p = 0x32;
        *DAT_00001c20 = 7;
      }
    }
    else {
      *DAT_00001c18 = 0;
    }
    cnt_p = DAT_00001c1c;
    if ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x10000000) == 0) {
      *DAT_00001c1c = 0;
    }
    else {
      *DAT_00001c1c = *DAT_00001c1c + 1;
      if (0x31 < *cnt_p) {
        *cnt_p = 0x32;
        *DAT_00001c20 = 8;
      }
    }
    evt = *DAT_00001c20;
  }
  return evt;
}

/* 0x00001AB8 —— P0.9 去抖（24V 交流方波检测，阈值 0xF=15）：
 *   持续高 0xF 拍 → 返回 1（清零计数）；低 → 清零 */
undefined4 debounce_p09(void)
{
  volatile uint8_t *p_cnt;

  p_cnt = DAT_00001c24;
  if ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x200) == 0) {   /* P0.9 低 */
    *DAT_00001c24 = 0;
  }
  else {
    *DAT_00001c24 = *DAT_00001c24 + 1;
    if (0xf < *p_cnt) {
      *p_cnt = 0;
      return 1;
    }
  }
  return 0;
}

/* 0x00001AE6 —— P1.16 去抖（外故障，阈值 0xFA=250）：
 *   高持续 0xFA 拍 → 返回 1；低持续 0xFA 拍 → 返回 2（边沿双向） */
undefined4 debounce_p116(void)
{
  volatile uint8_t *cnt_p;

  cnt_p = DAT_00001c28;
  if ((*(volatile uint *)(DAT_00001c0c + 0x34) & 0x10000) == 0) {   /* P1.16 低 */
    *DAT_00001c28 = 0;
  }
  else {
    *DAT_00001c28 = *DAT_00001c28 + 1;
    if (0xfa < *cnt_p) {
      *cnt_p = 0xfa;
      return 1;              /* 高沿 */
    }
  }
  cnt_p = DAT_00001c2c;
  if ((*(volatile uint *)(DAT_00001c0c + 0x34) & 0x10000) == 0) {
    *DAT_00001c2c = *DAT_00001c2c + 1;
    if (0xfa < *cnt_p) {
      *cnt_p = 0xfa;
      return 2;              /* 低沿 */
    }
  }
  else {
    *DAT_00001c2c = 0;
  }
  return 0;
}

/* 0x00001B3E —— P1.17 去抖（复位，阈值 0x32=50，同 P1.16 结构） */
undefined4 debounce_p117(void)
{
  volatile uint8_t *cnt_p;

  cnt_p = DAT_00001c30;
  if ((*(volatile uint *)(DAT_00001c0c + 0x34) & 0x20000) == 0) {   /* P1.17 低 */
    *DAT_00001c30 = 0;
  }
  else {
    *DAT_00001c30 = *DAT_00001c30 + 1;
    if (0x32 < *cnt_p) {
      *cnt_p = 0x32;
      return 1;
    }
  }
  cnt_p = DAT_00001c34;
  if ((*(volatile uint *)(DAT_00001c0c + 0x34) & 0x20000) == 0) {
    *DAT_00001c34 = *DAT_00001c34 + 1;
    if (0x32 < *cnt_p) {
      *cnt_p = 0x32;
      return 2;
    }
  }
  else {
    *DAT_00001c34 = 0;
  }
  return 0;
}

/* 0x00001B96 —— P0.6 去抖（急停，阈值 0x32=50） */
undefined4 debounce_p06(void)
{
  volatile uint8_t *cnt_p;

  cnt_p = DAT_00001c38;
  if ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x40) == 0) {      /* P0.6 低 */
    *DAT_00001c38 = 0;
  }
  else {
    *DAT_00001c38 = *DAT_00001c38 + 1;
    if (0x32 < *cnt_p) {
      *cnt_p = 0x32;
      return 1;
    }
  }
  cnt_p = DAT_00001c3c;
  if ((*(volatile uint *)(DAT_00001c0c + 0x14) & 0x40) == 0) {
    *DAT_00001c3c = *DAT_00001c3c + 1;
    if (0x32 < *cnt_p) {
      *cnt_p = 0x32;
      return 2;
    }
  }
  else {
    *DAT_00001c3c = 0;
  }
  return 0;
}

/* 0x00001BEE —— P0.2 与 P0.3 双低联锁检查：同时为 0 → 返回 1（安全联锁），否则 0
 *   main 中开机判断：<1 才进主循环，否则显示联锁错误屏 */
undefined4 chk_p02_p03(void)
{
  undefined4 ret;

  if (((*(volatile uint *)(DAT_00001c0c + 0x14) & 4) == 0) && ((*(volatile uint *)(DAT_00001c0c + 0x14) & 8) == 0)) {
    ret = 1;
  }
  else {
    ret = 0;
  }
  return ret;
}

/* 0x000010F8C —— P0 输入初始化（P0.2/3/28/27 等置输入，配合 read_input_p02） */
void gpio0_input_init(void)
{
  volatile uint32_t *fio;

  fio = DAT_00010fcc;
  *DAT_00010fcc = *DAT_00010fcc & 0xfffffffb;   /* P0.3 输入 */
  *fio = *fio & 0xefffffff;               /* P0.28 输入 */
  *fio = *fio & 0xf7ffffff;               /* P0.27 输入 */
  return;
}

/* 0x000010FAE —— 读 P0.2 状态到 0x100010FD0（RUN/STOP 模式选择读取） */
undefined4 read_input_p02(void)
{
  if ((*(volatile uint *)((uint32_t)DAT_00010fcc + 0x14) & 4) == 0) {
    *DAT_00010fd0 = 0;
  }
  else {
    *DAT_00010fd0 = 1;
  }
  return 0;
}
