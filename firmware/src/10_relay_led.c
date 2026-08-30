/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 10：继电器 / 状态指示灯输出控制
 *
 * 统一模式：level >= 1 → 置位（FIO*SET，输出高 = 动作）；level < 1 → 清零（FIO*CLR）
 *   PTR_DAT_00010640 = FIO 池基址 0x2009C000（value 型 globals，偏移互算术正确）：
 *     +0x18 FIO0SET / +0x1c FIO0CLR（P0 口输出，0x10/0x20/0x40 = P0.20/21/22）
 *     +0x38 FIO1SET / +0x3c FIO1CLR（P1 口输出 状态灯）
 * 继电器/指示灯映射（对照 HARDWARE_VERIFICATION_2026-08-20.md）：
 *   P0.20 = RLY3 备用（reg61 远程使能）  P0.21 = RLY2 报警   P0.22 = RLY1 运行
 *   P1.20-23 = 状态指示灯；P1.22 = 触发/运行指示（见 09_output_stage.c fio1_pin22_ctrl）
 * 导出：2026-08-21（L0 语义化：各函数入参统一命名 level）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* 0x00010588 —— P0.20 继电器输出（RLY3 备用 / reg61 远程使能）
 *   level>=1 → FIO0SET 置位 P0.20；否则 FIO0CLR 清零 */
void out_relay_p020(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_00010640 + 0x1c) = *(volatile uint *)(PTR_DAT_00010640 + 0x1c) | 0x100000;
  }
  else {
    *(volatile uint *)(PTR_DAT_00010640 + 0x18) = *(volatile uint *)(PTR_DAT_00010640 + 0x18) | 0x100000;
  }
}

/* 0x000105A8 —— P0.21 继电器输出（RLY2 报警） */
void out_relay_p021(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_00010640 + 0x1c) = *(volatile uint *)(PTR_DAT_00010640 + 0x1c) | 0x200000;
  }
  else {
    *(volatile uint *)(PTR_DAT_00010640 + 0x18) = *(volatile uint *)(PTR_DAT_00010640 + 0x18) | 0x200000;
  }
}

/* 0x000105C8 —— P1.20 状态灯控制输出 */
void fio1_pin20_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_00010640 + 0x3c) = *(volatile uint *)(PTR_DAT_00010640 + 0x3c) | 0x100000;
  }
  else {
    *(volatile uint *)(PTR_DAT_00010640 + 0x38) = *(volatile uint *)(PTR_DAT_00010640 + 0x38) | 0x100000;
  }
}

/* 0x000105E8 —— P1.21 状态灯控制输出 */
void fio1_pin21_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_00010640 + 0x3c) = *(volatile uint *)(PTR_DAT_00010640 + 0x3c) | 0x200000;
  }
  else {
    *(volatile uint *)(PTR_DAT_00010640 + 0x38) = *(volatile uint *)(PTR_DAT_00010640 + 0x38) | 0x200000;
  }
}

/* 0x00010608 —— P1.23 状态灯控制输出 */
void fio1_pin23_ctrl(int level)
{
  if (level < 1) {
    *(volatile uint *)(PTR_DAT_00010640 + 0x3c) = *(volatile uint *)(PTR_DAT_00010640 + 0x3c) | 0x800000;
  }
  else {
    *(volatile uint *)(PTR_DAT_00010640 + 0x38) = *(volatile uint *)(PTR_DAT_00010640 + 0x38) | 0x800000;
  }
}
