/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 12：闭环 PID（位置式/积分式，三路）
 *
 * closed_loop_integral：位置式 PID 计算，工作区在 0x100020D0 起的 RAM 全局。
 *   · 给定/反馈误差分两种符号路径，各按死区三段（上界/带内/下界）选择输出系数；
 *   · 分段除数表按误差绝对值选除数（误差越大除数越小 → 步进越快）；
 *   · 末段公式算位置式输出（0x10002130），并累加（0x10002120），
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

/* 0x000108B0 —— 位置式 PID（积分）闭环计算：给定 setpoint 与反馈 feedback 求误差，
 *   按误差符号分别用死区三段（上界 / 带内 / 下界）选输出系数；再按控制方式
 *   选分段除数表（误差越大除数越小 → 步进越快）；最后算位置式输出并累加、钳位。
 *   参数：setpoint=给定、feedback=反馈、coef_a/coef_b=PID 系数（写入 PID 工作槽，
 *   具体系数含义待核实）。p_pid 为工作寄存器指针（不同阶段指向不同寄存器，见内联），
 *   p_pid_out 指向位置增量输出。 */
int closed_loop_integral(int setpoint,int feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *p_pid;
  volatile uint32_t *p_pid_out;

  *DAT_00010cb0 = coef_a;                  /* PID 系数槽 A（=0x10002100，见公式 coef_a 项） */
  *DAT_00010cb4 = coef_b;                  /* PID 系数槽 B（=0x10002104，见公式 coef_b 项） */
  *DAT_00010cb8 = 3;
  *DAT_00010cbc = setpoint;                /* 给定 */
  *DAT_00010cc0 = feedback;                /* 反馈 */
  *DAT_00010cc8 = *DAT_00010cc4;           /* 上上次误差滚动 */
  *DAT_00010cc4 = *DAT_00010ccc;           /* 上次误差滚动 */
  *DAT_00010ccc = *DAT_00010cbc - *DAT_00010cc0;   /* 当前误差 */
  p_pid = DAT_00010cd0;                    /* p_pid → 误差值寄存器 0x10002118 */
  if (*DAT_00010cc0 <= *DAT_00010cbc) {
    *DAT_00010cd0 = *DAT_00010cbc - *DAT_00010cc0; /* 误差（正值路径） */
    if ((int)(uint)*g_cl_thresh_hi <= *p_pid) {     /* 误差 ≥ 死区上界 */
      *DAT_00010cdc = (uint)*g_cl_gain_big;
    }
    if ((*DAT_00010cd0 < (int)(uint)*g_cl_thresh_hi) && ((int)(uint)*g_cl_thresh_lo < *DAT_00010cd0)) {
      *DAT_00010cdc = (uint)*g_cl_gain_mid;         /* 死区上界内 */
    }
    if (*DAT_00010cd0 <= (int)(uint)*g_cl_thresh_lo) {
      *DAT_00010cdc = (uint)*g_cl_gain_small;         /* 误差 ≤ 死区下界 */
    }
  }
  p_pid = DAT_00010cd0;                    /* p_pid → 误差值寄存器（负值路径） */
  if (*DAT_00010cbc < *DAT_00010cc0) {
    *DAT_00010cd0 = *DAT_00010cc0 - *DAT_00010cbc; /* 误差（负值路径，绝对值） */
    if ((int)(uint)*g_cl_thresh_hi <= *p_pid) {
      *DAT_00010cdc = (uint)*g_cl_gain_big;
    }
    if ((*DAT_00010cd0 < (int)(uint)*g_cl_thresh_hi) && ((int)(uint)*g_cl_thresh_lo < *DAT_00010cd0)) {
      *DAT_00010cdc = (uint)*g_cl_gain_mid;
    }
    if (*DAT_00010cd0 <= (int)(uint)*g_cl_thresh_lo) {
      *DAT_00010cdc = (uint)*g_cl_gain_small;
    }
  }
  if (*g_gain_sel == '\0') {
    /* —— 通道 1 分段除数表（误差越大除数越小 → 步进越快）—— */
    if (*g_gain_a < 0xdc) { *DAT_00010cf4 = 8; }
    if ((0xdb < *g_gain_a) && (*g_gain_a < 0x226)) { *DAT_00010cf4 = 0xf; }
    if ((0x225 < *g_gain_a) && (*g_gain_a < 1000)) { *DAT_00010cf4 = 0x1e; }
    if ((999 < *g_gain_a) && (*g_gain_a < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
    if ((0x5db < *g_gain_a) && (*g_gain_a < 2000)) { *DAT_00010cf4 = 0x37; }
    if ((1999 < *g_gain_a) && (*g_gain_a < 0x9c4)) { *DAT_00010cf4 = 0x50; }
    if ((0x9c3 < *g_gain_a) && (*g_gain_a < 3000)) { *DAT_00010cf4 = 100; }
    if ((2999 < *g_gain_a) && (*g_gain_a < 4000)) { *DAT_00010cf4 = 0x78; }
    if ((3999 < *g_gain_a) && (*g_gain_a < 5000)) { *DAT_00010cf4 = 0x96; }
    if ((4999 < *g_gain_a) && (*g_gain_a < RANGE_MAX)) { *DAT_00010cf4 = 0xb4; }
    if (*DAT_00010cf8 == 1) {
      /* —— 通道 1 另套表（0x10010CFC 为误差源）—— */
      if (*g_gain_b < 0xdc) { *DAT_00010cf4 = 8; }
      if ((0xdb < *g_gain_b) && (*g_gain_b < 0x226)) { *DAT_00010cf4 = 0xf; }
      if ((0x225 < *g_gain_b) && (*g_gain_b < 1000)) { *DAT_00010cf4 = 0x1e; }
      if ((999 < *g_gain_b) && (*g_gain_b < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
      if ((0x5db < *g_gain_b) && (*g_gain_b < 2000)) { *DAT_00010cf4 = 0x37; }
      if ((1999 < *g_gain_b) && (*g_gain_b < 0x9c4)) { *DAT_00010cf4 = 0x50; }
      if ((0x9c3 < *g_gain_b) && (*g_gain_b < 3000)) { *DAT_00010cf4 = 100; }
      if ((2999 < *g_gain_b) && (*g_gain_b < 4000)) { *DAT_00010cf4 = 0x78; }
      if ((3999 < *g_gain_b) && (*g_gain_b < 5000)) { *DAT_00010cf4 = 0x96; }
      if ((4999 < *g_gain_b) && (*g_gain_b < RANGE_MAX)) { *DAT_00010cf4 = 0xb4; }
    }
  }
  if (*g_gain_sel == '\x01') {
    /* —— 通道 2 分段除数表（0x10010F40）—— */
    if (*g_gain_b < 0xdc) { *DAT_00010cf4 = 8; }
    if ((0xdb < *g_gain_b) && (*g_gain_b < 0x226)) { *DAT_00010cf4 = 0xf; }
    if ((0x225 < *g_gain_b) && (*g_gain_b < 1000)) { *DAT_00010cf4 = 0x1e; }
    if ((999 < *g_gain_b) && (*g_gain_b < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
    if ((0x5db < *g_gain_b) && (*g_gain_b < 2000)) { *DAT_00010cf4 = 0x37; }
    if ((1999 < *g_gain_b) && (*g_gain_b < 0x9c4)) { *DAT_00010cf4 = 0x50; }
    if ((0x9c3 < *g_gain_b) && (*g_gain_b < 3000)) { *DAT_00010cf4 = 100; }
    if ((2999 < *g_gain_b) && (*g_gain_b < 4000)) { *DAT_00010f40 = 0x78; }
    if ((3999 < *g_gain_b) && (*g_gain_b < 5000)) { *DAT_00010f40 = 0x96; }
    if ((4999 < *g_gain_b) && (*g_gain_b < RANGE_MAX)) { *DAT_00010f40 = 0xb4; }
    if (*DAT_00010f48 == 1) {
      if (*g_gain_a < 0xdc) { *DAT_00010f40 = 8; }
      if ((0xdb < *g_gain_a) && (*g_gain_a < 0x226)) { *DAT_00010f40 = 0xf; }
      if ((0x225 < *g_gain_a) && (*g_gain_a < 1000)) { *DAT_00010f40 = 0x1e; }
      if ((999 < *g_gain_a) && (*g_gain_a < 0x5dc)) { *DAT_00010f40 = 0x2a; }
      if ((0x5db < *g_gain_a) && (*g_gain_a < 2000)) { *DAT_00010f40 = 0x37; }
      if ((1999 < *g_gain_a) && (*g_gain_a < 0x9c4)) { *DAT_00010f40 = 0x50; }
      if ((0x9c3 < *g_gain_a) && (*g_gain_a < 3000)) { *DAT_00010f40 = 100; }
      if ((2999 < *g_gain_a) && (*g_gain_a < 4000)) { *DAT_00010f40 = 0x78; }
      if ((3999 < *g_gain_a) && (*g_gain_a < 5000)) { *DAT_00010f40 = 0x96; }
      if ((4999 < *g_gain_a) && (*g_gain_a < RANGE_MAX)) { *DAT_00010f40 = 0xb4; }
    }
  }
  if (*g_gain_sel == '\x02') {
    *DAT_00010f40 = 0x46;                       /* 控制方式 2：固定除数 0x46 */
  }
  p_pid = DAT_00010f54;                    /* p_pid → 增益寄存器 0x10002124（置 1） */
  *DAT_00010f54 = 1;
  p_pid_out = g_pid_integral;                /* 位置增量输出 0x10002130 */
  *g_pid_integral =
       /* 位置式 PID 分子：三项分别对应比例/积分/微分贡献（系数见各 DAT 槽）。
        * 原机码对该误差链按【带符号】算术：误差/上次/上上次误差为带符号量（int32），
        * 末段用 Cortex-M3 SDIV 做符号除法。若按无符号算，负误差 0xFFFFFE70 被当正数，
        * 分子回环成大正数 → 输出/钳位方向与金标准背离（W7 差分测试 已证实）。
        * 故对参与公式的各误差槽按 int32 取读、末段除按 int32；钳位比较也按符号。 */
       (int32_t)((int32_t)*DAT_00010f6c * (int32_t)*DAT_00010f58 * 10 *
                 ((int32_t)*DAT_00010f60 + (int32_t)*DAT_00010f64 * -2 + (int32_t)*DAT_00010f70) +
                 (int32_t)*DAT_00010f68 * (int32_t)*DAT_00010f58 * 2 * (int32_t)*DAT_00010f60 +
                 ((int32_t)*DAT_00010f60 - (int32_t)*DAT_00010f64) * (int32_t)*DAT_00010f5c *
                   (int32_t)*p_pid * (int32_t)*DAT_00010f58 * 2) /
       (int32_t)*DAT_00010f40;              /* 除以本次选定的除数（误差分段表，SDIV） */
  p_pid = DAT_00010f78;                    /* p_pid → 累加器 0x10002120 */
  *DAT_00010f78 = *DAT_00010f78 + *p_pid_out;      /* 位置式累加（补码加，位结果一致） */
  if ((int32_t)DAT_00010f7c < (int32_t)*p_pid) {   /* 上限钳位 0x00116520（符号比较） */
    *DAT_00010f78 = DAT_00010f7c;
  }
  if ((int32_t)*DAT_00010f78 < (int32_t)DAT_00010f80) {   /* 下限钳位 0x0005CC60（符号比较） */
    *DAT_00010f78 = DAT_00010f80;
  }
  return *DAT_00010f78;
}

/* 0x00010F0A —— 闭环节流包装：每次调用把 *DAT_00010f84（0x100020F4，重算计数）加 1，
 *   计数非 0 时才清零并真正调用一次 closed_loop_integral()，其输出缓存到
 *   *g_cl_cached_out（0x1000212C）；中间各次调用直接返回上次缓存值，
 *   从而把 PID 重算节流到固定周期（每 N 个 tick 一次）。
 *   参数含义与 closed_loop_integral 一致：setpoint=给定、feedback=反馈、
 *   coef_a/coef_b=PID 系数槽。 */
uint32_t
closed_loop_wrapper(uint32_t setpoint,uint32_t feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *p_recalc_cnt;
  uint32_t result;

  p_recalc_cnt = DAT_00010f84;
  *DAT_00010f84 = *DAT_00010f84 + 1;
  if (*p_recalc_cnt != 0) {
    *p_recalc_cnt = 0;
    result = closed_loop_integral(setpoint,feedback,coef_a,coef_b);
    *g_cl_cached_out = result;
  }
  return *g_cl_cached_out;
}
