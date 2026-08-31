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

  clk_base = DAT_000022b0;
  *(volatile uint *)(DAT_000022b0 + 4) = *(volatile uint *)(DAT_000022b0 + 4) & 0xffc03fff;
  *(volatile uint *)(clk_base + 4) = *(volatile uint *)(clk_base + 4) | 0x154000;      /* PCLK ADC=CCLK */
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0xcfffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0x30000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0xcfffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0x30000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0x3fffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0xc0000000;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) & 0x3fffffff;
  *(volatile uint *)(clk_base + 0xc) = *(volatile uint *)(clk_base + 0xc) | 0xc0000000;
  *(volatile uint *)(DAT_000022b8 + 0xc4) = *g_pconp | 0x1000;      /* PCONP ADC 上电 */
  *g_adc = DAT_000022bc;                                 /* AD0CR 初值 */
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

  avg_idx = adc_scan_idx;
  *adc_scan_idx = *adc_scan_idx + 1;
  if (5 < *avg_idx) {
    *avg_idx = 0;
  }
  /* —— ch2（SEL=4，IA）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 4;
  adc0_start();
  sample = adc0_wait_done();
  adc_ch0_raw[*adc_scan_idx] = sample;
  /* —— ch1（SEL=2，IB）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 2;
  adc0_start();
  sample = adc0_wait_done();
  adc_ch1_raw[*adc_scan_idx] = sample;
  /* —— ch0（SEL=1，IC）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 1;
  adc0_start();
  sample = adc0_wait_done();
  adc_ch2_raw[*adc_scan_idx] = sample;
  /* —— ch5（SEL=0x20，Ug 给定）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 0x20;
  adc0_start();
  sample = adc0_wait_done();
  adc_ch3_raw[*adc_scan_idx] = sample;
  /* —— ch3（SEL=8，IF）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 8;
  adc0_start();
  sample32 = adc0_wait_done();
  *(volatile undefined4 *)(adc_ch4_raw + (uint)*adc_scan_idx * 4) = sample32;
  /* —— ch4（SEL=0x10，Uf）—— */
  val_ptr = g_adc;
  *g_adc = *g_adc & 0xffffffc0;
  *val_ptr = *val_ptr | 0x10;
  adc0_start();
  sample32 = adc0_wait_done();
  *(volatile undefined4 *)(adc_ch5_raw + (uint)*adc_scan_idx * 4) = sample32;

  /* —— ch2 平均（每轮转满 5 点计算）—— */
  avg_idx = adc_avg_idx_0;
  if (*adc_scan_idx == 0) {
    *adc_avg_idx_0 = *adc_avg_idx_0 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = adc_ch0_buf;
    *(volatile uint *)(adc_ch0_buf + (uint)*adc_avg_idx_0 * 4) =
         (uint)(*adc_ch0_raw + adc_ch0_raw[1] + adc_ch0_raw[2] + adc_ch0_raw[3] +
               adc_ch0_raw[4]) / 5;
    val_ptr = adc_avg_work;
    *adc_avg_work = *(volatile uint *)(sample + (uint)*adc_avg_idx_0 * 4);
    out_ptr = DAT_000022f4;
    *DAT_000022f4 = (*gain_coef * *val_ptr * 2) / *DAT_000022f0;
    if ((*cfg_word == '\0') && (*out_ptr < 10)) {
      *out_ptr = 0;
    }
  }
  /* —— ch1 平均（→ reg42 IB）—— */
  avg_idx = adc_avg_idx_1;
  if (*adc_scan_idx == 1) {
    *adc_avg_idx_1 = *adc_avg_idx_1 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = adc_ch1_buf;
    *(volatile uint *)(adc_ch1_buf + (uint)*adc_avg_idx_1 * 4) =
         (uint)(*adc_ch1_raw + adc_ch1_raw[1] + adc_ch1_raw[2] + adc_ch1_raw[3] +
               adc_ch1_raw[4]) / 5;
    val_ptr = adc_avg_work;
    *adc_avg_work = *(volatile uint *)(sample + (uint)*adc_avg_idx_1 * 4);
    out_ptr = DAT_00002308;
    *DAT_00002308 = (*gain_coef * *val_ptr * 2) / *DAT_00002304;
    if ((*cfg_word == '\0') && (*out_ptr < 10)) {
      *out_ptr = 0;
    }
  }
  /* —— ch0 平均（→ reg43 IC）—— */
  avg_idx = adc_avg_idx_2;
  if (*adc_scan_idx == 2) {
    *adc_avg_idx_2 = *adc_avg_idx_2 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = adc_ch2_buf;
    *(volatile uint *)(adc_ch2_buf + (uint)*adc_avg_idx_2 * 4) =
         (uint)(*adc_ch2_raw + adc_ch2_raw[1] + adc_ch2_raw[2] + adc_ch2_raw[3] +
               adc_ch2_raw[4]) / 5;
    val_ptr = adc_avg_work;
    *adc_avg_work = *(volatile uint *)(sample + (uint)*adc_avg_idx_2 * 4);
    *adc_conv_ch2 = *val_ptr;
    val_ptr = DAT_0000231c;
    *DAT_0000231c = (*gain_coef * *adc_avg_work * 2) / *DAT_00002318;
    if ((*cfg_word == '\0') && (*val_ptr < 10)) {
      *val_ptr = 0;
    }
  }
  /* —— ch5 平均（→ reg40 读回源 Ug）—— */
  avg_idx = adc_avg_idx_5;
  if (*adc_scan_idx == 3) {
    *adc_avg_idx_5 = *adc_avg_idx_5 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    ch5_raw = adc_ch3_buf;
    adc_ch3_buf[*adc_avg_idx_5] =
         (uint)(*adc_ch3_raw + adc_ch3_raw[1] + adc_ch3_raw[2] + adc_ch3_raw[3] +
               adc_ch3_raw[4]) / 5;
    val_ptr = adc_avg_work;
    *adc_avg_work =
         (uint)(*ch5_raw + ch5_raw[1] + adc_ch3_buf[2] + adc_ch3_buf[3] + adc_ch3_buf[4] +
                adc_ch3_buf[5] + adc_ch3_buf[6] + adc_ch3_buf[7] + adc_ch3_buf[8] +
               adc_ch3_buf[9]) / 10;
    out_ptr = adc_conv_ch3;
    *adc_conv_ch3 = (*val_ptr * 0x65) / 400;      /* ×101/400 缩放 */
    if (*eeprom_adc_cfg == '\0') {
      if (1000 < *out_ptr) {
        *out_ptr = 1000;
      }
      if (*DAT_00002530 < 10) {
        *DAT_00002530 = 0;
      }
    }
    if (*DAT_00002534 == '\x01') {
      if (*DAT_00002530 < 0xcd) {
        *DAT_00002530 = 0;
      }
      val_ptr = DAT_00002530;
      if (0xcc < *DAT_00002530) {
        *DAT_00002530 = (*DAT_00002530 - 200) * 5 >> 2;
        if (1000 < *val_ptr) {
          *val_ptr = 1000;
        }
        if (*DAT_00002530 < 10) {
          *DAT_00002530 = 0;
        }
      }
      *DAT_00002538 = (*DAT_00002538 - 800) * 5 >> 2;
    }
    if (*gain_sel == '\0') {
      *adc_conv_aux2 = (*gain_a * *DAT_00002538) / 0xf78;    /* /3960 */
    }
    if (*gain_sel == '\x01') {
      *adc_conv_aux2 = (*gain_b * *DAT_00002538) / 0xf78;
    }
  }
  /* —— ch3 平均（→ reg44 IF；仅当 0x1000259C==4）—— */
  avg_idx = adc_avg_idx_3;
  if (*DAT_0000254c == '\x04') {
    *adc_avg_idx_3 = *adc_avg_idx_3 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = adc_ch4_buf;
    *(volatile uint *)(adc_ch4_buf + (uint)*adc_avg_idx_3 * 4) =
         (uint)(*((volatile uint32_t *)DAT_00002554) + ((volatile uint32_t *)DAT_00002554)[1] + ((volatile uint32_t *)DAT_00002554)[2] + ((volatile uint32_t *)DAT_00002554)[3] +
               ((volatile uint32_t *)DAT_00002554)[4]) / 5;
    val_ptr = DAT_00002538;
    *DAT_00002538 = *(volatile uint *)(sample + (uint)*adc_avg_idx_3 * 4);
    *DAT_0000255c = (*val_ptr * 0x65) / 400;
    val_ptr = DAT_00002538;
    *DAT_00002538 = (*gain_b * *DAT_00002538) / *DAT_00002560;   /* gain_b/reg54 */
    *adc_conv_ch4 = *val_ptr;
    *adc_conv_aux1 = *DAT_00002538;
    if ((*cfg_word == '\0') && (*adc_conv_ch4 < 10)) {
      *adc_conv_ch4 = 0;
    }
  }
  /* —— ch4 平均（→ reg45 Uf；仅当 0x1000259C==5）—— */
  avg_idx = adc_avg_idx_4;
  if (*DAT_0000254c == '\x05') {
    *adc_avg_idx_4 = *adc_avg_idx_4 + 1;
    if (9 < *avg_idx) {
      *avg_idx = 0;
    }
    sample = adc_ch5_buf;
    *(volatile uint *)(adc_ch5_buf + (uint)*adc_avg_idx_4 * 4) =
         (uint)(*((volatile uint32_t *)DAT_00002574) + ((volatile uint32_t *)DAT_00002574)[1] + ((volatile uint32_t *)DAT_00002574)[2] + ((volatile uint32_t *)DAT_00002574)[3] +
               ((volatile uint32_t *)DAT_00002574)[4]) / 5;
    val_ptr = DAT_00002538;
    *DAT_00002538 = *(volatile uint *)(sample + (uint)*adc_avg_idx_4 * 4);
    *DAT_0000257c = (*val_ptr * 0x65) / 400;
    val_ptr = DAT_00002538;
    *DAT_00002538 = (*gain_a * *DAT_00002538) / *DAT_00002580;   /* gain_a/reg55 */
    *adc_conv_ch5 = *val_ptr;
    *adc_conv_fb = *DAT_00002538;
    if ((*cfg_word == '\0') && (*adc_conv_ch5 < 10)) {
      *adc_conv_ch5 = 0;
    }
  }
  return;
}
