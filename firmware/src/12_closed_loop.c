/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 12：闭环 PID（位置式/积分式，三路）
 *
 * closed_loop_integral：位置式 PID 计算，工作区在 0x100020D0 起的 RAM 全局。
 *   · 给定/反馈误差分两种符号路径，各按死区三段（上界/带内/下界）选择输出系数；
 *   · 分段除数表按误差绝对值选除数（误差越大除数越小 → 步进越快）；
 *   · 末段公式算位置式输出（0x10002108），并累加到累加器（0x100020F8），
 *     上限钳 0x00116520、下限钳 0x0005CC60。
 * closed_loop_wrapper：0x100020F4 计数节流——计数清零才重算并缓存 0x1000212C，
 *   其余调用直接返回缓存（相当于按固定周期重算）。
 *
 * 调用点：src/09_output_stage.c 两处
 *   closed_loop_wrapper(setpoint,feedback,coef_a,coef_b) → 0x1000F2C4 / 0x1000F760；
 *   控制方式（0x10001634）选择走哪条通道，闭环输出随后做上下限钳位。
 *
 * 说明：globals 中 DAT_10xxx 已按访问语义分型（ptr_word / value）；
 *       p_pid / p_pid_out 原反编译为 int*，此处按 volatile uint32_t* 用
 *       （消除 "discards volatile" 警告，访问语义不变）。
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 触发/PID 全图 → docs/PROGRESS_2026-08-20.md、docs/state_machine_analysis.md
 *   · 闭环调用与钳位上下文 → src/09_output_stage.c
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"
#include "inc/consts.h"

/* 0x00010A9C —— 位置式 PID（积分）闭环计算：给定 setpoint 与反馈 feedback 求误差，
 *   按误差符号分别用死区三段（上界 / 带内 / 下界）选输出系数；再按控制方式
 *   选分段除数表（误差越大除数越小 → 步进越快）；最后算位置式输出并累加、钳位。
 *   参数：setpoint=给定、feedback=反馈、coef_a/coef_b=PID 系数（写入 PID 工作槽，
 *   具体系数含义待核实）。p_pid 为工作寄存器指针（不同阶段指向不同寄存器，见内联），
 *   p_pid_out 指向位置增量输出。 */
int closed_loop_integral(int setpoint,int feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *p_pid;
  volatile uint32_t *p_pid_out;

  *pid_kp = coef_a;                  /* PID 系数槽 A（=0x10002100，见公式 coef_a 项） */
  *pid_kd = coef_b;                  /* PID 系数槽 B（=0x10002104，见公式 coef_b 项） */
  *pid_ki = 3;
  *pid_target = setpoint;                /* 给定 */
  *pid_actual = feedback;                /* 反馈 */
  *pid_err_prev2 = *DAT_00010eb0;           /* 上上次误差滚动 */
  *DAT_00010eb0 = *DAT_00010eb8;           /* 上次误差滚动 */
  *DAT_00010eb8 = *pid_target - *pid_actual;   /* 当前误差 */
  p_pid = pid_err_abs;                    /* p_pid → 误差值寄存器 0x10002118 */
  if (*pid_actual <= *pid_target) {
    *pid_err_abs = *pid_target - *pid_actual; /* 误差（正值路径） */
    if ((int)(uint)*g_cl_thresh_hi <= *p_pid) {     /* 误差 ≥ 死区上界 */
      *pid_gain_sel = (uint)*g_cl_gain_big;
    }
    if ((*pid_err_abs < (int)(uint)*g_cl_thresh_hi) && ((int)(uint)*g_cl_thresh_lo < *pid_err_abs)) {
      *pid_gain_sel = (uint)*g_cl_gain_mid;         /* 死区上界内 */
    }
    if (*pid_err_abs <= (int)(uint)*g_cl_thresh_lo) {
      *pid_gain_sel = (uint)*g_cl_gain_small;         /* 误差 ≤ 死区下界 */
    }
  }
  p_pid = pid_err_abs;                    /* p_pid → 误差值寄存器（负值路径） */
  if (*pid_target < *pid_actual) {
    *pid_err_abs = *pid_actual - *pid_target; /* 误差（负值路径，绝对值） */
    if ((int)(uint)*g_cl_thresh_hi <= *p_pid) {
      *pid_gain_sel = (uint)*g_cl_gain_big;
    }
    if ((*pid_err_abs < (int)(uint)*g_cl_thresh_hi) && ((int)(uint)*g_cl_thresh_lo < *pid_err_abs)) {
      *pid_gain_sel = (uint)*g_cl_gain_mid;
    }
    if (*pid_err_abs <= (int)(uint)*g_cl_thresh_lo) {
      *pid_gain_sel = (uint)*g_cl_gain_small;
    }
  }
  if (*g_gain_sel == '\0') {
    /* —— 通道 1 分段除数表（误差越大除数越小 → 步进越快）—— */
    if (*g_gain_a < 0xdc) { *pid_divisor = 8; }
    if ((0xdb < *g_gain_a) && (*g_gain_a < 0x226)) { *pid_divisor = 0xf; }
    if ((0x225 < *g_gain_a) && (*g_gain_a < 1000)) { *pid_divisor = 0x1e; }
    if ((999 < *g_gain_a) && (*g_gain_a < 0x5dc)) { *pid_divisor = 0x2a; }
    if ((0x5db < *g_gain_a) && (*g_gain_a < 2000)) { *pid_divisor = 0x37; }
    if ((1999 < *g_gain_a) && (*g_gain_a < 0x9c4)) { *pid_divisor = 0x50; }
    if ((0x9c3 < *g_gain_a) && (*g_gain_a < 3000)) { *pid_divisor = 100; }
    if ((2999 < *g_gain_a) && (*g_gain_a < 4000)) { *pid_divisor = 0x78; }
    if ((3999 < *g_gain_a) && (*g_gain_a < 5000)) { *pid_divisor = 0x96; }
    if ((4999 < *g_gain_a) && (*g_gain_a < RANGE_MAX)) { *pid_divisor = 0xb4; }
    if (*DAT_00010ee4 == 1) {
      /* —— 通道 1 另套表（0x10010CFC 为误差源）—— */
      if (*g_gain_b < 0xdc) { *pid_divisor = 8; }
      if ((0xdb < *g_gain_b) && (*g_gain_b < 0x226)) { *pid_divisor = 0xf; }
      if ((0x225 < *g_gain_b) && (*g_gain_b < 1000)) { *pid_divisor = 0x1e; }
      if ((999 < *g_gain_b) && (*g_gain_b < 0x5dc)) { *pid_divisor = 0x2a; }
      if ((0x5db < *g_gain_b) && (*g_gain_b < 2000)) { *pid_divisor = 0x37; }
      if ((1999 < *g_gain_b) && (*g_gain_b < 0x9c4)) { *pid_divisor = 0x50; }
      if ((0x9c3 < *g_gain_b) && (*g_gain_b < 3000)) { *pid_divisor = 100; }
      if ((2999 < *g_gain_b) && (*g_gain_b < 4000)) { *pid_divisor = 0x78; }
      if ((3999 < *g_gain_b) && (*g_gain_b < 5000)) { *pid_divisor = 0x96; }
      if ((4999 < *g_gain_b) && (*g_gain_b < RANGE_MAX)) { *pid_divisor = 0xb4; }
    }
  }
  if (*gain_sel == '\x01') {
    /* —— 通道 2 分段除数表（0x10010F40）—— */
    if (*g_gain_b < 0xdc) { *pid_divisor = 8; }
    if ((0xdb < *g_gain_b) && (*g_gain_b < 0x226)) { *pid_divisor = 0xf; }
    if ((0x225 < *g_gain_b) && (*g_gain_b < 1000)) { *pid_divisor = 0x1e; }
    if ((999 < *g_gain_b) && (*g_gain_b < 0x5dc)) { *pid_divisor = 0x2a; }
    if ((0x5db < *g_gain_b) && (*g_gain_b < 2000)) { *pid_divisor = 0x37; }
    if ((1999 < *g_gain_b) && (*g_gain_b < 0x9c4)) { *pid_divisor = 0x50; }
    if ((0x9c3 < *g_gain_b) && (*g_gain_b < 3000)) { *pid_divisor = 100; }
    if ((2999 < *g_gain_b) && (*g_gain_b < 4000)) { *DAT_0001112c = 0x78; }
    if ((3999 < *g_gain_b) && (*g_gain_b < 5000)) { *DAT_0001112c = 0x96; }
    if ((4999 < *g_gain_b) && (*g_gain_b < RANGE_MAX)) { *DAT_0001112c = 0xb4; }
    if (*DAT_00011134 == 1) {
      if (*g_gain_a < 0xdc) { *DAT_0001112c = 8; }
      if ((0xdb < *g_gain_a) && (*g_gain_a < 0x226)) { *DAT_0001112c = 0xf; }
      if ((0x225 < *g_gain_a) && (*g_gain_a < 1000)) { *DAT_0001112c = 0x1e; }
      if ((999 < *g_gain_a) && (*g_gain_a < 0x5dc)) { *DAT_0001112c = 0x2a; }
      if ((0x5db < *g_gain_a) && (*g_gain_a < 2000)) { *DAT_0001112c = 0x37; }
      if ((1999 < *g_gain_a) && (*g_gain_a < 0x9c4)) { *DAT_0001112c = 0x50; }
      if ((0x9c3 < *g_gain_a) && (*g_gain_a < 3000)) { *DAT_0001112c = 100; }
      if ((2999 < *g_gain_a) && (*g_gain_a < 4000)) { *DAT_0001112c = 0x78; }
      if ((3999 < *g_gain_a) && (*g_gain_a < 5000)) { *DAT_0001112c = 0x96; }
      if ((4999 < *g_gain_a) && (*g_gain_a < RANGE_MAX)) { *DAT_0001112c = 0xb4; }
    }
  }
  if (*gain_sel == '\x02') {
    *DAT_0001112c = 0x46;                       /* 控制方式 2：固定除数 0x46 */
  }
  p_pid = pid_const;                    /* p_pid → 增益寄存器 0x10002124（置 1） */
  *pid_const = 1;
  p_pid_out = pid_integral;                /* 位置增量输出 0x10002108（OLD 0x110C0） */
  *pid_integral =
       /* 位置式 PID 分子：三项分别对应比例/积分/微分贡献（系数见各 DAT 槽）。
        * 原机码对该误差链按【带符号】算术：误差/上次/上上次误差为带符号量（int32），
        * 末段用 Cortex-M3 SDIV 做符号除法。若按无符号算，负误差 0xFFFFFE70 被当正数，
        * 分子回环成大正数 → 输出/钳位方向与金标准背离（W7 差分测试 已证实）。
        * 故对参与公式的各误差槽按 int32 取读、末段除按 int32；钳位比较也按符号。 */
       (int32_t)((int32_t)*DAT_00011158 * (int32_t)*DAT_00011144 * 10 *
                 ((int32_t)*DAT_0001114c + (int32_t)*DAT_00011150 * -2 + (int32_t)*DAT_0001115c) +
                 (int32_t)*DAT_00011154 * (int32_t)*DAT_00011144 * 2 * (int32_t)*DAT_0001114c +
                 ((int32_t)*DAT_0001114c - (int32_t)*DAT_00011150) * (int32_t)*DAT_00011148 *
                   (int32_t)*p_pid * (int32_t)*DAT_00011144 * 2) /
       (int32_t)*DAT_0001112c;              /* 除以本次选定的除数（误差分段表，SDIV） */
  p_pid = DAT_00011164;                    /* p_pid → 累加器 0x100020F8 */
  *DAT_00011164 = *DAT_00011164 + *p_pid_out;      /* 位置式累加（补码加，位结果一致） */
  if ((int32_t)DAT_00011168 < (int32_t)*p_pid) {   /* 上限钳位 0x00116520（符号比较） */
    *DAT_00011164 = DAT_00011168;
  }
  if ((int32_t)*DAT_00011164 < (int32_t)DAT_0001116c) {   /* 下限钳位 0x0005CC60（符号比较） */
    *DAT_00011164 = DAT_0001116c;
  }
  return *DAT_00011164;
}

/* 0x000110F6 —— 闭环节流包装（12p）：每次调用把 *DAT_00011170（0x100020CC，重算计数）
 *   加 1，计数非 0 时才清零并真正调用一次 closed_loop_integral()，其输出缓存到
 *   *DAT_00011174（0x10002104，= pid_kd 槽）；其余各次调用直接返回上次缓存值。
 *   注意（12p 与 6p 差异）：重算计数 = 0x100020CC、缓存 = 0x10002104，
 *   而 **0x100020F4 是 integral 公式中的增益选择槽（pid_gain_sel）**，不是重算计数
 *   （6p 文件曾误把 0x100020F4 当重算计数，12p 反汇编 0x110F6 证实应改 0x100020CC）。
 *   参数含义与 closed_loop_integral 一致：setpoint=给定、feedback=反馈、
 *   coef_a/coef_b=PID 系数槽。 */
uint32_t
closed_loop_wrapper(uint32_t setpoint,uint32_t feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *p_recalc_cnt;
  uint32_t result;

  p_recalc_cnt = DAT_00011170;
  *DAT_00011170 = *DAT_00011170 + 1;
  if (*p_recalc_cnt != 0) {
    *p_recalc_cnt = 0;
    result = closed_loop_integral(setpoint,feedback,coef_a,coef_b);
    *DAT_00011174 = result;
  }
  return *DAT_00011174;
}
