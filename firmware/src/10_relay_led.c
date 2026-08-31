/* =============================================================================
 * LPC1765FBD100 (PC12M-2 / 12 相晶闸管移相触发板，LPC1765 / Cortex-M3)
 * 反编译源码导出 — 模块 10：继电器 / 状态指示灯输出控制
 *
 * 12p（PC12M-2）标识：固件 backup/pc12m2_orig.bin（SHA 2bc60868…bd271bd1）
 *   12p 继电器/灯函数位于 0x107E0/0x10800/0x10820/0x10840（6p 为 0x10588..0x10608），
 *   12p **无 out_relay_p020（P0.20 继电器）**——94 函数清单不含 0x10588，state_machine
 *   (0x4464) 也不调用 → 6p-only，已删除（task #26）。
 *   0x107e0 = P0.21 报警继电器 RLY2（FIO0 +0x18/+0x1c，0x200000）
 *   0x10800 = P1.20 状态灯（FIO1 +0x38/+0x3c，0x100000）
 *   0x10820 = P1.21 状态灯（FIO1 +0x38/+0x3c，0x200000）
 *   0x10840 = P1.23 状态灯（FIO1 +0x38/+0x3c，0x800000）
 *
 * 统一模式：level >= 1 → 置位（FIO*SET，输出高 = 动作）；level < 1 → 清零（FIO*CLR）
 *   DAT_00010878 = FIO 池基址 0x2009C000（uint32_t 标量，偏移互算术正确）：
 *     +0x18 FIO0SET / +0x1c FIO0CLR（P0 口输出：0x200000 = P0.21 RLY2 报警）
 *     +0x38 FIO1SET / +0x3c FIO1CLR（P1 口输出 状态灯）
 * 继电器/指示灯映射（对照 HARDWARE_VERIFICATION_2026-08-20.md，12p 无 P0.20）：
 *   P0.21 = RLY2 报警   P0.22 = RLY1 运行（fio0_pin22_ctrl，见 09_output_stage.c）
 *   P1.20-23 = 状态指示灯；P1.22 = 触发/运行指示（09_output_stage.c fio1_pin22_ctrl）
 * 导出：2026-08-31（12p 重写：地址移位 + 删 6p-only out_relay_p020）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* 0x000107E0 —— P0.21 报警继电器输出（RLY2）
 *   level>=1 → FIO0SET 置位 P0.21；否则 FIO0CLR 清零（12p；6p 0x105A8） */
void out_relay_p021(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_00010878 + 0x1c) = *(volatile uint *)(DAT_00010878 + 0x1c) | 0x200000;
  }
  else {
    *(volatile uint *)(DAT_00010878 + 0x18) = *(volatile uint *)(DAT_00010878 + 0x18) | 0x200000;
  }
}

/* 0x00010800 —— P1.20 状态灯控制输出（12p；6p 0x105C8） */
void fio1_pin20_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_00010878 + 0x3c) = *(volatile uint *)(DAT_00010878 + 0x3c) | 0x100000;
  }
  else {
    *(volatile uint *)(DAT_00010878 + 0x38) = *(volatile uint *)(DAT_00010878 + 0x38) | 0x100000;
  }
}

/* 0x00010820 —— P1.21 状态灯控制输出（12p；6p 0x105E8） */
void fio1_pin21_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_00010878 + 0x3c) = *(volatile uint *)(DAT_00010878 + 0x3c) | 0x200000;
  }
  else {
    *(volatile uint *)(DAT_00010878 + 0x38) = *(volatile uint *)(DAT_00010878 + 0x38) | 0x200000;
  }
}

/* 0x00010840 —— P1.23 状态灯控制输出（12p；6p 0x10608） */
void fio1_pin23_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(DAT_00010878 + 0x3c) = *(volatile uint *)(DAT_00010878 + 0x3c) | 0x800000;
  }
  else {
    *(volatile uint *)(DAT_00010878 + 0x38) = *(volatile uint *)(DAT_00010878 + 0x38) | 0x800000;
  }
}
