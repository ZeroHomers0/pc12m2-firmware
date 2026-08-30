/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 05：ADC0 多通道采样 + 标定换算
 *
 * 采样链（三相电源反馈 + 给定）：
 *   · ADC0（g_adc = AD0CR 0x40034000）：adc0_start 置 START bit、adc0_wait_done
 *     等 GDR bit31 DONE 后读 12 位（&0xffff >> 4）
 *   · 逐通道扫描，通道轮转计数 0x10002314（0..5）：
 *       ch2=IA(三相电流A, SEL=4)、ch1=IB(SEL=2)、ch0=IC(SEL=1)、ch5=Ug 给定(SEL=0x20)、
 *       ch3=IF(SEL=8)、ch4=Uf(SEL=0x10)   —— 每通道 5 点原始采样存 0x10002318..0x1000232C
 *   · 每轮转满 5 点求 5 点平均（ch5 再求 10 点平均），按互感器比(0x1000233C=param4)
 *     与 ADC 标定除数（0x10002340 等）换算 → 电流/电压反馈（→ reg40 Ug/reg42 IB/reg43 IC/
 *     reg44 IF/reg45 Uf，见 MENU_PARAMETER_MAPPING.md）
 * 导出：2026-08-21（L0 语义化：iVar→clk_base/sample、puVar→val_ptr/out_ptr 等）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"

/* 0x00001F04 —— ADC0 初始化：CLKDIV、PCLKSEL（ADC 时钟分频）、使能 ADC 电源
 *   DAT_00002300=时钟/PCLKSEL 基址（+4 PCLK ADC=CCLK、+0xC 分频/CKLK 位）、
 *   DAT_00002308=SCB 0x400FC000（+0xC4 PCONP 上电 bit12 ADC）、DAT_0000230C=AD0CR 初值 0x00201820 */
void adc_init(void)
{
  int clk_base;

  clk_base = DAT_00002300;
  *(volatile uint *)(DAT_00002300 + 4) = *(volatile uint *)(DAT_00002300 + 4) & 0xffc03fff;
  *(volatile uint *)(clk_base + 4) = *(volatile uint *)(clk_base + 4) | 0x154000;      /* PCLK ADC=CCLK */
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0xcfffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0x30000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0xcfffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0x30000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0x3fffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0xc0000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0x3fffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0xc0000000;
  *(volatile uint *)(DAT_00002308 + 0xc4) = *g_pconp | 0x1000;      /* PCONP ADC 上电 */
  *g_adc = DAT_0000230c;                                 /* AD0CR 初值 */
  return;
}

/* 0x00001F80 —— ADC0 启动转换（CR bit27=START） */
void adc0_start(void)
{
  volatile uint32_t *adcr;

  adcr = g_adc;
  *g_adc = *g_adc & 0xf8ffffff;
  *adcr = *adcr | 0x1000000;
  return;
}

/* 0x00001FA6 —— 等转换完成（GDR bit31 DONE）并返回 12 位结果（>>4）
 * 注意：g_adc 是 uint32_t*，不能直接 +4（会按元素偏移 +16 字节读到 AD0DR0）。
 * 必须先转整数做字节偏移，才能读到 AD0GDR(0x40034004)。 */
uint adc0_wait_done(void)
{
  do {
  } while ((*(volatile uint *)((uint)g_adc + 4) & 0x80000000) == 0);
  return (*(volatile uint *)((uint)g_adc + 4) & 0xffff) >> 4;
}

/* 0x00001FBC —— 逐通道扫描：每通道 5 点循环采样存原始数组，
 *   各通道在对应索引点（0..5 轮转）计算 5 点平均并做标定换算
 *   0x10002314=通道轮转计数（0..5）；0x10002330/4C/5C/70/A0/C0=各通道平均索引
 *   （换算公式 0x1000233C=param4 互感器比，0x10002340 等=ADC 标定除数）
 * 局部变量角色（反编译寄存器复用，请注意跨段复用）：
 *   val_ptr   — 复用：采样段=AD0CR；平均段=中间换算值指针（0x10002338/0x10002588）
 *   out_ptr   — 标定输出值指针（0x10002344/58/6C/94 等）
 *   avg_idx   — 通道平均索引计数器（0x10002330/4C/5C/70/A0/C0）
 *   sample    — 复用：采样段=ADC 原始 12 位结果；平均段=5 点平均数组基址
 *   sample32  — ADC 原始结果（写入 32 位缓冲 ch3/ch4） */
void adc0_scan_channels(void)
{
  volatile uint32_t *val_ptr;
  volatile uint8_t *avg_idx;
  volatile uint32_t *out_ptr;
  volatile uint32_t *ch5_raw;
  int sample;
  uint32_t sample32;

  avg_idx = DAT_00002314;
  *DAT_00002314 = *DAT_00002314 + 1;
  if (5 < *avg_idx) {
    *avg_idx = 0;
  }
  /* —— ch2（SEL=4，IA）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 4;
  adc0_start();
  sample = adc0_wait_done();
  DAT_00002318[*DAT_00002314] = sample;
  /* —— ch1（SEL=2，IB）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 2;
  adc0_start();
  sample = adc0_wait_done();
  DAT_0000231c[*DAT_00002314] = sample;
  /* —— ch0（SEL=1，IC）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 1;
  adc0_start();
  sample = adc0_wait_done();
  DAT_00002320[*DAT_00002314] = sample;
  /* —— ch5（SEL=0x20，Ug 给定）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 0x20;
  adc0_start();
  sample = adc0_wait_done();
  DAT_00002324[*DAT_00002314] = sample;
  /* —— ch3（SEL=8，IF）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 8;
  adc0_start();
  sample32 = adc0_wait_done();
  *(volatile undefined4 *)(DAT_00002328 + (uint)*DAT_00002314 * 4) = sample32;
  /* —— ch4（SEL=0x10，Uf）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 0x10;
  adc0_start();
  sample32 = adc0_wait_done();
  *(volatile undefined4 *)(DAT_0000232c + (uint)*DAT_00002314 * 4) = sample32;

  /* —— ch2 平均（每轮转满 5 点计算）—— */
  avg_idx = DAT_00002330;
  if (*DAT_00002314 == 0) {
    *DAT_00002330 = *DAT_00002330 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = DAT_00002334;
    *(volatile uint *)(DAT_00002334 + (uint)*DAT_00002330 * 4) =
         (uint)(*DAT_00002318 + DAT_00002318[1] + DAT_00002318[2] + DAT_00002318[3] +
               DAT_00002318[4]) / 5;
    val_ptr = DAT_00002338;
    *DAT_00002338 = *(volatile uint *)(sample + (uint)*DAT_00002330 * 4);
    out_ptr = DAT_00002344;
    *DAT_00002344 = (*DAT_0000233c * *val_ptr * 2) / *DAT_00002340;
    if ((*g_cfg_word == '\0') && (*out_ptr < 10)) {
      *out_ptr = 0;
    }
  }
  /* —— ch1 平均（→ reg42 IB）—— */
  avg_idx = DAT_0000234c;
  if (*DAT_00002314 == 1) {
    *DAT_0000234c = *DAT_0000234c + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = DAT_00002350;
    *(volatile uint *)(DAT_00002350 + (uint)*DAT_0000234c * 4) =
         (uint)(*DAT_0000231c + DAT_0000231c[1] + DAT_0000231c[2] + DAT_0000231c[3] +
               DAT_0000231c[4]) / 5;
    val_ptr = DAT_00002338;
    *DAT_00002338 = *(volatile uint *)(sample + (uint)*DAT_0000234c * 4);
    out_ptr = DAT_00002358;
    *DAT_00002358 = (*DAT_0000233c * *val_ptr * 2) / *DAT_00002354;
    if ((*g_cfg_word == '\0') && (*out_ptr < 10)) {
      *out_ptr = 0;
    }
  }
  /* —— ch0 平均（→ reg43 IC）—— */
  avg_idx = DAT_0000235c;
  if (*DAT_00002314 == 2) {
    *DAT_0000235c = *DAT_0000235c + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = DAT_00002360;
    *(volatile uint *)(DAT_00002360 + (uint)*DAT_0000235c * 4) =
         (uint)(*DAT_00002320 + DAT_00002320[1] + DAT_00002320[2] + DAT_00002320[3] +
               DAT_00002320[4]) / 5;
    val_ptr = DAT_00002338;
    *DAT_00002338 = *(volatile uint *)(sample + (uint)*DAT_0000235c * 4);
    *DAT_00002364 = *val_ptr;
    val_ptr = DAT_0000236c;
    *DAT_0000236c = (*DAT_0000233c * *DAT_00002338 * 2) / *DAT_00002368;
    if ((*g_cfg_word == '\0') && (*val_ptr < 10)) {
      *val_ptr = 0;
    }
  }
  /* —— ch5 平均（→ reg40 读回源 Ug）—— */
  avg_idx = DAT_00002370;
  if (*DAT_00002314 == 3) {
    *DAT_00002370 = *DAT_00002370 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    ch5_raw = DAT_00002374;
    DAT_00002374[*DAT_00002370] =
         (uint)(*DAT_00002324 + DAT_00002324[1] + DAT_00002324[2] + DAT_00002324[3] +
               DAT_00002324[4]) / 5;
    val_ptr = DAT_00002338;
    *DAT_00002338 =
         (uint)(*ch5_raw + ch5_raw[1] + DAT_00002374[2] + DAT_00002374[3] + DAT_00002374[4] +
                DAT_00002374[5] + DAT_00002374[6] + DAT_00002374[7] + DAT_00002374[8] +
               DAT_00002374[9]) / 10;
    out_ptr = DAT_00002378;
    *DAT_00002378 = (*val_ptr * 0x65) / 400;      /* ×101/400 缩放 */
    if (*DAT_0000237c == '\0') {
      if (1000 < *out_ptr) {
        *out_ptr = 1000;
      }
      if (*DAT_00002580 < 10) {
        *DAT_00002580 = 0;
      }
    }
    if (*DAT_00002584 == '\x01') {
      if (*DAT_00002580 < 0xcd) {
        *DAT_00002580 = 0;
      }
      val_ptr = DAT_00002580;
      if (0xcc < *DAT_00002580) {
        *DAT_00002580 = (*DAT_00002580 - 200) * 5 >> 2;
        if (1000 < *val_ptr) {
          *val_ptr = 1000;
        }
        if (*DAT_00002580 < 10) {
          *DAT_00002580 = 0;
        }
      }
      *DAT_00002588 = (*DAT_00002588 - 800) * 5 >> 2;
    }
    if (*g_gain_sel == '\0') {
      *DAT_00002594 = (*g_gain_a * *DAT_00002588) / 0xf78;    /* /3960 */
    }
    if (*g_gain_sel == '\x01') {
      *DAT_00002594 = (*g_gain_b * *DAT_00002588) / 0xf78;
    }
  }
  /* —— ch3 平均（→ reg44 IF；仅当 0x1000259C==4）—— */
  avg_idx = DAT_000025a0;
  if (*DAT_0000259c == '\x04') {
    *DAT_000025a0 = *DAT_000025a0 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = DAT_000025a8;
    *(volatile uint *)(DAT_000025a8 + (uint)*DAT_000025a0 * 4) =
         (uint)(*DAT_000025a4 + DAT_000025a4[1] + DAT_000025a4[2] + DAT_000025a4[3] +
               DAT_000025a4[4]) / 5;
    val_ptr = DAT_00002588;
    *DAT_00002588 = *(volatile uint *)(sample + (uint)*DAT_000025a0 * 4);
    *DAT_000025ac = (*val_ptr * 0x65) / 400;
    val_ptr = DAT_00002588;
    *DAT_00002588 = (*g_gain_b * *DAT_00002588) / *DAT_000025b0;   /* gain_b/reg54 */
    *DAT_000025b4 = *val_ptr;
    *DAT_000025b8 = *DAT_00002588;
    if ((*g_cfg_word == '\0') && (*DAT_000025b4 < 10)) {
      *DAT_000025b4 = 0;
    }
  }
  /* —— ch4 平均（→ reg45 Uf；仅当 0x1000259C==5）—— */
  avg_idx = DAT_000025c0;
  if (*DAT_0000259c == '\x05') {
    *DAT_000025c0 = *DAT_000025c0 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = DAT_000025c8;
    *(volatile uint *)(DAT_000025c8 + (uint)*DAT_000025c0 * 4) =
         (uint)(*DAT_000025c4 + DAT_000025c4[1] + DAT_000025c4[2] + DAT_000025c4[3] +
               DAT_000025c4[4]) / 5;
    val_ptr = DAT_00002588;
    *DAT_00002588 = *(volatile uint *)(sample + (uint)*DAT_000025c0 * 4);
    *DAT_000025cc = (*val_ptr * 0x65) / 400;
    val_ptr = DAT_00002588;
    *DAT_00002588 = (*g_gain_a * *DAT_00002588) / *DAT_000025d0;   /* gain_a/reg55 */
    *DAT_000025d4 = *val_ptr;
    *DAT_000025d8 = *DAT_00002588;
    if ((*g_cfg_word == '\0') && (*DAT_000025d4 < 10)) {
      *DAT_000025d4 = 0;
    }
  }
  return;
}
