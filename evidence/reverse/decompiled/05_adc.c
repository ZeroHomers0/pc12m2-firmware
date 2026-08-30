/* =============================================================================
 * LPC1765FBD100 (ST33C / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 05：ADC0 采样 + 5 点移动平均 + 标定换算
 *
 * 通道（与 HARDWARE_VERIFICATION / PROGRESS §4b 确证）：
 *   顺序 SEL=4→2→1→0x20→8→0x10：ch2(AD0.2)=IA、ch1(AD0.1)=IB、
 *   ch0(AD0.0)=IC、ch5(AD0.5)=Ug(给定/参考)、ch3(AD0.3)=IF、ch4(AD0.4)=Uf
 *   0x1000259C=通道选择（4→ch3、5→ch4）
 * 换算：测量值 = (0x1000233C[param4 互感器比] × 平均 × 2) / 标定除数
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · reg↔ADC 唯一映射（reg40-45 = Ug/IA/IB/IC/IF/Uf）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §一.12b、docs/PROGRESS_2026-08-20.md §4b
 *   · 采样硬件链（1W 0.1R×3 + LM2904 运放 + CJ431 基准）→ HARDWARE_VERIFICATION §六.B.6
 *   · 实测标定核对 → APPLICATION_GUIDE_2026-08-21.md §八.2
 * ========================================================================== */

/* 0x00001F04 —— ADC0 初始化：CLKDIV、PCLKSEL（ADC 时钟）、使能 ADC 电源 */
void adc_init(void)
{
  int iVar1;

  iVar1 = DAT_00002300;
  *(uint *)(DAT_00002300 + 4) = *(uint *)(DAT_00002300 + 4) & 0xffc03fff;
  *(uint *)(iVar1 + 4) = *(uint *)(iVar1 + 4) | 0x154000;      /* PCLK ADC=CCLK */
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) & 0xcfffffff;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) | 0x30000000;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) & 0xcfffffff;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) | 0x30000000;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) & 0x3fffffff;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) | 0xc0000000;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) & 0x3fffffff;
  *(uint *)(iVar1 + 0xc) = *(uint *)(iVar1 + 0xc) | 0xc0000000;
  *(uint *)(DAT_00002308 + 0xc4) = *DAT_00002304 | 0x1000;      /* PCONP ADC 上电 */
  *DAT_00002310 = DAT_0000230c;                                 /* AD0CR 初值 */
  return;
}

/* 0x00001F80 —— ADC0 启动转换（CR bit27=START） */
void adc0_start(void)
{
  uint *puVar1;

  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xf8ffffff;
  *puVar1 = *puVar1 | 0x1000000;
  return;
}

/* 0x00001FA6 —— 等转换完成（GDR bit31 DONE）并返回 12 位结果（>>4） */
uint adc0_wait_done(void)
{
  do {
  } while ((*(uint *)(DAT_00002310 + 4) & 0x80000000) == 0);
  return (*(uint *)(DAT_00002310 + 4) & 0xffff) >> 4;
}

/* 0x00001FBC —— 逐通道扫描：每通道 5 点循环采样存原始数组，
 *   各通道在对应索引点（0..5 轮转）计算 5 点平均并做标定换算
 *   0x10002314=通道轮转计数（0..5）；0x10002330/4C/5C/70/A0/C0=各通道平均索引
 *   （换算公式 0x1000233C=param4 互感器比，0x10002340 等=ADC 标定除数） */
void adc0_scan_channels(void)
{
  uint *puVar1;
  byte *pbVar2;
  uint *puVar3;
  int *piVar4;
  int iVar5;
  undefined4 uVar6;

  pbVar2 = DAT_00002314;
  *DAT_00002314 = *DAT_00002314 + 1;
  if (5 < *pbVar2) {
    *pbVar2 = 0;
  }
  /* —— ch2（SEL=4，IA）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 4;
  adc0_start();
  iVar5 = adc0_wait_done();
  DAT_00002318[*DAT_00002314] = iVar5;
  /* —— ch1（SEL=2，IB）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 2;
  adc0_start();
  iVar5 = adc0_wait_done();
  DAT_0000231c[*DAT_00002314] = iVar5;
  /* —— ch0（SEL=1，IC）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 1;
  adc0_start();
  iVar5 = adc0_wait_done();
  DAT_00002320[*DAT_00002314] = iVar5;
  /* —— ch5（SEL=0x20，Ug 给定）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 0x20;
  adc0_start();
  iVar5 = adc0_wait_done();
  DAT_00002324[*DAT_00002314] = iVar5;
  /* —— ch3（SEL=8，IF）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 8;
  adc0_start();
  uVar6 = adc0_wait_done();
  *(undefined4 *)(DAT_00002328 + (uint)*DAT_00002314 * 4) = uVar6;
  /* —— ch4（SEL=0x10，Uf）—— */
  puVar1 = DAT_00002310;
  *DAT_00002310 = *DAT_00002310 & 0xffffffc0;
  *puVar1 = *puVar1 | 0x10;
  adc0_start();
  uVar6 = adc0_wait_done();
  *(undefined4 *)(DAT_0000232c + (uint)*DAT_00002314 * 4) = uVar6;

  /* —— ch2 平均（每轮转满 5 点计算）—— */
  pbVar2 = DAT_00002330;
  if (*DAT_00002314 == 0) {
    *DAT_00002330 = *DAT_00002330 + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    iVar5 = DAT_00002334;
    *(uint *)(DAT_00002334 + (uint)*DAT_00002330 * 4) =
         (uint)(*DAT_00002318 + DAT_00002318[1] + DAT_00002318[2] + DAT_00002318[3] +
               DAT_00002318[4]) / 5;
    puVar1 = DAT_00002338;
    *DAT_00002338 = *(uint *)(iVar5 + (uint)*DAT_00002330 * 4);
    puVar3 = DAT_00002344;
    *DAT_00002344 = (*DAT_0000233c * *puVar1 * 2) / *DAT_00002340;
    if ((*DAT_00002348 == '\0') && (*puVar3 < 10)) {
      *puVar3 = 0;
    }
  }
  /* —— ch1 平均（→ reg42 IB）—— */
  pbVar2 = DAT_0000234c;
  if (*DAT_00002314 == 1) {
    *DAT_0000234c = *DAT_0000234c + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    iVar5 = DAT_00002350;
    *(uint *)(DAT_00002350 + (uint)*DAT_0000234c * 4) =
         (uint)(*DAT_0000231c + DAT_0000231c[1] + DAT_0000231c[2] + DAT_0000231c[3] +
               DAT_0000231c[4]) / 5;
    puVar1 = DAT_00002338;
    *DAT_00002338 = *(uint *)(iVar5 + (uint)*DAT_0000234c * 4);
    puVar3 = DAT_00002358;
    *DAT_00002358 = (*DAT_0000233c * *puVar1 * 2) / *DAT_00002354;
    if ((*DAT_00002348 == '\0') && (*puVar3 < 10)) {
      *puVar3 = 0;
    }
  }
  /* —— ch0 平均（→ reg43 IC）—— */
  pbVar2 = DAT_0000235c;
  if (*DAT_00002314 == 2) {
    *DAT_0000235c = *DAT_0000235c + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    iVar5 = DAT_00002360;
    *(uint *)(DAT_00002360 + (uint)*DAT_0000235c * 4) =
         (uint)(*DAT_00002320 + DAT_00002320[1] + DAT_00002320[2] + DAT_00002320[3] +
               DAT_00002320[4]) / 5;
    puVar1 = DAT_00002338;
    *DAT_00002338 = *(uint *)(iVar5 + (uint)*DAT_0000235c * 4);
    *DAT_00002364 = *puVar1;
    puVar1 = DAT_0000236c;
    *DAT_0000236c = (*DAT_0000233c * *DAT_00002338 * 2) / *DAT_00002368;
    if ((*DAT_00002348 == '\0') && (*puVar1 < 10)) {
      *puVar1 = 0;
    }
  }
  /* —— ch5 平均（→ reg40 读回源 Ug）—— */
  pbVar2 = DAT_00002370;
  if (*DAT_00002314 == 3) {
    *DAT_00002370 = *DAT_00002370 + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    piVar4 = DAT_00002374;
    DAT_00002374[*DAT_00002370] =
         (uint)(*DAT_00002324 + DAT_00002324[1] + DAT_00002324[2] + DAT_00002324[3] +
               DAT_00002324[4]) / 5;
    puVar1 = DAT_00002338;
    *DAT_00002338 =
         (uint)(*piVar4 + piVar4[1] + DAT_00002374[2] + DAT_00002374[3] + DAT_00002374[4] +
                DAT_00002374[5] + DAT_00002374[6] + DAT_00002374[7] + DAT_00002374[8] +
               DAT_00002374[9]) / 10;
    puVar3 = DAT_00002378;
    *DAT_00002378 = (*puVar1 * 0x65) / 400;      /* ×101/400 缩放 */
    if (*DAT_0000237c == '\0') {
      if (1000 < *puVar3) {
        *puVar3 = 1000;
      }
      if (*DAT_00002580 < 10) {
        *DAT_00002580 = 0;
      }
    }
    if (*DAT_00002584 == '\x01') {
      if (*DAT_00002580 < 0xcd) {
        *DAT_00002580 = 0;
      }
      puVar1 = DAT_00002580;
      if (0xcc < *DAT_00002580) {
        *DAT_00002580 = (*DAT_00002580 - 200) * 5 >> 2;
        if (1000 < *puVar1) {
          *puVar1 = 1000;
        }
        if (*DAT_00002580 < 10) {
          *DAT_00002580 = 0;
        }
      }
      *DAT_00002588 = (*DAT_00002588 - 800) * 5 >> 2;
    }
    if (*DAT_0000258c == '\0') {
      *DAT_00002594 = (*DAT_00002590 * *DAT_00002588) / 0xf78;    /* /3960 */
    }
    if (*DAT_0000258c == '\x01') {
      *DAT_00002594 = (*DAT_00002598 * *DAT_00002588) / 0xf78;
    }
  }
  /* —— ch3 平均（→ reg44 IF；仅当 0x1000259C==4）—— */
  pbVar2 = DAT_000025a0;
  if (*DAT_0000259c == '\x04') {
    *DAT_000025a0 = *DAT_000025a0 + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    iVar5 = DAT_000025a8;
    *(uint *)(DAT_000025a8 + (uint)*DAT_000025a0 * 4) =
         (uint)(*DAT_000025a4 + DAT_000025a4[1] + DAT_000025a4[2] + DAT_000025a4[3] +
               DAT_000025a4[4]) / 5;
    puVar1 = DAT_00002588;
    *DAT_00002588 = *(uint *)(iVar5 + (uint)*DAT_000025a0 * 4);
    *DAT_000025ac = (*puVar1 * 0x65) / 400;
    puVar1 = DAT_00002588;
    *DAT_00002588 = (*DAT_00002598 * *DAT_00002588) / *DAT_000025b0;   /* gain_b/reg54 */
    *DAT_000025b4 = *puVar1;
    *DAT_000025b8 = *DAT_00002588;
    if ((*DAT_000025bc == '\0') && (*DAT_000025b4 < 10)) {
      *DAT_000025b4 = 0;
    }
  }
  /* —— ch4 平均（→ reg45 Uf；仅当 0x1000259C==5）—— */
  pbVar2 = DAT_000025c0;
  if (*DAT_0000259c == '\x05') {
    *DAT_000025c0 = *DAT_000025c0 + 1;
    if (9 < *pbVar2) {
      *pbVar2 = 0;
    }
    iVar5 = DAT_000025c8;
    *(uint *)(DAT_000025c8 + (uint)*DAT_000025c0 * 4) =
         (uint)(*DAT_000025c4 + DAT_000025c4[1] + DAT_000025c4[2] + DAT_000025c4[3] +
               DAT_000025c4[4]) / 5;
    puVar1 = DAT_00002588;
    *DAT_00002588 = *(uint *)(iVar5 + (uint)*DAT_000025c0 * 4);
    *DAT_000025cc = (*puVar1 * 0x65) / 400;
    puVar1 = DAT_00002588;
    *DAT_00002588 = (*DAT_00002590 * *DAT_00002588) / *DAT_000025d0;   /* gain_a/reg55 */
    *DAT_000025d4 = *puVar1;
    *DAT_000025d8 = *DAT_00002588;
    if ((*DAT_000025bc == '\0') && (*DAT_000025d4 < 10)) {
      *DAT_000025d4 = 0;
    }
  }
  return;
}
