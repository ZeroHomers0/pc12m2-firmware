/* =============================================================================
 * LPC1765FBD100 (ST33C / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 02：12864 图形 LCD 显示层
 *
 * 硬件：12864 图形 LCD（P1 口，KS0107/KS0108 双控制器）
 *   控制线（反编译确证 2026-08-21）：
 *     P1.24 = 背光/控制线（lcd_ctrl_line；state_machine 屏幕超时>5000 拍后关闭）
 *     P1.25 = CS2（下半屏片选）   P1.26 = CS1（上半屏片选）
 *     P1.27 = RS（A0 命令/数据：命令=0 / 数据=1）
 *     P1.28 = E（使能脉冲）
 *   数据线 DB0-7（按位 SET/CLR，非连续）：DB7→P1.15、DB6→P1.14、DB5→P1.10、
 *     DB4→P1.9、DB3→P1.8、DB2→P1.4、DB1→P1.1、DB0→P1.0
 *   FIO1 位（SET=+0x38、CLR=+0x3C，FIO 池基址 0x2009C000）
 * 字体：8×8 ASCII 表 @0x10000FC0（0x24 字符）、16×16 汉字表 @0x10000FCC（0x8F 字）
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · 12864 图形 LCD（双控制器 KS0107/KS0108）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §一.14、§二.1
 *   · 菜单屏渲染 / 编辑状态分发 → docs/MENU_PARAMETER_MAPPING.md §0
 *   ✅ 引脚差异复核完成（2026-08-21）：反编译确证硬件印证正确——P1.24=背光、P1.25/26=CS2/CS1、
 *     P1.27=RS、P1.28=E；旧头注（P1.24=CS/P1.25=RES/P1.26=A0/P1.27=R/W）为误。
 *     另确证 DB 数据线非连续：DB0-7=P1.0/1/4/8/9/10/14/15。
 * ========================================================================== */

/* ==================== 底层 GPIO 控制 ==================== */

/* 0x000007B6 —— LCD 背光/控制线：param_1<1 → 关闭（CLR bit24），否则开启（SET bit24）
 * （state_machine 屏幕超时>5000 拍后调 lcd_ctrl_line(0) 关闭；非 RS！RS 实为 bit27）
 * （DAT_00000BB4=0x2009C000 池基址；+0x3C=CLR / +0x38=SET） */
void lcd_ctrl_line(int param_1)
{
  if (param_1 < 1) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x1000000;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x1000000;
  }
  return;
}

/* 0x000007D6 —— 写一个字节到数据线 DB0-7（P1.0-7），按位 SET/CLR */
void lcd_data_byte(uint param_1)
{
  if ((param_1 & 0x80) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x8000;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x8000;
  }
  if ((param_1 & 0x40) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x4000;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x4000;
  }
  if ((param_1 & 0x20) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x400;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x400;
  }
  if ((param_1 & 0x10) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x200;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x200;
  }
  if ((param_1 & 8) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x100;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x100;
  }
  if ((param_1 & 4) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x10;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x10;
  }
  if ((param_1 & 2) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 2;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 2;
  }
  if ((param_1 & 1) == 0) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 1;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 1;
  }
  return;
}

/* 0x000008F4 —— 写数据（RS=1），param_2==0 正写 / else 反写（^0xFF） */
void disp_data(undefined4 param_1,int param_2,undefined4 param_3)
{
  int iVar1;
  uint extraout_r3;

  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x8000000;  /* P1.27 RS=1（数据模式） */
  Delay(1,iVar1,param_3,param_1);
  if (param_2 == 0) {
    lcd_data_byte(extraout_r3);
  }
  else {
    lcd_data_byte(extraout_r3 ^ 0xff);
  }
  Delay(1);
  *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x10000000;  /* P1.28 E=1 */
  Delay(1);
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x10000000;  /* P1.28 E=0 */
  Delay(1);
  return;
}

/* 0x0000094A —— 写命令（RS=0） */
void disp_cmd(undefined4 param_1,undefined4 param_2,undefined4 param_3)
{
  int iVar1;
  undefined4 extraout_r3;

  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x8000000;  /* P1.27 RS=0（命令模式） */
  Delay(1,iVar1,param_3,param_1);
  lcd_data_byte(extraout_r3);
  Delay(1);
  *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x10000000;  /* E=1 */
  Delay(1);
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x10000000;  /* E=0 */
  Delay(1);
  return;
}

/* 0x00000992 —— 清屏：上下两半各 8 页（0xB8..0xBF），每页 64 字节写 0 */
void disp_clear(void)
{
  int iVar1;
  byte bVar2;
  byte bVar3;

  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x4000000;   /* CS1=1（上半屏） */
  *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x2000000;                 /* CS2=0 */
  Delay(10);
  disp_cmd(0xc0);                          /* 起始行=0 */
  for (bVar3 = 0; iVar1 = DAT_00000bb4, bVar3 < 8; bVar3 = bVar3 + 1) {
    disp_cmd(bVar3 + 0xb8);                /* 页 0..7 */
    for (bVar2 = 0; bVar2 < 0x40; bVar2 = bVar2 + 1) {
      disp_data(0);
    }
  }
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x4000000;
  *(uint *)(iVar1 + 0x38) = *(uint *)(iVar1 + 0x38) | 0x2000000;
  Delay(10);
  disp_cmd(0xc0);
  for (bVar3 = 0; bVar3 < 8; bVar3 = bVar3 + 1) {
    disp_cmd(bVar3 + 0xb8);
    for (bVar2 = 0; bVar2 < 0x40; bVar2 = bVar2 + 1) {
      disp_data(0);
    }
  }
  return;
}

/* 0x00000A30 —— P1 口 GPIO 初始化 + LCD 上电初始化序列
 *   P1.0-7(DB)+P1.24-28(控制) 置输出；然后 CS 切换 + disp_cmd(0xC0)+disp_cmd(0x3F) 开显示 */
void gpio1_init(void)
{
  int iVar1;

  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x20) = *(uint *)(DAT_00000bb4 + 0x20) | 0x8000000;   /* FIODIR bit27 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x10000000;                /* bit28 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x4000000;                 /* bit26 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x2000000;                 /* bit25 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 1;                         /* bit0 DB0 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 2;                         /* bit1 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x10;                      /* bit4 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x100;                     /* bit8 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x200;                     /* bit9 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x400;                     /* bit10 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x4000;                    /* bit14 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x8000;                    /* bit15 */
  *(uint *)(iVar1 + 0x20) = *(uint *)(iVar1 + 0x20) | 0x1000000;                 /* bit24 */
  *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x1000000;                 /* RES=0 */
  *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x8000000;                 /* R/W=0 */
  Delay(1);
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x10000000;  /* E=0 */
  Delay(1);
  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x4000000;
  *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x2000000;
  Delay(1);
  disp_cmd(0xc0);
  disp_cmd(0x3f);                          /* DISPLAY ON */
  Delay(1);
  iVar1 = DAT_00000bb4;
  *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x4000000;
  *(uint *)(iVar1 + 0x38) = *(uint *)(iVar1 + 0x38) | 0x2000000;
  Delay(1);
  disp_cmd(0xc0);
  disp_cmd(0x3f);
  Delay(1);
  disp_clear();
  return;
}

/* ==================== 字符渲染 ==================== */

/* 0x00000B44 —— 渲染 8×8 ASCII 字符
 *   查表 0x10000BB8（0x24 个 8×8 字符映射表）→ 字形表 0x10000FC0（每字 0x10 字节）
 *   param_3<8 → 上半屏（CS1），否则下半屏（CS2，param_3-8）
 *   param_2=行，param_3=列×8+0x40 地址 */
void disp_render_char8(uint param_1,char param_2,uint param_3,undefined4 param_4)
{
  int iVar1;
  uint uVar2;
  uint uVar3;

  iVar1 = DAT_00000bb4;
  uVar3 = 0;
  while( true ) {
    if (0x23 < uVar3) {
      return;
    }
    if (*(byte *)(DAT_00000bb8 + uVar3) == param_1) break;
    uVar3 = uVar3 + 1 & 0xff;
  }
  if ((int)param_3 < 8) {
    *(uint *)(DAT_00000bb4 + 0x3c) = *(uint *)(DAT_00000bb4 + 0x3c) | 0x4000000;
    *(uint *)(iVar1 + 0x38) = *(uint *)(iVar1 + 0x38) | 0x2000000;
  }
  else {
    *(uint *)(DAT_00000bb4 + 0x38) = *(uint *)(DAT_00000bb4 + 0x38) | 0x4000000;
    *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x2000000;
    param_3 = param_3 - 8 & 0xff;
  }
  Delay(10);
  disp_cmd(0xc0);
  disp_cmd(param_2 * '\x02' + -0x48);      /* 页 = 行×2 - 0x48 */
  for (uVar2 = 0; uVar2 < 8; uVar2 = uVar2 + 1 & 0xff) {
    disp_cmd(param_3 * 8 + 0x40 + uVar2 & 0xff);   /* 列地址 */
    disp_data(*(undefined1 *)(DAT_00000fc0 + uVar3 * 0x10 + uVar2),param_4);
  }
  disp_cmd(param_2 * '\x02' + -0x47);      /* 下一行 */
  for (uVar2 = 0; uVar2 < 8; uVar2 = uVar2 + 1 & 0xff) {
    disp_cmd(param_3 * 8 + 0x40 + uVar2 & 0xff);
    disp_data(*(undefined1 *)(DAT_00000fc0 + uVar3 * 0x10 + uVar2 + 8),param_4);
  }
  return;
}

/* 0x00000C3A —— 渲染 16×16 汉字（GB 双字节）
 *   查表 0x10000FC4（0x8F 个汉字，每字 2 字节码）→ 字形 0x10000FCC（每字 0x20 字节）
 *   行占 2 页（上半字 + 下半字） */
void disp_render_char16(uint param_1,uint param_2,char param_3,int param_4,undefined4 param_5)
{
  int iVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;

  iVar1 = DAT_00000fc8;
  uVar4 = 0;
  uVar2 = (param_4 - (param_4 >> 0x1f) & 0x1ffU) >> 1;
  while( true ) {
    if (0x8e < uVar4) {
      return;
    }
    if ((*(byte *)(DAT_00000fc4 + uVar4 * 2) == param_1) &&
       (*(byte *)(DAT_00000fc4 + uVar4 * 2 + 1) == param_2)) break;
    uVar4 = uVar4 + 1 & 0xff;
  }
  if (uVar2 < 4) {
    *(uint *)(DAT_00000fc8 + 0x3c) = *(uint *)(DAT_00000fc8 + 0x3c) | 0x4000000;
    *(uint *)(iVar1 + 0x38) = *(uint *)(iVar1 + 0x38) | 0x2000000;
  }
  else {
    *(uint *)(DAT_00000fc8 + 0x38) = *(uint *)(DAT_00000fc8 + 0x38) | 0x4000000;
    *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x2000000;
    uVar2 = uVar2 - 4 & 0xff;
  }
  Delay(10);
  disp_cmd(0xc0);
  disp_cmd(param_3 * '\x02' + -0x48);
  for (uVar3 = 0; uVar3 < 0x10; uVar3 = uVar3 + 1 & 0xff) {
    disp_cmd(uVar2 * 0x10 + 0x40 + uVar3 & 0xff);
    disp_data(*(undefined1 *)(DAT_00000fcc + uVar4 * 0x20 + uVar3),param_5);
  }
  disp_cmd(param_3 * '\x02' + -0x47);
  for (uVar3 = 0; uVar3 < 0x10; uVar3 = uVar3 + 1 & 0xff) {
    disp_cmd(uVar2 * 0x10 + 0x40 + uVar3 & 0xff);
    disp_data(*(undefined1 *)(DAT_00000fcc + uVar4 * 0x20 + uVar3 + 0x10),param_5);
  }
  return;
}

/* 0x00000D3C —— 字符串渲染：逐字符，<0xA1 走 8×8 半角，>=0xA1 走 16×16 全角（双字节） */
void disp_string(int param_1,undefined4 param_2,uint param_3,undefined4 param_4)
{
  uint uVar1;
  undefined4 uVar2;

  uVar2 = param_4;
  for (uVar1 = 0; (uVar1 < 0x10 && (*(char *)(param_1 + uVar1) != '\0')); uVar1 = uVar1 + 1 & 0xff)
  {
    if (*(byte *)(param_1 + uVar1) < 0xa1) {
      disp_render_char8(*(undefined1 *)(param_1 + uVar1),param_2,param_3,param_4,uVar2);
      param_3 = param_3 + 1;
    }
    else {
      uVar2 = param_4;
      disp_render_char16(*(undefined1 *)(param_1 + uVar1),*(undefined1 *)(param_1 + uVar1 + 1),
                         param_2,param_3);
      param_3 = param_3 + 2;
      uVar1 = uVar1 + 1 & 0xff;
    }
    param_3 = param_3 & 0xff;
  }
  return;
}

/* ==================== 数字/数值格式化 ==================== */

/* 0x00000D94 —— 单个数字 0..9 渲染 */
void disp_digit(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)
{
  switch(param_1) {
  case 0:
    disp_render_char8(0x30,param_2,param_3,param_4);
    break;
  case 1:
    disp_render_char8(0x31,param_2,param_3,param_4);
    break;
  case 2:
    disp_render_char8(0x32,param_2,param_3,param_4);
    break;
  case 3:
    disp_render_char8(0x33,param_2,param_3,param_4);
    break;
  case 4:
    disp_render_char8(0x34,param_2,param_3,param_4);
    break;
  case 5:
    disp_render_char8(0x35,param_2,param_3,param_4);
    break;
  case 6:
    disp_render_char8(0x36,param_2,param_3,param_4);
    break;
  case 7:
    disp_render_char8(0x37,param_2,param_3,param_4);
    break;
  case 8:
    disp_render_char8(0x38,param_2,param_3,param_4);
    break;
  case 9:
    disp_render_char8(0x39,param_2,param_3,param_4);
  }
  return;
}

/* 0x00000E42 —— 3 位数字（前置空格补齐） */
void disp_number3(int param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 100 & 0xff,param_2,param_3,param_4);
  }
  if (param_1 < 10) {
    disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 10) % 10 & 0xff,param_2,param_3 + 1U & 0xff,param_4);
  }
  disp_digit(param_1 % 10 & 0xff,param_2,param_3 + 2U & 0xff,param_4);
  return;
}

/* 0x00000ED0 —— 4 位数字（前置空格补齐） */
void disp_uint4(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 1000) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 1000 & 0xff,param_2,param_3,param_4);
  }
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 100) % 10,param_2,param_3 + 1U & 0xff,param_4);
  }
  if (param_1 < 10) {
    disp_render_char8(0x20,param_2,param_3 + 2U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 10) % 10,param_2,param_3 + 2U & 0xff,param_4);
  }
  disp_digit(param_1 % 10,param_2,param_3 + 3U & 0xff,param_4);
  return;
}

/* 0x00000F8C —— 5 位数字 */
void disp_uint5(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 10000) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 10000 & 0xff,param_2,param_3,param_4);
  }
  if (param_1 < 1000) {
    disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 1000) % 10,param_2,param_3 + 1U & 0xff,param_4);
  }
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3 + 2U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 100) % 10,param_2,param_3 + 2U & 0xff,param_4);
  }
  if (param_1 < 10) {
    disp_render_char8(0x20,param_2,param_3 + 3U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 10) % 10,param_2,param_3 + 3U & 0xff,param_4);
  }
  disp_digit(param_1 % 10,param_2,param_3 + 4U & 0xff,param_4);
  return;
}

/* 0x00001092 —— 6 位数字（DAT_0000149C=最大位权除数） */
void disp_number(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (DAT_0000149c - 1 < param_1) {
    disp_digit(param_1 / DAT_0000149c & 0xff,param_2,param_3,param_4);
  }
  else {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  if (param_1 < 10000) {
    disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 10000) % 10,param_2,param_3 + 1U & 0xff,param_4);
  }
  if (param_1 < 1000) {
    disp_render_char8(0x20,param_2,param_3 + 2U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 1000) % 10,param_2,param_3 + 2U & 0xff,param_4);
  }
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3 + 3U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 100) % 10,param_2,param_3 + 3U & 0xff,param_4);
  }
  if (param_1 < 10) {
    disp_render_char8(0x20,param_2,param_3 + 4U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 10) % 10,param_2,param_3 + 4U & 0xff,param_4);
  }
  disp_digit(param_1 % 10,param_2,param_3 + 5U & 0xff,param_4);
  return;
}

/* 0x000011BC —— 有符号角度显示（±XX，主从偏移 100±60°）
 *   param_1<100 → '-' + (100-param_1)；param_1>=100 → '+'（0 时不显示符号）+ (param_1-100) */
void disp_signed_angle(int param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  uint uVar1;

  if (param_1 < 100) {
    disp_render_char8(0x2d,param_2,param_3,param_4);   /* '-' */
    uVar1 = 100U - param_1 & 0xff;
    if (uVar1 < 10) {
      disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
    }
    else {
      disp_digit((uVar1 / 10) % 10,param_2,param_3 + 1U & 0xff,param_4);
    }
    disp_digit(uVar1 % 10,param_2,param_3 + 2U & 0xff,param_4);
  }
  else {
    uVar1 = param_1 - 100U & 0xff;
    if (uVar1 == 0) {
      disp_render_char8(0x20,param_2,param_3,param_4);
    }
    else {
      disp_render_char8(0x2b,param_2,param_3,param_4);   /* '+' */
    }
    if (uVar1 < 10) {
      disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
    }
    else {
      disp_digit((uVar1 / 10) % 10,param_2,param_3 + 1U & 0xff,param_4);
    }
    disp_digit(uVar1 % 10,param_2,param_3 + 2U & 0xff,param_4);
  }
  return;
}

/* 0x000012B0 —— 偏移量显示（中心 0x148=328，±显示；用于恢复出厂屏的偏移值） */
void disp_offset(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 0x148) {
    disp_render_char8(0x2b,param_2,param_3,param_4);   /* '+' */
    param_1 = 0x148 - param_1;
    if (param_1 < 100) {
      disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
    }
    else {
      disp_digit(param_1 / 100 & 0xff,param_2,param_3 + 1U & 0xff,param_4);
    }
    if (param_1 < 10) {
      disp_render_char8(0x20,param_2,param_3 + 2U & 0xff,param_4);
    }
    else {
      disp_digit((param_1 / 10) % 10,param_2,param_3 + 2U & 0xff,param_4);
    }
    disp_digit(param_1 % 10,param_2,param_3 + 3U & 0xff,param_4);
  }
  else {
    param_1 = param_1 - 0x148;
    if (param_1 == 0) {
      disp_render_char8(0x20,param_2,param_3,param_4);
    }
    else {
      disp_render_char8(0x2d,param_2,param_3,param_4);   /* '-' */
    }
    if (param_1 < 100) {
      disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
    }
    else {
      disp_digit(param_1 / 100 & 0xff,param_2,param_3 + 1U & 0xff,param_4);
    }
    if (param_1 < 10) {
      disp_render_char8(0x20,param_2,param_3 + 2U & 0xff,param_4);
    }
    else {
      disp_digit((param_1 / 10) % 10,param_2,param_3 + 2U & 0xff,param_4);
    }
    disp_digit(param_1 % 10,param_2,param_3 + 3U & 0xff,param_4);
  }
  return;
}

/* 0x000013E8 —— 2 位数字 */
void disp_uint2(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 10) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 10 & 0xff,param_2,param_3,param_4);
  }
  disp_digit(param_1 % 10,param_2,param_3 + 1U & 0xff,param_4);
  return;
}

/* 0x0000143C —— 固定 1 位小数（4 位整数 + '.' + 1 位小数），如 XX.X */
void disp_fixed_1dec(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 1000) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 1000 & 0xff,param_2,param_3,param_4);
  }
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3 + 1U & 0xff,param_4);
  }
  else {
    disp_digit((param_1 / 100) % 10,param_2,param_3 + 1U & 0xff,param_4);
  }
  disp_digit((param_1 / 10) % 10,param_2,param_3 + 2U & 0xff,param_4);
  disp_render_char8(0x2e,param_2,param_3 + 3U & 0xff,param_4);   /* '.' */
  disp_digit(param_1 % 10,param_2,param_3 + 4U & 0xff,param_4);
  return;
}

/* 0x000014F6 —— 1 位小数（2 位整数 + '.' + 1 位小数） */
void disp_decimal1(uint param_1,undefined4 param_2,int param_3,undefined4 param_4)
{
  if (param_1 < 100) {
    disp_render_char8(0x20,param_2,param_3,param_4);
  }
  else {
    disp_digit(param_1 / 100 & 0xff,param_2,param_3,param_4);
  }
  disp_digit((param_1 / 10) % 10,param_2,param_3 + 1U & 0xff,param_4);
  disp_render_char8(0x2e,param_2,param_3 + 2U & 0xff,param_4);   /* '.' */
  disp_digit(param_1 % 10,param_2,param_3 + 3U & 0xff,param_4);
  return;
}

/* ==================== 整屏渲染 ==================== */

/* 0x0000427C —— 开机画面（4 行：ST33C / 版本 V2.0.2016 / SINEP0WER / 电话）
 *   右上角显示增益档位值（gain_sel 对应增益，cfg_1710 组）；底部运行状态/子状态文字 */
void disp_splash_screen(void)
{
  disp_string(&DAT_00004370,0,0);
  disp_string(&DAT_00004384,1,0);
  disp_string(&DAT_00004398,2,0);
  disp_string(&LAB_000043ac,3,0);
  if (*DAT_000043c0 == '\0') {
    disp_fixed_1dec(*DAT_000047c0,0,9);
  }
  if (*DAT_000047c4 == '\x01') {
    disp_fixed_1dec(*DAT_000047c8,0,9);
  }
  if (*DAT_000047c4 == '\x02') {
    disp_fixed_1dec(*DAT_000047cc,0,9);
  }
  disp_uint4(*DAT_000047d0,1,9,0);
  disp_uint4(*DAT_000047d4,2,9,0);
  if (*DAT_000047d8 == 0) {
    if (*DAT_000047e4 == '\0') {
      disp_string(&DAT_000047e8,3,10,0);
    }
    if (*DAT_000047e4 == '\x01') {
      disp_string(&DAT_000047f0,3,10,0);
    }
  }
  else {
    disp_string(&DAT_000047dc,3,10,0);
  }
  if (*DAT_000047f8 == '\0') {
    disp_string(&DAT_000047fc,3,0);
  }
  if (*DAT_000047f8 == '\x01') {
    disp_string(&DAT_00004804,3,0);
  }
  if (*DAT_000047f8 == '\x02') {
    disp_string(&DAT_0000480c,3,0);
  }
  return;
}

/* 0x0000448A —— 静态信息屏（4 行静态文字，如版本信息页） */
void disp_screen_static(void)
{
  disp_clear();
  disp_string(&DAT_00004814,0,0,1);
  disp_string(&DAT_00004824,1,0);
  disp_string(&DAT_00004834,2,0);
  disp_string(&DAT_00004844,3,0);
  return;
}

/* 0x000044C2 —— 标定屏：4 行标定值 + 光标行高亮（ADC 标定除数显示；与相位校准相关）
 *   0x100048A4=光标行；0x100048A8/AC/B0/B4=4 个标定值 */
void disp_screen_calib(void)
{
  disp_string(&DAT_00004854,0,0);
  disp_string(&DAT_00004868,1,0);
  disp_string(&DAT_0000487c,2,0);
  disp_string(&DAT_00004890,3,0);
  if (*DAT_000048a4 == '\0') {
    disp_uint4(*DAT_000048a8,0,0xb,1);
  }
  else {
    disp_uint4(*DAT_000048a8,0,0xb);
  }
  if (*DAT_000048a4 == '\x01') {
    disp_uint4(*DAT_000048ac,1,0xb);
  }
  else {
    disp_uint4(*DAT_000048ac,1,0xb,0);
  }
  if (*DAT_000048a4 == '\x02') {
    disp_uint4(*DAT_000048b0,2,0xb,1);
  }
  else {
    disp_uint4(*DAT_000048b0,2,0xb,0);
  }
  if (*DAT_000048a4 == '\x03') {
    disp_uint4(*DAT_000048b4,3,0xb,1);
  }
  else {
    disp_uint4(*DAT_000048b4,3,0xb,0);
  }
  return;
}
