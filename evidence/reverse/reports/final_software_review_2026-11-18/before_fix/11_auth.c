/* =============================================================================
 * PC12M-2（12 相）LPC1765 反编译源码导出 — 模块 11：1-Wire 挑战-应答认证（防抄板）
 * 固件：pc12m2_orig.bin（12 相孪生板；6p 参照：PC6M-10 11_auth.c）
 *
 * 认证总线（GPIO2，均经 ADuM1201 数字隔离器）：
 *   P2.1（FIO2 bit1, 0x2）数据线 出（挑战位逐位串出）
 *   P2.2（FIO2 bit2, 0x4）数据线 入（应答位逐位串入）
 *   P2.3（FIO2 bit3, 0x8）时钟线（低/高沿采数据）
 *   P2.4（FIO2 bit4, 0x10）复位/使能线（认证开始拉低，结束释放）
 *   FIO 池 0x2009C000：DAT_00010a74=基址，+0x54 FIO2PIN、+0x58 FIO2SET、+0x5c FIO2CLR
 *
 * 12p 认证（与 6p 指令等价、地址平移，但结果变量不同）：
 *   · 0x108D2 auth_set_timeout：超时窗口 0x100020C4 = 50000（6p 0x100020EC）
 *   · 0x108DC auth_challenge：24 位挑战（3 字节），仅 bit8..23 期间的 16 位应答
 *     被读回，与两字节期望值（由板上参数计算）比对；结果直接写
 *     **auth_pass_flag(0x100020C0) = 0/1**（6p 写 *DAT_1089c/108a0/108a4）
 *   · 0x10A38 auth_verify_loop：循环调 auth_set_timeout+auth_challenge，直至
 *     auth_pass_flag==0 通过（计数=10 退出）或重试计数(0x100020C8)>=5。
 *     **12p 反逻辑：auth_pass_flag==0 = 认证通过**（6p 0x10000750==1 放行）。
 *     （6p auth_retry 的 param_sync_live_to_eeprom() 落盘在 12p 循环中无对应调用）
 *
 * 挑战参数（12p 变量）：
 *   组 A(bit0-7)：  *gain_a(0x10001630) + *startup_div(0x10001644)
 *                 + *(byte)stop_div(0x10001645) + *counter2(0x1000164c) + 0x31
 *   组 B(bit8-15)： *(byte)gain_c(0x1000163c) + *counter2(0x1000164c)
 *                 + *llim_angle(0x10001648) + 0xc
 *   组 C(bit16-23)：固定 0x55
 *   （6p 组 A 用 *g_out_fine、组 B 用 *g_out_fine；12p 对应位置为 counter2=0x1000164c）
 *
 * 十二相整理：2026-08-31
 *
 * 交叉引用：
 *   · 认证链路/引脚 → docs/analysis/HARDWARE_VERIFICATION_2026-08-31.md
 *   · 开机调用序列 → src/01_startup.c（main：…→ auth_verify_loop → wdt_init）
 *   · GPIO2 方向/初值初始化 → src/13_gpio_init.c（gpio2_init）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"

/* 0x000108D2 —— 认证超时窗口设置：把 50000（一个较大计数值）写入 0x100020C4，
 *   用作认证步骤/看门狗的判定窗口（本次挑战允许的等待时长）。
 *   OLD 反汇编 movw r0,#0xc350 → str → bx lr，退出时 R0=50000（编译器寄存器复用），
 *   A/B 校验按 R0 末值对比，故按副作用原样返回该值。 */
uint auth_set_timeout(void)
{
  return *(volatile uint *)0x100020C4 = 50000;
}

/* 0x000108DC —— 1-Wire 认证挑战一帧：逐位串出 24 位挑战、串回 16 位应答并比对。
 *
 * 时序（每 bit_idx 一次）：
 *   拉低 P2.1 数据线 → 按挑战位 MSB 决定是否拉高 P2.1（置 1 才 SET）
 *   延时 2000 → P2.3 时钟拉低（下降沿）→ 延时 1000 → bit_idx>7 时读 P2.2（FIO2PIN&4）
 *   延时 1000 → P2.3 时钟拉高。
 * 挑战三字节在 bit_idx==0/8/0x10 处生成写入 challenge_byte：
 *   byte A = (*gain_a + *startup_div + *(byte)stop_div + *counter2 + 0x31) & 0xff
 *   byte B = (*(byte)gain_c + *counter2 + *llim_angle + 0xc) & 0xff
 *   byte C = 固定 0x55
 * 期望应答两字节（对 challenge_byte 的散列混合）：
 *   exp_resp_hi（bit0 组） = (b^0xc2 + b|0x1b + b&0xb2) & 0xff
 *   exp_resp_lo（bit8 组） = (b^0x3f + b|0xa9 + b&0xbc) & 0xff
 * 读回 response_bits（bit8..23 共 16 位）与两期望值比对：高字节比 exp_resp_hi、
 *   低字节比 exp_resp_lo；命中 → **auth_pass_flag=0（通过）**，否则 → =1（失败）。
 *   注意：期望值是高字节在前读回（response_bits>>8），与 bit0/bit8 两组计算对应。
 *   注意：读宽按反汇编（ldrb）对 uint32_t* 变量强制 (volatile uint8_t*)。 */
uint auth_challenge(void)
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
  *(volatile uint *)(DAT_00010a74 + 0x5c) = *(volatile uint *)(DAT_00010a74 + 0x5c) | 0x10;  /* FIO2CLR P2.4=复位线拉低 */
  for (bit_idx = 0; fio_base = DAT_00010a74, bit_idx < 0x18; bit_idx = bit_idx + 1) {  /* 24 bit */
    if (bit_idx == 0) {
      /* 挑战组 A（bit0-7）：4 个板上参数求和 +0x31；期望应答高位 exp_resp_hi */
      challenge_byte = *gain_a + (uint)*startup_div + (uint)*(volatile uint8_t *)stop_div + (uint)*counter2 + 0x31;
      exp_resp_hi = ((challenge_byte ^ 0xc2) + (challenge_byte | 0x1b) + (challenge_byte & 0xb2)) & 0xff;
    }
    if (bit_idx == 8) {
      /* 挑战组 B（bit8-15）：3 个板上参数求和 +0xc；期望应答低位 exp_resp_lo */
      challenge_byte = (uint)*(volatile uint8_t *)gain_c + (uint)*counter2 + *llim_angle + 0xc;
      exp_resp_lo = ((challenge_byte ^ 0x3f) + (challenge_byte | 0xa9) + (challenge_byte & 0xbc)) & 0xff;
    }
    if (bit_idx == 0x10) {
      challenge_byte = 0x55;                                 /* 挑战组 C（bit16-23）：固定 0x55 */
    }
    *(volatile uint *)(DAT_00010a74 + 0x5c) = *(volatile uint *)(DAT_00010a74 + 0x5c) | 2;  /* FIO2CLR P2.1=数据线拉低 */
    if ((challenge_byte & 0x80) != 0) {
      *(volatile uint *)(fio_base + 0x58) = *(volatile uint *)(fio_base + 0x58) | 2;              /* 挑战位=1 → FIO2SET P2.1 */
    }
    challenge_byte = challenge_byte << 1;
    for (delay_cnt = 0; delay_cnt < 2000; delay_cnt++) {             /* 位建立延时 */
    }
    *(volatile uint *)(DAT_00010a74 + 0x5c) = *(volatile uint *)(DAT_00010a74 + 0x5c) | 8;  /* FIO2CLR P2.3=时钟拉低 */
    for (delay_cnt = 0; delay_cnt < 1000; delay_cnt++) {             /* 时钟低延时 */
    }
    if ((7 < bit_idx) && (response_bits = response_bits * 2, (*(volatile uint *)(DAT_00010a74 + 0x54) & 4) != 0)) {
      response_bits = response_bits + 1;                                                  /* bit8 起读 FIO2PIN P2.2 */
    }
    for (delay_cnt = 0; delay_cnt < 1000; delay_cnt++) {             /* 时钟高延时 */
    }
    *(volatile uint *)(DAT_00010a74 + 0x58) = *(volatile uint *)(DAT_00010a74 + 0x58) | 8;  /* FIO2SET P2.3=时钟拉高 */
  }
  *(volatile uint *)(DAT_00010a74 + 0x58) = *(volatile uint *)(DAT_00010a74 + 0x58) | 0x10; /* FIO2SET P2.4=复位线释放 */
  if ((exp_resp_hi == response_bits >> 8) && ((response_bits & 0xff) == exp_resp_lo)) {
    *(volatile uint8_t *)auth_pass_flag = 0;                          /* 应答匹配 → 认证通过（12p 反逻辑：0=通过） */
  }
  else {
    *(volatile uint8_t *)auth_pass_flag = 1;                          /* 应答不匹配 → 认证失败 */
  }
  return challenge_byte;   /* OLD 退出时 R0=challenge_byte 末态(组C 0x55 连续左移=0x5500)，A/B 按 R0 末值对比 */
}

/* 0x00010A38 —— 开机认证循环（main 调用，替代 6p auth_retry）：
 *   重试计数 0x100020C8 清零 → 循环（<5 次）：
 *     auth_set_timeout() 设超时窗口 → auth_challenge() 挑战一次（写 auth_pass_flag）
 *     若 auth_pass_flag==0（通过）→ 计数=10（跳过剩余）→ 计数+1 退出
 *   至多 5 次挑战；全部失败则放弃（计数到 5 退出），main 随后按 auth_pass_flag 决定。
 *   计数 0x100020C8 = auth_retry（字节），与 6p 0x100020F0 对应。 */
uint auth_verify_loop(void)
{
  volatile uint8_t *cnt  = (volatile uint8_t *)auth_retry;        /* 0x100020C8 重试计数 */
  volatile uint8_t *flag = (volatile uint8_t *)auth_pass_flag;    /* 0x100020C0 认证标志(0=通过) */

  *cnt = 0;
  while (*cnt < 5) {
    auth_set_timeout();                    /* bl 0x108D2 */
    auth_challenge();                      /* bl 0x108DC */
    if (*flag == 0) {                      /* 认证通过 → 计数置 10 退出 */
      *cnt = 10;
    }
    *cnt = *cnt + 1;
  }
  return *cnt;   /* OLD 退出时 R0=末次循环检查读的 cnt（全失败=5，通过路径=11），A/B 按 R0 末值对比 */
}
