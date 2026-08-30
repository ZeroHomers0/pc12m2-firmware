/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 11：1-Wire 认证（auth_challenge 24 位挑战/应答）
 *
 * 硬件：DAT_0001087C = FIO 池基址 0x2009C000
 *   +0x54=FIO2PIN（读）、+0x58=FIO2SET（置位）、+0x5C=FIO2CLR（清零）
 *   引脚分配：P2.1=数据输出、P2.2=数据输入、P2.3=时钟、P2.4=复位（gpio2_init 配置）
 * 流程（每 bit：拉低数据线 → 输出挑战位 → 时钟脉冲 → 回读应答位）：
 *   · 24 位挑战字（0x18 bit）逐位移出：
 *       bit0-7   = f(DAT_00010884/88/8C/90 + 0x31)   ← 期望应答字节 A
 *       bit8-15  = f(DAT_00010894/90/98 + 0x0C)      ← 期望应答字节 B
 *       bit16-23 = 0x55（固定）
 *   · bit8 起回读 16 位应答（uVar5），高字节比对字节 A、低字节比对字节 B
 *   成功：*DAT_000108A4=1；失败：*DAT_0001089C=1 / *DAT_000108A0=1
 *   认证状态标志：*DAT_000108A8（0=未认证 1=重试中 2=已认证）、
 *   *DAT_000108AC=重试计数；超时 *DAT_00010880=50000（auth_set_timeout）
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · ADuM1201 隔离认证链路（P2.1-2.4，U25）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §二.5、§六.A.3
 *   · 远程端器件不在本板（外部模块，排针 DC3）→ §六.B.3
 *   · 锁机 / 换件注意事项 → APPLICATION_GUIDE_2026-08-21.md §四.3.②、§五.3
 * ========================================================================== */

/* 0x00010696 —— 认证超时窗口设置 */
void auth_set_timeout(void)
{
  *DAT_00010880 = 50000;
  return;
}

/* 0x000106A0 —— 1-Wire 认证挑战：发送 24 位挑战、回读 16 位应答并比对 */
void auth_challenge(void)
{
  int iVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;

  uVar6 = 0;
  uVar7 = 0;
  uVar2 = 0;
  uVar5 = 0;
  *(uint *)(DAT_0001087c + 0x5c) = *(uint *)(DAT_0001087c + 0x5c) | 0x10;  /* FIO2CLR P2.4=复位线拉低 */
  for (uVar3 = 0; iVar1 = DAT_0001087c, uVar3 < 0x18; uVar3 = uVar3 + 1) {  /* 24 bit */
    if (uVar3 == 0) {
      /* 挑战字节 A（bit0-7）+ 期望应答 A */
      uVar2 = *DAT_00010884 + (uint)*DAT_00010888 + (uint)*DAT_0001088c + (uint)*DAT_00010890 + 0x31
      ;
      uVar6 = (uVar2 ^ 0xc2) + (uVar2 | 0x1b) + (uVar2 & 0xb2) & 0xff;
    }
    if (uVar3 == 8) {
      /* 挑战字节 B（bit8-15）+ 期望应答 B */
      uVar2 = (uint)*DAT_00010894 + (uint)*DAT_00010890 + *DAT_00010898 + 0xc;
      uVar7 = (uVar2 ^ 0x3f) + (uVar2 | 0xa9) + (uVar2 & 0xbc) & 0xff;
    }
    if (uVar3 == 0x10) {
      uVar2 = 0x55;                                 /* 挑战字节 C（bit16-23）固定 0x55 */
    }
    *(uint *)(DAT_0001087c + 0x5c) = *(uint *)(DAT_0001087c + 0x5c) | 2;  /* FIO2CLR P2.1=数据线拉低 */
    if ((uVar2 & 0x80) != 0) {
      *(uint *)(iVar1 + 0x58) = *(uint *)(iVar1 + 0x58) | 2;              /* 挑战位=1 → FIO2SET P2.1 */
    }
    uVar2 = uVar2 << 1;
    for (uVar4 = 0; uVar4 < 2000; uVar4 = uVar4 + 1) {                    /* 位建立延时 */
    }
    *(uint *)(DAT_0001087c + 0x5c) = *(uint *)(DAT_0001087c + 0x5c) | 8;  /* FIO2CLR P2.3=时钟拉低 */
    for (uVar4 = 0; uVar4 < 1000; uVar4 = uVar4 + 1) {                    /* 时钟低延时 */
    }
    if ((7 < uVar3) && (uVar5 = uVar5 * 2, (*(uint *)(DAT_0001087c + 0x54) & 4) != 0)) {
      uVar5 = uVar5 + 1;                                                  /* bit8 起读 FIO2PIN P2.2 */
    }
    for (uVar4 = 0; uVar4 < 1000; uVar4 = uVar4 + 1) {                    /* 时钟高延时 */
    }
    *(uint *)(DAT_0001087c + 0x58) = *(uint *)(DAT_0001087c + 0x58) | 8;  /* FIO2SET P2.3=时钟拉高 */
  }
  *(uint *)(DAT_0001087c + 0x58) = *(uint *)(DAT_0001087c + 0x58) | 0x10; /* FIO2SET P2.4=复位线释放 */
  if ((uVar6 == uVar5 >> 8) && ((uVar5 & 0xff) == uVar7)) {
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

/* 0x00010820 —— 认证重试：*DAT_000108A8==1 时最多重试 5 次，
 *   成功后 *DAT_000108A8=2；每次重试后同步参数到 EEPROM */
void auth_retry(void)
{
  byte *pbVar1;

  pbVar1 = DAT_000108ac;
  if (*DAT_000108a8 == 1) {
    *DAT_000108ac = *DAT_000108ac + 1;
    *pbVar1 = 0;
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
