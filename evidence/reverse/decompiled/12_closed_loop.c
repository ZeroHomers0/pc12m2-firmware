/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 12：闭环 PID 控制器
 *
 * closed_loop_integral：位置式 PID（输出=累计量），含误差死区 + 分段除数表：
 *   · 输入：param_1=给定、param_2=反馈、param_3/4=系数
 *     缓存 0x10010CB0/CB4/CB8；误差 0x10010CCC=给定-反馈；历史 0x10010CC4
 *   · 死区 0x10010CD4/0x10010CE0：误差大 → 增益 0x10010CD8；
 *     死区上界内 → 增益 0x10010CE4；下界内 → 增益 0x10010CE8
 *   · 分段除数表（自适应积分步长）：误差区间
 *       <0xDC/0x226/1000/0x5DC/2000/0x9C4/3000/4000/5000/0x1771
 *       → 除数 8/0x0F/0x1E/0x2A/0x37/0x50/100/0x78/0x96/0xB4
 *     通道选择：0x10010CEC==0 用表 0x10010CF4、==1 用表 0x10010F40
 *   · 控制方式 2（0x10010F50==2）：固定除数 0x46
 *   · PID 公式（累加 0x10010F78，钳位 0x10010F7C 上限 / 0x10010F80 下限）：
 *     Δout = (P*10*(e + e1*-2 + int) + I*2*e + (e-e1)*D*1*2) / divisor
 *       P=0x10010F6C  I=0x10010F68  D=0x10010F5C  系数=0x10010F58
 * closed_loop_wrapper：节流器（0x10010F84 计数==0 才调用），缓存 0x10010F88
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 闭环链路（FUN_00010f0a → FUN_000108b0）→ docs/PROGRESS_2026-08-20.md §4e
 *   · PID 菜单 9 屏（档 + P/I + 隐藏分区增益）→ docs/MENU_PARAMETER_MAPPING.md §4
 *   · 闭环误差分区参数 = EEPROM reg 0x6E-0x72 → DATA_SEGMENT_2026-08-21.md §3.3
 * ========================================================================== */

/* 0x000108B0 —— 位置式 PID（积分）闭环计算 */
int closed_loop_integral(int param_1,int param_2,undefined4 param_3,undefined4 param_4)
{
  int *piVar1;
  int *piVar2;

  *DAT_00010cb0 = param_3;
  *DAT_00010cb4 = param_4;
  *DAT_00010cb8 = 3;
  *DAT_00010cbc = param_1;                  /* 给定 */
  *DAT_00010cc0 = param_2;                  /* 反馈 */
  *DAT_00010cc8 = *DAT_00010cc4;
  *DAT_00010cc4 = *DAT_00010ccc;
  *DAT_00010ccc = *DAT_00010cbc - *DAT_00010cc0;   /* 当前误差 */
  piVar1 = DAT_00010cd0;
  if (*DAT_00010cc0 <= *DAT_00010cbc) {
    *DAT_00010cd0 = *DAT_00010cbc - *DAT_00010cc0; /* 误差（正值路径） */
    if ((int)(uint)*DAT_00010cd4 <= *piVar1) {     /* 误差 ≥ 死区上界 */
      *DAT_00010cdc = (uint)*DAT_00010cd8;
    }
    if ((*DAT_00010cd0 < (int)(uint)*DAT_00010cd4) && ((int)(uint)*DAT_00010ce0 < *DAT_00010cd0)) {
      *DAT_00010cdc = (uint)*DAT_00010ce4;         /* 死区上界内 */
    }
    if (*DAT_00010cd0 <= (int)(uint)*DAT_00010ce0) {
      *DAT_00010cdc = (uint)*DAT_00010ce8;         /* 误差 ≤ 死区下界 */
    }
  }
  piVar1 = DAT_00010cd0;
  if (*DAT_00010cbc < *DAT_00010cc0) {
    *DAT_00010cd0 = *DAT_00010cc0 - *DAT_00010cbc; /* 误差（负值路径，绝对值） */
    if ((int)(uint)*DAT_00010cd4 <= *piVar1) {
      *DAT_00010cdc = (uint)*DAT_00010cd8;
    }
    if ((*DAT_00010cd0 < (int)(uint)*DAT_00010cd4) && ((int)(uint)*DAT_00010ce0 < *DAT_00010cd0)) {
      *DAT_00010cdc = (uint)*DAT_00010ce4;
    }
    if (*DAT_00010cd0 <= (int)(uint)*DAT_00010ce0) {
      *DAT_00010cdc = (uint)*DAT_00010ce8;
    }
  }
  if (*DAT_00010cec == '\0') {
    /* —— 通道 1 分段除数表（误差越大除数越小 → 步进越快）—— */
    if (*DAT_00010cf0 < 0xdc) { *DAT_00010cf4 = 8; }
    if ((0xdb < *DAT_00010cf0) && (*DAT_00010cf0 < 0x226)) { *DAT_00010cf4 = 0xf; }
    if ((0x225 < *DAT_00010cf0) && (*DAT_00010cf0 < 1000)) { *DAT_00010cf4 = 0x1e; }
    if ((999 < *DAT_00010cf0) && (*DAT_00010cf0 < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
    if ((0x5db < *DAT_00010cf0) && (*DAT_00010cf0 < 2000)) { *DAT_00010cf4 = 0x37; }
    if ((1999 < *DAT_00010cf0) && (*DAT_00010cf0 < 0x9c4)) { *DAT_00010cf4 = 0x50; }
    if ((0x9c3 < *DAT_00010cf0) && (*DAT_00010cf0 < 3000)) { *DAT_00010cf4 = 100; }
    if ((2999 < *DAT_00010cf0) && (*DAT_00010cf0 < 4000)) { *DAT_00010cf4 = 0x78; }
    if ((3999 < *DAT_00010cf0) && (*DAT_00010cf0 < 5000)) { *DAT_00010cf4 = 0x96; }
    if ((4999 < *DAT_00010cf0) && (*DAT_00010cf0 < 0x1771)) { *DAT_00010cf4 = 0xb4; }
    if (*DAT_00010cf8 == 1) {
      /* —— 通道 1 另套表（0x10010CFC 为误差源）—— */
      if (*DAT_00010cfc < 0xdc) { *DAT_00010cf4 = 8; }
      if ((0xdb < *DAT_00010cfc) && (*DAT_00010cfc < 0x226)) { *DAT_00010cf4 = 0xf; }
      if ((0x225 < *DAT_00010cfc) && (*DAT_00010cfc < 1000)) { *DAT_00010cf4 = 0x1e; }
      if ((999 < *DAT_00010cfc) && (*DAT_00010cfc < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
      if ((0x5db < *DAT_00010cfc) && (*DAT_00010cfc < 2000)) { *DAT_00010cf4 = 0x37; }
      if ((1999 < *DAT_00010cfc) && (*DAT_00010cfc < 0x9c4)) { *DAT_00010cf4 = 0x50; }
      if ((0x9c3 < *DAT_00010cfc) && (*DAT_00010cfc < 3000)) { *DAT_00010cf4 = 100; }
      if ((2999 < *DAT_00010cfc) && (*DAT_00010cfc < 4000)) { *DAT_00010cf4 = 0x78; }
      if ((3999 < *DAT_00010cfc) && (*DAT_00010cfc < 5000)) { *DAT_00010cf4 = 0x96; }
      if ((4999 < *DAT_00010cfc) && (*DAT_00010cfc < 0x1771)) { *DAT_00010cf4 = 0xb4; }
    }
  }
  if (*DAT_00010cec == '\x01') {
    /* —— 通道 2 分段除数表（0x10010F40）—— */
    if (*DAT_00010cfc < 0xdc) { *DAT_00010cf4 = 8; }
    if ((0xdb < *DAT_00010cfc) && (*DAT_00010cfc < 0x226)) { *DAT_00010cf4 = 0xf; }
    if ((0x225 < *DAT_00010cfc) && (*DAT_00010cfc < 1000)) { *DAT_00010cf4 = 0x1e; }
    if ((999 < *DAT_00010cfc) && (*DAT_00010cfc < 0x5dc)) { *DAT_00010cf4 = 0x2a; }
    if ((0x5db < *DAT_00010cfc) && (*DAT_00010cfc < 2000)) { *DAT_00010cf4 = 0x37; }
    if ((1999 < *DAT_00010cfc) && (*DAT_00010cfc < 0x9c4)) { *DAT_00010cf4 = 0x50; }
    if ((0x9c3 < *DAT_00010cfc) && (*DAT_00010cfc < 3000)) { *DAT_00010cf4 = 100; }
    if ((2999 < *DAT_00010cfc) && (*DAT_00010cfc < 4000)) { *DAT_00010f40 = 0x78; }
    if ((3999 < *DAT_00010f44) && (*DAT_00010f44 < 5000)) { *DAT_00010f40 = 0x96; }
    if ((4999 < *DAT_00010f44) && (*DAT_00010f44 < 0x1771)) { *DAT_00010f40 = 0xb4; }
    if (*DAT_00010f48 == 1) {
      if (*DAT_00010f4c < 0xdc) { *DAT_00010f40 = 8; }
      if ((0xdb < *DAT_00010f4c) && (*DAT_00010f4c < 0x226)) { *DAT_00010f40 = 0xf; }
      if ((0x225 < *DAT_00010f4c) && (*DAT_00010f4c < 1000)) { *DAT_00010f40 = 0x1e; }
      if ((999 < *DAT_00010f4c) && (*DAT_00010f4c < 0x5dc)) { *DAT_00010f40 = 0x2a; }
      if ((0x5db < *DAT_00010f4c) && (*DAT_00010f4c < 2000)) { *DAT_00010f40 = 0x37; }
      if ((1999 < *DAT_00010f4c) && (*DAT_00010f4c < 0x9c4)) { *DAT_00010f40 = 0x50; }
      if ((0x9c3 < *DAT_00010f4c) && (*DAT_00010f4c < 3000)) { *DAT_00010f40 = 100; }
      if ((2999 < *DAT_00010f4c) && (*DAT_00010f4c < 4000)) { *DAT_00010f40 = 0x78; }
      if ((3999 < *DAT_00010f4c) && (*DAT_00010f4c < 5000)) { *DAT_00010f40 = 0x96; }
      if ((4999 < *DAT_00010f4c) && (*DAT_00010f4c < 0x1771)) { *DAT_00010f40 = 0xb4; }
    }
  }
  if (*DAT_00010f50 == '\x02') {
    *DAT_00010f40 = 0x46;                       /* 控制方式 2：固定除数 0x46 */
  }
  piVar1 = DAT_00010f54;
  *DAT_00010f54 = 1;
  piVar2 = DAT_00010f74;
  *DAT_00010f74 =
       (*DAT_00010f6c * *DAT_00010f58 * 10 * (*DAT_00010f60 + *DAT_00010f64 * -2 + *DAT_00010f70) +
       *DAT_00010f68 * *DAT_00010f58 * 2 * *DAT_00010f60 +
       (*DAT_00010f60 - *DAT_00010f64) * *DAT_00010f5c * *piVar1 * *DAT_00010f58 * 2) /
       *DAT_00010f40;
  piVar1 = DAT_00010f78;
  *DAT_00010f78 = *DAT_00010f78 + *piVar2;      /* 位置式累加 */
  if (DAT_00010f7c < *piVar1) {
    *DAT_00010f78 = DAT_00010f7c;               /* 上限钳位 */
  }
  if (*DAT_00010f78 < DAT_00010f80) {
    *DAT_00010f78 = DAT_00010f80;               /* 下限钳位 */
  }
  return *DAT_00010f78;
}

/* 0x00010F0A —— 闭环节流包装：0x10010F84 计数==0 才重算并缓存 0x10010F88 */
undefined4
closed_loop_wrapper(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)
{
  int *piVar1;
  undefined4 uVar2;

  piVar1 = DAT_00010f84;
  *DAT_00010f84 = *DAT_00010f84 + 1;
  if (*piVar1 != 0) {
    *piVar1 = 0;
    uVar2 = closed_loop_integral(param_1,param_2,param_3,param_4);
    *DAT_00010f88 = uVar2;
  }
  return *DAT_00010f88;
}
