/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 11：1-Wire 挑战-应答认证（防抄板，经 ADuM1201 隔离）
 *
 * 认证总线（GPIO2，均经 ADuM1201 数字隔离器）：
 *   P2.1（FIO2 bit1, 0x2）数据线 出（挑战位逐位串出）
 *   P2.2（FIO2 bit2, 0x4）数据线 入（应答位逐位串入）
 *   P2.3（FIO2 bit3, 0x8）时钟线（低/高沿采数据）
 *   P2.4（FIO2 bit4, 0x10）复位/使能线（认证开始拉低，结束释放）
 *   FIO 池 0x2009C000：DAT_0001087c=基址，+0x54 FIO2PIN、+0x58 FIO2SET、+0x5c FIO2CLR
 *   DAT_00010880=0x100020EC 认证超时窗口计数；DAT_000108a8=0x10001730 重试状态机。
 *
 * 流程：auth_challenge 逐位发出 24 位挑战（3 字节），仅 bit8..23 期间的 16 位应答
 *   被读回，与两字节期望值（由板上参数计算）比对；失败走 auth_retry 最多重试 5 次，
 *   每次后 param_sync_live_to_eeprom() 落盘。重试 4 次仍失败则放弃（允许继续运行）。
 *
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 认证链路/引脚 → CLAUDE.md「硬件事实」、docs/HARDWARE_VERIFICATION_2026-08-20.md
 *   · 开机调用序列 → src/01_startup.c（gpio2_init → auth_challenge ×3 → auth_retry）
 *   · GPIO2 方向/初值初始化 → src/13_gpio_init.c（gpio2_init）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"

void param_sync_live_to_eeprom(void);  /* 06_param_system.c */

/* 0x00010696 —— 认证超时窗口设置：把 50000（一个较大计数值）写入 0x100020EC，
 *   用作认证步骤/看门狗的判定窗口（本次挑战允许的等待时长）。 */
void auth_set_timeout(void)
{
  *DAT_00010880 = 50000;
  return;
}

/* 0x000106A0 —— 1-Wire 认证挑战一帧：逐位串出 24 位挑战、串回 16 位应答并比对。
 *
 * 时序（每 bit_idx 一次）：
 *   拉低 P2.1 数据线 → 按挑战位 MSB 决定是否拉高 P2.1（置 1 才 SET）
 *   延时 2000 → P2.3 时钟拉低（下降沿）→ 延时 1000 → bit_idx>7 时读 P2.2（FIO2PIN&4）
 *   延时 1000 → P2.3 时钟拉高。
 * 挑战三字节在 bit_idx==0/8/0x10 处生成写入 challenge_byte：
 *   byte A = (*DAT_10884 + *DAT_10888 + *DAT_1088c + *DAT_10890 + 0x31) & 0xff
 *   byte B = (*DAT_10894 + *DAT_10890 + *DAT_10898 + 0xc) & 0xff
 *   byte C = 固定 0x55
 * 期望应答两字节（对 challenge_byte 的散列混合）：
 *   exp_resp_hi（bit0 组） = (b^0xc2 + b|0x1b + b&0xb2) & 0xff
 *   exp_resp_lo（bit8 组） = (b^0x3f + b|0xa9 + b&0xbc) & 0xff
 * 读回 response_bits（bit8..23 共 16 位）与两期望值比对：高字节比 exp_resp_hi、
 *   低字节比 exp_resp_lo；命中 → *DAT_1089c=0 / *DAT_108a0=0 / *DAT_108a4=1（通过），
 *   否则 → *DAT_1089c=1 / *DAT_108a0=1 / *DAT_108a4=0（失败）。
 * 注意：期望值是高字节在前读回（response_bits>>8），与 bit0/bit8 两组计算对应。 */
void auth_challenge(void)
{
  int fio_base;
  uint challenge_byte;
  uint bit_idx;
  volatile uint delay_cnt;
  uint response_bits;
  uint exp_resp_hi;
  uint exp_resp_lo;

  exp_resp_hi = 0;
  exp_resp_lo = 0;
  challenge_byte = 0;
  response_bits = 0;
  *(volatile uint *)(DAT_0001087c + 0x5c) = *(volatile uint *)(DAT_0001087c + 0x5c) | 0x10;  /* FIO2CLR P2.4=复位线拉低 */
  for (bit_idx = 0; fio_base = DAT_0001087c, bit_idx < 0x18; bit_idx = bit_idx + 1) {  /* 24 bit */
    if (bit_idx == 0) {
      /* 挑战组 A（bit0-7）：4 个板上参数求和 +0x31；期望应答高位 exp_resp_hi */
      challenge_byte = *g_gain_b + (uint)*DAT_00010888 + (uint)*DAT_0001088c + (uint)*g_out_fine + 0x31
      ;
      exp_resp_hi = ((challenge_byte ^ 0xc2) + (challenge_byte | 0x1b) + (challenge_byte & 0xb2)) & 0xff;
    }
    if (bit_idx == 8) {
      /* 挑战组 B（bit8-15）：3 个板上参数求和 +0xc；期望应答低位 exp_resp_lo */
      challenge_byte = (uint)*DAT_00010894 + (uint)*g_out_fine + *DAT_00010898 + 0xc;
      exp_resp_lo = ((challenge_byte ^ 0x3f) + (challenge_byte | 0xa9) + (challenge_byte & 0xbc)) & 0xff;
    }
    if (bit_idx == 0x10) {
      challenge_byte = 0x55;                                 /* 挑战组 C（bit16-23）：固定 0x55 */
    }
    *(volatile uint *)(DAT_0001087c + 0x5c) = *(volatile uint *)(DAT_0001087c + 0x5c) | 2;  /* FIO2CLR P2.1=数据线拉低 */
    if ((challenge_byte & 0x80) != 0) {
      *(volatile uint *)(fio_base + 0x58) = *(volatile uint *)(fio_base + 0x58) | 2;              /* 挑战位=1 → FIO2SET P2.1 */
    }
    challenge_byte = challenge_byte << 1;
    for (delay_cnt = 0; delay_cnt < 2000; delay_cnt++) {             /* 位建立延时 */
    }
    *(volatile uint *)(DAT_0001087c + 0x5c) = *(volatile uint *)(DAT_0001087c + 0x5c) | 8;  /* FIO2CLR P2.3=时钟拉低 */
    for (delay_cnt = 0; delay_cnt < 1000; delay_cnt++) {             /* 时钟低延时 */
    }
    if ((7 < bit_idx) && (response_bits = response_bits * 2, (*(volatile uint *)(DAT_0001087c + 0x54) & 4) != 0)) {
      response_bits = response_bits + 1;                                                  /* bit8 起读 FIO2PIN P2.2 */
    }
    for (delay_cnt = 0; delay_cnt < 1000; delay_cnt++) {             /* 时钟高延时 */
    }
    *(volatile uint *)(DAT_0001087c + 0x58) = *(volatile uint *)(DAT_0001087c + 0x58) | 8;  /* FIO2SET P2.3=时钟拉高 */
  }
  *(volatile uint *)(DAT_0001087c + 0x58) = *(volatile uint *)(DAT_0001087c + 0x58) | 0x10; /* FIO2SET P2.4=复位线释放 */
  if ((exp_resp_hi == response_bits >> 8) && ((response_bits & 0xff) == exp_resp_lo)) {
    *DAT_000108a0 = 0;                                                    /* 应答匹配 → 认证通过 */
    *DAT_0001089c = 0;
    *DAT_000108a4 = 1;
  }
  else {
    *DAT_0001089c = 1;                                                    /* 应答不匹配 → 认证失败 */
    *DAT_000108a0 = 1;
    *DAT_000108a4 = 0;
  }
  return;
}

/* 0x00010820 —— 认证重试：仅当重试状态机 *DAT_000108a8==1 时进入（开机认证未定）。
 *   每次 auth_challenge() 后若成功（*DAT_0001089c==0）→ 状态=2、重试计数=10（跳过）；
 *   若重试计数达到 4 仍失败 → 状态=2（放弃认证，允许继续运行；防锁死）。
 *   每次迭代末尾 param_sync_live_to_eeprom() 落盘参数。
 *   计数 *DAT_000108ac=0x100020F0（字节），pub：p_retry_cnt。 */
void auth_retry(void)
{
  volatile uint8_t *p_retry_cnt;

  p_retry_cnt = DAT_000108ac;
  if (*DAT_000108a8 == 1) {
    *DAT_000108ac = *DAT_000108ac + 1;
    *p_retry_cnt = 0;
    while (*DAT_000108ac < 5) {
      auth_set_timeout();
      auth_challenge();
      if (*DAT_0001089c == '\0') {                  /* 认证成功 */
        *DAT_000108a8 = 2;
        *DAT_000108ac = 10;
      }
      if (*DAT_000108ac == 4) {                     /* 已重试 4 次仍失败 */
        *DAT_000108a8 = 2;                          /* 放弃认证（允许继续运行） */
      }
      param_sync_live_to_eeprom();
      *DAT_000108ac = *DAT_000108ac + 1;
    }
  }
  return;
}
