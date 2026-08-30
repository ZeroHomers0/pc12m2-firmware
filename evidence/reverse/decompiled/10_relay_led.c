/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 10：继电器 / LED 输出（FIO0/FIO1 高低电平控制）
 *
 * 统一模式：param_1>=1 → 置位（FIO*SET）；param_1<1 → 清零（FIO*CLR）
 *   PTR_DAT_00010640 = FIO 池基址 0x2009C000（+0x18=FIO0SET +0x1C=FIO0CLR
 *   +0x38=FIO1SET +0x3C=FIO1CLR）
 * 引脚分配（软件编号 vs 硬件 RLY 编号错位，2026-08-21 复核）：
 *   P0.20 → 软件「继电器1」= 硬件 RLY3 备用（out_relay_p020；reg61 远程使能）
 *   P0.21 → 软件「继电器2」= 硬件 RLY2 报警（out_relay_p021）
 *   P1.20 → 控制 1（fio1_pin20_ctrl）
 *   P1.21 → 控制 2（fio1_pin21_ctrl）
 *   P1.23 → 控制 3（fio1_pin23_ctrl）
 *   运行继电器 RLY1=P0.22 见 09_output_stage.c fio0_pin22_ctrl
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 继电器硬件（RLY1/2/3 = P0.22/21/20，ULN2003A U29）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §二.3
 *   · 状态 LED（P1.20-23：恒压/恒流/运行/故障）→ §二.4
 *   ✅ 编号差异复核完成（2026-08-21）：out_relay_p020 仅被 state_machine(0x49F4)
 *     与 modbus_dispatch(0xE02C/0xE034=reg61 远程输出使能) 调用 → 软件「继电器1」= P0.20
 *     = 硬件 RLY3 备用继电器；运行继电器 RLY1=P0.22 由 fio0_pin22_ctrl 控制（09 模块）。
 * ========================================================================== */

/* 0x00010588 —— P0.20 继电器输出 */
void out_relay_p020(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_00010640 + 0x1c) = *(uint *)(PTR_DAT_00010640 + 0x1c) | 0x100000;
  }
  else {
    *(uint *)(PTR_DAT_00010640 + 0x18) = *(uint *)(PTR_DAT_00010640 + 0x18) | 0x100000;
  }
  return;
}

/* 0x000105A8 —— P0.21 继电器输出 */
void out_relay_p021(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_00010640 + 0x1c) = *(uint *)(PTR_DAT_00010640 + 0x1c) | 0x200000;
  }
  else {
    *(uint *)(PTR_DAT_00010640 + 0x18) = *(uint *)(PTR_DAT_00010640 + 0x18) | 0x200000;
  }
  return;
}

/* 0x000105C8 —— P1.20 控制输出 */
void fio1_pin20_ctrl(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_00010640 + 0x3c) = *(uint *)(PTR_DAT_00010640 + 0x3c) | 0x100000;
  }
  else {
    *(uint *)(PTR_DAT_00010640 + 0x38) = *(uint *)(PTR_DAT_00010640 + 0x38) | 0x100000;
  }
  return;
}

/* 0x000105E8 —— P1.21 控制输出 */
void fio1_pin21_ctrl(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_00010640 + 0x3c) = *(uint *)(PTR_DAT_00010640 + 0x3c) | 0x200000;
  }
  else {
    *(uint *)(PTR_DAT_00010640 + 0x38) = *(uint *)(PTR_DAT_00010640 + 0x38) | 0x200000;
  }
  return;
}

/* 0x00010608 —— P1.23 控制输出 */
void fio1_pin23_ctrl(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(PTR_DAT_00010640 + 0x3c) = *(uint *)(PTR_DAT_00010640 + 0x3c) | 0x800000;
  }
  else {
    *(uint *)(PTR_DAT_00010640 + 0x38) = *(uint *)(PTR_DAT_00010640 + 0x38) | 0x800000;
  }
  return;
}
