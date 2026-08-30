/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 13：NVIC 使能 / GPIO2（1-Wire 认证口）初始化
 *
 * nvic_enable_irq：NVIC ISER[param>>5] |= 1<<(param&0x1F)（param=IRQ 号）
 *   ISER 基址 0xE000E100（-0x1FFF1F00 == 0xE000E100）
 * gpio2_init：DAT_0001087C=FIO 池基址 0x2009C000
 *   +0x40=FIO2DIR、+0x58=FIO2SET
 *   配置 1-Wire 认证口：P2.4(=复位线)、P2.3(=时钟线) 设为输出；
 *   P2.1(=数据线) 先设输出再改输入（双向）；初值置高
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 1-Wire 认证总线初始化 → 11_auth.c（auth_challenge 使用）
 *   · ADuM1201 隔离链路硬件 → docs/HARDWARE_VERIFICATION_2026-08-20.md §二.5
 * ========================================================================== */

/* 0x00010628 —— NVIC IRQ 使能（ISER 位操作） */
void nvic_enable_irq(uint param_1)
{
  *(int *)((param_1 >> 5) * 4 + -0x1fff1f00) = 1 << (param_1 & 0x1f);
  return;
}

/* 0x0001064C —— GPIO2（1-Wire 认证总线）引脚方向/初值初始化 */
void gpio2_init(void)
{
  int iVar1;

  iVar1 = DAT_0001087c;                          /* FIO 池基址 0x2009C000 */
  *(uint *)(DAT_0001087c + 0x40) = *(uint *)(DAT_0001087c + 0x40) | 0x10;  /* FIO2DIR P2.4=输出 */
  *(uint *)(iVar1 + 0x40) = *(uint *)(iVar1 + 0x40) | 8;                   /* P2.3=输出 */
  *(uint *)(iVar1 + 0x40) = *(uint *)(iVar1 + 0x40) | 2;                   /* P2.1=输出 */
  *(uint *)(iVar1 + 0x40) = *(uint *)(iVar1 + 0x40) & 0xfffffffb;          /* P2.1=改输入（双向） */
  *(uint *)(iVar1 + 0x58) = *(uint *)(iVar1 + 0x58) | 0x10;                /* FIO2SET P2.4=高 */
  *(uint *)(iVar1 + 0x58) = *(uint *)(iVar1 + 0x58) | 8;                   /* P2.3=高 */
  *(uint *)(iVar1 + 0x58) = *(uint *)(iVar1 + 0x58) | 2;                   /* P2.1=高 */
  return;
}
