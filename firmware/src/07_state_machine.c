/* =============================================================================
 * 07_state_machine.c — state_machine(0x458C) C 级还原
 * 目标B W1b：替换 firmware/stub.c 占位。依据 tools/_sm_case*.txt（Ghidra
 *   disassemble_function 全量反汇编落盘）逐段还原；数据地址一律以反汇编字面量
 *   SRAM 值为准，字符串参数（disp_string 第1参）直传原 flash 地址（flash XIP 直读，
 *   字符串→运行期地址映射属 W7 遗留）。绝不臆造：每 case 均对照对应反汇编段。
 *
 * 函数：0x0000458C-0xAB44（UI 状态机主分发，MENU 驱动）
 * 调用点：main() 主循环 state_machine(*key)
 * 分发链（顺序 if 级联，遇 return 即返回；历史说明见 docs/history/_SM_W1B_PROGRESS.md）：
 *   entry(0x458C)→case1(0x4B16)→caseA(0x541C)→case62(0x5572)→case63(0x5748)
 *   →case2(0x6134)→case3(0x69D6)→case4(0x7C1A)→case5(0x8780)→case6(0x8C1A)
 *   →case7(0x910C)→case8(0x9A84)→caseB(0x9C5C)→case9(0x9D86)→case5A(0x9E14)
 *   →caseC(0x9FB8)→case14(0xA04E)→case1E(0xA2C8)→0xAB44 返回。
 *   MENU 值→case：1→case1、0xa→caseA、0x62→case62、0x63→case63、2→case2、3→case3、
 *   4→case4、5→case5、6→case6、7→case7、8→case8、0xb→caseB、9→case9、0x5a→case5A、
 *   0xc→caseC、0x14→case14、0x1e→case1E。
 *
 * r4=key 语义：1=确认、2=DOWN/减、3=UP/加、4=SET/退出、5=启动、6=停机、
 *   0x16=快加、0x21=快减、0x17=统计清零、0xe=初始参数密码、数字键0-9输密码。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* ---- 依赖函数前向声明（签名与定义模块核实一致；形参名语义化，无行为影响） ---- */
void lcd_ctrl_line(int on);                               /* 02_lcd_display.c:23 显示控制行开关 */
void disp_clear(void);                                    /* 02_lcd_display.c:124 */
void disp_render_char8(uint ch, char row, uint col, uint32_t attr); /* :203 单字符 */
void disp_string(int str_addr, uint32_t row, uint col, uint32_t attr);  /* :288 字符串 */
void disp_number3(int value, uint32_t row, int col, uint32_t attr);  /* :352 */
void disp_uint4(uint value, uint32_t row, int col, uint32_t attr);   /* :371 */
void disp_uint5(uint value, uint32_t row, int col, uint32_t attr);   /* :396 */
void disp_number(int value, uint32_t row, int col, uint32_t attr);   /* :427 */
void disp_signed_angle(int angle, uint32_t row, int col, uint32_t attr); /* :465 */
void disp_offset(uint offset, uint32_t row, int col, uint32_t attr);  /* :500 */
void disp_uint2(uint value, uint32_t row, int col, uint32_t attr);   /* :545 */
void disp_fixed_1dec(uint value, uint32_t row, int col, uint32_t attr); /* :558 */
void disp_splash_screen(void);                            /* 02_lcd_display.c:597 */
void disp_screen_static(void);                            /* :638 */
void disp_screen_calib(void);                             /* :650 */
void param_sync_live_to_eeprom(void);                     /* 06_param_system.c */
void i2c_write_reg(uint32_t data, uint32_t reg_addr);      /* 04_i2c.c */
void wd_feed(void);                                       /* 01_startup.c:95 */
void fio0_pin22_ctrl(int on);                             /* 09_output_stage.c */
void fio1_pin22_ctrl(int on);                             /* 09_output_stage.c */
void gpio_outputs_set(void);                              /* 09_output_stage.c:0xE79A */
void run_stop_preset(void);                               /* 09_output_stage.c:0xF9AA */
void fio1_pin20_ctrl(int on);                             /* 10_relay_led.c */
void fio1_pin21_ctrl(int on);                             /* 10_relay_led.c */
void fio1_pin23_ctrl(int on);                             /* 10_relay_led.c */
void out_relay_p020(int on);                              /* 10_relay_led.c */
void out_relay_p021(int on);                              /* 10_relay_led.c */
uint8_t debounce_p09(void);                               /* 03_input_debounce.c */
uint8_t debounce_p116(void);                              /* 03_input_debounce.c */
uint8_t debounce_p117(void);                              /* 03_input_debounce.c */
uint8_t debounce_p06(void);                               /* 03_input_debounce.c */
uint8_t scan_run_stop(void);                              /* 03_input_debounce.c:195 */

/* ---- 数据指针（真实 SRAM 地址，volatile 因被 ISR/去抖写入；宽度按反汇编 ldr/strb 区分） ---- */
#define MENU       ((volatile uint8_t*)0x10001744)
#define MENU2      ((volatile uint8_t*)0x10001745)
#define MENU3      ((volatile uint8_t*)0x10001746)
#define SCAN_STOP  ((volatile uint8_t*)0x10001747)
#define EVCODE     ((volatile uint32_t*)0x10001748)
#define TIMEOUT    ((volatile uint32_t*)0x10001764)
#define TIMEOUT2   ((volatile uint32_t*)0x10001760)
#define TIMEOUT3   ((volatile uint32_t*)0x10001778)
#define TIMEOUT4   ((volatile uint32_t*)0x10001770)
#define IDLE       ((volatile uint32_t*)0x10001768)
#define LATCH_OUT  ((volatile uint32_t*)0x1000161c)
#define LATCH_IN   ((volatile uint32_t*)0x10001620)
#define FAULT      ((volatile uint32_t*)0x10001624)
#define RUN        ((volatile uint8_t*)0x10001628)
#define RUN_REQ    ((volatile uint8_t*)0x1000177d)
#define STOP_REQ   ((volatile uint8_t*)0x1000177e)
#define STOP_PEND  ((volatile uint8_t*)0x1000177f)
#define STAT1      ((volatile uint8_t*)0x10001781)
#define RESET2     ((volatile uint8_t*)0x10001782)
#define DB_116     ((volatile uint8_t*)0x10001780)
#define DB_117     ((volatile uint8_t*)0x1000177c)
#define STATUS     ((volatile uint8_t*)0x100015cc)
#define DISP_MODE  ((volatile uint8_t*)0x100015cd)
#define DISP2      ((volatile uint8_t*)0x100015cf)
#define CTRL_MODE  ((volatile uint8_t*)0x10001634)
#define DISP_SEL   ((volatile uint8_t*)0x10001655)
#define RESET_MODE ((volatile uint8_t*)0x10001658)
#define ESTOP      ((volatile uint8_t*)0x10001657)
#define FEEDBACK   ((volatile uint8_t*)0x10001659)
#define INPUT_SEL  ((volatile uint8_t*)0x1000165a)
#define V_RANGE    ((volatile uint32_t*)0x1000163c)
#define A_RANGE    ((volatile uint32_t*)0x10001638)
#define TARGET     ((volatile uint32_t*)0x100015a8)
#define MANUAL     ((volatile uint32_t*)0x100015d8)
#define TARGET_AMP ((volatile uint32_t*)0x10001774)
#define V_AMP      ((volatile uint32_t*)0x100015d0)
#define V_AMP2     ((volatile uint32_t*)0x100015b4)
#define FREQ       ((volatile uint32_t*)0x10001788)
#define TICK       ((volatile uint32_t*)0x10001600)
#define MIN_NOW    ((volatile uint32_t*)0x100015fc)
#define HOUR_NOW   ((volatile uint32_t*)0x100015f8)
#define HOUR_TOTAL ((volatile uint32_t*)0x10001604)
#define MIN_TOTAL  ((volatile uint32_t*)0x10001608)
#define SYNC_30    ((volatile uint32_t*)0x10001730)
#define SYNC_34    ((volatile uint32_t*)0x10001734)
#define SYNC_2C    ((volatile uint32_t*)0x1000172c)
#define PWD_BUF    ((volatile uint8_t*)0x100015f2)
#define PWD_A      ((volatile uint8_t*)0x100015e0)
#define PWD_B      ((volatile uint8_t*)0x100015e6)
#define PWD_C      ((volatile uint8_t*)0x100015ec)
#define COM_ADDR   ((volatile uint8_t*)0x100016ff)
#define BAUD_IDX   ((volatile uint32_t*)0x10001700)
#define PARITY     ((volatile uint8_t*)0x10001704)
#define COM_CHK    ((volatile uint8_t*)0x10001705)
#define BAUD_TBL   ((volatile uint32_t*)0x100017bc)
#define PID_MODE   ((volatile uint32_t*)0x10001710)
#define PHASE_OFF  ((volatile uint32_t*)0x1000162c)
#define BAL_ANG    ((volatile uint8_t*)0x10001694)

/* =============================================================================
 * case3 当前项值渲染 (0x7458-0x7A32 精简)：按项号 it 显示其值/枚举到 (row,0xb)。
 * 所有值串地址与枚举宽度均经 LPC1765.bin 校验，禁止臆造。
 * ========================================================================== */
static void sm3_draw_item(uint32_t it, uint32_t row, uint32_t attr)
{
  volatile uint8_t  *b4c = (volatile uint8_t*)0x1000164c;
  volatile uint8_t  *b4d = (volatile uint8_t*)0x1000164d;
  volatile uint8_t  *b54 = (volatile uint8_t*)0x10001654;
  volatile uint8_t  *b56 = (volatile uint8_t*)0x10001656;
  volatile uint32_t *w40 = (volatile uint32_t*)0x10001640;
  volatile uint32_t *w44 = (volatile uint32_t*)0x10001644;
  volatile uint32_t *w48 = (volatile uint32_t*)0x10001648;
  volatile uint32_t *w50 = (volatile uint32_t*)0x10001650;
  volatile uint32_t *w60 = (volatile uint32_t*)0x10001660;
  switch (it) {
    case 0:
      if (*CTRL_MODE == 0) { disp_string((int)0x6594, row, 0xb, attr); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
      else if (*CTRL_MODE == 1) { disp_string((int)0x659c, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
      else { disp_string((int)0x65a4, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
      break;
    case 1: disp_uint4(*V_RANGE, row, 0xb, attr); break;
    case 2: disp_uint4(*A_RANGE, row, 0xb, attr); break;
    case 3: disp_uint4(*w40, row, 0xb, attr); break;
    case 4: disp_uint4(*w48, row, 0xb, attr); break;
    case 5: disp_uint4(*w44, row, 0xb, attr); break;
    case 6: disp_number3(*b4c, row, 0xb, attr); break;
    case 7: disp_number3(*b4d, row, 0xb, attr); break;
    case 8: disp_number3(*w50, row, 0xb, attr); break;
    case 9: disp_signed_angle(*b54, row, 0xb, attr); break;
    case 10:
      if (*DISP_SEL == 0) disp_string((int)0x7998, row, 0xb, attr);
      else if (*DISP_SEL == 1) disp_string((int)0x79a0, row, 0xb, attr);
      else disp_string((int)0x79a8, row, 0xb, attr);
      break;
    case 11:
      if (*b56 == 0) disp_string((int)0x79b4, row, 0xb, attr);
      else disp_string((int)0x79bc, row, 0xb, attr);
      break;
    case 12:
      if (*ESTOP == 0) disp_string((int)0x6018, row, 0xb, attr);
      else if (*ESTOP == 1) disp_string((int)0x6020, row, 0xb, attr);
      else disp_string((int)0x6028, row, 0xb, attr);
      break;
    case 13:
      if (*FEEDBACK == 0) disp_string((int)0x6038, row, 0xb, attr);
      else disp_string((int)0x6040, row, 0xb, attr);
      break;
    case 14:
      if (*INPUT_SEL == 0) disp_string((int)0x6048, row, 0xb, attr);
      else disp_string((int)0x6050, row, 0xb, attr);
      break;
    case 15: disp_number3(*w60, row, 0xb, attr); break;
  }
}

/* =============================================================================
 * case3 整页值渲染 (0x7458-0x7C1A)：TIMEOUT3 计数到 0xFB 后重绘当前页全部 4 项值，
 * 当前项高亮(attr=1)、其余正常(attr=0)。原厂导航/编辑后仅重画标签会把值列清掉，
 * 靠此公共尾部整页重绘恢复——正是"未选中行值被清除"的修复点。
 * ========================================================================== */
static void sm3_draw_page(uint32_t it)
{
  uint32_t page = it >> 2;
  uint32_t k;
  for (k = 0; k < 4; k++)
    sm3_draw_item((page << 2) + k, k, ((page << 2) + k) == it ? 1 : 0);
}

/* =============================================================================
 * case4 单项目值渲染 (0x8252-0x85BE)：按项号 it 显示保护参数值+单位到 (row,0xb)。
 * attr=1 高亮当前项；0 普通。值串/单位地址全部 bin 校验（§13），禁止臆造。
 * ========================================================================== */
static void sm4_draw_value(uint32_t it, uint32_t row, uint32_t attr)
{
  volatile uint32_t *w_c0 = (volatile uint32_t*)0x100016c0; /* 过压保护 */
  volatile uint8_t  *b_c4 = (volatile uint8_t*)0x100016c4;  /* 过压时间 */
  volatile uint32_t *w_c8 = (volatile uint32_t*)0x100016c8; /* 欠压保护 */
  volatile uint8_t  *b_cc = (volatile uint8_t*)0x100016cc;  /* 欠压时间 */
  volatile uint32_t *w_d0 = (volatile uint32_t*)0x100016d0; /* IF过载保护 */
  volatile uint8_t  *b_d4 = (volatile uint8_t*)0x100016d4;  /* IF过载时间 */
  volatile uint32_t *w_d8 = (volatile uint32_t*)0x100016d8; /* CT过载保护 */
  volatile uint8_t  *b_dc = (volatile uint8_t*)0x100016dc;  /* CT过载时间 */
  volatile uint8_t  *b_dd = (volatile uint8_t*)0x100016dd;  /* 缺相 */
  volatile uint8_t  *b_de = (volatile uint8_t*)0x100016de;  /* 三相平衡 */
  switch (it) {
    case 0:
      if (*w_c0) { disp_uint4(*w_c0,row,0xb,attr); disp_string(0x7974,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 1: disp_uint4(*b_c4,row,0xb,attr); break;
    case 2:
      if (*w_c8) { disp_uint4(*w_c8,row,0xb,attr); disp_string(0x7974,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 3: disp_uint4(*b_cc,row,0xb,attr); break;
    case 4:
      if (*w_d0) { disp_uint4(*w_d0,row,0xb,attr); disp_string(0x7980,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 5: disp_uint4(*b_d4,row,0xb,attr); break;
    case 6:
      if (*w_d8) { disp_uint4(*w_d8,row,0xb,attr); disp_string(0x7980,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 7: disp_uint4(*b_dc,row,0xb,attr); break;
    case 8:
      if (*b_dd) disp_string(0x6a94,row,0xb,attr);
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 9:
      if (*b_de >= 0xa) { disp_uint4(*b_de,row,0xb,attr); disp_string(0x86e0,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
  }
}

/* 绘制当前项所在整页（10 项 = 页0:0-3, 页1:4-7, 页2:8-9），高亮当前项 */
static void sm4_draw_page(uint32_t it)
{
  uint32_t page = it >> 2;
  uint32_t start = page << 2;
  uint32_t n = (page < 2) ? 4 : 2;
  uint32_t k;
  for (k = 0; k < n; k++) {
    uint32_t item = start + k;
    sm4_draw_value(item, k, (item == it) ? 1 : 0);
  }
  /* 页0/1 第 4 行之后若仍有空行则留空对齐（页2 padding 已在 n=2 覆盖） */
  if (page == 2) { /* rows 2,3 由标题符串 0x5ba4 已画空格，无需再清 */ }
}

/* case5 通讯：单行值渲染（it=0..3 本机地址/波特率/校验位/通讯校验，row=it，attr=0/1 高亮） */
static void sm5_draw_value(uint32_t it, uint32_t attr)
{
  switch (it) {
    case 0: disp_uint5(*COM_ADDR, 0, 0xb, attr); break;
    case 1: disp_number((int)BAUD_TBL[*BAUD_IDX], 1, 0xa, attr); break;
    case 2:
      if (*PARITY == 0) disp_string(0x6a78, 2, 0xa, attr);
      else if (*PARITY == 1) disp_string(0x6a80, 2, 0xa, attr);
      else if (*PARITY == 2) disp_string(0x6a88, 2, 0xa, attr);
      else disp_string(0x8b2c, 2, 0xa, attr);   /* '1 ST0P' 校验名 */
      break;
    case 3:
      if (*COM_CHK) disp_string(0x6a94, 3, 0xb, attr);
      else disp_string(0x6038, 3, 0xb, attr);
      break;
  }
}

/* case5 通讯：整页重绘（4 项单页，高亮当前项） */
static void sm5_draw_page(uint32_t it)
{
  uint32_t k;
  for (k = 0; k < 4; k++) sm5_draw_value(k, (k == it) ? 1 : 0);
}

/* case6 密码错/对的延时循环（0x8C5A-0x8C8E 段）：
 * 外层 LATCH_OUT 计数到 0x2710=10000，内层 LATCH_IN 计数到 0x3E8=1000，
 * 每个外层进位喂一次狗（wd_feed@0x238）。 */
static void sm6_delay_loop(void)
{
  *LATCH_OUT = 0;
  for (;;) {
    *LATCH_IN = 0;
    do { (*LATCH_IN)++; } while (*LATCH_IN < 0x3e8);
    wd_feed();
    (*LATCH_OUT)++;
    if (*LATCH_OUT >= 0x2710) break;
  }
}

/* =============================================================================
 * state_machine(0x458C)
 * 流程：entry 公共逻辑（TIMEOUT4/去抖/故障码/启停/统计）→ MENU 分发到各 case。
 * 各 case 以顺序 if 级联实现；case 内部按 key 分发。
 * ========================================================================== */
void state_machine(int key)
{
  uint32_t delay_cnt, uB, uC;
  uint32_t i;
  (void)key;
  (void)delay_cnt;
  (void)uB;
  (void)uC;
  (void)i;

  /* ================= entry 公共逻辑 (0x458C-0x4B16) ================= */
  (*TIMEOUT4)++;
  if (key > 0) { *TIMEOUT4 = 0; lcd_ctrl_line(1); }
  if (*TIMEOUT4 > 0x1388) { *TIMEOUT4 = 0; lcd_ctrl_line(0); }

  if (debounce_p09() == 1) {                    /* P0.9 高电平：累计/影子值→EEPROM */
    if (*HOUR_TOTAL != *(volatile uint32_t*)0x1000160c) {
      *(volatile uint32_t*)0x1000160c = *HOUR_TOTAL;
      i2c_write_reg((*HOUR_TOTAL >> 8) & 0xff, 0x97);
      i2c_write_reg(*HOUR_TOTAL & 0xff, 0x98);
    }
    if (*MIN_TOTAL != *(volatile uint32_t*)0x10001610) {
      *(volatile uint32_t*)0x10001610 = *MIN_TOTAL;
      i2c_write_reg((*MIN_TOTAL >> 8) & 0xff, 0x99);
      i2c_write_reg(*MIN_TOTAL & 0xff, 0x9a);
    }
    if (*(volatile uint32_t*)0x10001614 != *(volatile uint32_t*)0x10001618) {
      *(volatile uint32_t*)0x10001618 = *(volatile uint32_t*)0x10001614;
      i2c_write_reg((*(volatile uint32_t*)0x10001618 >> 8) & 0xff, 0x9b);
      i2c_write_reg(*(volatile uint32_t*)0x10001618 & 0xff, 0x9c);
    }
    if (*MANUAL != *(volatile uint32_t*)0x100015dc) {
      *(volatile uint32_t*)0x100015dc = *MANUAL;
      i2c_write_reg((*MANUAL >> 8) & 0xff, 0x1d);
      i2c_write_reg(*MANUAL & 0xff, 0x1e);
    }
  }

  if (*FAULT != 0) {                            /* 故障：置事件码 + 关输出 + 停机 */
    *EVCODE = 0;
    if (*FAULT & 0x4) *EVCODE = 1;
    if (*FAULT & 0x2) *EVCODE = 1;
    if (*FAULT & 0x1) *EVCODE = 1;
    if (*FAULT & 0x8) *EVCODE = 2;
    if (*FAULT & 0x200) *EVCODE = 3;
    if (*FAULT & 0x40) *EVCODE = 4;
    if (*FAULT & 0x400) *EVCODE = 5;
    if (*FAULT & 0x10) *EVCODE = 6;
    if (*FAULT & 0x20) *EVCODE = 7;
    if (*FAULT & 0x100) *EVCODE = 8;
    if (*FAULT & 0x80) *EVCODE = 9;
    if (*FAULT & 0x4000) *EVCODE = 0xa;
    if (*FAULT & 0x8000) *EVCODE = 0xb;
    if (*FAULT & 0x800) *EVCODE = 0xc;
    if (*FAULT & 0x2000) *EVCODE = 0xd;
    if (*FAULT & 0x1000) *EVCODE = 0xe;
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
    out_relay_p021(1);
    fio1_pin23_ctrl(1);
    *(volatile uint8_t*)0x10001785 = 0;
    *STOP_PEND = 1;
    *STOP_REQ = 0;
    *RUN = 0;
    *(volatile uint32_t*)0x10001ffc = 0;
    *(volatile uint32_t*)0x10002068 = 0;
    *(volatile uint32_t*)0x1000206c = 0;
    *(volatile uint32_t*)0x10002070 = 0;
    *(volatile uint32_t*)0x10002000 = 0;
    gpio_outputs_set();                          /* 0xE79A */
  } else {
    out_relay_p021(0);
    fio1_pin23_ctrl(0);
    *EVCODE = 0;
  }

  if (*RUN != 0) {                              /* 运行统计 */
    fio0_pin22_ctrl(1);
    fio1_pin22_ctrl(1);
    (*TICK)++;
    if (*TICK > 0x7530) {
      *TICK = 0;
      (*MIN_NOW)++;
      (*MIN_TOTAL)++;
      if (*MIN_NOW >= 0x3c) { *MIN_NOW = 0; (*HOUR_NOW)++; }
      if (*MIN_TOTAL >= 0x3c) {
        *MIN_TOTAL = 0;
        (*HOUR_TOTAL)++;
        (*(volatile uint32_t*)0x10001614)++;
      }
      if (*(volatile uint32_t*)0x10001614 >= 0x140) *(volatile uint32_t*)0x10001614 = 0;
    }
    if (*(volatile uint32_t*)0x10001614 == 0x78 && *SYNC_30 == 0) {
      *SYNC_30 = 1; *SYNC_34 = 0; param_sync_live_to_eeprom();
    }
    if (*(volatile uint32_t*)0x10001614 == 0x12c && *SYNC_30 == 2) {
      *SYNC_30 = 0; *SYNC_34 = 1; param_sync_live_to_eeprom();
    }
  }

  if (*SYNC_2C != 1) { out_relay_p020(1); out_relay_p021(1); fio0_pin22_ctrl(1); }

  *DB_116 = debounce_p116();
  if (*FAULT == 0 && *DB_116 == 2) *FAULT |= 0x4000;

  (*((volatile uint32_t*)0x10001750))++;
  if (*(volatile uint32_t*)0x10001750 <= 0x64) { goto do_dispatch; }
  *(volatile uint32_t*)0x10001750 = 0;
  if (*(volatile uint8_t*)0x100020c0 == 0 && *(volatile uint8_t*)0x100016dd > 0) {
    (*(volatile uint32_t*)0x10001754)++;
    if (*(volatile uint32_t*)0x10001754 == 5) { *(volatile uint32_t*)0x10001754 = 0; *FAULT |= 0x1; }
  } else { *(volatile uint32_t*)0x10001754 = 0; *FAULT &= ~0x1; }
  if (*(volatile uint8_t*)0x100020c1 == 0 && *(volatile uint8_t*)0x100016dd > 0) {
    (*(volatile uint32_t*)0x10001758)++;
    if (*(volatile uint32_t*)0x10001758 == 5) { *(volatile uint32_t*)0x10001758 = 0; *FAULT |= 0x2; }
  } else { *(volatile uint32_t*)0x10001758 = 0; *FAULT &= ~0x2; }
  if (*(volatile uint8_t*)0x100020c2 == 0 && *(volatile uint8_t*)0x100016dd > 0) {
    (*(volatile uint32_t*)0x1000175c)++;
    if (*(volatile uint32_t*)0x1000175c == 5) { *(volatile uint32_t*)0x1000175c = 0; *FAULT |= 0x4; }
  } else { *(volatile uint32_t*)0x1000175c = 0; *FAULT &= ~0x4; }
  *(volatile uint8_t*)0x100020c0 = 0;
  *(volatile uint8_t*)0x100020c1 = 0;
  *(volatile uint8_t*)0x100020c2 = 0;
  /* fallthrough → dispatch */

do_dispatch:
  /* ================= 分发链（MENU 值→case 段） ================= */
  if (*MENU == 1) {
    /* ---------- case1 运行状态屏 (0x4B16-0x541C) ---------- */
    if (key == 0x17 && *RUN == 0) {
      *MENU = 0xc; *MENU2 = 0; *TIMEOUT = 0;
      disp_clear();
      disp_string((int)0x4d58, 0, 0, 0);
      disp_string((int)0x4d6c, 1, 0, 0);
      disp_string((int)0x4d80, 2, 0, 0);
      disp_string((int)0x4d6c, 3, 0, 0);
      disp_uint5(*(volatile uint32_t*)0x100015f8, 1, 3, 0);
      disp_uint2(*(volatile uint32_t*)0x100015fc, 1, 0xa, 0);
      disp_uint5(*(volatile uint32_t*)0x10001604, 3, 3, 0);
      disp_uint2(*(volatile uint32_t*)0x10001608, 3, 0xa, 0);
      return;
    }
    if (key == 1 && *RUN == 0) {
      *MENU = 0xa;
      *MENU2 = 0; *TIMEOUT = 0;
      disp_clear();
      disp_string((int)0x4d9c, 1, 0, 0);
      disp_string((int)0x4dac, 3, 7, 0);
      *TIMEOUT2 = 0x3c; *IDLE = 0; return;
    }
    if (key == 0xe) {
      *MENU = 0x62;
      *MENU2 = 0; *TIMEOUT = 0;
      disp_clear();
      disp_string((int)0x4db4, 0, 0, 0);
      disp_string((int)0x4dc8, 1, 0, 0);
      disp_string((int)0x4dac, 3, 7, 0); return;
    }
    if (key == 4 && *RUN == 0 && *FAULT != 0) {
      *MENU = 0x14; *TIMEOUT3 = 0x1f4;
      *MENU2 = 0; *TIMEOUT = 0;
      disp_clear(); return;
    }
    (*IDLE)++;
    if (*IDLE >= 0x15e) {
      *IDLE = 0;
      if (*DISP_SEL == 0) disp_fixed_1dec(*FREQ, 0, 9, 0);
      else if (*DISP_SEL == 1) disp_fixed_1dec(*TARGET, 0, 9, 0);
      else disp_fixed_1dec(*MANUAL, 0, 9, 0);
      disp_uint4(*(volatile uint32_t*)0x10001590, 1, 9, 0);
      disp_uint4(*(volatile uint32_t*)0x10001594, 2, 9, 0);
      if (*FAULT != 0) { *STATUS = 0; disp_string((int)0x47dc, 3, 0xa, 0); }
      else if (*RUN == 0 && *STATUS != 1) { *STATUS = 1; disp_string((int)0x47e8, 3, 0xa, 0); }
      if (*CTRL_MODE == 0 && *DISP_MODE != 1) {
        *DISP_MODE = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);  /* 0x4E44：r0=1 → P1.20 置位高；反编译曾误作 0，已还原 */
        disp_string((int)0x47fc, 3, 0, 0);
      } else if (*CTRL_MODE == 1 && *DISP_MODE != 2) {
        *DISP_MODE = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
        disp_string((int)0x4804, 3, 0, 0);
      } else if (*CTRL_MODE == 2 && *DISP_MODE != 3) {
        *DISP_MODE = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0);
        disp_string((int)0x480c, 3, 0, 0);
      }
    }

    /* 0x4CF2 直接跳到 0x4EBC：显示刷新未到 350 tick 时仍须执行输入扫描。
     * 这段不能放在 IDLE>=0x15e 内，否则复位/急停/RUN-STOP 只会每 350 次扫描一次。 */
    *DB_117 = debounce_p117();
      if (*FAULT != 0) {
        if (*DB_117 == 2 && *(volatile uint8_t*)0x10001658 == 0) {  /* 复位流程 */
          *FAULT = 0; *RUN = 0; *STOP_PEND = 1; *STOP_REQ = 0;
          disp_string((int)0x522c, 3, 0xa, 0);   /* BIN 0x4EF6：复位(行3,列0xa) */
          /* 双层延时：LATCH_IN 内层 0→0x7d0（do-while），LATCH_OUT 外层到 0xbb8（0x4EFA-0x4F36） */
          *LATCH_OUT = 0;
          for (;;) { *LATCH_IN = 0; do { (*LATCH_IN)++; } while (*LATCH_IN < 0x7d0); wd_feed(); (*LATCH_OUT)++; if (*LATCH_OUT >= 0xbb8) break; }
          disp_string((int)0x523c, 3, 0xa, 0);   /* BIN 0x4F40：重启(行3,列0xa) */
          *LATCH_OUT = 0;
          for (;;) { *LATCH_IN = 0; do { (*LATCH_IN)++; } while (*LATCH_IN < 0x7d0); wd_feed(); (*LATCH_OUT)++; if (*LATCH_OUT >= 0xbb8) break; }
          while (1) {}
        }
      }
        if (*(volatile uint8_t*)0x10001658 == 1) {
          if (*DB_117 != 2 && *DISP_MODE != 1) {
            *DISP2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
            disp_string((int)0x47fc, 3, 0, 0);
          }
          if (*DB_117 == 2 && *DISP_MODE != 2) {
            *DISP2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
            disp_string((int)0x4804, 3, 0, 0);
          }
        }
        if (*(volatile uint8_t*)0x10001658 == 2) {
          if (*DB_117 != 2) *RESET2 = 0; else *RESET2 = 1;
        }
        *DB_117 = debounce_p06();
        if (*FAULT == 0 && *DB_117 == 2 && *(volatile uint8_t*)0x10001657 == 0) {
          *RUN_REQ = 1; *RUN = 0; *STOP_PEND = 1; *STOP_REQ = 0;
          if (*STAT1 == 0) { disp_string((int)0x47e8, 3, 0xa, 0); *STAT1 = 1; }
          return;
        }
        /* ESTOP(0x10001657)==1/2：恒压切换/复位设置（逻辑同 RESET_MODE==1/2） */
        if (*(volatile uint8_t*)0x10001657 == 1) {
          if (*DB_117 != 2 && *DISP_MODE != 1) {
            *DISP2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
            disp_string((int)0x47fc, 3, 0, 0);
          }
          if (*DB_117 == 2 && *DISP_MODE != 2) {
            *DISP2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
            disp_string((int)0x4804, 3, 0, 0);
          }
        }
        if (*(volatile uint8_t*)0x10001657 == 2) {
          if (*DB_117 != 2) *RESET2 = 0; else *RESET2 = 1;
        }
        if (*(volatile uint8_t*)0x10001658 != 2 && *(volatile uint8_t*)0x10001657 != 2) *RESET2 = 0;
        *SCAN_STOP = scan_run_stop();
        if (*FAULT == 0 && *STOP_REQ == 0 && *(volatile uint8_t*)0x10001785 == 1 && *DISP_SEL == 0) {
          *(volatile uint8_t*)0x10001785 = 1; *STOP_REQ = 1; *STOP_PEND = 0; *RUN_REQ = 0;
          *RUN = 1; *STAT1 = 0; *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
          disp_string((int)0x47f0, 3, 0xa, 0);
        }
        if (*FAULT == 0 && *STOP_PEND == 0 && *(volatile uint8_t*)0x10001785 == 0 && *DISP_SEL == 0) {
          *STOP_PEND = 1; *STOP_REQ = 0; *RUN = 0;
          disp_string((int)0x47e8, 3, 0xa, 0);
        }
        if (*RUN == 0 && *DISP_SEL != 0) *(volatile uint8_t*)0x10001785 = 0;
        if (*FAULT == 0 && *STOP_REQ == 0 && (key == 5 || *SCAN_STOP == 7)) {
          if (*(volatile uint8_t*)0x10001656 == 0) {
            *(volatile uint8_t*)0x10001785 = 1; *STOP_REQ = 1; *STOP_PEND = 0; *RUN_REQ = 0;
            *RUN = 1; *STAT1 = 0; *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
            disp_string((int)0x47f0, 3, 0xa, 0);
          } else if (*SCAN_STOP == 7 && *(volatile uint8_t*)0x10001656 == 1 && *DISP_SEL != 0) {
            *(volatile uint8_t*)0x10001785 = 1; *STOP_REQ = 1; *STOP_PEND = 0; *RUN_REQ = 0;
            *RUN = 1; *STAT1 = 0; *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
            disp_string((int)0x47f0, 3, 0xa, 0);
          }
        }
        if (*FAULT == 0 && *STOP_PEND == 0 && (key == 6 || *SCAN_STOP == 8)) {
          if (*(volatile uint8_t*)0x10001656 == 0) {
            *(volatile uint8_t*)0x10001785 = 0; *STOP_PEND = 1; *STOP_REQ = 0; *RUN = 0;
            disp_string((int)0x47e8, 3, 0xa, 0);
          } else if (*SCAN_STOP == 8 && *(volatile uint8_t*)0x10001656 == 1 && *DISP_SEL != 0) {
            *(volatile uint8_t*)0x10001785 = 0; *STOP_PEND = 1; *STOP_REQ = 0; *RUN = 0;
            disp_string((int)0x47e8, 3, 0xa, 0);
          }
        }
      /* 0x52FA：DISP_SEL 三分支不受 FAULT 门控——原厂 0x52A2 在 FAULT!=0 时
       * cbnz 跳到 0x52FA 仍执行三分支，仅 RUN/STOP 逻辑(0x52A8-0x52F6)被 FAULT 跳过。
       * 若放回 FAULT==0 分支内，FAULT=7(缺相)时首页第一行上下键将永远不执行。 */
      if (*DISP_SEL == 0) {
        *TARGET = *FREQ;
        if (*CTRL_MODE == 0) *TARGET_AMP = (*FREQ * *V_RANGE) / 1000;
        else *TARGET_AMP = (*FREQ * *A_RANGE) / 1000;
        *V_AMP = *TARGET_AMP; *V_AMP2 = *TARGET_AMP;
      } else if (*DISP_SEL == 1) {
        *V_AMP = *V_AMP2;
      } else {
        if (key == 2 || key == 0x16) {
          (*MANUAL)++; if (*MANUAL > 0x3e8) *MANUAL = 0x3e8;
          if (*MANUAL < 0xa) *MANUAL = 0xa;
          disp_fixed_1dec(*MANUAL, 0, 9, 0);
        }
        if (key == 3 || key == 0x21) {
          if (*MANUAL > 0xa) (*MANUAL)--;
          else { *MANUAL = 1; (*MANUAL)--; }
          disp_fixed_1dec(*MANUAL, 0, 9, 0);
        }
        *TARGET = *MANUAL;
        if (*CTRL_MODE == 0) *(volatile uint32_t*)0x100015d4 = (*MANUAL * *V_RANGE) / 1000;
        else *(volatile uint32_t*)0x100015d4 = (*MANUAL * *A_RANGE) / 1000;
        *V_AMP = *(volatile uint32_t*)0x100015d4;   /* 0x541A→0x541E：仅 DISP_SEL==2 覆盖；0/1 走 0x4C1C→0x541C 跳过 */
        *V_AMP2 = *(volatile uint32_t*)0x100015d4;
      }
    return;
  }

  if (*MENU == 0xa) {
    /* ---------- caseA 参数密码屏 (0x541C-0x5572) ---------- */
    if (key == 1) {
      *MENU2 = 0;
      while (*MENU2 < 6) {
        if (PWD_BUF[*MENU2] != PWD_A[*MENU2]) {
          disp_clear();
          disp_string((int)0x56dc, 1, 4, 0);
          /* 密码错延时：delay_cnt 计 0x3e8 次，循环体喂狗(wd_feed)并累加 LATCH_OUT；外层至 0x2710 */
          *LATCH_IN = 0;
          for (delay_cnt = 0; delay_cnt < 0x3e8; delay_cnt++) { wd_feed(); (*LATCH_OUT)++; }
          *MENU = 1; disp_splash_screen(); return;
        }
        PWD_BUF[*MENU2] = 0; (*MENU2)++;
      }
      *MENU = 2; *MENU2 = 0; disp_screen_static(); return;
    }
    if (key == 4) { *MENU = 1; disp_splash_screen(); return; }
    if (key > 0) {
      if (*MENU2 < 6) { PWD_BUF[*MENU2] = key; disp_render_char8('*', 1, *MENU2 + 7, 0); (*MENU2)++; }
      return;
    }
    (*IDLE)++;
    if (*IDLE >= 0x1f4) {
      *IDLE = 0; (*TIMEOUT2)--;
      disp_number3(*TIMEOUT2, 3, 6, 0);
      if (*TIMEOUT2 == 0) { *MENU = 1; disp_splash_screen(); return; }
    }
    return;
  }

  if (*MENU == 0x62) {
    /* ---------- case62 初始密码屏 (0x5572-0x5748) ---------- */
    if (key == 1) {
      *MENU2 = 0;
      while (*MENU2 < 6) {
        if (PWD_BUF[*MENU2] != PWD_C[*MENU2]) {
          disp_clear();
          disp_string((int)0x56dc, 1, 4, 0);
          /* 密码错延时（同 caseA）：delay_cnt 计 0x3e8 次，喂狗 + LATCH_OUT++ */
          *LATCH_IN = 0;
          for (delay_cnt = 0; delay_cnt < 0x3e8; delay_cnt++) { wd_feed(); (*LATCH_OUT)++; }
          *MENU = 1; disp_splash_screen(); return;
        }
        PWD_BUF[*MENU2] = 0; (*MENU2)++;
      }
      *MENU = 0x63; *MENU2 = 0; disp_clear(); disp_screen_calib(); return;
    }
    if (key == 4) { *MENU = 1; disp_splash_screen(); return; }
    if (key > 0) {
      if (*MENU2 < 6) { PWD_BUF[*MENU2] = key; disp_render_char8('*', 1, *MENU2 + 7, 0); (*MENU2)++; }
      return;
    }
    (*IDLE)++;
    if (*IDLE >= 0x1f4) {
      *IDLE = 0; (*TIMEOUT2)--;
      disp_number3(*TIMEOUT2, 3, 6, 0);
      if (*TIMEOUT2 == 0) { *MENU = 1; disp_splash_screen(); return; }
    }
    return;
  }

  if (*MENU == 0x63) {
    /* ---------- case63 初始参数 (0x5748-0x6134)，MENU2=项0-10，MENU3=0导航/1编辑 ---------- */
    if (key == 1) {
      *TIMEOUT = 0; (*MENU3)++; if (*MENU3 > 1) *MENU3 = 0;
      if (*MENU3 == 0) *TIMEOUT3 = 0xfa; else *TIMEOUT3 = 0x1f4;
    }
    if (key == 4) {
      *TIMEOUT = 0; param_sync_live_to_eeprom(); *MENU = 1; disp_splash_screen(); return;
    }
    if (key == 2 || key == 3) {
      if (*MENU3 == 0) {
        *TIMEOUT = 0;
        if (key == 3) { (*MENU2)++; if (*MENU2 > 0xa) *MENU2 = 0xa; }
        if (key == 2) { if (*MENU2 > 0) (*MENU2)--; }
        if (*MENU2 < 4) {
          disp_string((int)0x4854, 0, 0, 0); disp_string((int)0x4868, 1, 0, 0);
          disp_string((int)0x487c, 2, 0, 0); disp_string((int)0x4890, 3, 0, 0);
        }
        if (*MENU2 >= 4 && *MENU2 < 8) {
          disp_string((int)0x5b18, 0, 0, 0); disp_string((int)0x5b2c, 1, 0, 0);
          disp_string((int)0x5b40, 2, 0, 0); disp_string((int)0x5b54, 3, 0, 0);
        }
        if (*MENU2 >= 8 && *MENU2 < 0xc) {
          disp_string((int)0x5b68, 0, 0, 0); disp_string((int)0x5b7c, 1, 0, 0);
          disp_string((int)0x5b90, 2, 0, 0); disp_string((int)0x5ba4, 3, 0, 0);
        }
        *TIMEOUT3 = 0xfa;
      } else {
        if (key == 2 || key == 0x16) {
          *TIMEOUT = 0;
          if (*MENU2 == 0) { (*((volatile uint32_t*)0x10001698))++; if (*((volatile uint32_t*)0x10001698) > 0x1194) *((volatile uint32_t*)0x10001698) = 0x1194; }
          if (*MENU2 == 1) { (*((volatile uint32_t*)0x100016a0))++; if (*((volatile uint32_t*)0x100016a0) > 0x1194) *((volatile uint32_t*)0x100016a0) = 0x1194; }
          if (*MENU2 == 2) { (*((volatile uint32_t*)0x100016a8))++; if (*((volatile uint32_t*)0x100016a8) > 0x1194) *((volatile uint32_t*)0x100016a8) = 0x1194; }
          if (*MENU2 == 3) { (*((volatile uint32_t*)0x100016b0))++; if (*((volatile uint32_t*)0x100016b0) > 0x1194) *((volatile uint32_t*)0x100016b0) = 0x1194; }
          if (*MENU2 == 4) { (*((volatile uint32_t*)0x100016b8))++; if (*((volatile uint32_t*)0x100016b8) > 0x1194) *((volatile uint32_t*)0x100016b8) = 0x1194; }
          if (*MENU2 == 5) { (*ESTOP)++; if (*ESTOP > 2) *ESTOP = 2; }
          if (*MENU2 == 6) { (*RESET_MODE)++; if (*RESET_MODE > 2) *RESET_MODE = 2; }
          if (*MENU2 == 7) { (*FEEDBACK)++; if (*FEEDBACK > 1) *FEEDBACK = 1; }
          if (*MENU2 == 8) { (*INPUT_SEL)++; if (*INPUT_SEL > 1) *INPUT_SEL = 1; }
          if (*MENU2 == 9) { (*((volatile uint8_t*)0x1000165b))++; if (*(volatile uint8_t*)0x1000165b > 1) *(volatile uint8_t*)0x1000165b = 1; }
          if (*MENU2 == 0xa) { (*(volatile uint32_t*)0x10001660)++; if (*(volatile uint32_t*)0x10001660 > 0xb4) *(volatile uint32_t*)0x10001660 = 0xb4; }
        }
        if (key == 3 || key == 0x21) {
          *TIMEOUT = 0;
          if (*MENU2 == 0) { if (*((volatile uint32_t*)0x10001698) > 0xdac) (*((volatile uint32_t*)0x10001698))--; }
          if (*MENU2 == 1) { if (*((volatile uint32_t*)0x100016a0) > 0xdac) (*((volatile uint32_t*)0x100016a0))--; }
          if (*MENU2 == 2) { if (*((volatile uint32_t*)0x100016a8) > 0xdac) (*((volatile uint32_t*)0x100016a8))--; }
          if (*MENU2 == 3) { if (*((volatile uint32_t*)0x100016b0) > 0xdac) (*((volatile uint32_t*)0x100016b0))--; }
          if (*MENU2 == 4) { if (*((volatile uint32_t*)0x100016b8) > 0xdac) (*((volatile uint32_t*)0x100016b8))--; }
          if (*MENU2 == 5) { if (*ESTOP > 0) (*ESTOP)--; }
          if (*MENU2 == 6) { if (*RESET_MODE > 0) (*RESET_MODE)--; }
          if (*MENU2 == 7) { if (*FEEDBACK > 0) (*FEEDBACK)--; }
          if (*MENU2 == 8) { if (*INPUT_SEL > 0) (*INPUT_SEL)--; }
          if (*MENU2 == 9) { if (*(volatile uint8_t*)0x1000165b > 0) (*(volatile uint8_t*)0x1000165b)--; }
          if (*MENU2 == 0xa) { if (*(volatile uint32_t*)0x10001660 > 0) (*(volatile uint32_t*)0x10001660)--; }
        }
        *TIMEOUT3 = 0xfa;
      }
    }
    (*TIMEOUT3)++;
    if (*TIMEOUT3 == 0xfb) {
      /* 0x5C82-0x5F72：整页重绘，当前项反显（attr=1），其余 attr=0。
       * 行号=项号%4：项0-3→row0-3、项4-7→row0-3、项8-0xa→row0-2。
       * 项0-4=4位数值(disp_uint4，地址 0x10001698/a0/a8/b0/b8)；
       * 项5=ESTOP、项6=RESET_MODE、项7=FEEDBACK、项8=INPUT_SEL、
       * 项9=0x1000165b、项0xa=起始相位(8位装入 disp_number3)。 */
      if (*MENU2 < 4) {
        disp_uint4(*((volatile uint32_t*)0x10001698), 0, 0xb, (*MENU2 == 0) ? 1 : 0);
        disp_uint4(*((volatile uint32_t*)0x100016a0), 1, 0xb, (*MENU2 == 1) ? 1 : 0);
        disp_uint4(*((volatile uint32_t*)0x100016a8), 2, 0xb, (*MENU2 == 2) ? 1 : 0);
        disp_uint4(*((volatile uint32_t*)0x100016b0), 3, 0xb, (*MENU2 == 3) ? 1 : 0);
      } else if (*MENU2 < 8) {
        /* item4=输出电压数值 row0 */
        disp_uint4(*((volatile uint32_t*)0x100016b8), 0, 0xb, (*MENU2 == 4) ? 1 : 0);
        /* item5=ESTOP row1：0=急停/1=外控/2=限相 */
        if (*ESTOP == 0) disp_string((int)0x6018, 1, 0xb, (*MENU2 == 5) ? 1 : 0);
        else if (*ESTOP == 1) disp_string((int)0x6020, 1, 0xb, (*MENU2 == 5) ? 1 : 0);
        else disp_string((int)0x6028, 1, 0xb, (*MENU2 == 5) ? 1 : 0);
        /* item6=RESET_MODE row2：0=复位/1=外控/2=限相 */
        if (*RESET_MODE == 0) disp_string((int)0x6030, 2, 0xb, (*MENU2 == 6) ? 1 : 0);
        else if (*RESET_MODE == 1) disp_string((int)0x6020, 2, 0xb, (*MENU2 == 6) ? 1 : 0);
        else disp_string((int)0x6028, 2, 0xb, (*MENU2 == 6) ? 1 : 0);
        /* item7=FEEDBACK row3：==0→关闭(0x6038)；!=0→检测(0x6040) */
        if (*FEEDBACK == 0) disp_string((int)0x6038, 3, 0xb, (*MENU2 == 7) ? 1 : 0);
        else disp_string((int)0x6040, 3, 0xb, (*MENU2 == 7) ? 1 : 0);
      } else if (*MENU2 < 0xc) {
        /* item8=INPUT_SEL row0：0=电压/1=电流 */
        if (*INPUT_SEL == 0) disp_string((int)0x6048, 0, 0xb, (*MENU2 == 8) ? 1 : 0);
        else disp_string((int)0x6050, 0, 0xb, (*MENU2 == 8) ? 1 : 0);
        /* item9=控制方式 row1：0=全控/1=半控 */
        if (*(volatile uint8_t*)0x1000165b == 0) disp_string((int)0x6058, 1, 0xb, (*MENU2 == 9) ? 1 : 0);
        else disp_string((int)0x6060, 1, 0xb, (*MENU2 == 9) ? 1 : 0);
        /* item0xa=起始相位 row2（8 位装入） */
        disp_number3(*(volatile uint8_t*)0x10001660, 2, 0xb, (*MENU2 == 0xa) ? 1 : 0);
      }
    }
    /* TIMEOUT3 超 0x1F4 → 回绕为 0；编辑态按 MENU2 用空格串擦除当前项值列
     * （与 0xFB 整页重绘交替 → 值"反显/消失"闪烁，周期≈501 帧，0x5F74-0x60FE）。
     * 擦除串：0x5B38=4空格(项0-4 数值)、0x6474=5空格(项5-9 文字)、0x647C=3空格(项0xa 3位)。
     * 行号=项号%4。 */
    if (*TIMEOUT3 > 0x1f4) {
      *TIMEOUT3 = 0;
      if (*MENU3 == 0) return;              /* 查看态：本帧提前返回，跳过 TIMEOUT++ */
      if (*MENU2 == 0) disp_string(0x5b38, 0, 0xb, 0);
      if (*MENU2 == 1) disp_string(0x5b38, 1, 0xb, 0);
      if (*MENU2 == 2) disp_string(0x5b38, 2, 0xb, 0);
      if (*MENU2 == 3) disp_string(0x5b38, 3, 0xb, 0);
      if (*MENU2 == 4) disp_string(0x5b38, 0, 0xb, 0);
      if (*MENU2 == 5) disp_string(0x6474, 1, 0xb, 0);
      if (*MENU2 == 6) disp_string(0x6474, 2, 0xb, 0);
      if (*MENU2 == 7) disp_string(0x6474, 3, 0xb, 0);
      if (*MENU2 == 8) disp_string(0x6474, 0, 0xb, 0);
      if (*MENU2 == 9) disp_string(0x6474, 1, 0xb, 0);
      if (*MENU2 == 0xa) disp_string(0x647c, 2, 0xb, 0);
    }
    /* 编辑空闲超时回主屏（0x6102-0x6132）：TIMEOUT 每帧累加（非仅擦除帧）；MENU3==0 帧已提前返回 */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; param_sync_live_to_eeprom(); *MENU = 1; disp_splash_screen(); return; }
    return;
  }

  if (*MENU == 2) {
    /* ---------- case2 主菜单页1 (0x6134-0x69D6)，MENU2=选项0-8 ---------- */
    if (key == 4) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); return; }
    if (key == 2 || key == 3) {
      *TIMEOUT = 0;
      if (key == 3) { (*MENU2)++; if (*MENU2 > 7) *MENU2 = 7; }
      if (key == 2) { if (*MENU2 > 0) (*MENU2)--; }
      if (*MENU2 < 4) {
        disp_string((int)0x6488, 0, 0, 0); disp_string((int)0x649c, 1, 0, 0);
        disp_string((int)0x64b0, 2, 0, 0); disp_string((int)0x64c4, 3, 0, 0);
      }
      if (*MENU2 >= 4 && *MENU2 < 8) {
        disp_string((int)0x64d8, 0, 0, 0); disp_string((int)0x64ec, 1, 0, 0);
        disp_string((int)0x6500, 2, 0, 0); disp_string((int)0x6514, 3, 0, 0);
      }
      if (*MENU2 >= 8 && *MENU2 < 0xc) {
        disp_string((int)0x6528, 0, 0, 0); disp_string((int)0x5ba4, 1, 0, 0);
        disp_string((int)0x5ba4, 2, 0, 0); disp_string((int)0x5ba4, 3, 0, 0);
      }
      if (*MENU2 == 0) disp_string((int)0x6488, 0, 0, 1);
      if (*MENU2 == 1) disp_string((int)0x649c, 1, 0, 1);
      if (*MENU2 == 2) disp_string((int)0x64b0, 2, 0, 1);
      if (*MENU2 == 3) disp_string((int)0x64c4, 3, 0, 1);
      if (*MENU2 == 4) disp_string((int)0x64d8, 0, 0, 1);
      if (*MENU2 == 5) disp_string((int)0x64ec, 1, 0, 1);
      if (*MENU2 == 6) disp_string((int)0x6500, 2, 0, 1);
      if (*MENU2 == 7) disp_string((int)0x6514, 3, 0, 1);
      if (*MENU2 == 8) disp_string((int)0x6528, 0, 0, 1);
    }
    if (key == 1) {
      *TIMEOUT = 0; *MENU3 = 0;
      if (*MENU2 == 0) {
        *MENU = 3; *MENU2 = 0;
        disp_string((int)0x6540, 0, 0, 0); disp_string((int)0x6554, 1, 0, 0);
        disp_string((int)0x6568, 2, 0, 0); disp_string((int)0x657c, 3, 0, 0);
        if (*CTRL_MODE == 0) { disp_string((int)0x6594, 0, 0xb, 1); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
        if (*CTRL_MODE == 1) { disp_string((int)0x659c, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
        if (*CTRL_MODE == 2) { disp_string((int)0x65a4, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
        disp_uint4(*V_RANGE, 1, 0xb, 0);
        disp_uint4(*A_RANGE, 2, 0xb, 0);
        disp_uint4(*(volatile uint32_t*)0x10001640, 3, 0xb, 0);
        *TIMEOUT3 = 0xfa;
      }
      if (*MENU2 == 1) {
        *MENU = 4; *MENU2 = 0;
        disp_string((int)0x65bc, 0, 0, 0); disp_string((int)0x65d0, 1, 0, 0);
        disp_string((int)0x65e4, 2, 0, 0); disp_string((int)0x65f8, 3, 0, 0);
        disp_uint4(*(volatile uint32_t*)0x100016c0, 0, 0xb, 1);
        disp_uint4(*(volatile uint8_t*)0x100016c4, 1, 0xb, 0);
        disp_uint4(*(volatile uint32_t*)0x100016c8, 2, 0xb, 0);
        disp_uint4(*(volatile uint8_t*)0x100016cc, 3, 0xb, 0);
        *TIMEOUT3 = 0xfa;
      }
      if (*MENU2 == 2) {
        *MENU = 5; *MENU2 = 0;
        disp_string((int)0x6a18, 0, 0, 0); disp_string((int)0x6a2c, 1, 0, 0);
        disp_string((int)0x6a40, 2, 0, 0); disp_string((int)0x6a54, 3, 0, 0);
        disp_uint5(*COM_ADDR, 0, 0xb, 1);
        disp_number(BAUD_TBL[*BAUD_IDX], 1, 0xa, 0);
        if (*PARITY == 0) disp_string((int)0x6a78, 2, 0xa, 0);
        if (*PARITY == 1) disp_string((int)0x6a80, 2, 0xa, 0);
        if (*PARITY == 2) disp_string((int)0x6a88, 2, 0xa, 0);
        if (*COM_CHK == 0) disp_string((int)0x6038, 3, 0xb, 0);
        else disp_string((int)0x6a94, 3, 0xb, 0);
        *TIMEOUT3 = 0xfa;
      }
      if (*MENU2 == 3) {
        *MENU = 6; *MENU2 = 0; *LATCH_OUT = 0; disp_clear();
        /* BIN 0x6712 是 ldr r0,[0x6aa0] 取字面量值 0x4D9C（"  密码:------"），
         * 不是取 0x6aa0 处的指令字节——0x6AA0 处 4 字节=9C 4D 00 00 会只显示"M"。 */
        disp_string((int)0x4d9c, 1, 0, 0);
      }
      if (*MENU2 == 4) {
        *MENU = 7; *MENU2 = 0;
        disp_string((int)0x6aa4, 0, 0, 0); disp_string((int)0x6ab8, 1, 0, 0);
        disp_string((int)0x6acc, 2, 0, 0); disp_string((int)0x6ae0, 3, 0, 0);
        if (*(volatile uint8_t*)0x10001710 == 1) { disp_string((int)0x6af8, 0, 0xb, 1); disp_uint2(*(volatile uint8_t*)0x10001711, 1, 0xb, 0); disp_uint2(*(volatile uint8_t*)0x10001712, 2, 0xb, 0); }
        if (*(volatile uint8_t*)0x10001710 == 2) { disp_string((int)0x6b08, 0, 0xb, 1); disp_uint2(*(volatile uint8_t*)0x10001713, 1, 0xb, 0); disp_uint2(*(volatile uint8_t*)0x10001712, 2, 0xb, 0); }
        if (*(volatile uint8_t*)0x10001710 == 3) { disp_string((int)0x6b14, 0, 0xb, 1); disp_uint2(*(volatile uint8_t*)0x10001715, 1, 0xb, 0); disp_uint2(*(volatile uint8_t*)0x10001716, 2, 0xb, 0); }
        if (*(volatile uint8_t*)0x10001710 == 4) { disp_string((int)0x6b24, 0, 0xb, 1); disp_uint2(*(volatile uint8_t*)0x10001717, 1, 0xb, 0); disp_uint2(*(volatile uint8_t*)0x10001718, 2, 0xb, 0); }
        *TIMEOUT3 = 0xfa;
      }
      if (*MENU2 == 5) {
        *MENU = 8; *MENU2 = 0; disp_clear();
        disp_string((int)0x6b34, 0, 4, 0); disp_string((int)0x6b40, 1, 2, 0);
        disp_string((int)0x6b4c, 2, 2, 0); disp_offset(*PHASE_OFF, 2, 7, 1);
        disp_string((int)0x6b58, 3, 0, 0);
      }
      if (*MENU2 == 6) {
        *MENU = 0xb; *MENU2 = 0; *LATCH_OUT = 0; disp_clear();
        disp_string((int)0x4d58, 0, 0, 0); disp_string((int)0x4d6c, 1, 0, 0);
        disp_string((int)0x4d80, 2, 0, 0); disp_string((int)0x4d6c, 3, 0, 0);
        disp_uint5(*HOUR_NOW, 1, 3, 0); disp_uint2(*MIN_NOW, 1, 0xa, 0);
        disp_uint5(*HOUR_TOTAL, 3, 3, 0); disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
      }
      if (*MENU2 == 7) {
        *MENU = 9; *MENU2 = 0; disp_clear();
        disp_string((int)0x6b78, 0, 0, 0); disp_string((int)0x6b84, 1, 0, 0);
        disp_string((int)0x6b94, 2, 0, 0); disp_string((int)0x6ba4, 3, 0, 0);
      }
      if (*MENU2 == 8) {
        *MENU = 0x5a; *MENU2 = 0; disp_clear();
        disp_string((int)0x6bb8, 0, 4, 0); disp_string((int)0x6b40, 1, 2, 0);
        disp_string((int)0x6b4c, 2, 2, 0); disp_signed_angle(*BAL_ANG, 2, 7, 1);
        disp_string((int)0x6b58, 3, 0, 0);
      }
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* ================= case3 基本参数屏 (0x69D6-0x7C1A) =================
     说明：MENU2=当前项号 0-15；MENU3=编辑态标志(0=在项间导航,1=修改当前项值)。
     值映射(全部 bin 校验)：0=CTRL_MODE 0..2 | 1=V_RANGE 0x1770 | 2=A_RANGE 0x1770
       | 3=0x10001640 0x1770 | 4=限位0x10001648(<=V_RANGE+1) | 5=限位0x10001644(<=A_RANGE+1)
       | 6=0x1000164c 0xc8 | 7=0x1000164d 0xc8 | 8=0x10001650 0xb4 | 9=0x10001654 0xa0
       | 10=DISP_SEL 0..2 | 11=0x10001656 0..1 | 12=ESTOP 0..2 | 13=FEEDBACK 0..1
       | 14=INPUT_SEL 0..1 | 15=0x10001660 0xb4                                 */
  if (*MENU == 3) {
    volatile uint8_t  *b4c = (volatile uint8_t*)0x1000164c;
    volatile uint8_t  *b4d = (volatile uint8_t*)0x1000164d;
    volatile uint8_t  *b54 = (volatile uint8_t*)0x10001654;
    volatile uint8_t  *b56 = (volatile uint8_t*)0x10001656;
    volatile uint32_t *w40 = (volatile uint32_t*)0x10001640;
    volatile uint32_t *w44 = (volatile uint32_t*)0x10001644;
    volatile uint32_t *w48 = (volatile uint32_t*)0x10001648;
    volatile uint32_t *w50 = (volatile uint32_t*)0x10001650;
    volatile uint32_t *w60 = (volatile uint32_t*)0x10001660;
    uint32_t it = *MENU2;                              /* 项号 */

    /* 原汇编公共尾部 0x7446-0x744E 对刷新计数加一。提前执行可保持所有
     * 提前返回/辅助绘制调用下的同一可观察结果；按键分支在下方写入最终值。 */
    (*TIMEOUT3)++;

    /* ---- key==1：在 查看/编辑 之间切换 MENU3，并复位修改空闲计时 ---- */
    if (key == 1) {
      *TIMEOUT = 0;
      (*MENU3)++;
      if (*MENU3 > 1) *MENU3 = 0;
      *TIMEOUT3 = 0xfb;
      if (*MENU3 == 1) *TIMEOUT3 = 0x1f5;
    }

    /* ---- key==4：保存并退回 参数子菜单(type2 屏) ---- */
    else if (key == 4) {
      *TIMEOUT = 0; *MENU = 2; *MENU2 = 0;
      param_sync_live_to_eeprom(); disp_clear();
      disp_string((int)0x4814,0,0,1); disp_string((int)0x4824,1,0,0);
      disp_string((int)0x4834,2,0,0); disp_string((int)0x4844,3,0,0);
    }

    /* ---- key==2/3 且 *MENU3==0：项间导航 (MENU2=0..15) ---- */
    else if ((key == 2 || key == 3) && *MENU3 == 0) {
      *TIMEOUT = 0;
      if (key == 3) { (*MENU2)++; if (*MENU2 > 0xf) *MENU2 = 0xf; }
      else          { if (*MENU2 > 0) (*MENU2)--; }
      *TIMEOUT3 = 0xfb;
      /* 重绘新项所在页标题(值列清空)；值由尾部整页重绘恢复 */
      it = *MENU2;
      switch (it >> 2) {
        case 0: disp_string((int)0x6540,0,0,0); disp_string((int)0x6554,1,0,0);
                disp_string((int)0x6568,2,0,0); disp_string((int)0x657c,3,0,0); break;
        case 1: disp_string((int)0x6fe4,0,0,0); disp_string((int)0x6ff8,1,0,0);
                disp_string((int)0x700c,2,0,0); disp_string((int)0x7020,3,0,0); break;
        case 2: disp_string((int)0x7034,0,0,0); disp_string((int)0x7048,1,0,0);
                disp_string((int)0x705c,2,0,0); disp_string((int)0x7070,3,0,0); break;
        default: disp_string((int)0x7084,0,0,0); disp_string((int)0x7098,1,0,0);
                disp_string((int)0x70ac,2,0,0); disp_string((int)0x70c0,3,0,0); break;
      }
    }

    /* ---- key==2/0x16/3/0x21 且 *MENU3==1：修改当前项值 ---- */
    else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *MENU3 == 1) {
      *TIMEOUT = 0;
      *TIMEOUT3 = 0xfb;
      it = *MENU2;
      /* 步进：数字项 key==0x16 快加 +5、key==0x21 快减 -5 */
      if (it >= 1 && it <= 5) {
        if (key == 0x16) {                     /* 快加 +5 */
          if (it == 1) { *V_RANGE += 5; if (*V_RANGE > 0x1770) *V_RANGE = 0x1770; }
          else if (it == 2) { *A_RANGE += 5; if (*A_RANGE > 0x1770) *A_RANGE = 0x1770; }
          else if (it == 3) { *w40 += 5;      if (*w40 > 0x1770) *w40 = 0x1770; }
          else if (it == 4) { *w48 += 5;      if (*w48 > *V_RANGE + 1) *w48 = *V_RANGE + 1; }
          else              { *w44 += 5;      if (*w44 > *A_RANGE + 1) *w44 = *A_RANGE + 1; }
        } else if (key == 0x21) {              /* 快减 -5 (数字项下限 0xf) */
          if (it == 1) { if (*V_RANGE < 0x10) *V_RANGE = 0xf; *V_RANGE -= 5; }
          else if (it == 2) { if (*A_RANGE < 0x10) *A_RANGE = 0xf; *A_RANGE -= 5; }
          else if (it == 3) { if (*w40 < 0x10) *w40 = 0xf;      *w40 -= 5; }
          else if (it == 4) { if (*w48 < 0x10) *w48 = 0xf;      *w48 -= 5; }
          else              { if (*w44 < 0x10) *w44 = 0xf;      *w44 -= 5; }
        } else {                               /* key==2/3：+1/-1 */
          if (key == 2) {
            if (it == 1) { (*V_RANGE)++; if (*V_RANGE > 0x1770) *V_RANGE = 0x1770; }
            else if (it == 2) { (*A_RANGE)++; if (*A_RANGE > 0x1770) *A_RANGE = 0x1770; }
            else if (it == 3) { (*w40)++; if (*w40 > 0x1770) *w40 = 0x1770; }
            else if (it == 4) { (*w48)++; if (*w48 > *V_RANGE + 1) *w48 = *V_RANGE + 1; }
            else { (*w44)++; if (*w44 > *A_RANGE + 1) *w44 = *A_RANGE + 1; }
          } else {                             /* key==3：-1 (数字项下限 0xb) */
            if (it == 1) { if (*V_RANGE > 0xa) (*V_RANGE)--; }
            else if (it == 2) { if (*A_RANGE > 0xa) (*A_RANGE)--; }
            else if (it == 3) { if (*w40 > 0xa) (*w40)--; }
            else if (it == 4) { if (*w48 > 0xa) (*w48)--; }
            else { if (*w44 > 0xa) (*w44)--; }
          }
        }
      } else {
        /* 非数字项(0,6..15)：key==2/0x16 +1、key==3/0x21 -1 */
        if (key == 3 || key == 0x21) {         /* 减 */
          switch (it) {
            case 0: if (*CTRL_MODE == 0) *CTRL_MODE = 3; (*CTRL_MODE)--; break;
            case 6: if (*b4c > 0) (*b4c)--; break;
            case 7: if (*b4d > 0) (*b4d)--; break;
            case 8: if (*w50 != 0) (*w50)--; break;
            case 9: if (*b54 > 0x28) (*b54)--; break;
            case 10: if (*DISP_SEL > 0) (*DISP_SEL)--; break;
            case 11: if (*b56 > 0) (*b56)--; break;
            case 12: if (*ESTOP > 0) (*ESTOP)--; break;
            case 13: if (*FEEDBACK > 0) (*FEEDBACK)--; break;
            case 14: if (*INPUT_SEL > 0) (*INPUT_SEL)--; break;
            case 15: if (*w60 != 0) (*w60)--; break;
          }
        } else {                               /* key==2/0x16：加 */
          switch (it) {
            case 0: (*CTRL_MODE)++; if (*CTRL_MODE > 2) *CTRL_MODE = 0; break;
            case 6: (*b4c)++; if (*b4c > 0xc8) *b4c = 0xc8; break;
            case 7: (*b4d)++; if (*b4d > 0xc8) *b4d = 0xc8; break;
            case 8: (*w50)++; if (*w50 > 0xb4) *w50 = 0xb4; break;
            case 9: (*b54)++; if (*b54 > 0xa0) *b54 = 0xa0; break;
            case 10: (*DISP_SEL)++; if (*DISP_SEL > 2) *DISP_SEL = 2; break;   /* BIN 钳位 */
            case 11: (*b56)++; if (*b56 > 1) *b56 = 1; break;                  /* BIN 钳位 */
            case 12: (*ESTOP)++; if (*ESTOP > 2) *ESTOP = 2; break;            /* BIN 钳位 */
            case 13: (*FEEDBACK)++; if (*FEEDBACK > 1) *FEEDBACK = 1; break;   /* BIN 钳位 */
            case 14: (*INPUT_SEL)++; if (*INPUT_SEL > 1) *INPUT_SEL = 1; break; /* BIN 钳位 */
            case 15: (*w60)++; if (*w60 > 0xb4) *w60 = 0xb4; break;
          }
        }
      }
    }

    /* ---- TIMEOUT3 计数到 0xFB：整页重绘当前页全部 4 项值(当前项高亮)，恢复被标签重绘清掉的值列 ---- */
    if (*TIMEOUT3 == 0xfb) sm3_draw_page(*MENU2);

    /* ---- TIMEOUT3 超过 0x1F4：回绕为 0；编辑态按 MENU2 用空格擦除当前项值列 ---- */
    /*     与 0xFB 整页重绘(当前项反显)交替 → 值"反显/消失"闪烁，周期≈501 帧 (0x7A32-0x7BD8)。
     *     空格串：0x6474=5空格(item0/10..14)、0x7068=4空格(item1..7)、0x647C=0x6474+8=3空格(item8/9/15)。
     *     item4/5 按 V_RANGE/A_RANGE 是否顶到限位选宽/窄擦除串。 */
    if (*TIMEOUT3 > 0x1f4) {
      *TIMEOUT3 = 0;
      if (*MENU3 == 0) return;              /* 查看态：本帧提前返回(b.w 0x4ba8=pop{r4})，跳过 TIMEOUT++ */
      switch (it) {
        case 0:  disp_string(0x6474, 0, 0xb, 0); break;
        case 1:  disp_string(0x7068, 1, 0xb, 0); break;
        case 2:  disp_string(0x7068, 2, 0xb, 0); break;
        case 3:  disp_string(0x7068, 3, 0xb, 0); break;
        case 4:  if (*V_RANGE >= *w48) disp_string(0x7068, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 5:  if (*A_RANGE >= *w44) disp_string(0x7068, 1, 0xb, 0);
                 else disp_string(0x6474, 1, 0xb, 0);
                 break;
        case 6:  disp_string(0x7068, 2, 0xb, 0); break;
        case 7:  disp_string(0x7068, 3, 0xb, 0); break;
        case 8:  disp_string(0x6474 + 0x8, 0, 0xb, 0); break;
        case 9:  disp_string(0x6474 + 0x8, 1, 0xb, 0); break;
        case 10: disp_string(0x6474, 2, 0xb, 0); break;
        case 11: disp_string(0x6474, 3, 0xb, 0); break;
        case 12: disp_string(0x6474, 0, 0xb, 0); break;
        case 13: disp_string(0x6474, 1, 0xb, 0); break;
        case 14: disp_string(0x6474, 2, 0xb, 0); break;
        case 15: disp_string(0x6474 + 0x8, 3, 0xb, 0); break;
      }
    }

    /* ---- 恒压/恒流(CTRL_MODE<2)且软起时间 b4c 未配置时自动置 1 (0x7BE2-0x7BF4)；
     *     随后编辑空闲超时回到主屏 (0x7BD8-0x7C16) ---- */
    (*TIMEOUT)++;
    if (*CTRL_MODE < 2 && *b4c == 0) *b4c = 1;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* ================= case4 保护参数屏 (0x7C1A-0x8780) =================
     说明：MENU2=当前项号 0-9；MENU3=编辑态标志(0=导航,1=修改)。10 项 = 3 页。
     项映射(全部 bin 校验，§13)：0=过压0x100016c0(<=V_RANGE) 1=过压时间0x100016c4(<=0xc8)
       2=欠压0x100016c8(<=V_RANGE) 3=欠压时间0x100016cc(<=0xc8)
       4=IF过载0x100016d0(<=A_RANGE) 5=IF过载时间0x100016d4(<=0xc8)
       6=CT过载0x100016d8(<=0x10001640) 7=CT过载时间0x100016dc(<=0xc8)
       8=缺相0x100016dd(0..1) 9=三相平衡0x100016de(0..0x3c,%)
     编辑方向按反汇编：2/0x16 增、3/0x21 减；word 项 0x16/0x21 走 ±5。          */
  if (*MENU == 4) {
    (*TIMEOUT3)++;
    /* ---- key==1：切换编辑态 ---- */
    if (key == 1) {
      *TIMEOUT = 0;
      (*MENU3)++;
      if (*MENU3 > 1) *MENU3 = 0;
      *TIMEOUT3 = (*MENU3 == 0) ? 0xfb : 0x1f5;
    }

    /* ---- key==4：保存并退回 参数子菜单(type2 屏) ---- */
    else if (key == 4) {
      *TIMEOUT = 0; *MENU = 2; *MENU2 = 1;
      param_sync_live_to_eeprom(); disp_clear();
      disp_string((int)0x4814,0,0,0); disp_string((int)0x4824,1,0,1);
      disp_string((int)0x4834,2,0,0); disp_string((int)0x4844,3,0,0);
    }

    /* ---- key==2/3 且 *MENU3==0：项间导航 (MENU2=0..9) + 画页标题 ---- */
    else if ((key == 2 || key == 3) && *MENU3 == 0) {
      *TIMEOUT = 0;
      *TIMEOUT3 = 0xfb;
      if (key == 3) { (*MENU2)++; if (*MENU2 > 9) *MENU2 = 9; }
      else          { if (*MENU2 > 0) (*MENU2)--; }
      /* 画当前项所在页 4 行标题；页2 的 2/3 行用空串占位 */
      if (*MENU2 < 4) {
        disp_string((int)0x65bc,0,0,0); disp_string((int)0x65d0,1,0,0);
        disp_string((int)0x65e4,2,0,0); disp_string((int)0x65f8,3,0,0);
      } else if (*MENU2 < 8) {
        disp_string((int)0x7e10,0,0,0); disp_string((int)0x7e24,1,0,0);
        disp_string((int)0x7e38,2,0,0); disp_string((int)0x7e4c,3,0,0);
      } else {
        disp_string((int)0x7e60,0,0,0); disp_string((int)0x7e74,1,0,0);
        disp_string((int)0x5ba4,2,0,0); disp_string((int)0x5ba4,3,0,0);
      }
    }

    /* ---- key==2/0x16/3/0x21 且 *MENU3==1：修改当前项值 ---- */
    else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *MENU3 == 1) {
      uint32_t it = *MENU2;
      *TIMEOUT = 0;
      /* 增：key==2 / key==0x16 → byte 项 +1（时间/缺相/三相平衡），word 项 +1(2)/+5(0x16) */
      if (key == 2 || key == 0x16) {
        /* byte 项 1/3/5/7 +1 clamp 0xc8；8 +1 clamp 1；9 +1 clamp 0x3c */
        if (it == 1) { (*((volatile uint8_t*)0x100016c4))++; if (*((volatile uint8_t*)0x100016c4) > 0xc8) *((volatile uint8_t*)0x100016c4) = 0xc8; }
        else if (it == 3) { (*((volatile uint8_t*)0x100016cc))++; if (*((volatile uint8_t*)0x100016cc) > 0xc8) *((volatile uint8_t*)0x100016cc) = 0xc8; }
        else if (it == 5) { (*((volatile uint8_t*)0x100016d4))++; if (*((volatile uint8_t*)0x100016d4) > 0xc8) *((volatile uint8_t*)0x100016d4) = 0xc8; }
        else if (it == 7) { (*((volatile uint8_t*)0x100016dc))++; if (*((volatile uint8_t*)0x100016dc) > 0xc8) *((volatile uint8_t*)0x100016dc) = 0xc8; }
        else if (it == 8) { (*((volatile uint8_t*)0x100016dd))++; if (*((volatile uint8_t*)0x100016dd) > 1) *((volatile uint8_t*)0x100016dd) = 1; }
        else if (it == 9) { (*((volatile uint8_t*)0x100016de))++; if (*((volatile uint8_t*)0x100016de) > 0x3c) *((volatile uint8_t*)0x100016de) = 0x3c; }
        /* word 项 0/2/4/6：key==2 +1 / key==0x16 +5，上限 V_RANGE / A_RANGE / w40 */
        if (it == 0) {
          *(volatile uint32_t*)0x100016c0 += (key == 0x16) ? 5 : 1;
          if (*(volatile uint32_t*)0x100016c0 > *V_RANGE) *(volatile uint32_t*)0x100016c0 = *V_RANGE;
        } else if (it == 2) {
          *(volatile uint32_t*)0x100016c8 += (key == 0x16) ? 5 : 1;
          if (*(volatile uint32_t*)0x100016c8 > *V_RANGE) *(volatile uint32_t*)0x100016c8 = *V_RANGE;
        } else if (it == 4) {
          *(volatile uint32_t*)0x100016d0 += (key == 0x16) ? 5 : 1;
          if (*(volatile uint32_t*)0x100016d0 > *A_RANGE) *(volatile uint32_t*)0x100016d0 = *A_RANGE;
        } else if (it == 6) {
          *(volatile uint32_t*)0x100016d8 += (key == 0x16) ? 5 : 1;
          if (*(volatile uint32_t*)0x100016d8 > *(volatile uint32_t*)0x10001640) *(volatile uint32_t*)0x100016d8 = *(volatile uint32_t*)0x10001640;
        }
      } else if (key == 3 || key == 0x21) {
        /* 减：byte 项 1/3/5/7/8 +1... 实为 >0 → -1；item9 下限 0x9 */
        if (it == 1) { uint8_t v=*((volatile uint8_t*)0x100016c4); if (v) *((volatile uint8_t*)0x100016c4) = v-1; }
        else if (it == 3) { uint8_t v=*((volatile uint8_t*)0x100016cc); if (v) *((volatile uint8_t*)0x100016cc) = v-1; }
        else if (it == 5) { uint8_t v=*((volatile uint8_t*)0x100016d4); if (v) *((volatile uint8_t*)0x100016d4) = v-1; }
        else if (it == 7) { uint8_t v=*((volatile uint8_t*)0x100016dc); if (v) *((volatile uint8_t*)0x100016dc) = v-1; }
        else if (it == 8) { uint8_t v=*((volatile uint8_t*)0x100016dd); if (v) *((volatile uint8_t*)0x100016dd) = v-1; }
        else if (it == 9) { uint8_t v=*((volatile uint8_t*)0x100016de); if (v > 9) *((volatile uint8_t*)0x100016de) = v-1; }
        /* 减：word 项 0/2/4/6：key==3 若 v>0 → -1(下限0)；key==0x21 若 v<6 先置5，再 -5(下限0) */
        if (it == 0) { uint32_t v=*(volatile uint32_t*)0x100016c0; if (key==0x21) { uint32_t w=(v<6)?5:v; *(volatile uint32_t*)0x100016c0 = w-5; } else if (v) *(volatile uint32_t*)0x100016c0=v-1; }
        else if (it == 2) { uint32_t v=*(volatile uint32_t*)0x100016c8; if (key==0x21) { uint32_t w=(v<6)?5:v; *(volatile uint32_t*)0x100016c8 = w-5; } else if (v) *(volatile uint32_t*)0x100016c8=v-1; }
        else if (it == 4) { uint32_t v=*(volatile uint32_t*)0x100016d0; if (key==0x21) { uint32_t w=(v<6)?5:v; *(volatile uint32_t*)0x100016d0 = w-5; } else if (v) *(volatile uint32_t*)0x100016d0=v-1; }
        else if (it == 6) { uint32_t v=*(volatile uint32_t*)0x100016d8; if (key==0x21) { uint32_t w=(v<6)?5:v; *(volatile uint32_t*)0x100016d8 = w-5; } else if (v) *(volatile uint32_t*)0x100016d8=v-1; }
      }
      *TIMEOUT3 = 0xfb;
    }

    /* ---- 刷新节流：TIMEOUT3==0xfb 时重绘当前页（高亮当前项） ---- */
    if (*TIMEOUT3 == 0xfb) sm4_draw_page(*MENU2);

    /* ---- TIMEOUT3 超过 0x1F4：回绕为 0；编辑态按 MENU2 用空格擦除当前项值列 ---- */
    /*     与 0xFB 整页重绘(当前项反显)交替 → 值"反显/消失"闪烁，周期≈501 帧 (0x85C4-0x874E)。
     *     窄串 0x7E6C=4 空格（word 项 0/2/4/6 值≠0、byte 项 1/3/5/7）；
     *     宽串 0x6474=5 空格（word 项值==0、byte 项 8/9）。 */
    if (*TIMEOUT3 > 0x1f4) {
      *TIMEOUT3 = 0;
      if (*MENU3 == 0) return;              /* 查看态：本帧提前返回 */
      switch (*MENU2) {
        case 0:  if (*(volatile uint32_t*)0x100016c0 != 0) disp_string(0x7e6c, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 1:  disp_string(0x7e6c, 1, 0xb, 0); break;
        case 2:  if (*(volatile uint32_t*)0x100016c8 != 0) disp_string(0x7e6c, 2, 0xb, 0);
                 else disp_string(0x6474, 2, 0xb, 0);
                 break;
        case 3:  disp_string(0x7e6c, 3, 0xb, 0); break;
        case 4:  if (*(volatile uint32_t*)0x100016d0 != 0) disp_string(0x7e6c, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 5:  disp_string(0x7e6c, 1, 0xb, 0); break;
        case 6:  if (*(volatile uint32_t*)0x100016d8 != 0) disp_string(0x7e6c, 2, 0xb, 0);
                 else disp_string(0x6474, 2, 0xb, 0);
                 break;
        case 7:  disp_string(0x7e6c, 3, 0xb, 0); break;
        case 8:  disp_string(0x6474, 0, 0xb, 0); break;
        case 9:  disp_string(0x6474, 1, 0xb, 0); break;
      }
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case5 通讯屏 (0x8780-0x8C1A)，MENU==5，4 项（MENU2=0-3）单页 ---------- */
  if (*MENU == 5) {
    (*TIMEOUT3)++;
    /* key==1：进入/退出编辑模式（MENU3 0<->1） */
    if (key == 1) {
      *TIMEOUT = 0;
      (*MENU3)++;
      if (*MENU3 > 1) *MENU3 = 0;
      *TIMEOUT3 = (*MENU3 == 0) ? 0xfb : 0x1f5;
    }
    /* key==4：返回基本参数主菜单（MENU=2/MENU2=2），立即写回 EEPROM */
    else if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 2;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x4814, 0, 0, 0);
      disp_string(0x4824, 1, 0, 0);
      disp_string(0x4834, 2, 0, 1);
      disp_string(0x4844, 3, 0, 0);
    }
    /* 导航（MENU3==0，key2/3 上下移，仅 4 项） */
    else if ((key == 2 || key == 3) && *MENU3 == 0) {
      *TIMEOUT = 0;
      *TIMEOUT3 = 0xfb;
      if (key == 3) { (*MENU2)++; if (*MENU2 > 3) *MENU2 = 3; }
      if (key == 2) { if (*MENU2 > 0) (*MENU2)--; }
      if (*MENU2 < 4) {   /* 重绘通讯页标题帧（值列 0xb/0xa 由刷新块绘制） */
        disp_string(0x6a18, 0, 0, 0);
        disp_string(0x6a2c, 1, 0, 0);
        disp_string(0x6a40, 2, 0, 0);
        disp_string(0x6a54, 3, 0, 0);
      }
    }
    /* 编辑（MENU3==1，key2/0x16 增、key3/0x21 减；byte 项恒 ±1，word 项恒 ±1） */
    else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *MENU3 == 1) {
      *TIMEOUT3 = 0xfb;
      if (key == 2 || key == 0x16) {   /* 增 */
        *TIMEOUT = 0;
        if (*MENU2 == 0) { if (*COM_ADDR >= 0xf6) *COM_ADDR = 0xf6; (*COM_ADDR)++; }
        if (*MENU2 == 1) { if (*BAUD_IDX >= 7) *BAUD_IDX = 6; (*BAUD_IDX)++; }
        if (*MENU2 == 2) { (*PARITY)++; if (*PARITY > 3) *PARITY = 3; }
        if (*MENU2 == 3) *COM_CHK = 1;
      }
      if (key == 3 || key == 0x21) {   /* 减 */
        *TIMEOUT = 0;
        if (*MENU2 == 0) { if (*COM_ADDR > 1) (*COM_ADDR)--; }
        if (*MENU2 == 1) { if (*BAUD_IDX != 0) (*BAUD_IDX)--; }
        if (*MENU2 == 2) { if (*PARITY > 0) (*PARITY)--; }
        if (*MENU2 == 3) *COM_CHK = 0;
      }
    }

    /* ---- 刷新节流：TIMEOUT3==0xfb 时整页重绘（高亮当前项） ---- */
    if (*TIMEOUT3 == 0xfb) { if (*MENU2 < 4) sm5_draw_page(*MENU2); }

    /* ---- 编辑空闲超时：清空当前项所在行（闪烁）后返回主屏 ---- */
    if (*TIMEOUT3 > 0x1f4) {
      *TIMEOUT3 = 0;
      if (*MENU3 == 0) return;
      if (*MENU2 == 0 || *MENU2 == 4) disp_string(0x6474, 0, 0xb, 0);
      if (*MENU2 == 1 || *MENU2 == 5) disp_string(0x8f44, 1, 0xa, 0);
      if (*MENU2 == 2 || *MENU2 == 6) disp_string(0x8f44, 2, 0xa, 0);
      if (*MENU2 == 3 || *MENU2 == 7) disp_string(0x6474, 3, 0xb, 0);
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case8 相位参数校准 (0x9A84-0x9C5C)，MENU==8 ---------- */
  if (*MENU == 8) {
    *((volatile uint8_t*)0x100015ce) = 1;
    /* 相位偏移 增（key3/0x21 上限 0x2b0）/ 减（key2/0x16 下限 0x45） */
    if (key == 3 || key == 0x21) {
      *TIMEOUT = 0;
      (*PHASE_OFF)++;
      if (*PHASE_OFF > 0x2b0) *PHASE_OFF = 0x2b0;
      disp_offset(*PHASE_OFF, 2, 7, 1);
    }
    if (key == 2 || key == 0x16) {
      *TIMEOUT = 0;
      if (*PHASE_OFF < 0x45) *PHASE_OFF = 0x45;
      (*PHASE_OFF)--;
      disp_offset(*PHASE_OFF, 2, 7, 1);
    }
    /* 运行状态行显示（FAULT!=0 停机 / RUN 控制 STATUS 0/1/2） */
    if (*FAULT != 0) {
      *STATUS = 0;
      disp_string(0x47dc, 3, 0xa, 0);
    } else {
      if (*RUN == 0 && *STATUS != 1) { *STATUS = 1; disp_string(0x47dc + 0xc, 3, 0xa, 0); }
      if (*RUN == 1 && *STATUS != 2) { *STATUS = 2; disp_string(0x47dc + 0x14, 3, 0xa, 0); }
    }
    if (key == 5) { *RUN = 1; *TIMEOUT = 0; }
    if (key == 6) { *RUN = 0; *TIMEOUT = 0; gpio_outputs_set(); }
    run_stop_preset();
    if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 5;
      disp_clear();
      disp_string(0x6474 + 0x64, 0, 0, 0);
      disp_string(0x6474 + 0x78, 1, 0, 1);
      disp_string(0x6474 + 0x8c, 2, 0, 0);
      disp_string(0x6474 + 0xa0, 3, 0, 0);
      if (*PHASE_OFF != *((volatile uint32_t*)0x10001630)) {  /* 写 EEPROM reg 0xc9/0xca */
        *((volatile uint32_t*)0x10001630) = *PHASE_OFF;
        i2c_write_reg((uint16_t)*((volatile uint32_t*)0x10001630) >> 8, 0xc9);
        i2c_write_reg((uint8_t)*((volatile uint32_t*)0x10001630), 0xca);
      }
      gpio_outputs_set();
      *RUN = 0;
      run_stop_preset();
      *((volatile uint8_t*)0x100015ce) = 0;
      *STATUS = 0;
      return;
    }
    /* 超时尾（0x3a98=15000） */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x3a98) {
      *TIMEOUT = 0;
      *RUN = 0;
      gpio_outputs_set();
      *MENU = 1;
      disp_splash_screen();
      *((volatile uint8_t*)0x100015ce) = 0;
      *STATUS = 0;
    }
    return;
  }

  /* =====================================================================*/
  /* ---------- caseB 运行时间查询 (0x9C5C-0x9D86)，MENU==0xb ---------- */
  if (*MENU == 0xb) {
    if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 6;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x6474 + 0x64, 0, 0, 0);
      disp_string(0x6474 + 0x78, 1, 0, 0);
      disp_string(0x6474 + 0x8c, 2, 0, 1);
      disp_string(0x6514, 3, 0, 0);
      return;
    }
    /* key==0x17 统计清零 → 4 个时间 word 归零并重显 */
    if (key == 0x17) {
      *HOUR_NOW = 0;
      *MIN_NOW = 0;
      *HOUR_TOTAL = 0;
      *MIN_TOTAL = 0;
      disp_uint5(*HOUR_NOW, 1, 3, 0);
      disp_uint2(*MIN_NOW, 1, 0xa, 0);
      disp_uint5(*HOUR_TOTAL, 3, 3, 0);
      disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case9 产品版本信息 (0x9D86-0x9E14)，MENU==9 ---------- */
  if (*MENU == 9) {
    if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 7;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x6514 - 0x3c, 0, 0, 0);
      disp_string(0x6514 - 0x28, 1, 0, 0);
      disp_string(0x6514 - 0x14, 2, 0, 0);
      disp_string(0x6514, 3, 0, 1);
      return;
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x3a98) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case5A 电流手动平衡 (0x9E14-0x9FB8)，MENU==0x5a ---------- */
  if (*MENU == 0x5a) {
    *((volatile uint8_t*)0x100015ce) = 1;
    /* BAL_ANG 增（key2/0x16 上限 0xc7=199）/ 减（key3/0x21 下限 2） */
    if (key == 2 || key == 0x16) {
      *TIMEOUT = 0;
      (*BAL_ANG)++;
      if (*BAL_ANG > 0xc7) *BAL_ANG = 0xc7;
      disp_signed_angle(*BAL_ANG, 2, 7, 1);
    }
    if (key == 3 || key == 0x21) {
      *TIMEOUT = 0;
      if (*BAL_ANG < 2) *BAL_ANG = 2;
      (*BAL_ANG)--;
      disp_signed_angle(*BAL_ANG, 2, 7, 1);
    }
    /* 运行状态行显示（与 case8 相同） */
    if (*FAULT != 0) {
      disp_string(0x47dc, 3, 0xa, 0);
    } else {
      if (*RUN == 0 && *STATUS != 1) { *STATUS = 1; disp_string(0x47dc + 0xc, 3, 0xa, 0); }
      if (*RUN == 1 && *STATUS != 2) { *STATUS = 2; disp_string(0x47dc + 0x14, 3, 0xa, 0); }
    }
    if (key == 5) { *RUN = 1; *TIMEOUT = 0; }
    if (key == 6) { *RUN = 0; *TIMEOUT = 0; }
    run_stop_preset();
    if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 8;
      disp_clear();
      disp_string((int)0xa130, 0, 0, 1);
      disp_string((int)0xa140, 1, 0, 0);
      disp_string((int)0xa140, 2, 0, 0);
      disp_string((int)0xa140, 3, 0, 0);
      if (*BAL_ANG != *((volatile uint8_t*)0x10001695)) {  /* 写 EEPROM reg 0x1c */
        *((volatile uint8_t*)0x10001695) = *BAL_ANG;
        i2c_write_reg(*((volatile uint8_t*)0x10001695), 0x1c);
      }
      *RUN = 0;
      run_stop_preset();
      *((volatile uint8_t*)0x100015ce) = 0;
      return;
    }
    /* 超时尾 */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x3a98) {
      *TIMEOUT = 0;
      *RUN = 0;
      *MENU = 1;
      disp_splash_screen();
      *((volatile uint8_t*)0x100015ce) = 0;
    }
    return;
  }

  /* =====================================================================*/
  /* ---------- caseC 运行时间清零/查询 (0x9FB8-0xA04E)，MENU==0xc ---------- */
  if (*MENU == 0xc) {
    if (key == 4) { *MENU = 1; disp_splash_screen(); return; }
    if (key == 0x17) {
      *HOUR_NOW = 0;
      *MIN_NOW = 0;
      *HOUR_TOTAL = 0;
      *MIN_TOTAL = 0;
      disp_uint5(*HOUR_NOW, 1, 3, 0);
      disp_uint2(*MIN_NOW, 1, 0xa, 0);
      disp_uint5(*HOUR_TOTAL, 3, 3, 0);
      disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case14 运行状态监控页 (0xA04E-0xA2C8)，MENU==0x14 ---------- */
  if (*MENU == 0x14) {
    if (key == 4) { *MENU = 1; disp_splash_screen(); return; }
    /* 每 0xfa 次刷新状态行（标题 + 各故障位对应状态串） */
    (*TIMEOUT3)++;
    if (*TIMEOUT3 > 0xfa) {
      *TIMEOUT3 = 0;
      disp_string((int)0xa158, 0, 4, 0);
      if (*FAULT == 0) {
        disp_string((int)0xa164, 2, 0, 0);
      } else {
        if (*FAULT & 0x4)   disp_string((int)0xa178, 2, 0, 0);
        if (*FAULT & 0x2)   disp_string((int)0xa178, 2, 0, 0);
        if (*FAULT & 0x1)   disp_string((int)0xa178, 2, 0, 0);
        if (*FAULT & 0x8)   disp_string((int)0xa578, 2, 0, 0);
        if (*FAULT & 0x10)  disp_string((int)0xa590, 2, 0, 0);
        if (*FAULT & 0x20)  disp_string((int)0xa5a4, 2, 0, 0);
        if (*FAULT & 0x40)  disp_string((int)0xa5b8, 2, 0, 0);
        if (*FAULT & 0x80)  disp_string((int)0xa5cc, 2, 0, 0);
        if (*FAULT & 0x100) disp_string((int)0xa5e0, 2, 0, 0);
        if (*FAULT & 0x200) disp_string((int)0xa5f4, 2, 0, 0);
        if (*FAULT & 0x400) disp_string((int)0xa608, 2, 0, 0);
        if (*FAULT & 0x800) disp_string((int)0xa61c, 2, 0, 0);
        if (*FAULT & 0x1000) disp_string((int)0xa630, 2, 0, 0);
        if (*FAULT & 0x4000) disp_string((int)0xa644, 2, 0, 0);
        if (*FAULT & 0x8000) disp_string((int)0xa658, 2, 0, 0);
        if (*FAULT & 0x2000) disp_string((int)0xa66c, 2, 0, 0);
      }
    }
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case6 运行时间查询 + 初始参数密码 (0x8C1A-0x910C)，MENU==6 ---------- */
  if (*MENU == 6) {
    if (key == 1) {
      /* 输入前密码 PWD_B（0x100015e6）：逐位校验 PWD_BUF(0x100015f2) */
      *MENU2 = 0;
      while (*MENU2 < 6) {
        if (PWD_BUF[*MENU2] == PWD_B[*MENU2]) {
          PWD_BUF[*MENU2] = 0;
          (*MENU2)++;
        } else {
          /* 密码错：显示 '密码错' 标题（0x56dc）+ 延时 3 段 → 回主菜单 4 行 */
          disp_clear();
          disp_string(0x56dc, 1, 4, 0);
          sm6_delay_loop();
          *MENU = 2;
          *MENU2 = 3;
          disp_clear();
          disp_string(0x4814, 0, 0, 0);
          disp_string(0x4814 + 0x10, 1, 0, 0);
          disp_string(0x4814 + 0x20, 2, 0, 0);
          disp_string(0x4814 + 0x30, 3, 0, 1);
          return;
        }
      }
      /* 密码对：提示 0x8f6c → 延时 → 0x8f78 → 延时 → 0x8f88 → 延时 → 清 EEPROM reg5/6 → 死等 */
      disp_clear();
      disp_string((int)0x8f6c, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f78, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f88, 1, 0, 0);
      sm6_delay_loop();
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      for (;;) {}   /* 0x8DEE 死等（系统重置进入初始参数） */
    }
    else if (key == 0xe) {
      /* 初始密码 PWD_C（0x100015ec）：校验 PWD_BUF */
      *MENU2 = 0;
      while (*MENU2 < 6) {
        if (PWD_BUF[*MENU2] == PWD_C[*MENU2]) {
          PWD_BUF[*MENU2] = 0;
          (*MENU2)++;
        } else {
          disp_clear();
          disp_string(0x56dc, 1, 4, 0);
          sm6_delay_loop();
          *MENU = 2;
          *MENU2 = 3;
          disp_clear();
          disp_string(0x4814, 0, 0, 0);
          disp_string(0x4814 + 0x10, 1, 0, 0);
          disp_string(0x4814 + 0x20, 2, 0, 0);
          disp_string(0x4814 + 0x30, 3, 0, 1);
          return;
        }
      }
      /* 密码对：清 EEPROM reg5/6/7/8 → 死等 */
      disp_clear();
      disp_string((int)0x8f9c, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f78, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f88, 1, 0, 0);
      sm6_delay_loop();
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      i2c_write_reg(0, 7);
      i2c_write_reg(0, 8);
      for (;;) {}
    }
    else if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 3;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x4814, 0, 0, 0);
      disp_string(0x4814 + 0x10, 1, 0, 0);
      disp_string(0x4814 + 0x20, 2, 0, 0);
      disp_string(0x4814 + 0x30, 3, 0, 1);
      return;
    }
    else if (key > 0) {
      /* 密码数字输入（key==其它正值都当数字）——key<=0 走超时尾 */
      if (*MENU2 < 6) {
        PWD_BUF[*MENU2] = key;
        disp_render_char8(0x2a, 1, (uint8_t)(*MENU2 + 7), 0);
        (*MENU2)++;
      }
      return;
    }
    /* 超时尾（0x1388=5000） */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
    return;
  }

  /* =====================================================================*/
  /* ---------- case7 PID 参数设置 (0x910C-0x9A84)，MENU==7 ---------- */
  /* 局部字节访问宏：PID 槽与 PID_MODE(0x10001710) 相邻且全程 ldrb，
   * 必须按 byte 宽访问，否则 word 读写会污染相邻槽（PID_MODE 宏是 word，勿用）。 */
#define SM7B(addr) (*(volatile uint8_t*)(addr))
  if (*MENU == 7) {
    (*TIMEOUT3)++;
    /* key==1 编辑/浏览切换：MENU3 在 0/1 间翻转，随之调整 O 刷新节奏 */
    if (key == 1) {
      *TIMEOUT = 0;
      (*MENU3)++;
      if (*MENU3 > 1) *MENU3 = 0;
      if (*MENU3 == 0) *TIMEOUT3 = 0xfb;
      if (*MENU3 == 1) *TIMEOUT3 = 0x1f5;
    }
    /* key==4 回主菜单：当前 PID 模式槽复制到显示缓冲 0x1000170e/0x1000170f */
    else if (key == 4) {
      *TIMEOUT = 0;
      *MENU = 2;
      *MENU2 = 4;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x64d8, 0, 0, 1);
      disp_string(0x64d8 + 0x14, 1, 0, 0);
      disp_string(0x64d8 + 0x28, 2, 0, 0);
      disp_string(0x64d8 + 0x3c, 3, 0, 0);
      if (SM7B(0x10001710) == 1) { SM7B(0x1000170e) = SM7B(0x10001711); SM7B(0x1000170f) = SM7B(0x10001712); }
      if (SM7B(0x10001710) == 2) { SM7B(0x1000170e) = SM7B(0x10001713); SM7B(0x1000170f) = SM7B(0x10001714); }
      if (SM7B(0x10001710) == 3) { SM7B(0x1000170e) = SM7B(0x10001715); SM7B(0x1000170f) = SM7B(0x10001716); }
      if (SM7B(0x10001710) == 4) { SM7B(0x1000170e) = SM7B(0x10001717); SM7B(0x1000170f) = SM7B(0x10001718); }
    }
    /* key==2/3/0x16/0x21：*MENU3==1 编辑，否则 (key==2/3 && PIDMODE==4) 导航 */
    if (key == 2 || key == 0x16 || key == 3 || key == 0x21) {
      if (*MENU3 == 1) {
        *TIMEOUT3 = 0xfb;
        /* ---------- 编辑 ---------- */
        *TIMEOUT = 0;
        if (key == 3 || key == 0x21) {
          /* 降方向（0x933C-0x94E8） */
          if (*MENU2 == 0) { SM7B(0x10001710)++; if (SM7B(0x10001710) >= 4) SM7B(0x10001710) = 4; }
          if (*MENU2 == 1) { if (SM7B(0x10001710) == 4) { if (SM7B(0x10001717) > 1) SM7B(0x10001717)--; } }
          if (*MENU2 == 2) { if (SM7B(0x10001710) == 4) { if (SM7B(0x10001718) > 1) SM7B(0x10001718)--; } }
          if (*MENU2 == 4) { if (SM7B(0x10001722) > 1) SM7B(0x10001722)--; }
          if (*MENU2 == 5) { if (SM7B(0x10001723) > 1) SM7B(0x10001723)--; }
          if (*MENU2 == 6) { if (SM7B(0x10001724) > 1) SM7B(0x10001724)--; }
          if (*MENU2 == 7) { if (SM7B(0x10001725) > 1) SM7B(0x10001725)--; }
          if (*MENU2 == 8) { if (SM7B(0x10001726) > 1) SM7B(0x10001726)--; }
        } else {
          /* 升方向（0x94EA-0x960E） */
          if (*MENU2 == 0) { if (SM7B(0x10001710) > 1) SM7B(0x10001710)--; }
          if (*MENU2 == 1) { if (SM7B(0x10001710) == 4) { SM7B(0x10001717)++; if (SM7B(0x10001717) >= 0x80) SM7B(0x10001717) = 0x80; } }
          if (*MENU2 == 2) { if (SM7B(0x10001710) == 4) { SM7B(0x10001718)++; if (SM7B(0x10001718) >= 0x80) SM7B(0x10001718) = 0x80; } }
          if (*MENU2 == 4) { SM7B(0x10001722)++; if (SM7B(0x10001722) >= 0xfa) SM7B(0x10001722) = 0xfa; }
          if (*MENU2 == 5) { SM7B(0x10001723)++; if (SM7B(0x10001723) > SM7B(0x10001722)) SM7B(0x10001723) = SM7B(0x10001722); }
          if (*MENU2 == 6) { SM7B(0x10001724)++; if (SM7B(0x10001724) >= 0xfa) SM7B(0x10001724) = 0xfa; }
          if (*MENU2 == 7) { SM7B(0x10001725)++; if (SM7B(0x10001725) > SM7B(0x10001724)) SM7B(0x10001725) = SM7B(0x10001724); }
          if (*MENU2 == 8) { SM7B(0x10001726)++; if (SM7B(0x10001726) > SM7B(0x10001725)) SM7B(0x10001726) = SM7B(0x10001725); }
        }
      }
      else if ((key == 2 || key == 3) && SM7B(0x10001710) == 4) {
        /* ---------- 导航（0x921E-0x931A）：切换子项页标题 ---------- */
        *TIMEOUT = 0;
        *TIMEOUT3 = 0xfb;
        if (key == 3) { (*MENU2)++; if (*MENU2 > 8) *MENU2 = 8; }
        if (key == 2) { if (*MENU2 > 0) (*MENU2)--; }
        if (*MENU2 < 4) {
          disp_string(0x6aa4, 0, 0, 0);
          disp_string(0x6aa4 + 0x14, 1, 0, 0);
          disp_string(0x6aa4 + 0x28, 2, 0, 0);
          disp_string(0x6aa4 + 0x3c, 3, 0, 0);
        }
        if (*MENU2 >= 4 && *MENU2 < 8) {
          disp_string((int)0x9400, 0, 0, 0);
          disp_string((int)0x9414, 1, 0, 0);
          disp_string((int)0x9428, 2, 0, 0);
          disp_string((int)0x943c, 3, 0, 0);
        }
        if (*MENU2 >= 8 && *MENU2 < 0xc) {
          disp_string((int)0x9450, 0, 0, 0);
          disp_string(0x5ba4, 1, 0, 0);
          disp_string(0x5ba4, 2, 0, 0);
          disp_string(0x5ba4, 3, 0, 0);
        }
      }
    }
    /* ---------- 刷新区（0x9614-0x9984）：TIMEOUT3 计到 0xfb 时按 MENU2 重绘 ---------- */
    if (*TIMEOUT3 == 0xfb) {
      if (*MENU2 < 4) {
        /* 模式名 row0 col0xb（PIDMODE 1-4 → 0x6af8 基） */
        if (*MENU2 == 0) {
          if (SM7B(0x10001710) == 1) disp_string(0x6af8, 0, 0xb, 1);
          if (SM7B(0x10001710) == 2) disp_string(0x6af8 + 0x10, 0, 0xb, 1);
          if (SM7B(0x10001710) == 3) disp_string(0x6af8 + 0x1c, 0, 0xb, 1);
          if (SM7B(0x10001710) == 4) disp_string(0x6af8 + 0x2c, 0, 0xb, 1);
        } else {
          if (SM7B(0x10001710) == 1) disp_string(0x6af8, 0, 0xb, 0);
          if (SM7B(0x10001710) == 2) disp_string(0x6af8 + 0x10, 0, 0xb, 0);
          if (SM7B(0x10001710) == 3) disp_string(0x6af8 + 0x1c, 0, 0xb, 0);
          if (SM7B(0x10001710) == 4) disp_string(0x6af8 + 0x2c, 0, 0xb, 0);
        }
        /* P 值 row1（PIDMODE 1-4 槽 = 0x10001711/13/15/17） */
        if (*MENU2 == 1) {
          if (SM7B(0x10001710) == 1) disp_uint4(SM7B(0x10001711), 1, 0xb, 1);
          if (SM7B(0x10001710) == 2) disp_uint4(SM7B(0x10001713), 1, 0xb, 1);
          if (SM7B(0x10001710) == 3) disp_uint4(SM7B(0x10001715), 1, 0xb, 1);
          if (SM7B(0x10001710) == 4) disp_uint4(SM7B(0x10001717), 1, 0xb, 1);
        } else {
          if (SM7B(0x10001710) == 1) disp_uint4(SM7B(0x10001711), 1, 0xb, 0);
          if (SM7B(0x10001710) == 2) disp_uint4(SM7B(0x10001713), 1, 0xb, 0);
          if (SM7B(0x10001710) == 3) disp_uint4(SM7B(0x10001715), 1, 0xb, 0);
          if (SM7B(0x10001710) == 4) disp_uint4(SM7B(0x10001717), 1, 0xb, 0);
        }
        /* I 值 row2（PIDMODE 1-4 槽 = 0x10001712/14/16/18） */
        if (*MENU2 == 2) {
          if (SM7B(0x10001710) == 1) disp_uint4(SM7B(0x10001712), 2, 0xb, 1);
          if (SM7B(0x10001710) == 2) disp_uint4(SM7B(0x10001714), 2, 0xb, 1);
          if (SM7B(0x10001710) == 3) disp_uint4(SM7B(0x10001716), 2, 0xb, 1);
          if (SM7B(0x10001710) == 4) disp_uint4(SM7B(0x10001718), 2, 0xb, 1);
        } else {
          if (SM7B(0x10001710) == 1) disp_uint4(SM7B(0x10001712), 2, 0xb, 0);
          if (SM7B(0x10001710) == 2) disp_uint4(SM7B(0x10001714), 2, 0xb, 0);
          if (SM7B(0x10001710) == 3) disp_uint4(SM7B(0x10001716), 2, 0xb, 0);
          if (SM7B(0x10001710) == 4) disp_uint4(SM7B(0x10001718), 2, 0xb, 0);
        }
        /* MENU2<4 仅 mode/P/I 三行（PI 控制无 D 槽）。MENU2==3 无值可显，走下行 inv=0 */
        /* （反汇编 cmp *MENU2,#4 / blt 0x994e 确认：MENU2<4 全送 <8→0x9984，无 row3 绘制） */
      } else if (*MENU2 < 8) {
        /* 增益子项 row0-3（0x10001722-25） */
        if (*MENU2 == 4) disp_uint4(SM7B(0x10001722), 0, 0xb, 1);
        else           disp_uint4(SM7B(0x10001722), 0, 0xb, 0);
        if (*MENU2 == 5) disp_uint4(SM7B(0x10001723), 1, 0xb, 1);
        else           disp_uint4(SM7B(0x10001723), 1, 0xb, 0);
        if (*MENU2 == 6) disp_uint4(SM7B(0x10001724), 2, 0xb, 1);
        else           disp_uint4(SM7B(0x10001724), 2, 0xb, 0);
        if (*MENU2 == 7) disp_uint4(SM7B(0x10001725), 3, 0xb, 1);
        else           disp_uint4(SM7B(0x10001725), 3, 0xb, 0);
      } else if (*MENU2 >= 8 && *MENU2 < 0xc) {
        /* 增益子项 row0（0x10001726） */
        if (*MENU2 == 8) disp_uint4(SM7B(0x10001726), 0, 0xb, 1);
        else           disp_uint4(SM7B(0x10001726), 0, 0xb, 0);
      }
    }
    /* ---------- 超时清高亮（0x9984-0x9A56） ---------- */
    if (*TIMEOUT3 > 0x1f4) {
      *TIMEOUT3 = 0;
      if (*MENU3 == 0) return;                 /* b.w 0x4ba8 回到 case1 主界面 */
      /* 按 MENU2 清当前行（0x6474 空格，col0xb） */
      if (*MENU2 == 0) disp_string(0x6474, 0, 0xb, 0);
      if (*MENU2 == 1) disp_string(0x6474, 1, 0xb, 0);
      if (*MENU2 == 2) disp_string(0x6474, 2, 0xb, 0);
      if (*MENU2 == 3) disp_string(0x6474, 3, 0xb, 0);
      if (*MENU2 == 4) disp_string(0x6474, 0, 0xb, 0);
      if (*MENU2 == 5) disp_string(0x6474, 1, 0xb, 0);
      if (*MENU2 == 6) disp_string(0x6474, 2, 0xb, 0);
      if (*MENU2 == 7) disp_string(0x6474, 3, 0xb, 0);
      if (*MENU2 == 8) disp_string(0x6474, 0, 0xb, 0);
    }
    /* ---------- 超时尾（0x9A56-0x9A82，0xc350=50000） ---------- */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0xc350) {
      *TIMEOUT = 0;
      *MENU = 1;
      disp_splash_screen();
    }
    return;
  }
#undef SM7B

  /* ------------------------------------------------------------------
   * case1E 运行主界面 (0xA2C8-0xAB44)，MENU==0x1e
   * 主界面：运行状态显示 + 故障/启停/急停 + 手动幅值调节。
   * 逐段对照 tools/_sm_case1E_0xA2C8_0xAB46.txt 翻译（零臆造；字符串直传原地址）。
   * ---------------------------------------------------------------- */
  if (*MENU == 0x1e) {
    volatile uint8_t  *s17  = (volatile uint8_t*)0x10001785;  /* 运行状态位(STAT2 类) */
    volatile uint8_t  *s656 = (volatile uint8_t*)0x10001656;  /* 启动使能位 */
    volatile uint32_t *v98  = (volatile uint32_t*)0x10001598;
    volatile uint32_t *v9c  = (volatile uint32_t*)0x1000159c;
    volatile uint32_t *va0  = (volatile uint32_t*)0x100015a0;
    volatile uint32_t *v5d4 = (volatile uint32_t*)0x100015d4;

    /* ---- key==4 回主菜单（0xA2D0-A2E4） ---- */
    if (key == 4) { *MENU = 1; disp_splash_screen(); *TIMEOUT = 0; return; }

    /* ---- key==1 且当前停机 → 初始参数屏（0xA2E6-A32C） ---- */
    if (key == 1 && *RUN == 0) {
      disp_clear();
      *MENU = 0xa; *MENU2 = 0; *TIMEOUT = 0; *TIMEOUT2 = 0x3c; *IDLE = 0;
      disp_string((int)0x4d9c, 1, 0, 0);
      disp_string((int)0x4d9c + 0x10, 3, 7, 0);
      return;
    }

    /* ---- IDLE 周期刷新 + 继电器/模式切换（0xA32E-A43C） ---- */
    (*IDLE)++;
    if (*IDLE >= 0x15e) {
      *IDLE = 0;
      disp_uint4(*v98, 0, 9, 0);   /* 0x10001598 */
      disp_uint4(*v9c, 1, 9, 0);   /* 0x1000159c */
      disp_uint4(*va0, 2, 9, 0);   /* 0x100015a0 */
      if (*FAULT != 0) {
        *STATUS = 0;
        disp_string((int)0x47dc, 3, 0xa, 0);
      } else if (*RUN == 0 && *STATUS != 1) {
        *STATUS = 1;
        disp_string((int)0x47dc + 0xc, 3, 0xa, 0);
      }
      if (*CTRL_MODE == 0) {
        if (*DISP_MODE != 1) { *DISP_MODE = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x20, 3, 0, 0); }  /* 0xA3B0/B8：CTRL_MODE!=0 或 DISP_MODE==1 时跳过 */
      }
      if (*CTRL_MODE == 1) {
        if (*DISP_MODE != 2) { *DISP_MODE = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x47dc + 0x28, 3, 0, 0); }
      }
      if (*CTRL_MODE == 2) {
        if (*DISP_MODE != 3) { *DISP_MODE = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x30, 3, 0, 0); }
      }
    }

    /* ---- 状态机：复位按键去抖 → 故障停机斜坡 / STOP 段（0xA43C-A708） ---- */
    *DB_117 = debounce_p117();
    if (*FAULT != 0 && *DB_117 == 2) {
      if (*RESET_MODE == 0) {
        *FAULT = 0; *RUN = 0; *STOP_PEND = 1; *STOP_REQ = 0;
        disp_string((int)0x522c, 3, 0xa, 0);
        *LATCH_OUT = 0;
        for (;;) { *LATCH_IN = 0; do { (*LATCH_IN)++; } while (*LATCH_IN < 0x7d0); wd_feed(); (*LATCH_OUT)++; if (*LATCH_OUT >= 0xbb8) break; }
        disp_string((int)0x522c + 0x10, 3, 0xa, 0);
        *LATCH_OUT = 0;
        for (;;) { *LATCH_IN = 0; do { (*LATCH_IN)++; } while (*LATCH_IN < 0x7d0); wd_feed(); (*LATCH_OUT)++; if (*LATCH_OUT >= 0xbb8) break; }
        for (;;) { }              /* 0xA50A 故障停机后锁定，等看门狗复位 */
      }
    }
    if (*RESET_MODE == 1) {
      if (*DB_117 != 2 && *DISP_MODE != 1) { *DISP2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x20, 3, 0, 0); }
      if (*DB_117 == 2 && *DISP_MODE != 2) { *DISP2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x4804, 3, 0, 0); }
    }
    if (*RESET_MODE == 2) {
      if (*DB_117 != 2) *RESET2 = 0;
      if (*DB_117 == 2) *RESET2 = 1;
    }
    *DB_117 = debounce_p06();     /* 急停去抖覆盖 DB_117 */

    /* ---- 故障/急停/启停逻辑（0xA710-A96C） ---- */
    if (*FAULT == 0 && *DB_117 == 2 && *ESTOP == 0) {
      *RUN_REQ = 1; *RUN = 0; *STOP_PEND = 1; *STOP_REQ = 0;
      if (*STAT1 == 0) { disp_string((int)0x4804 - 0x1c, 3, 0xa, 0); *STAT1 = 1; }
      return;
    }
    if (*ESTOP == 1) {
      if (*DB_117 != 2 && *DISP_MODE != 1) { *DISP2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x4804 - 0x8, 3, 0, 0); }
      if (*DB_117 == 2 && *DISP_MODE != 2) { *DISP2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x4804, 3, 0, 0); }
    }
    if (*ESTOP == 2) { if (*DB_117 != 2) *RESET2 = 0; if (*DB_117 == 2) *RESET2 = 1; }
    if (*RESET_MODE != 2 && *ESTOP != 2) *RESET2 = 0;
    *SCAN_STOP = scan_run_stop();
    if (*FAULT == 0 && *STOP_REQ == 0 && *s17 == 1 && *DISP_SEL == 0) {
      *STOP_REQ = 1; *STOP_PEND = 0; *RUN_REQ = 0; *RUN = 1; *STAT1 = 0;
      *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
      disp_string((int)0x4804 - 0x14, 3, 0xa, 0);
    }
    if (*FAULT == 0 && *STOP_PEND == 0 && *s17 == 0 && *DISP_SEL == 0) {  /* 0xA85E 块A停机（s17==0，无 key 要求） */
      *STOP_PEND = 1; *STOP_REQ = 0; *RUN = 0;
      disp_string((int)0x4804 - 0x1c, 3, 0xa, 0);
    }
    if (*RUN == 0 && *DISP_SEL != 0) *s17 = 0;
    if (*FAULT == 0 && *STOP_REQ == 0) {
      if (key == 5 || *SCAN_STOP == 7) {
        if (*s656 == 0 || (*SCAN_STOP == 7 && *s656 == 1 && *DISP_SEL != 0)) {
          *s17 = 1; *STOP_REQ = 1; *STOP_PEND = 0; *RUN_REQ = 0; *RUN = 1; *STAT1 = 0;
          *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
          disp_string((int)0x4804 - 0x14, 3, 0xa, 0);
        }
      }
    }
    if (*FAULT == 0 && *STOP_PEND == 0) {
      if (key == 6 || *SCAN_STOP == 8) {
        if (*s656 == 0 || (*SCAN_STOP == 8 && *s656 == 1 && *DISP_SEL != 0)) {
          *s17 = 0; *STOP_PEND = 1; *STOP_REQ = 0; *RUN = 0;
          disp_string((int)0x4804 - 0x1c, 3, 0xa, 0);
        }
      }
    }

    /* ---- 幅值/频率计算 + 手动调节（0xA96C-AA9A） ---- */
    if (*DISP_SEL == 0) {
      *TARGET = *FREQ;
      if (*CTRL_MODE == 0) *TARGET_AMP = (*FREQ * *V_RANGE) / 1000;
      if (*CTRL_MODE == 1) *TARGET_AMP = (*FREQ * *A_RANGE) / 1000;
      *V_AMP = *TARGET_AMP;
      *V_AMP2 = *TARGET_AMP;
    }
    if (*DISP_SEL == 1) *V_AMP = *V_AMP2;
    if (*DISP_SEL == 2) {
      if (key == 2 || key == 0x16) {
        (*MANUAL)++;
        if (*MANUAL >= 0x3e8) *MANUAL = 0x3e8;
        if (*MANUAL <= 0xa) *MANUAL = 0xa;
        disp_fixed_1dec(*MANUAL, 0, 9, 0);
      }
      if (key == 3 || key == 0x21) {
        if (*MANUAL <= 0xa) *MANUAL = 0x1;
        (*MANUAL)--;
        disp_fixed_1dec(*MANUAL, 0, 9, 0);
      }
      *TARGET = *MANUAL;
      if (*CTRL_MODE == 0) *v5d4 = (*MANUAL * *V_RANGE) / 1000;
      if (*CTRL_MODE == 1) *v5d4 = (*MANUAL * *A_RANGE) / 1000;
      *V_AMP = *v5d4;
      *V_AMP2 = *v5d4;
    }

    /* ---- 超时尾（0xAA9A，0x1388=5000） ---- */
    (*TIMEOUT)++;
    if (*TIMEOUT >= 0x1388) {
      *TIMEOUT = 0;
      *MENU = 1;
      disp_splash_screen();
    }
    return;
  }
}
