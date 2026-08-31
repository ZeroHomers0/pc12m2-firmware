/* =============================================================================
 * 07_state_machine.c — state_machine(0x4464) C 级还原（12 相 PC12M-2）
 * 目标B W1b：替换 firmware/stub.c 占位。依据 evidence/reverse/disassembly/
 *   functions/00004464_FUN_00004464.txt（12p 全量反汇编金标准）逐段还原；数据
 *   地址一律以反汇编字面量 SRAM 值为准，字符串参数（disp_string 第1参）直传原
 *   flash 地址（flash XIP 直读）。绝不臆造：每 case 均对照对应反汇编段。
 *
 * 函数：0x00004464-0xA854（UI 状态机主分发，MENU 驱动；12p，共 17 case）
 * 调用点：main() 主循环 state_machine(*key)
 * 分发链（顺序 if 级联，遇 return 即返回）：
 *   entry(0x4464)→case1(0x48a0)→caseA(0x514c)→case62(0x532a)→case63(0x5472)
 *   →case2(0x5e18)→case3(0x6840)→case4(0x7962)→case5(0x84a2)→case6(0x8948)
 *   →case7(0x8e18)→case8(0x97a8)→caseB(0x99da)→case9(0x9ab6)→case5A(0x9b44)
 *   →caseC(0x9d2a)→case14(0x9dc0)→case1E(0x9fae)→共享超时尾(0xa828)→0xa854。
 *   MENU 值→case：1→case1、0xa→caseA、0x62→case62、0x63→case63、2→case2、
 *   3→case3、4→case4、5→case5、6→case6、7→case7、8→case8、0xb→caseB、
 *   9→case9、0x5a→case5A、0xc→caseC、0x14→case14、0x1e→case1E。
 *   注意：case1E 的真实范围是 0x9fae-0xa854；0xa058 是 MENU!=0x1e 的 cmp/bne
 *   跳板，直接汇入共享超时尾 0xa828（含完整 RUN/STOP 状态机，与 case1 同套门控）。
 *
 * r4=key 语义：1=确认、2=DOWN/减、3=UP/加、4=SET/退出、5=启动、6=停机、
 *   0x16=快加、0x21=快减、0x17=统计清零、0xe=初始参数密码、数字键0-9输密码。
 *   key<=0 走各 case 超时/idle 分支。
 *
 * 与 6p(0x458C) 的关键差异：
 *   - case1 无超时尾：主运行屏驻留，幅值块 b 0x4978 直接 return。
 *   - 共享超时尾 0xa828-0xa854 仅 case1E 使用（(*TIMEOUT)++；>=0x1388 回主屏）。
 *   - 超时尾常量：case7=0xc350（唯一）、case8/case9/case5A=0x3a98、其余 0x1388。
 *   - case63 key==4 不回返（落公共尾部）；超时尾含 param_sync_live_to_eeprom()。
 *   - prelude 运行统计仅两层（MIN→HOUR 进位，无 6p 第三层）。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* ---- 依赖函数前向声明（签名与定义模块核实一致；形参名语义化，无行为影响） ---- */
void lcd_ctrl_line(int on);                               /* 02_lcd_display.c 显示控制行开关 */
void disp_clear(void);                                    /* 02_lcd_display.c */
void disp_render_char8(uint ch, char row, uint col, uint32_t attr); /* 单字符 */
void disp_string(int str_addr, uint32_t row, uint col, uint32_t attr);  /* 字符串 */
void disp_number3(int value, uint32_t row, int col, uint32_t attr);
void disp_uint4(uint value, uint32_t row, int col, uint32_t attr);
void disp_uint5(uint value, uint32_t row, int col, uint32_t attr);
void disp_number(int value, uint32_t row, int col, uint32_t attr);
void disp_signed_angle(int angle, uint32_t row, int col, uint32_t attr);
void disp_offset(uint offset, uint32_t row, int col, uint32_t attr);
void disp_uint2(uint value, uint32_t row, int col, uint32_t attr);
void disp_fixed_1dec(uint value, uint32_t row, int col, uint32_t attr);
void disp_splash_screen(void);                            /* 02_lcd_display.c */
void disp_screen_static(void);
void disp_screen_calib(void);
void param_sync_live_to_eeprom(void);                     /* 06_param_system.c */
void i2c_write_reg(uint32_t data, uint32_t reg_addr);      /* 04_i2c.c (data,reg) */
void wd_feed(void);                                       /* 01_startup.c */
void fio0_pin22_ctrl(int on);                             /* 09_output_stage.c */
void fio1_pin22_ctrl(int on);                             /* 09_output_stage.c */
void gpio_outputs_set(void);                              /* 09_output_stage.c */
void run_stop_preset(void);                               /* 09_output_stage.c */
void fio1_pin20_ctrl(int on);                             /* 10_relay_led.c */
void fio1_pin21_ctrl(int on);                             /* 10_relay_led.c */
void fio1_pin23_ctrl(int on);                             /* 10_relay_led.c */
void out_relay_p021(int on);                              /* 10_relay_led.c */
uint8_t debounce_p09(void);                               /* 03_input_debounce.c */
uint8_t debounce_p116(void);                              /* 03_input_debounce.c */
uint8_t debounce_p117(void);                              /* 03_input_debounce.c */
uint8_t debounce_p06(void);                               /* 03_input_debounce.c */
uint8_t scan_run_stop(void);                              /* 03_input_debounce.c */

/* ---- 数据指针（12p 真实 SRAM 地址，volatile 因被 ISR/去抖写入；宽度按反汇编 ldr/strb 区分） ---- */
#define MENU         ((volatile uint8_t*)0x10001724)
#define MENU2        ((volatile uint8_t*)0x10001725)
#define MENU3        ((volatile uint8_t*)0x10001726)   /* 编辑态/锁标志（case3/4/5/7/63 共用） */
#define SCAN_RS      ((volatile uint8_t*)0x10001727)  /* scan_run_stop 结果 7=启 8=停 */
#define FAULT_CODE   ((volatile uint32_t*)0x10001728)
#define TIMEOUT      ((volatile uint32_t*)0x10001744)
#define TIMEOUT2     ((volatile uint32_t*)0x10001740)
#define TIMEOUT3     ((volatile uint32_t*)0x10001758)  /* 页刷新节流计数 */
#define IDLE         ((volatile uint32_t*)0x10001748)
#define LCD_TOUT     ((volatile uint32_t*)0x10001750)
#define FAULT        ((volatile uint32_t*)0x1000161c)  /* 故障字（简报曾标 LATCH_OUT） */
#define CFG          ((volatile uint8_t*)0x10001620)   /* 运行标志 cfg_word */
#define RUN_REQ      ((volatile uint8_t*)0x1000175d)
#define RUN          ((volatile uint8_t*)0x1000175e)
#define STOP         ((volatile uint8_t*)0x1000175f)
#define SYNC         ((volatile uint8_t*)0x10001760)
#define STAT_FL      ((volatile uint8_t*)0x10001761)
#define RESET2       ((volatile uint8_t*)0x10001762)
#define RUN_EN       ((volatile uint8_t*)0x10001765)
#define DB           ((volatile uint8_t*)0x1000175c)   /* P1.17 去抖结果 */
#define STATUS       ((volatile uint8_t*)0x100015cc)   /* 状态行锁存 1=停 2=运 */
#define MODE_L       ((volatile uint8_t*)0x100015cd)   /* 模式行锁存 */
#define SCR_SET      ((volatile uint8_t*)0x100015ce)   /* 屏内置位 */
#define MODE_L2      ((volatile uint8_t*)0x100015cf)   /* 模式锁存2（RESET_MODE/ESTOP 用） */
#define CTRL_MODE    ((volatile uint8_t*)0x1000162c)
#define DISP_SEL     ((volatile uint8_t*)0x1000164d)
#define PAIR_MODE    ((volatile uint8_t*)0x1000164e)
#define ESTOP        ((volatile uint8_t*)0x1000164f)
#define RESET_MODE   ((volatile uint8_t*)0x10001650)
#define DELAY_OUT    ((volatile uint32_t*)0x10001614)  /* 延时循环外环计数 */
#define DELAY_IN     ((volatile uint32_t*)0x10001618)  /* 延时循环内环计数 */
#define TARGET       ((volatile uint32_t*)0x100015a8)
#define V_AMP        ((volatile uint32_t*)0x100015d0)
#define V_AMP2       ((volatile uint32_t*)0x100015b4)
#define MANUAL       ((volatile uint32_t*)0x100015d8)
#define V5D4         ((volatile uint32_t*)0x100015d4)  /* MANUAL 派生显示 */
#define DCALC        ((volatile uint32_t*)0x10001754)  /* 幅值计算值 */
#define HSRC         ((volatile uint32_t*)0x10001768)  /* 首页值源 */
#define TICK         ((volatile uint32_t*)0x10001600)
#define MIN_NOW      ((volatile uint32_t*)0x100015fc)
#define HOUR_NOW     ((volatile uint32_t*)0x100015f8)
#define HOUR_TOTAL   ((volatile uint32_t*)0x10001604)
#define MIN_TOTAL    ((volatile uint32_t*)0x10001608)
#define GAIN0        ((volatile uint32_t*)0x10001634)  /* V 档增益 */
#define GAIN1        ((volatile uint32_t*)0x10001630)  /* A 档增益 */
#define GAIN_COEF    ((volatile uint32_t*)0x10001638)
#define FAULT_DET_EN ((volatile uint8_t*)0x100016d5)
#define FAULT_CHK_CYCLE ((volatile uint32_t*)0x10001730)
#define MISS_EINT2   ((volatile uint32_t*)0x10001734)
#define MISS_EINT3   ((volatile uint32_t*)0x10001738)
#define MISS_ADC     ((volatile uint32_t*)0x1000173c)
#define EINT2_TICK   ((volatile uint8_t*)0x10002098)
#define EINT3_TICK   ((volatile uint8_t*)0x10002099)
#define ADC_TICK     ((volatile uint8_t*)0x1000209a)
#define HOUR_TOTAL_SH ((volatile uint32_t*)0x1000160c)
#define MIN_TOTAL_SH  ((volatile uint32_t*)0x10001610)
#define MANUAL_SH     ((volatile uint32_t*)0x100015dc)
#define COM_ADDR     ((volatile uint8_t*)0x100016f7)
#define BAUD_IDX     ((volatile uint32_t*)0x100016f8)
#define PARITY       ((volatile uint8_t*)0x100016fc)
#define COM_CHK      ((volatile uint8_t*)0x100016fd)
#define BAUD_TBL     ((volatile uint32_t*)0x1000179c)  /* SRAM word 表，BAUD_IDX 索引 */
#define PID_MODE     ((volatile uint8_t*)0x10001708)
#define PHASE_OFF    ((volatile uint32_t*)0x10001624)  /* 相位偏移（word） */
#define BAL_ANG      ((volatile uint8_t*)0x1000168c)
#define PWD_BUF      ((volatile uint8_t*)0x100015f2)
#define PWD_A        ((volatile uint8_t*)0x100015e0)
#define PWD_B        ((volatile uint8_t*)0x100015e6)
#define PWD_C        ((volatile uint8_t*)0x100015ec)

/* =============================================================================
 * sm6_delay_loop — 密码屏延时循环（caseA/case62/case6 共用；dump 中内联 8 处）。
 *   外层 *0x10001614 计数到 0x2710=10000，内层 *0x10001618 计数到 0x3e8=1000，
 *   每个外层进位喂一次狗（wd_feed@0x238）。逻辑全同。
 * ========================================================================== */
static void sm6_delay_loop(void)
{
    *DELAY_OUT = 0;
    for (;;) {
        *DELAY_IN = 0;
        do { (*DELAY_IN)++; } while (*DELAY_IN < 0x3e8);
        wd_feed();
        (*DELAY_OUT)++;
        if (*DELAY_OUT >= 0x2710) break;
    }
}

/* =============================================================================
 * sm3_draw_item — case3 当前项值渲染（12p 0x71A6-0x76D8 内嵌重绘块提取）：
 *   按项号 it 显示其值/枚举到 (row,0xb)。item0 附带按 CTRL_MODE 驱动 fio1
 *   P0.20/P0.21。attr=1 高亮当前项。所有值串地址与枚举宽度均经 12p dump 校验。
 *   读宽注意：item8/15 修改用 word(ldr/str)、此处显示用 byte(ldrb)。
 * ========================================================================== */
static void sm3_draw_item(uint32_t it, uint32_t row, uint32_t attr)
{
    volatile uint8_t  *b2c = (volatile uint8_t*)0x1000162c; /* CTRL_MODE */
    volatile uint8_t  *b44 = (volatile uint8_t*)0x10001644; /* item6 软起时间 */
    volatile uint8_t  *b45 = (volatile uint8_t*)0x10001645; /* item7 */
    volatile uint8_t  *b48 = (volatile uint8_t*)0x10001648; /* item8 (byte 显示) */
    volatile uint8_t  *b4c = (volatile uint8_t*)0x1000164c; /* item9 角度 */
    volatile uint8_t  *b4d = (volatile uint8_t*)0x1000164d; /* DISP_SEL */
    volatile uint8_t  *b4e = (volatile uint8_t*)0x1000164e; /* item11 配对标志 */
    volatile uint8_t  *b4f = (volatile uint8_t*)0x1000164f; /* ESTOP */
    volatile uint8_t  *b51 = (volatile uint8_t*)0x10001651; /* item13 */
    volatile uint8_t  *b52 = (volatile uint8_t*)0x10001652; /* item14 */
    volatile uint8_t  *b58 = (volatile uint8_t*)0x10001658; /* item15 (byte 显示) */
    volatile uint32_t *w30 = (volatile uint32_t*)0x10001630; /* item2 */
    volatile uint32_t *w34 = (volatile uint32_t*)0x10001634; /* item1 */
    volatile uint32_t *w38 = (volatile uint32_t*)0x10001638; /* item3 */
    volatile uint32_t *w3c = (volatile uint32_t*)0x1000163c; /* item5 限位 */
    volatile uint32_t *w40 = (volatile uint32_t*)0x10001640; /* item4 限位 */
    switch (it) {
        case 0:
            if (*b2c == 0) { disp_string((int)0x64d0, row, 0xb, attr); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
            else if (*b2c == 1) { disp_string((int)0x64d8, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
            else { disp_string((int)0x64e0, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
            break;
        case 1: disp_uint4(*w34, row, 0xb, attr); break;
        case 2: disp_uint4(*w30, row, 0xb, attr); break;
        case 3: disp_uint4(*w38, row, 0xb, attr); break;
        case 4: /* 限位：item1>=item4 时显示值+V 单位(0x7488="V")，否则 0x5f54 */
            if (*w34 >= *w40) { disp_uint4(*w40, row, 0xb, attr); disp_string((int)0x7488, row, 0xf, 0); }
            else disp_string((int)0x5f54, row, 0xb, attr);
            break;
        case 5: /* 限位：item2>=item5 时显示值+A 单位(0x7490="A")，否则 0x5f54 */
            if (*w30 >= *w3c) { disp_uint4(*w3c, row, 0xb, attr); disp_string((int)0x7490, row, 0xf, 0); }
            else disp_string((int)0x5f54, row, 0xb, attr);
            break;
        case 6: disp_uint4(*b44, row, 0xb, attr); break;
        case 7: disp_uint4(*b45, row, 0xb, attr); break;
        case 8: disp_number3(*b48, row, 0xb, attr); break;
        case 9: disp_signed_angle(*b4c, row, 0xb, attr); break; /* 0x116c */
        case 10:
            if (*b4d == 0) disp_string((int)0x78ac, row, 0xb, attr);
            else if (*b4d == 1) disp_string((int)0x78b4, row, 0xb, attr);
            else disp_string((int)0x78bc, row, 0xb, attr);
            break;
        case 11:
            if (*b4e == 0) disp_string((int)0x78c8, row, 0xb, attr);
            else disp_string((int)0x78d0, row, 0xb, attr);
            break;
        case 12:
            if (*b4f == 0) disp_string((int)0x5b2c, row, 0xb, attr);
            else if (*b4f == 1) disp_string((int)0x5b34, row, 0xb, attr);
            else disp_string((int)0x5b3c, row, 0xb, attr);
            break;
        case 13:
            if (*b51 == 0) disp_string((int)0x5f54, row, 0xb, attr);
            else disp_string((int)0x5f5c, row, 0xb, attr);
            break;
        case 14:
            if (*b52 == 0) disp_string((int)0x5f68, row, 0xb, attr);
            else disp_string((int)0x5f70, row, 0xb, attr);
            break;
        case 15: disp_number3(*b58, row, 0xb, attr); break;
    }
}

/* =============================================================================
 * sm3_draw_page — case3 整页值渲染：TIMEOUT3 计数到 0xFB 后重绘当前页全部 4 项值，
 *   当前项高亮(attr=1)、其余正常(attr=0)。原厂导航/编辑后仅重画标签会把值列清掉，
 *   靠此公共尾部整页重绘恢复。
 * ========================================================================== */
static void sm3_draw_page(uint32_t it)
{
    uint32_t page = it >> 2;
    uint32_t k;
    for (k = 0; k < 4; k++)
        sm3_draw_item((page << 2) + k, k, ((page << 2) + k) == it ? 1 : 0);
}

/* =============================================================================
 * sm4_draw_value — case4 单项目值渲染（12p 内嵌 0x7FB6-0x8316）：按项号 it 显示
 *   保护参数值+单位到 (row,0xb)。attr=1 高亮当前项。值串/单位地址全部从 dump 提取。
 *   项映射：0=过压0x100016b8(w) 1=过压时间0x100016bc(b) 2=欠压0x100016c0(w)
 *     3=欠压时间0x100016c4(b) 4=IF过载0x100016c8(w) 5=IF过载时间0x100016cc(b)
 *     6=CT过载0x100016d0(w) 7=CT过载时间0x100016d4(b) 8=缺相0x100016d5(b)
 *     9=三相平衡0x100016d6(b)。
 * ========================================================================== */
static void sm4_draw_value(uint32_t it, uint32_t row, uint32_t attr)
{
    volatile uint32_t *w_b8 = (volatile uint32_t*)0x100016b8; /* 过压 */
    volatile uint8_t  *b_bc = (volatile uint8_t*)0x100016bc;  /* 过压时间 */
    volatile uint32_t *w_c0 = (volatile uint32_t*)0x100016c0; /* 欠压 */
    volatile uint8_t  *b_c4 = (volatile uint8_t*)0x100016c4;  /* 欠压时间 */
    volatile uint32_t *w_c8 = (volatile uint32_t*)0x100016c8; /* IF过载 */
    volatile uint8_t  *b_cc = (volatile uint8_t*)0x100016cc;  /* IF过载时间 */
    volatile uint32_t *w_d0 = (volatile uint32_t*)0x100016d0; /* CT过载 */
    volatile uint8_t  *b_d4 = (volatile uint8_t*)0x100016d4;  /* CT过载时间 */
    volatile uint8_t  *b_d5 = (volatile uint8_t*)0x100016d5;  /* 缺相 */
    volatile uint8_t  *b_d6 = (volatile uint8_t*)0x100016d6;  /* 三相平衡 */
    switch (it) {
        case 0:
            if (*w_b8) { disp_uint4(*w_b8, row, 0xb, attr); disp_string(0x7488, row, 0xf, 0); }
            else disp_string(0x5f54, row, 0xb, attr);
            break;
        case 1: disp_uint4(*b_bc, row, 0xb, attr); break;
        case 2:
            if (*w_c0) { disp_uint4(*w_c0, row, 0xb, attr); disp_string(0x7488, row, 0xf, 0); }
            else disp_string(0x5f54, row, 0xb, attr);
            break;
        case 3: disp_uint4(*b_c4, row, 0xb, attr); break;
        case 4:
            if (*w_c8) { disp_uint4(*w_c8, row, 0xb, attr); disp_string(0x7490, row, 0xf, 0); }
            else disp_string(0x5f54, row, 0xb, attr);
            break;
        case 5: disp_uint4(*b_cc, row, 0xb, attr); break;
        case 6:
            if (*w_d0) { disp_uint4(*w_d0, row, 0xb, attr); disp_string(0x7490, row, 0xf, 0); }
            else disp_string(0x5f54, row, 0xb, attr);
            break;
        case 7: disp_uint4(*b_d4, row, 0xb, attr); break;
        case 8:
            if (*b_d5) disp_string(0x65d4, row, 0xb, attr);
            else disp_string(0x5f54, row, 0xb, attr);
            break;
        case 9:
            if (*b_d6 >= 0xa) { disp_uint4(*b_d6, row, 0xb, attr); disp_string(0x8638, row, 0xf, 0); }
            else disp_string(0x5f54, row, 0xb, attr);
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
    for (k = 0; k < n; k++)
        sm4_draw_value(start + k, k, (start + k) == it ? 1 : 0);
}

/* =============================================================================
 * sm5_draw_value — case5 通讯屏单行值渲染（it=0..3，row=it，attr=0/1 高亮）。
 *   槽：COM_ADDR=0x100016f7(byte) BAUD_IDX=0x100016f8(word)
 *        PARITY=0x100016fc(byte) COM_CHK=0x100016fd(byte)
 *   波特率表 BAUD_TBL=0x1000179c（SRAM word 数组，BAUD_IDX 索引）。
 *   显示函数：item0 disp_uint5@0xf3c；item1 查表值 disp_number@0x1042；
 *     item2/3 枚举串地址 0x65b8/0x65c0/0x65c8/0x8a64('1 ST0P')、0x5f54/0x65d4。
 * ========================================================================== */
static void sm5_draw_value(uint32_t it, uint32_t attr)
{
    switch (it) {
        case 0: /* 本机地址 COM_ADDR (byte) */
            disp_uint5(*COM_ADDR, 0, 0xb, attr); /* bl 0xf3c */
            break;
        case 1: /* 波特率：BAUD_TBL[*BAUD_IDX]（SRAM 表 0x1000179c，序号 word） */
            disp_number(BAUD_TBL[*BAUD_IDX], 1, 0xa, attr); /* bl 0x1042 */
            break;
        case 2: /* 校验位 PARITY (byte)，4 种校验名 */
            if (*PARITY == 0) disp_string(0x65b8, 2, 0xa, attr);
            else if (*PARITY == 1) disp_string(0x65c0, 2, 0xa, attr);
            else if (*PARITY == 2) disp_string(0x65c8, 2, 0xa, attr);
            else disp_string(0x8a64, 2, 0xa, attr); /* '1 ST0P' */
            break;
        case 3: /* 通讯校验 COM_CHK (byte) */
            if (*COM_CHK) disp_string(0x65d4, 3, 0xb, attr);
            else disp_string(0x5f54, 3, 0xb, attr);
            break;
    }
}

/* case5 通讯：整页重绘（4 项单页，高亮当前项 it） */
static void sm5_draw_page(uint32_t it)
{
    uint32_t k;
    for (k = 0; k < 4; k++) sm5_draw_value(k, (k == it) ? 1 : 0);
}

/* =============================================================================
 * state_machine — 12p UI 状态机（0x4464-0xa854）。
 * 入口：prelude（LCD 背光超时、EEPROM 同步、FAULT→FAULT_CODE 16 位映射、运行统计、
 *       故障检测周期）→ 17 个 case（二进制 if 级联，遇 return 即返回）。
 * ========================================================================== */
void state_machine(int key)
{
    /* ================= prelude (0x4464-0x48a0) =================
       每帧执行：LCD 背光计数；运行时间/手动值同步到 EEPROM（P0.9==1 时）；
       FAULT 位 → FAULT_CODE 顺序映射（非 else-if）+ 故障输出钳位；
       CFG>0 运行统计（TICK 0x7530 进位 MIN→HOUR，仅两层）；
       EINT2/EINT3/ADC 缺失检测（每 0x64 周期）。 */
    (*LCD_TOUT)++;
    if (key > 0) { *LCD_TOUT = 0; lcd_ctrl_line(1); }
    if (*LCD_TOUT > 0x1388) { *LCD_TOUT = 0; lcd_ctrl_line(0); }

    if (debounce_p09() == 1) {
        if (*HOUR_TOTAL != *HOUR_TOTAL_SH) {
            *HOUR_TOTAL_SH = *HOUR_TOTAL;
            i2c_write_reg((*HOUR_TOTAL_SH >> 8) & 0xff, 0x97);
            i2c_write_reg(*HOUR_TOTAL_SH & 0xff, 0x98);
        }
        if (*MIN_TOTAL != *MIN_TOTAL_SH) {
            *MIN_TOTAL_SH = *MIN_TOTAL;
            i2c_write_reg((*MIN_TOTAL_SH >> 8) & 0xff, 0x99);
            i2c_write_reg(*MIN_TOTAL_SH & 0xff, 0x9a);
        }
        if (*MANUAL != *MANUAL_SH) {
            *MANUAL_SH = *MANUAL;
            i2c_write_reg((*MANUAL_SH >> 8) & 0xff, 0x1d);
            i2c_write_reg(*MANUAL_SH & 0xff, 0x1e);
        }
        param_sync_live_to_eeprom();
    }

    if (*FAULT != 0) {
        /* FAULT 位 → FAULT_CODE 顺序 if（非 else-if），低编号优先覆盖 */
        if (*FAULT & 0x4)   *FAULT_CODE = 1;
        if (*FAULT & 0x2)   *FAULT_CODE = 1;
        if (*FAULT & 0x1)   *FAULT_CODE = 1;
        if (*FAULT & 0x8)   *FAULT_CODE = 2;
        if (*FAULT & 0x200) *FAULT_CODE = 3;
        if (*FAULT & 0x40)  *FAULT_CODE = 4;
        if (*FAULT & 0x400) *FAULT_CODE = 5;
        if (*FAULT & 0x10)  *FAULT_CODE = 6;
        if (*FAULT & 0x20)  *FAULT_CODE = 7;
        if (*FAULT & 0x100) *FAULT_CODE = 8;
        if (*FAULT & 0x80)  *FAULT_CODE = 9;
        if (*FAULT & 0x4000)*FAULT_CODE = 0xa;
        if (*FAULT & 0x8000)*FAULT_CODE = 0xb;
        if (*FAULT & 0x800) *FAULT_CODE = 0xc;
        if (*FAULT & 0x2000)*FAULT_CODE = 0xd;
        if (*FAULT & 0x1000)*FAULT_CODE = 0xe;
        fio0_pin22_ctrl(0); fio1_pin22_ctrl(0); out_relay_p021(1); fio1_pin23_ctrl(1);
        *RUN_EN = 0; *STOP = 1; *RUN = 0; *CFG = 0;
        *(volatile uint32_t*)0x10001fdc = 0;
        *(volatile uint32_t*)0x10002040 = 0; *(volatile uint32_t*)0x10002044 = 0;
        *(volatile uint32_t*)0x10002048 = 0; *(volatile uint32_t*)0x10001fe0 = 0;
        gpio_outputs_set();
    } else {
        out_relay_p021(0); fio1_pin23_ctrl(0); *FAULT_CODE = 0;
    }

    if (*CFG > 0) {
        fio0_pin22_ctrl(1); fio1_pin22_ctrl(1);
        (*TICK)++;
        if (*TICK > 0x7530) {
            *TICK = 0;
            (*MIN_NOW)++; (*MIN_TOTAL)++;
            if (*MIN_NOW   >= 0x3c) { *MIN_NOW   = 0; (*HOUR_NOW)++; }
            if (*MIN_TOTAL >= 0x3c) { *MIN_TOTAL = 0; (*HOUR_TOTAL)++; }
        }
    }

    *SYNC = debounce_p116();
    if (*FAULT == 0 && *SYNC == 2) *FAULT |= 0x4000;
    (*FAULT_CHK_CYCLE)++;
    if (*FAULT_CHK_CYCLE > 0x64) {
        *FAULT_CHK_CYCLE = 0;
        if (*EINT2_TICK == 0 && *FAULT_DET_EN > 0) {
            (*MISS_EINT2)++; if (*MISS_EINT2 == 5) { *MISS_EINT2 = 0; *FAULT |= 0x1; }
        } else { *MISS_EINT2 = 0; *FAULT &= ~0x1; }
        if (*EINT3_TICK == 0 && *FAULT_DET_EN > 0) {
            (*MISS_EINT3)++; if (*MISS_EINT3 == 5) { *MISS_EINT3 = 0; *FAULT |= 0x2; }
        } else { *MISS_EINT3 = 0; *FAULT &= ~0x2; }
        if (*ADC_TICK == 0 && *FAULT_DET_EN > 0) {
            (*MISS_ADC)++; if (*MISS_ADC == 5) { *MISS_ADC = 0; *FAULT |= 0x4; }
        } else { *MISS_ADC = 0; *FAULT &= ~0x4; }
        *EINT2_TICK = 0; *EINT3_TICK = 0; *ADC_TICK = 0;
    }

    /* ================= case1 运行主界面 (12p 0x48a0-0x514c)，MENU==1 =================
       注意：case1 无超时尾——主运行屏驻留，幅值块 b 0x4978 直接 return。
       key 分发：0x17+停机→运行时间(0xc)；1+停机→初始参数密码(0xa)；0xe→二级密码(0x62)；
       4+停机+故障→故障状态(0x14)。之后为 IDLE 周期刷新 + 完整 RUN/STOP 门控。 */
    if (*MENU == 1) {
        /* ---- key==0x17 且 CFG==0：运行时间屏 (0x48a8) ---- */
        if (key == 0x17 && *CFG == 0) {
            *MENU = 0xc; *MENU2 = 0; *TIMEOUT = 0;
            disp_clear();
            disp_string(0x4ca0, 0, 0, 0);
            disp_string(0x4cb4, 1, 0, 0);
            disp_string(0x4cc8, 2, 0, 0);
            disp_string(0x4cb4, 3, 0, 0);
            disp_uint5(*HOUR_NOW, 1, 3, 0);
            disp_uint2(*MIN_NOW, 1, 0xa, 0);
            disp_uint5(*HOUR_TOTAL, 3, 3, 0);
            disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
            return;
        }
        /* ---- key==1 且 CFG==0：初始参数密码屏 (0x4934) ---- */
        if (key == 1 && *CFG == 0) {
            *MENU = 0xa; *MENU2 = 0; *TIMEOUT = 0; *TIMEOUT2 = 0x3c; *IDLE = 0;
            disp_clear();
            disp_string(0x4cf4, 1, 0, 0);
            disp_string(0x4d04, 3, 7, 0);
            return;
        }
        /* ---- key==0xe：二级密码屏 (0x497a) ---- */
        if (key == 0xe) {
            *MENU = 0x62; *MENU2 = 0; *TIMEOUT = 0; *TIMEOUT2 = 0x3c; *IDLE = 0;
            disp_clear();
            disp_string(0x4d0c, 0, 0, 0);
            disp_string(0x4d20, 1, 0, 0);
            disp_string(0x4d04, 3, 7, 0);
            return;
        }
        /* ---- key==4 且 CFG==0 且 FAULT!=0：故障状态屏 (0x49ca) ---- */
        if (key == 4 && *CFG == 0 && *FAULT != 0) {
            *MENU = 0x14; *MENU2 = 0; *TIMEOUT = 0; *TIMEOUT3 = 0x1f4;
            disp_clear();
            return;
        }

        /* ---- IDLE 周期刷新 (0x49fa-0x4b3c)：每 0x15e 帧重绘幅值/电压/状态/模式行 ---- */
        (*IDLE)++;
        if (*IDLE >= 0x15e) {
            *IDLE = 0;
            if (*DISP_SEL == 0) disp_fixed_1dec(*HSRC, 0, 9, 0);
            if (*DISP_SEL == 1) disp_fixed_1dec(*TARGET, 0, 9, 0);
            if (*DISP_SEL == 2) disp_fixed_1dec(*MANUAL, 0, 9, 0);
            disp_uint4(*(volatile uint32_t*)0x10001590, 1, 9, 0);
            disp_uint4(*(volatile uint32_t*)0x10001594, 2, 9, 0);
            if (*FAULT != 0) { *STATUS = 0; disp_string(0x4334, 3, 0xa, 0); }
            else if (*CFG == 0 && *STATUS != 1) { *STATUS = 1; disp_string(0x4340, 3, 0xa, 0); }
            /* 模式行：CTRL_MODE 0/1/2 → 恒压/恒流/手动 */
            if (*CTRL_MODE == 0) {
                if (*MODE_L != 1) { *MODE_L = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string(0x4354, 3, 0, 0); }
            }
            if (*CTRL_MODE == 1) {
                if (*MODE_L != 2) { *MODE_L = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string(0x435c, 3, 0, 0); }
            }
            if (*CTRL_MODE == 2) {
                if (*MODE_L != 3) { *MODE_L = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); disp_string(0x4364, 3, 0, 0); }
            }
        }

        /* ---- 复位去抖 P1.17 (0x4b3c)：故障复位斜坡 + RESET_MODE 模式切换 ---- */
        *DB = debounce_p117();
        if (*FAULT != 0 && *DB == 2 && *RESET_MODE == 0) {
            *FAULT = 0; *CFG = 0; *STOP = 1; *RUN = 0;
            disp_string(0x4d6c, 3, 0xa, 0);
            *DELAY_OUT = 0;
            do { *DELAY_IN = 0; do { (*DELAY_IN)++; } while (*DELAY_IN < 0x7d0); wd_feed(); (*DELAY_OUT)++; } while (*DELAY_OUT < 0xbb8);
            disp_string(0x4d7c, 3, 0xa, 0);
            *DELAY_OUT = 0;
            do { *DELAY_IN = 0; do { (*DELAY_IN)++; } while (*DELAY_IN < 0x7d0); wd_feed(); (*DELAY_OUT)++; } while (*DELAY_OUT < 0xbb8);
            for (;;) { } /* 故障停机后锁定，等看门狗复位 */
        }
        if (*RESET_MODE == 1) {
            if (*DB != 2 && *MODE_L != 1) {
                *MODE_L2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
                disp_string(0x4354, 3, 0, 0);
            }
            if (*DB == 2 && *MODE_L != 2) {
                *MODE_L2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
                disp_string(0x435c, 3, 0, 0);
            }
        }
        if (*RESET_MODE == 2) {
            if (*DB != 2) *RESET2 = 0;
            if (*DB == 2) *RESET2 = 1;
        }

        /* ---- 急停去抖 P0.6 (0x4c66)：触发 → 停机并锁定 ---- */
        *DB = debounce_p06();
        if (*FAULT == 0 && *DB == 2 && *ESTOP == 0) {
            *RUN_REQ = 1; *CFG = 0; *STOP = 1; *RUN = 0;
            if (*STAT_FL == 0) { disp_string(0x4340, 3, 0xa, 0); *STAT_FL = 1; }
            return;
        }
        if (*ESTOP == 1) {
            if (*DB != 2 && *MODE_L != 1) {
                *MODE_L2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
                disp_string(0x4354, 3, 0, 0);
            }
            if (*DB == 2 && *MODE_L != 2) {
                *MODE_L2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
                disp_string(0x435c, 3, 0, 0);
            }
        }
        if (*ESTOP == 2) {
            if (*DB != 2) *RESET2 = 0;
            if (*DB == 2) *RESET2 = 1;
        }
        if (*RESET_MODE != 2 && *ESTOP != 2) *RESET2 = 0;

        *SCAN_RS = scan_run_stop();

        /* ---- 块A 持续运行 (0x4ebc)：run_en==1 且显示正常 → 强制 RUN ---- */
        if (*FAULT == 0 && *RUN == 0 && *RUN_EN == 1 && *DISP_SEL == 0) {
            *RUN = 1; *STOP = 0; *RUN_REQ = 0; *CFG = 1; *STAT_FL = 0;
            *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
            disp_string(0x4348, 3, 0xa, 0);
        }
        /* ---- 块B 持续停机 (0x4f0c)：run_en==0 且显示正常 → 强制 STOP ---- */
        if (*FAULT == 0 && *STOP == 0 && *RUN_EN == 0 && *DISP_SEL == 0) {
            *STOP = 1; *RUN = 0; *CFG = 0;
            disp_string(0x4340, 3, 0xa, 0);
        }
        if (*CFG == 0 && *DISP_SEL != 0) *RUN_EN = 0;

        /* ---- key5/扫描7 启动 (0x4f54) ---- */
        if (*FAULT == 0 && *RUN == 0) {
            if (key == 5 || *SCAN_RS == 7) {
                if (*PAIR_MODE == 0 || (*SCAN_RS == 7 && *PAIR_MODE == 1 && *DISP_SEL != 0)) {
                    *RUN_EN = 1; *RUN = 1; *STOP = 0; *RUN_REQ = 0; *CFG = 1; *STAT_FL = 0;
                    *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
                    disp_string(0x4348, 3, 0xa, 0);
                }
            }
        }
        /* ---- key6/扫描8 停机 (0x4fc2) ---- */
        if (*FAULT == 0 && *STOP == 0) {
            if (key == 6 || *SCAN_RS == 8) {
                if (*PAIR_MODE == 0 || (*SCAN_RS == 8 && *PAIR_MODE == 1 && *DISP_SEL != 0)) {
                    *RUN_EN = 0; *STOP = 1; *RUN = 0; *CFG = 0;
                    disp_string(0x4340, 3, 0xa, 0);
                }
            }
        }

        /* ---- 幅值/频率计算 (0x501a-0x5148)：DISP_SEL 0=自动 1=显示保持 2=手动 ---- */
        if (*DISP_SEL == 0) {
            *TARGET = *HSRC;
            if (*CTRL_MODE == 0) *DCALC = (*HSRC * *GAIN0) / 1000;
            if (*CTRL_MODE == 1) *DCALC = (*HSRC * *GAIN1) / 1000;
            *V_AMP = *DCALC; *V_AMP2 = *DCALC;
        }
        if (*DISP_SEL == 1) *V_AMP = *V_AMP2;
        if (*DISP_SEL == 2) {
            /* 手动幅值：key2/0x16 增（clamp 0x3e8/0xa）、key3/0x21 减（<=0xa 先置 1 再 --） */
            if (key == 2 || key == 0x16) {
                (*MANUAL)++;
                if (*MANUAL >= 0x3e8) *MANUAL = 0x3e8;
                if (*MANUAL <= 0xa)  *MANUAL = 0xa;
                disp_fixed_1dec(*MANUAL, 0, 9, 0);
            }
            if (key == 3 || key == 0x21) {
                if (*MANUAL <= 0xa) *MANUAL = 1;
                (*MANUAL)--;
                disp_fixed_1dec(*MANUAL, 0, 9, 0);
            }
            *TARGET = *MANUAL;
            if (*CTRL_MODE == 0) *V5D4 = (*MANUAL * *GAIN0) / 1000;
            if (*CTRL_MODE == 1) *V5D4 = (*MANUAL * *GAIN1) / 1000;
            *V_AMP = *V5D4; *V_AMP2 = *V5D4;
        }

        /* case1 无超时尾：主运行屏驻留，直接返回 (b 0x4978) */
        return;
    }

    /* ================= caseA 初始参数密码校验 (0x514c-0x532a)，MENU==0xa =================
       key==1 校验 PWD_A(0x100015e0)；成功→基本参数主菜单(MENU=2, disp_screen_static)，
       失败→"密码错"(0x5210)+延时+回主屏；key==4 退出；数字键逐位输入；idle 倒计时。 */
    if (*MENU == 0xa) {
        volatile uint32_t *t2 = (volatile uint32_t*)0x10001740; /* TIMEOUT2 */
        volatile uint32_t *idle = (volatile uint32_t*)0x10001748; /* IDLE */

        if (key == 1) {
            *MENU2 = 0;
            while (*MENU2 < 6) {
                if (PWD_BUF[*MENU2] == PWD_A[*MENU2]) {
                    PWD_BUF[*MENU2] = 0; (*MENU2)++;
                } else {
                    disp_clear();
                    disp_string(0x5210, 1, 4, 0);
                    sm6_delay_loop();
                    *MENU = 1; disp_splash_screen();
                    return;
                }
            }
            *MENU = 2; *MENU2 = 0;
            disp_screen_static();
            return;
        }
        if (key == 4) {
            *MENU = 1; disp_splash_screen();
            return;
        }
        if (key > 0) { /* 数字键逐位输入 */
            if (*MENU2 < 6) {
                PWD_BUF[*MENU2] = (uint8_t)key;
                disp_render_char8(0x2a, 1, (uint8_t)(*MENU2 + 7), 0);
                (*MENU2)++;
            }
            return;
        }
        /* key<=0：idle 倒计时，每 0x1f4 帧 TIMEOUT2--，到 0 回主屏 */
        (*idle)++;
        if (*idle >= 0x1f4) {
            *idle = 0;
            (*t2)--;
            disp_number3((uint8_t)(*t2), 3, 6, 0);
            if (*t2 == 0) { *MENU = 1; disp_splash_screen(); return; }
        }
        return;
    }

    /* ================= case62 二级密码校验 (0x532a-0x5472)，MENU==0x62 =================
       key==1 校验 PWD_C(0x100015ec)；成功→设置屏(MENU=0x63, disp_screen_calib)，
       失败→"密码错"+延时+回主屏；key==4 退出；数字键输入；idle 倒计时。 */
    if (*MENU == 0x62) {
        volatile uint32_t *t2 = (volatile uint32_t*)0x10001740;
        volatile uint32_t *idle = (volatile uint32_t*)0x10001748;

        if (key == 1) {
            *MENU2 = 0;
            while (*MENU2 < 6) {
                if (PWD_BUF[*MENU2] == PWD_C[*MENU2]) {
                    PWD_BUF[*MENU2] = 0; (*MENU2)++;
                } else {
                    disp_clear();
                    disp_string(0x5210, 1, 4, 0);
                    sm6_delay_loop();
                    *MENU = 1; disp_splash_screen();
                    return;
                }
            }
            *MENU = 0x63; *MENU2 = 0;
            disp_clear();
            disp_screen_calib();
            return;
        }
        if (key == 4) {
            *MENU = 1; disp_splash_screen();
            return;
        }
        if (key > 0) { /* 数字键逐位输入 */
            if (*MENU2 < 6) {
                PWD_BUF[*MENU2] = (uint8_t)key;
                disp_render_char8(0x2a, 1, (uint8_t)(*MENU2 + 7), 0);
                (*MENU2)++;
            }
            return;
        }
        /* key<=0：idle 倒计时 */
        (*idle)++;
        if (*idle >= 0x1f4) {
            *idle = 0;
            (*t2)--;
            disp_number3((uint8_t)(*t2), 3, 6, 0);
            if (*t2 == 0) { *MENU = 1; disp_splash_screen(); return; }
        }
        return;
    }

    /* ================= case63 设置屏 (0x5472-0x5e18)，MENU==0x63 =================
       锁标志 0x10001726=0 查看态 / =1 编辑态；光标 0x10001725(MENU2)=0..10。
       值映射(12p bin 逐条校验)：
         0-4=0x10001690/98/a0/a8/b0 (word，加 clamp 0x1194 / 减门控 >0xdac)
         5=0x1000164f | 6=0x10001650 (byte clamp 2) | 7=0x10001651 | 8=0x10001652
           | 9=0x10001653 (byte clamp 1) | 0xa=0x10001658 (word 加 clamp 0xb4 / 减 >0)
       显示：项5-9 为文字串；项0xa 显示按 ldrb 8位装入 disp_number3。
       key==4 保存不回返（落公共尾部）；超时尾含 param_sync_live_to_eeprom()。 */
    if (*MENU == 0x63) {
        volatile uint8_t  *m25 = (volatile uint8_t*)0x10001725; /* 光标/子项 */
        volatile uint8_t  *m26 = (volatile uint8_t*)0x10001726; /* 锁标志 0/1 */
        volatile uint32_t *w44 = (volatile uint32_t*)0x10001744; /* TIMEOUT */
        volatile uint32_t *w58 = (volatile uint32_t*)0x10001758; /* TIMEOUT3 */

        /* ---- key==1：锁标志 0/1 翻转并设刷新初值 (0x547a-0x54b4) ---- */
        if (key == 1) {
            *w44 = 0;
            (*m26)++;
            if (*m26 > 1) *m26 = 0;
            if (*m26 == 0) *w58 = 0xfa;  /* 锁开(查看)→ 慢刷 */
            if (*m26 == 1) *w58 = 0x1f4; /* 锁合(编辑)→ 快刷 */
        }

        /* ---- key==4：保存参数并退回主屏 (0x54b6-0x54ca)；12p 不回返，落公共尾部 ---- */
        if (key == 4) {
            *w44 = 0; param_sync_live_to_eeprom(); *MENU = 1; disp_splash_screen();
        }

        /* ---- key==2/3 且锁开：光标导航 + 页标题 (0x54ce-0x55d8) ---- */
        if (key == 2 || key == 3) {
            if (*m26 == 0) {
                *w44 = 0;
                if (key == 3) { (*m25)++; if (*m25 > 0xa) *m25 = 0xa; }
                if (key == 2) { if (*m25 > 0) (*m25)--; }
                if (*m25 < 4) {
                    disp_string(0x43ac, 0, 0, 0); disp_string(0x43c0, 1, 0, 0);
                    disp_string(0x43d4, 2, 0, 0); disp_string(0x43e8, 3, 0, 0);
                }
                if (*m25 >= 4 && *m25 < 8) {
                    disp_string(0x564c, 0, 0, 0); disp_string(0x5660, 1, 0, 0);
                    disp_string(0x5674, 2, 0, 0); disp_string(0x5688, 3, 0, 0);
                }
                if (*m25 >= 8 && *m25 < 0xc) {
                    disp_string(0x569c, 0, 0, 0); disp_string(0x56b0, 1, 0, 0);
                    disp_string(0x56c4, 2, 0, 0); disp_string(0x56d8, 3, 0, 0);
                }
                *w58 = 0xfa;
            }
        }

        /* ---- key∈{2,0x16,3,0x21} 且锁合：编辑当前项 (0x55de-0x598a) ---- */
        if (key == 2 || key == 0x16 || key == 3 || key == 0x21) {
            if (*m26 == 1) {
                if (key == 2 || key == 0x16) { /* 加 */
                    *w44 = 0;
                    if (*m25 == 0) { (*(volatile uint32_t*)0x10001690)++; if (*(volatile uint32_t*)0x10001690 > 0x1194) *(volatile uint32_t*)0x10001690 = 0x1194; }
                    if (*m25 == 1) { (*(volatile uint32_t*)0x10001698)++; if (*(volatile uint32_t*)0x10001698 > 0x1194) *(volatile uint32_t*)0x10001698 = 0x1194; }
                    if (*m25 == 2) { (*(volatile uint32_t*)0x100016a0)++; if (*(volatile uint32_t*)0x100016a0 > 0x1194) *(volatile uint32_t*)0x100016a0 = 0x1194; }
                    if (*m25 == 3) { (*(volatile uint32_t*)0x100016a8)++; if (*(volatile uint32_t*)0x100016a8 > 0x1194) *(volatile uint32_t*)0x100016a8 = 0x1194; }
                    if (*m25 == 4) { (*(volatile uint32_t*)0x100016b0)++; if (*(volatile uint32_t*)0x100016b0 > 0x1194) *(volatile uint32_t*)0x100016b0 = 0x1194; }
                    if (*m25 == 5) { (*((volatile uint8_t*)0x1000164f))++; if (*(volatile uint8_t*)0x1000164f > 2) *(volatile uint8_t*)0x1000164f = 2; }
                    if (*m25 == 6) { (*((volatile uint8_t*)0x10001650))++; if (*(volatile uint8_t*)0x10001650 > 2) *(volatile uint8_t*)0x10001650 = 2; }
                    if (*m25 == 7) { (*((volatile uint8_t*)0x10001651))++; if (*(volatile uint8_t*)0x10001651 > 1) *(volatile uint8_t*)0x10001651 = 1; }
                    if (*m25 == 8) { (*((volatile uint8_t*)0x10001652))++; if (*(volatile uint8_t*)0x10001652 > 1) *(volatile uint8_t*)0x10001652 = 1; }
                    if (*m25 == 9) { (*((volatile uint8_t*)0x10001653))++; if (*(volatile uint8_t*)0x10001653 > 1) *(volatile uint8_t*)0x10001653 = 1; }
                    if (*m25 == 0xa) { (*(volatile uint32_t*)0x10001658)++; if (*(volatile uint32_t*)0x10001658 > 0xb4) *(volatile uint32_t*)0x10001658 = 0xb4; }
                }
                if (key == 3 || key == 0x21) { /* 减：word 项门控 >0xdac、byte 项门控 >0 */
                    *w44 = 0;
                    if (*m25 == 0) { if (*(volatile uint32_t*)0x10001690 > 0xdac) (*(volatile uint32_t*)0x10001690)--; }
                    if (*m25 == 1) { if (*(volatile uint32_t*)0x10001698 > 0xdac) (*(volatile uint32_t*)0x10001698)--; }
                    if (*m25 == 2) { if (*(volatile uint32_t*)0x100016a0 > 0xdac) (*(volatile uint32_t*)0x100016a0)--; }
                    if (*m25 == 3) { if (*(volatile uint32_t*)0x100016a8 > 0xdac) (*(volatile uint32_t*)0x100016a8)--; }
                    if (*m25 == 4) { if (*(volatile uint32_t*)0x100016b0 > 0xdac) (*(volatile uint32_t*)0x100016b0)--; }
                    if (*m25 == 5) { if (*(volatile uint8_t*)0x1000164f > 0) (*(volatile uint8_t*)0x1000164f)--; }
                    if (*m25 == 6) { if (*(volatile uint8_t*)0x10001650 > 0) (*(volatile uint8_t*)0x10001650)--; }
                    if (*m25 == 7) { if (*(volatile uint8_t*)0x10001651 > 0) (*(volatile uint8_t*)0x10001651)--; }
                    if (*m25 == 8) { if (*(volatile uint8_t*)0x10001652 > 0) (*(volatile uint8_t*)0x10001652)--; }
                    if (*m25 == 9) { if (*(volatile uint8_t*)0x10001653 > 0) (*(volatile uint8_t*)0x10001653)--; }
                    if (*m25 == 0xa) { if (*(volatile uint32_t*)0x10001658 > 0) (*(volatile uint32_t*)0x10001658)--; }
                }
                *w58 = 0xfa;
            }
        }

        /* ---- 公共尾部 (0x598c-0x5e16)：TIMEOUT3 每帧 +1 ---- */
        (*w58)++;
        /* TIMEOUT3==0xfb → 整页重绘，当前项反显(attr=1) (0x599e-0x5ce2) */
        if (*w58 == 0xfb) {
            if (*m25 < 4) {
                disp_uint4(*(volatile uint32_t*)0x10001690, 0, 0xb, (*m25 == 0) ? 1 : 0);
                disp_uint4(*(volatile uint32_t*)0x10001698, 1, 0xb, (*m25 == 1) ? 1 : 0);
                disp_uint4(*(volatile uint32_t*)0x100016a0, 2, 0xb, (*m25 == 2) ? 1 : 0);
                disp_uint4(*(volatile uint32_t*)0x100016a8, 3, 0xb, (*m25 == 3) ? 1 : 0);
            } else if (*m25 >= 4 && *m25 < 8) {
                /* item4=0x100016b0 数值 row0 */
                disp_uint4(*(volatile uint32_t*)0x100016b0, 0, 0xb, (*m25 == 4) ? 1 : 0);
                /* item5=0x1000164f row1：0→0x5b2c、1→0x5b34、2→0x5b3c */
                if (*(volatile uint8_t*)0x1000164f == 0) disp_string(0x5b2c, 1, 0xb, (*m25 == 5) ? 1 : 0);
                else if (*(volatile uint8_t*)0x1000164f == 1) disp_string(0x5b34, 1, 0xb, (*m25 == 5) ? 1 : 0);
                else disp_string(0x5b3c, 1, 0xb, (*m25 == 5) ? 1 : 0);
                /* item6=0x10001650 row2：0→0x5f48、1→0x5b34、2→0x5b3c */
                if (*(volatile uint8_t*)0x10001650 == 0) disp_string(0x5f48, 2, 0xb, (*m25 == 6) ? 1 : 0);
                else if (*(volatile uint8_t*)0x10001650 == 1) disp_string(0x5b34, 2, 0xb, (*m25 == 6) ? 1 : 0);
                else disp_string(0x5b3c, 2, 0xb, (*m25 == 6) ? 1 : 0);
                /* item7=0x10001651 row3：0→0x5f54、非0→0x5f5c */
                if (*(volatile uint8_t*)0x10001651 == 0) disp_string(0x5f54, 3, 0xb, (*m25 == 7) ? 1 : 0);
                else disp_string(0x5f5c, 3, 0xb, (*m25 == 7) ? 1 : 0);
            } else if (*m25 >= 8 && *m25 < 0xc) {
                /* item8=0x10001652 row0：0→0x5f68、非0→0x5f70 */
                if (*(volatile uint8_t*)0x10001652 == 0) disp_string(0x5f68, 0, 0xb, (*m25 == 8) ? 1 : 0);
                else disp_string(0x5f70, 0, 0xb, (*m25 == 8) ? 1 : 0);
                /* item9=0x10001653 row1：0→0x5f7c、非0→0x5f84 */
                if (*(volatile uint8_t*)0x10001653 == 0) disp_string(0x5f7c, 1, 0xb, (*m25 == 9) ? 1 : 0);
                else disp_string(0x5f84, 1, 0xb, (*m25 == 9) ? 1 : 0);
                /* item0xa=0x10001658 row2：显示按 ldrb 8位装入 disp_number3 (0x5cbc) */
                disp_number3(*(volatile uint8_t*)0x10001658, 2, 0xb, (*m25 == 0xa) ? 1 : 0);
            }
        }
        /* TIMEOUT3>0x1f4 → 回绕 0；编辑态按光标项用空格串擦除值列 (0x5ce2-0x5de4)；
           擦除串：0x566c=4空格(项0-4 数值)、0x5f98=4空格(项5-9 文字)、0x5fa0=3空格(项0xa)。
           查看态(锁==0)本帧提前返回，跳过 TIMEOUT++。 */
        if (*w58 > 0x1f4) {
            *w58 = 0;
            if (*m26 == 0) return;
            if (*m25 == 0) disp_string(0x566c, 0, 0xb, 0);
            if (*m25 == 1) disp_string(0x566c, 1, 0xb, 0);
            if (*m25 == 2) disp_string(0x566c, 2, 0xb, 0);
            if (*m25 == 3) disp_string(0x566c, 3, 0xb, 0);
            if (*m25 == 4) disp_string(0x566c, 0, 0xb, 0);
            if (*m25 == 5) disp_string(0x5f98, 1, 0xb, 0);
            if (*m25 == 6) disp_string(0x5f98, 2, 0xb, 0);
            if (*m25 == 7) disp_string(0x5f98, 3, 0xb, 0);
            if (*m25 == 8) disp_string(0x5f98, 0, 0xb, 0);
            if (*m25 == 9) disp_string(0x5f98, 1, 0xb, 0);
            if (*m25 == 0xa) disp_string(0x5fa0, 2, 0xb, 0);
        }
        /* 编辑空闲超时回主屏 (0x5de6-0x5e16)：超时路径含 param_sync_live_to_eeprom() */
        (*w44)++;
        if (*w44 >= 0x1388) {
            *w44 = 0; param_sync_live_to_eeprom(); *MENU = 1; disp_splash_screen(); return;
        }
        return;
    }

    /* ================= case2 主菜单 (0x5e18-0x6840)，MENU==2 =================
       9 选项（MENU2=0..8）。key==4 回主屏；key==2/3 光标导航 + 高亮；
       key==1 清 0x10001726 后按选项分发到各子屏。不调 0x3534（进入不保存）。 */
    if (*MENU == 2) {
        volatile uint8_t  *menu = (volatile uint8_t*)0x10001724;
        volatile uint8_t  *m2   = (volatile uint8_t*)0x10001725;
        volatile uint32_t *to   = (volatile uint32_t*)0x10001744;

        /* key==4 SET 返回主屏 @0x5e20 */
        if (key == 4) { *to = 0; *menu = 1; disp_splash_screen(); return; }

        /* key==2(DOWN)/3(UP) 光标导航 @0x5e38，clamp 0..8（12p 有 9 项） */
        if (key == 2 || key == 3) {
            *to = 0;
            if (key == 3) { (*m2)++; if (*m2 > 8) *m2 = 8; } /* UP */
            if (key == 2) { if (*m2 > 0) (*m2)--; }          /* DOWN */
            if (*m2 < 4) { /* 页1 选项0-3 */
                disp_string(0x5fac, 0, 0, 0); disp_string(0x5fc0, 1, 0, 0);
                disp_string(0x5fd4, 2, 0, 0); disp_string(0x5fe8, 3, 0, 0);
            }
            if (*m2 >= 4 && *m2 < 8) { /* 页2 选项4-7 */
                disp_string(0x5ffc, 0, 0, 0); disp_string(0x6010, 1, 0, 0);
                disp_string(0x6024, 2, 0, 0); disp_string(0x6038, 3, 0, 0);
            }
            if (*m2 >= 8 && *m2 < 0xc) { /* 页3 仅选项8，行1-3擦除 */
                disp_string(0x604c, 0, 0, 0);
                disp_string(0x56d8, 1, 0, 0); disp_string(0x56d8, 2, 0, 0);
                disp_string(0x56d8, 3, 0, 0);
            }
            /* 高亮当前项 @0x6060 */
            if (*m2 == 0) disp_string(0x5fac, 0, 0, 1);
            if (*m2 == 1) disp_string(0x5fc0, 1, 0, 1);
            if (*m2 == 2) disp_string(0x5fd4, 2, 0, 1);
            if (*m2 == 3) disp_string(0x5fe8, 3, 0, 1);
            if (*m2 == 4) disp_string(0x5ffc, 0, 0, 1);
            if (*m2 == 5) disp_string(0x6010, 1, 0, 1);
            if (*m2 == 6) disp_string(0x6024, 2, 0, 1);
            if (*m2 == 7) disp_string(0x6038, 3, 0, 1);
            if (*m2 == 8) disp_string(0x604c, 0, 0, 1);
        }

        /* key==1 确认：TIMEOUT 清零、0x10001726(=MENU3 编辑态/锁标志)清零后分发 @0x6120 */
        if (key == 1) {
            *to = 0;
            *(volatile uint8_t*)0x10001726 = 0;
            if (*m2 == 0) { /* 选项0 → case3 基本参数 @0x6134 */
                *menu = 3; *m2 = 0;
                disp_string(0x647c, 0, 0, 0); disp_string(0x6490, 1, 0, 0);
                disp_string(0x64a4, 2, 0, 0); disp_string(0x64b8, 3, 0, 0);
                if (*CTRL_MODE == 0) { disp_string(0x64d0, 0, 0xb, 1); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
                if (*CTRL_MODE == 1) { disp_string(0x64d8, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
                if (*CTRL_MODE == 2) { disp_string(0x64e0, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
                disp_uint4(*GAIN0, 1, 0xb, 0);
                disp_uint4(*GAIN1, 2, 0xb, 0);
                disp_uint4(*GAIN_COEF, 3, 0xb, 0);
                *TIMEOUT3 = 0xfa;
            }
            if (*m2 == 1) { /* 选项1 → case4 保护参数 @0x620a */
                *menu = 4; *m2 = 0;
                disp_string(0x64f8, 0, 0, 0); disp_string(0x650c, 1, 0, 0);
                disp_string(0x6520, 2, 0, 0); disp_string(0x6534, 3, 0, 0);
                disp_uint4(*(volatile uint32_t*)0x100016b8, 0, 0xb, 1);
                disp_uint4(*(volatile uint8_t*)0x100016bc, 1, 0xb, 0);
                disp_uint4(*(volatile uint32_t*)0x100016c0, 2, 0xb, 0);
                disp_uint4(*(volatile uint8_t*)0x100016c4, 3, 0xb, 0);
                *TIMEOUT3 = 0xfa;
            }
            if (*m2 == 2) { /* 选项2 → case5 通讯设置 @0x6290 */
                *menu = 5; *m2 = 0;
                disp_string(0x6558, 0, 0, 0); disp_string(0x656c, 1, 0, 0);
                disp_string(0x6580, 2, 0, 0); disp_string(0x6594, 3, 0, 0);
                disp_uint5(*COM_ADDR, 0, 0xb, 1); /* 通讯地址 */
                disp_number(BAUD_TBL[*BAUD_IDX], 1, 0xa, 0); /* 波特率查表 @0x1042 */
                if (*PARITY == 0) disp_string(0x65b8, 2, 0xa, 0);
                if (*PARITY == 1) disp_string(0x65c0, 2, 0xa, 0);
                if (*PARITY == 2) disp_string(0x65c8, 2, 0xa, 0);
                if (*COM_CHK == 0) disp_string(0x5f54, 3, 0xb, 0);
                else disp_string(0x65d4, 3, 0xb, 0);
                *TIMEOUT3 = 0xfa;
            }
            if (*m2 == 3) { /* 选项3 → case6 密码 @0x635e */
                *menu = 6; *m2 = 0;
                *DELAY_OUT = 0;
                disp_clear();
                disp_string(0x4cf4, 1, 0, 0); /* "  密码:------" */
            }
            if (*m2 == 4) { /* 选项4 → case7 PID @0x6386 */
                *menu = 7; *m2 = 0;
                disp_string(0x65e4, 0, 0, 0); disp_string(0x65f8, 1, 0, 0);
                disp_string(0x660c, 2, 0, 0); disp_string(0x6620, 3, 0, 0);
                if (*PID_MODE == 1) { disp_string(0x6638, 0, 0xb, 1); disp_uint4(*(volatile uint8_t*)0x10001709, 1, 0xb, 0); disp_uint4(*(volatile uint8_t*)0x1000170a, 2, 0xb, 0); }
                if (*PID_MODE == 2) { disp_string(0x6648, 0, 0xb, 1); disp_uint4(*(volatile uint8_t*)0x1000170b, 1, 0xb, 0); disp_uint4(*(volatile uint8_t*)0x1000170a, 2, 0xb, 0); }
                if (*PID_MODE == 3) { disp_string(0x6654, 0, 0xb, 1); disp_uint4(*(volatile uint8_t*)0x1000170d, 1, 0xb, 0); disp_uint4(*(volatile uint8_t*)0x1000170e, 2, 0xb, 0); }
                if (*PID_MODE == 4) { disp_string(0x6664, 0, 0xb, 1); disp_uint4(*(volatile uint8_t*)0x1000170f, 1, 0xb, 0); disp_uint4(*(volatile uint8_t*)0x10001710, 2, 0xb, 0); }
                *TIMEOUT3 = 0xfa;
            }
            if (*m2 == 5) { /* 选项5 → case8 相位校准 @0x6696 */
                *menu = 8; *m2 = 0; disp_clear();
                disp_string(0x6a80, 0, 4, 0); disp_string(0x6a8c, 1, 2, 0);
                disp_string(0x6a98, 2, 2, 0);
                disp_offset(*PHASE_OFF, 2, 7, 1); /* 起始相位 */
                disp_string(0x6aa4, 3, 0, 0);
            }
            if (*m2 == 6) { /* 选项6 → caseB 运行时间 @0x66ec */
                *menu = 0xb; *m2 = 0;
                *DELAY_OUT = 0; disp_clear();
                disp_string(0x4ca0, 0, 0, 0); disp_string(0x4cb4, 1, 0, 0);
                disp_string(0x4cc8, 2, 0, 0); disp_string(0x4cb4, 3, 0, 0);
                disp_uint5(*HOUR_NOW, 1, 3, 0);
                disp_uint2(*MIN_NOW, 1, 0xa, 0);
                disp_uint5(*HOUR_TOTAL, 3, 3, 0);
                disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
            }
            if (*m2 == 7) { /* 选项7 → case9 版本 @0x677a */
                *menu = 9; *m2 = 0; disp_clear();
                disp_string(0x6acc, 0, 0, 0); disp_string(0x6ad8, 1, 0, 0);
                disp_string(0x6ae8, 2, 0, 0); disp_string(0x6af8, 3, 0, 0);
            }
            if (*m2 == 8) { /* 选项8 → case5A 平衡角 @0x67c2 */
                *menu = 0x5a; *m2 = 0; disp_clear();
                disp_string(0x6b0c, 0, 4, 0); disp_string(0x6a8c, 1, 2, 0);
                disp_string(0x6a98, 2, 2, 0);
                disp_signed_angle(*BAL_ANG, 2, 7, 1); /* BAL_ANG；0x116c */
                disp_string(0x6aa4, 3, 0, 0);
            }
        }

        /* 超时尾 @0x6810：TIMEOUT 每帧累加，≥0x1388 回主屏 */
        (*to)++;
        if (*to >= 0x1388) { *to = 0; *menu = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case3 基本参数屏 (0x6840-0x7962)，MENU==3 =================
       MENU2(0x10001725)=当前项号 0-15（4 页×4 项）；MENU3(0x10001726)=编辑态标志。
       项映射(全部 12p bin 校验)：
         0=CTRL_MODE 0x1000162c 0..2(环绕) | 1=0x10001634 word 0x1770
         | 2=0x10001630 word 0x1770 | 3=0x10001638 word 0x1770
         | 4=限位0x10001640 word(<=0x10001634+1) | 5=限位0x1000163c word(<=0x10001630+1)
         | 6=软起0x10001644 byte 0xc8 | 7=0x10001645 byte 0xc8
         | 8=0x10001648 word 0xb4 | 9=角度0x1000164c byte 0xa0(disp_signed_angle)
         | 10=DISP_SEL 0x1000164d 0..2 | 11=配对标志0x1000164e 0..1
         | 12=ESTOP 0x1000164f 0..2 | 13=0x10001651 0..1
         | 14=0x10001652 0..1 | 15=0x10001658 word 0xb4
       读宽注意：item8/15 修改用 word(ldr/str)、显示用 byte(ldrb)。 */
    if (*MENU == 3) {
        volatile uint8_t  *b2c = (volatile uint8_t*)0x1000162c; /* CTRL_MODE */
        volatile uint8_t  *b44 = (volatile uint8_t*)0x10001644; /* item6 软起时间 */
        volatile uint8_t  *b45 = (volatile uint8_t*)0x10001645; /* item7 */
        volatile uint8_t  *b4c = (volatile uint8_t*)0x1000164c; /* item9 角度 */
        volatile uint8_t  *b4d = (volatile uint8_t*)0x1000164d; /* DISP_SEL */
        volatile uint8_t  *b4e = (volatile uint8_t*)0x1000164e; /* item11 配对标志 */
        volatile uint8_t  *b4f = (volatile uint8_t*)0x1000164f; /* ESTOP */
        volatile uint8_t  *b51 = (volatile uint8_t*)0x10001651; /* item13 */
        volatile uint8_t  *b52 = (volatile uint8_t*)0x10001652; /* item14 */
        volatile uint32_t *w30 = (volatile uint32_t*)0x10001630; /* item2 */
        volatile uint32_t *w34 = (volatile uint32_t*)0x10001634; /* item1 */
        volatile uint32_t *w38 = (volatile uint32_t*)0x10001638; /* item3 */
        volatile uint32_t *w3c = (volatile uint32_t*)0x1000163c; /* item5 限位 */
        volatile uint32_t *w40 = (volatile uint32_t*)0x10001640; /* item4 限位 */
        volatile uint32_t *w48 = (volatile uint32_t*)0x10001648; /* item8 (word) */
        volatile uint32_t *w58 = (volatile uint32_t*)0x10001658; /* item15 (word) */
        uint32_t it = *MENU2;

        /* ---- key==1：查看/编辑 切换 MENU3，并复位修改空闲计时 (0x684a-0x6884) ---- */
        if (key == 1) {
            *TIMEOUT = 0;
            (*MENU3)++;
            if (*MENU3 > 1) *MENU3 = 0;
            /* 进编辑→0x1f4、出编辑→0xfa；公共尾部 ++ 后分别成 0x1f5(擦除)/0xfb(重绘) */
            *TIMEOUT3 = (*MENU3 == 0) ? 0xfa : 0x1f4;
        }
        /* ---- key==4：保存并退回 参数子菜单(type2 屏) (0x6886-0x68d6) ---- */
        else if (key == 4) {
            *TIMEOUT = 0; *MENU = 2; *MENU2 = 0;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x436c, 0, 0, 1); disp_string(0x437c, 1, 0, 0);
            disp_string(0x438c, 2, 0, 0); disp_string(0x439c, 3, 0, 0);
        }
        /* ---- key==2/3 且 *MENU3==0：项间导航 (MENU2=0..15) (0x68da-0x6a26) ---- */
        else if ((key == 2 || key == 3) && *MENU3 == 0) {
            *TIMEOUT = 0;
            if (key == 3) { (*MENU2)++; if (*MENU2 > 0xf) *MENU2 = 0xf; }
            else          { if (*MENU2 > 0) (*MENU2)--; }
            it = *MENU2;
            switch (it >> 2) { /* 重绘新项所在页标题(值列清空；值由尾部整页重绘恢复) */
                case 0: disp_string(0x647c, 0, 0, 0); disp_string(0x6490, 1, 0, 0);
                        disp_string(0x64a4, 2, 0, 0); disp_string(0x64b8, 3, 0, 0); break;
                case 1: disp_string(0x6b28, 0, 0, 0); disp_string(0x6b3c, 1, 0, 0);
                        disp_string(0x6b50, 2, 0, 0); disp_string(0x6b64, 3, 0, 0); break;
                case 2: disp_string(0x6b78, 0, 0, 0); disp_string(0x6b8c, 1, 0, 0);
                        disp_string(0x6ba0, 2, 0, 0); disp_string(0x6bb4, 3, 0, 0); break;
                default: disp_string(0x6bc8, 0, 0, 0); disp_string(0x6bdc, 1, 0, 0);
                        disp_string(0x6bf0, 2, 0, 0); disp_string(0x6c04, 3, 0, 0); break;
            }
            *TIMEOUT3 = 0xfa; /* 尾部 ++ 成 0xfb 触发整页重绘 */
        }
        /* ---- key∈{2,0x16,3,0x21} 且 *MENU3==1：修改当前项值 (0x6a28-0x7196) ---- */
        else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *MENU3 == 1) {
            *TIMEOUT = 0;
            it = *MENU2;
            if (it == 0) { /* CTRL_MODE 枚举(环绕) */
                if (key == 3 || key == 0x21) { if (*b2c == 0) *b2c = 3; (*b2c)--; }
                else { (*b2c)++; if (*b2c > 2) *b2c = 0; }
            }
            else if (it >= 1 && it <= 5) { /* 数字项(word)：+1/+5/-1/-5 */
                if (key == 2) { /* +1 */
                    if (it == 1) { (*w34)++; if (*w34 > 0x1770) *w34 = 0x1770; }
                    else if (it == 2) { (*w30)++; if (*w30 > 0x1770) *w30 = 0x1770; }
                    else if (it == 3) { (*w38)++; if (*w38 > 0x1770) *w38 = 0x1770; }
                    else if (it == 4) { (*w40)++; if (*w40 > *w34 + 1) *w40 = *w34 + 1; }
                    else              { (*w3c)++; if (*w3c > *w30 + 1) *w3c = *w30 + 1; }
                }
                else if (key == 0x16) { /* 快加 +5 */
                    if (it == 1) { *w34 += 5; if (*w34 > 0x1770) *w34 = 0x1770; }
                    else if (it == 2) { *w30 += 5; if (*w30 > 0x1770) *w30 = 0x1770; }
                    else if (it == 3) { *w38 += 5; if (*w38 > 0x1770) *w38 = 0x1770; }
                    else if (it == 4) { *w40 += 5; if (*w40 > *w34 + 1) *w40 = *w34 + 1; }
                    else              { *w3c += 5; if (*w3c > *w30 + 1) *w3c = *w30 + 1; }
                }
                else if (key == 3) { /* -1 (下限 0xa) */
                    if (it == 1) { if (*w34 > 0xa) (*w34)--; }
                    else if (it == 2) { if (*w30 > 0xa) (*w30)--; }
                    else if (it == 3) { if (*w38 > 0xa) (*w38)--; }
                    else if (it == 4) { if (*w40 > 0xa) (*w40)--; }
                    else              { if (*w3c > 0xa) (*w3c)--; }
                }
                else { /* 快减 -5 (值<0x10 先置 0xf，再 -5 → 下限 0xa) */
                    if (it == 1) { if (*w34 < 0x10) *w34 = 0xf; *w34 -= 5; }
                    else if (it == 2) { if (*w30 < 0x10) *w30 = 0xf; *w30 -= 5; }
                    else if (it == 3) { if (*w38 < 0x10) *w38 = 0xf; *w38 -= 5; }
                    else if (it == 4) { if (*w40 < 0x10) *w40 = 0xf; *w40 -= 5; }
                    else              { if (*w3c < 0x10) *w3c = 0xf; *w3c -= 5; }
                }
            }
            else { /* 非数字项(6..15)：减门控/加钳位 */
                if (key == 3 || key == 0x21) { /* 减 */
                    switch (it) {
                        case 6: if (*b44 > 0) (*b44)--; break;
                        case 7: if (*b45 > 0) (*b45)--; break;
                        case 8: if (*w48 != 0) (*w48)--; break;
                        case 9: if (*b4c > 0x28) (*b4c)--; break; /* 下限 0x28 */
                        case 10: if (*b4d > 0) (*b4d)--; break;
                        case 11: if (*b4e > 0) (*b4e)--; break;
                        case 12: if (*b4f > 0) (*b4f)--; break;
                        case 13: if (*b51 > 0) (*b51)--; break;
                        case 14: if (*b52 > 0) (*b52)--; break;
                        case 15: if (*w58 != 0) (*w58)--; break;
                    }
                }
                else { /* 加 (key==2/0x16) */
                    switch (it) {
                        case 6: (*b44)++; if (*b44 > 0xc8) *b44 = 0xc8; break;
                        case 7: (*b45)++; if (*b45 > 0xc8) *b45 = 0xc8; break;
                        case 8: (*w48)++; if (*w48 > 0xb4) *w48 = 0xb4; break;
                        case 9: (*b4c)++; if (*b4c > 0xa0) *b4c = 0xa0; break;
                        case 10: (*b4d)++; if (*b4d > 2) *b4d = 2; break;
                        case 11: (*b4e)++; if (*b4e > 1) *b4e = 1; break;
                        case 12: (*b4f)++; if (*b4f > 2) *b4f = 2; break;
                        case 13: (*b51)++; if (*b51 > 1) *b51 = 1; break;
                        case 14: (*b52)++; if (*b52 > 1) *b52 = 1; break;
                        case 15: (*w58)++; if (*w58 > 0xb4) *w58 = 0xb4; break;
                    }
                }
            }
            *TIMEOUT3 = 0xfa; /* 0x7192 */
        }

        /* ---- 公共尾部 (0x7198-0x791e)：TIMEOUT3 计数到 0xFB 整页重绘 / 超过 0x1F4 回绕擦除 ---- */
        (*TIMEOUT3)++;
        if (*TIMEOUT3 == 0xfb) sm3_draw_page(*MENU2);

        /* 编辑态按 MENU2 用空格擦除当前项值列 (0x7708-0x791e)；与 0xFB 整页重绘交替
         * → 值"反显/消失"闪烁。空格串：0x5f98=4空格(item0/10..14)、0x6bac=4空格(item1..7)、
         *   0x5fa0=3空格(item8/9/15)。item4/5 按量程是否顶到限位选串。 */
        if (*TIMEOUT3 > 0x1f4) {
            *TIMEOUT3 = 0;
            if (*MENU3 == 0) return; /* 查看态：本帧提前返回，跳过 TIMEOUT++ */
            switch (it) {
                case 0:  disp_string(0x5f98, 0, 0xb, 0); break;
                case 1:  disp_string(0x6bac, 1, 0xb, 0); break;
                case 2:  disp_string(0x6bac, 2, 0xb, 0); break;
                case 3:  disp_string(0x6bac, 3, 0xb, 0); break;
                case 4:  if (*w34 >= *w40) disp_string(0x6bac, 0, 0xb, 0);
                         else disp_string(0x5f98, 0, 0xb, 0); break;
                case 5:  if (*w30 >= *w3c) disp_string(0x6bac, 1, 0xb, 0);
                         else disp_string(0x5f98, 1, 0xb, 0); break;
                case 6:  disp_string(0x6bac, 2, 0xb, 0); break;
                case 7:  disp_string(0x6bac, 3, 0xb, 0); break;
                case 8:  disp_string(0x5fa0, 0, 0xb, 0); break;
                case 9:  disp_string(0x5fa0, 1, 0xb, 0); break;
                case 10: disp_string(0x5f98, 2, 0xb, 0); break;
                case 11: disp_string(0x5f98, 3, 0xb, 0); break;
                case 12: disp_string(0x5f98, 0, 0xb, 0); break;
                case 13: disp_string(0x5f98, 1, 0xb, 0); break;
                case 14: disp_string(0x5f98, 2, 0xb, 0); break;
                case 15: disp_string(0x5fa0, 3, 0xb, 0); break;
            }
        }

        /* ---- 恒压/恒流(CTRL_MODE<2)且软起时间 b44 未配置时自动置 1 (0x792a-0x793c)；
         *      随后编辑空闲超时回到主屏 (0x793e-0x7960) ---- */
        (*TIMEOUT)++;
        if (*b2c < 2 && *b44 == 0) *b44 = 1;
        if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case4 保护参数屏 (0x7962-0x84a2)，MENU==4 =================
       MENU2(0x10001725)=当前项号 0-9；0x10001726=编辑态标志。10 项 = 3 页。
       项映射(全部 12p dump)：0=过压0x100016b8(word,上限*0x10001634)
         1=过压时间0x100016bc(byte,<=0xc8) 2=欠压0x100016c0(word,上限*0x10001634)
         3=欠压时间0x100016c4(byte,<=0xc8) 4=IF过载0x100016c8(word,上限*0x10001630)
         5=IF过载时间0x100016cc(byte,<=0xc8) 6=CT过载0x100016d0(word,上限*0x10001638)
         7=CT过载时间0x100016d4(byte,<=0xc8) 8=缺相0x100016d5(byte,0..1)
         9=三相平衡0x100016d6(byte,0..0x3c,减下限 0x9)。 */
    if (*MENU == 4) {
        volatile uint8_t  *m25 = (volatile uint8_t*)0x10001725; /* MENU2：当前项号 0-9 */
        volatile uint8_t  *m26 = (volatile uint8_t*)0x10001726; /* 编辑/锁定标志 */
        volatile uint32_t *t44 = (volatile uint32_t*)0x10001744; /* TIMEOUT */
        volatile uint32_t *t58 = (volatile uint32_t*)0x10001758; /* TIMEOUT3 */
        volatile uint32_t *w_b8 = (volatile uint32_t*)0x100016b8; /* 过压 */
        volatile uint8_t  *b_bc = (volatile uint8_t*)0x100016bc;  /* 过压时间 */
        volatile uint32_t *w_c0 = (volatile uint32_t*)0x100016c0; /* 欠压 */
        volatile uint8_t  *b_c4 = (volatile uint8_t*)0x100016c4;  /* 欠压时间 */
        volatile uint32_t *w_c8 = (volatile uint32_t*)0x100016c8; /* IF过载 */
        volatile uint8_t  *b_cc = (volatile uint8_t*)0x100016cc;  /* IF过载时间 */
        volatile uint32_t *w_d0 = (volatile uint32_t*)0x100016d0; /* CT过载 */
        volatile uint8_t  *b_d4 = (volatile uint8_t*)0x100016d4;  /* CT过载时间 */
        volatile uint8_t  *b_d5 = (volatile uint8_t*)0x100016d5;  /* 缺相 */
        volatile uint8_t  *b_d6 = (volatile uint8_t*)0x100016d6;  /* 三相平衡 */
        volatile uint32_t *w30 = (volatile uint32_t*)0x10001630;  /* IF过载上限参照 */
        volatile uint32_t *w34 = (volatile uint32_t*)0x10001634;  /* 过压/欠压上限参照 */
        volatile uint32_t *w38 = (volatile uint32_t*)0x10001638;  /* CT过载上限参照 */

        /* ---- key==1：切换编辑态 (0x796A-0x79A4) ---- */
        if (key == 1) {
            *t44 = 0;
            (*m26)++;
            if (*m26 > 1) *m26 = 0;
            *t58 = (*m26 == 0) ? 0xfa : 0x1f4;
        }
        /* ---- key==4：保存并退回 参数子菜单(type2 屏) (0x79A6-0x79F6) ---- */
        else if (key == 4) {
            *t44 = 0; *MENU = 2; *m25 = 1;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x436c, 0, 0, 0); disp_string(0x437c, 1, 0, 1);
            disp_string(0x438c, 2, 0, 0); disp_string(0x439c, 3, 0, 0);
        }
        /* ---- key==2/3 且 查看态：项间导航 (MENU2=0..9) + 画页标题 (0x79FA-0x7B06) ---- */
        else if ((key == 2 || key == 3) && *m26 == 0) {
            *t44 = 0;
            if (key == 3) { (*m25)++; if (*m25 > 9) *m25 = 9; }
            else          { if (*m25 > 0) (*m25)--; }
            /* 画当前项所在页 4 行标题；页2 的 2/3 行用 4 空格串 0x56d8 占位 */
            if (*m25 < 4) {
                disp_string(0x64f8, 0, 0, 0); disp_string(0x650c, 1, 0, 0);
                disp_string(0x6520, 2, 0, 0); disp_string(0x6534, 3, 0, 0);
            }
            else if (*m25 < 8) {
                disp_string(0x7d30, 0, 0, 0); disp_string(0x7d44, 1, 0, 0);
                disp_string(0x7d58, 2, 0, 0); disp_string(0x7d6c, 3, 0, 0);
            }
            else {
                disp_string(0x7d80, 0, 0, 0); disp_string(0x7d94, 1, 0, 0);
                disp_string(0x56d8, 2, 0, 0); disp_string(0x56d8, 3, 0, 0);
            }
            *t58 = 0xfa;
        }
        /* ---- key∈{2,0x16,3,0x21} 且 编辑态：修改当前项值 (0x7B08-0x7FA2) ---- */
        else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *m26 == 1) {
            uint32_t it = *m25;
            *t44 = 0;
            if (key == 2 || key == 0x16) {
                /* 增：byte 项 1/3/5/7/8/9 +1 clamp */
                if (it == 1) { (*b_bc)++; if (*b_bc > 0xc8) *b_bc = 0xc8; }
                else if (it == 3) { (*b_c4)++; if (*b_c4 > 0xc8) *b_c4 = 0xc8; }
                else if (it == 5) { (*b_cc)++; if (*b_cc > 0xc8) *b_cc = 0xc8; }
                else if (it == 7) { (*b_d4)++; if (*b_d4 > 0xc8) *b_d4 = 0xc8; }
                else if (it == 8) { (*b_d5)++; if (*b_d5 > 1) *b_d5 = 1; }
                else if (it == 9) { (*b_d6)++; if (*b_d6 > 0x3c) *b_d6 = 0x3c; }
                /* word 项：key==2 +1、key==0x16 +5 */
                if (it == 0) { *w_b8 += (key == 0x16) ? 5 : 1; if (*w_b8 > *w34) *w_b8 = *w34; }
                else if (it == 2) { *w_c0 += (key == 0x16) ? 5 : 1; if (*w_c0 > *w34) *w_c0 = *w34; }
                else if (it == 4) { *w_c8 += (key == 0x16) ? 5 : 1; if (*w_c8 > *w30) *w_c8 = *w30; }
                else if (it == 6) { *w_d0 += (key == 0x16) ? 5 : 1; if (*w_d0 > *w38) *w_d0 = *w38; }
            } else {
                /* 减：byte 项 1/3/5/7/8 若 >0 → -1；item9 下限 0x9 */
                if (it == 1) { uint8_t v = *b_bc; if (v) *b_bc = v - 1; }
                else if (it == 3) { uint8_t v = *b_c4; if (v) *b_c4 = v - 1; }
                else if (it == 5) { uint8_t v = *b_cc; if (v) *b_cc = v - 1; }
                else if (it == 7) { uint8_t v = *b_d4; if (v) *b_d4 = v - 1; }
                else if (it == 8) { uint8_t v = *b_d5; if (v) *b_d5 = v - 1; }
                else if (it == 9) { uint8_t v = *b_d6; if (v > 0x9) *b_d6 = v - 1; }
                /* 减：word 项 key==3 若 v>0 → -1(下限0)；key==0x21 若 v<6 先置 5，再 -5 */
                if (it == 0) { uint32_t v = *w_b8; if (key == 0x21) { uint32_t w = (v < 6) ? 5 : v; *w_b8 = w - 5; } else if (v) *w_b8 = v - 1; }
                else if (it == 2) { uint32_t v = *w_c0; if (key == 0x21) { uint32_t w = (v < 6) ? 5 : v; *w_c0 = w - 5; } else if (v) *w_c0 = v - 1; }
                else if (it == 4) { uint32_t v = *w_c8; if (key == 0x21) { uint32_t w = (v < 6) ? 5 : v; *w_c8 = w - 5; } else if (v) *w_c8 = v - 1; }
                else if (it == 6) { uint32_t v = *w_d0; if (key == 0x21) { uint32_t w = (v < 6) ? 5 : v; *w_d0 = w - 5; } else if (v) *w_d0 = v - 1; }
            }
            *t58 = 0xfa;
        }

        /* ---- 刷新节流：TIMEOUT3 自增；==0xfb 重绘当前页（高亮当前项） (0x7FA4-0x8316) ---- */
        (*t58)++;
        if (*t58 == 0xfb) sm4_draw_page(*m25);

        /* ---- TIMEOUT3 > 0x1F4：回绕为 0；查看态返回；编辑态按 MENU2 用空格擦除值列 ----
         *     擦除串：0x7d8c=4 空格（word 项值≠0、byte 项 1/3/5/7）、
         *             0x5f98=4 空格（word 项值==0、byte 项 8/9）。12p 两者同为 4 空格。 */
        if (*t58 > 0x1f4) {
            *t58 = 0;
            if (*m26 == 0) return; /* 查看态：本帧提前返回 */
            switch (*m25) {
                case 0: disp_string((*w_b8 != 0) ? 0x7d8c : 0x5f98, 0, 0xb, 0); break;
                case 1: disp_string(0x7d8c, 1, 0xb, 0); break;
                case 2: disp_string((*w_c0 != 0) ? 0x7d8c : 0x5f98, 2, 0xb, 0); break;
                case 3: disp_string(0x7d8c, 3, 0xb, 0); break;
                case 4: disp_string((*w_c8 != 0) ? 0x7d8c : 0x5f98, 0, 0xb, 0); break;
                case 5: disp_string(0x7d8c, 1, 0xb, 0); break;
                case 6: disp_string((*w_d0 != 0) ? 0x7d8c : 0x5f98, 2, 0xb, 0); break;
                case 7: disp_string(0x7d8c, 3, 0xb, 0); break;
                case 8: disp_string(0x5f98, 0, 0xb, 0); break;
                case 9: disp_string(0x5f98, 1, 0xb, 0); break;
            }
        }
        (*t44)++;
        if (*t44 >= 0x1388) { *t44 = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case5 通讯屏 (0x84a2-0x8948)，MENU==5 =================
       4 项（MENU2=0..3）单页。槽：COM_ADDR=0x100016f7(byte)、BAUD_IDX=0x100016f8(word)、
       PARITY=0x100016fc(byte)、COM_CHK=0x100016fd(byte)；波特率表 BAUD_TBL=0x1000179c。
       编辑态标志复用 0x10001726。刷新节流 (*t3)++ 位于 key 处理后的公共尾（OLD 0x8712），
       再判 ==0xfb 整页重绘（2026-08-31 修正：原实现误置 case 开头致 A/B 差 1）。 */
    if (*MENU == 5) {
        volatile uint8_t  *m2    = (volatile uint8_t*)0x10001725; /* 光标/子项 */
        volatile uint8_t  *menu3 = (volatile uint8_t*)0x10001726; /* 编辑态标志 */
        volatile uint32_t *tout  = (volatile uint32_t*)0x10001744; /* TIMEOUT */
        volatile uint32_t *t3    = (volatile uint32_t*)0x10001758; /* TIMEOUT3 */

        /* key==1：编辑态 0<->1 切换（TIMEOUT3=0xfa/0x1f4） */
        if (key == 1) {
            *tout = 0;
            (*menu3)++;
            if (*menu3 > 1) *menu3 = 0;
            *t3 = (*menu3 == 0) ? 0xfa : 0x1f4;
        }
        /* key==4：保存退出到基本参数主菜单（MENU=2/MENU2=2），落公共尾 */
        else if (key == 4) {
            *tout = 0;
            *MENU = 2; *m2 = 2;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x436c, 0, 0, 0); disp_string(0x437c, 1, 0, 0);
            disp_string(0x438c, 2, 0, 1); disp_string(0x439c, 3, 0, 0);
        }
        /* 导航（查看态，key3 上/key2 下，仅 4 项） */
        else if ((key == 2 || key == 3) && *menu3 == 0) {
            *tout = 0;
            if (key == 3) { (*m2)++; if (*m2 > 3) *m2 = 3; }
            if (key == 2) { if (*m2 > 0) (*m2)--; }
            if (*m2 < 4) { /* 重绘通讯页 4 行标题框 */
                disp_string(0x6558, 0, 0, 0); disp_string(0x656c, 1, 0, 0);
                disp_string(0x6580, 2, 0, 0); disp_string(0x6594, 3, 0, 0);
            }
            *t3 = 0xfa;
        }
        /* 编辑（编辑态，key2/0x16 增、key3/0x21 减） */
        else if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *menu3 == 1) {
            if (key == 2 || key == 0x16) { /* 增 */
                *tout = 0;
                if (*m2 == 0) { /* 本机地址 byte，>=0xf6 回 0xf6 再 ++（有效上限 0xf7） */
                    if (*COM_ADDR >= 0xf6) *COM_ADDR = 0xf6;
                    (*COM_ADDR)++;
                }
                if (*m2 == 1) { /* 波特率序号 word，>=7 回 6 再 ++（有效上限 7） */
                    if (*BAUD_IDX >= 7) *BAUD_IDX = 6;
                    (*BAUD_IDX)++;
                }
                if (*m2 == 2) { /* 校验位 byte，++ 上限 3 */
                    (*PARITY)++;
                    if (*PARITY > 3) *PARITY = 3;
                }
                if (*m2 == 3) *COM_CHK = 1; /* 通讯校验置 1 */
            }
            if (key == 3 || key == 0x21) { /* 减 */
                *tout = 0;
                if (*m2 == 0) { /* 本机地址 byte，下限 1 */
                    if (*COM_ADDR > 1) (*COM_ADDR)--;
                }
                if (*m2 == 1) { /* 波特率序号 word，下限 0 */
                    if (*BAUD_IDX != 0) (*BAUD_IDX)--;
                }
                if (*m2 == 2) { /* 校验位 byte，下限 0 */
                    if (*PARITY > 0) (*PARITY)--;
                }
                if (*m2 == 3) *COM_CHK = 0; /* 通讯校验清 0 */
            }
            *t3 = 0xfa;
        }

        /* ---- 刷新节流：TIMEOUT3++ 后 ==0xfb 整页重绘（高亮当前项） ---- */
        (*t3)++;
        if (*t3 == 0xfb) { if (*m2 < 4) sm5_draw_page(*m2); }

        /* ---- 编辑空闲超时：清空当前项所在行（闪烁）后返回主屏 ---- */
        if (*t3 > 0x1f4) {
            *t3 = 0;
            if (*menu3 == 0) return; /* 查看态不擦值 */
            if (*m2 == 0 || *m2 == 4) disp_string(0x5f98, 0, 0xb, 0);
            if (*m2 == 1 || *m2 == 5) disp_string(0x8a74, 1, 0xa, 0);
            if (*m2 == 2 || *m2 == 6) disp_string(0x8a74, 2, 0xa, 0);
            if (*m2 == 3 || *m2 == 7) disp_string(0x5f98, 3, 0xb, 0);
        }
        (*tout)++;
        if (*tout >= 0x1388) { *tout = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case6 初始参数密码校验 (0x8948-0x8e18)，MENU==6 =================
       纯密码屏（无运行时间查询）：key==1 校验 PWD_B(0x100015e6)、key==0xe 校验
       PWD_C(0x100015ec)、缓冲 PWD_BUF(0x100015f2)。密码对后清 EEPROM 密码槽死等。 */
    if (*MENU == 6) {
        volatile uint8_t  *m2      = (volatile uint8_t*)0x10001725; /* 密码位计数 */
        volatile uint32_t *tout    = (volatile uint32_t*)0x10001744; /* TIMEOUT */
        volatile uint8_t  *pwd_buf = (volatile uint8_t*)0x100015f2;

        if (key == 1) {
            /* 输入前密码 PWD_B（0x100015e6）：逐位校验 PWD_BUF */
            *m2 = 0;
            while (*m2 < 6) {
                if (pwd_buf[*m2] == PWD_B[*m2]) {
                    pwd_buf[*m2] = 0; (*m2)++;
                } else {
                    /* 密码错：显示 '密码错'(0x5210) + 延时 → 回基本参数主菜单 4 行 */
                    disp_clear();
                    disp_string(0x5210, 1, 4, 0);
                    sm6_delay_loop();
                    *MENU = 2; *m2 = 3;
                    disp_clear();
                    disp_string(0x436c, 0, 0, 0); disp_string(0x437c, 1, 0, 0);
                    disp_string(0x438c, 2, 0, 0); disp_string(0x439c, 3, 0, 1);
                    return;
                }
            }
            /* 密码对：三段空白提示 → 清 EEPROM reg5/6 → 死等（系统重置进入初始参数） */
            disp_clear();
            disp_string(0x8a98, 1, 0, 0); sm6_delay_loop();
            disp_string(0x8ea8, 1, 0, 0); sm6_delay_loop();
            disp_string(0x8eb8, 1, 0, 0); sm6_delay_loop();
            i2c_write_reg(0, 5);
            i2c_write_reg(0, 6);
            for (;;) { } /* 0x8b80 死等 */
        }
        else if (key == 0xe) {
            /* 初始密码 PWD_C（0x100015ec）：校验 PWD_BUF */
            *m2 = 0;
            while (*m2 < 6) {
                if (pwd_buf[*m2] == PWD_C[*m2]) {
                    pwd_buf[*m2] = 0; (*m2)++;
                } else {
                    disp_clear();
                    disp_string(0x5210, 1, 4, 0);
                    sm6_delay_loop();
                    *MENU = 2; *m2 = 3;
                    disp_clear();
                    disp_string(0x436c, 0, 0, 0); disp_string(0x437c, 1, 0, 0);
                    disp_string(0x438c, 2, 0, 0); disp_string(0x439c, 3, 0, 1);
                    return;
                }
            }
            /* 密码对：三段空白提示 → 清 EEPROM reg5/6/7/8 → 死等 */
            disp_clear();
            disp_string(0x8ee0, 1, 0, 0); sm6_delay_loop();
            disp_string(0x8ea8, 1, 0, 0); sm6_delay_loop();
            disp_string(0x8eb8, 1, 0, 0); sm6_delay_loop();
            i2c_write_reg(0, 5);
            i2c_write_reg(0, 6);
            i2c_write_reg(0, 7);
            i2c_write_reg(0, 8);
            for (;;) { } /* 0x8d5c 死等 */
        }
        else if (key == 4) {
            *tout = 0; *MENU = 2; *m2 = 3;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x436c, 0, 0, 0); disp_string(0x437c, 1, 0, 0);
            disp_string(0x438c, 2, 0, 0); disp_string(0x439c, 3, 0, 1);
            return;
        }
        else if (key > 0) {
            /* 密码数字输入（key==其它正值都当数字）——key<=0 走超时尾 */
            if (*m2 < 6) {
                pwd_buf[*m2] = (uint8_t)key;
                disp_render_char8(0x2a, 1, (uint8_t)(*m2 + 7), 0); /* 显示 '*' @0xaf4 */
                (*m2)++;
            }
            return;
        }
        /* 超时尾（0x1388=5000） */
        (*tout)++;
        if (*tout >= 0x1388) { *tout = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case7 PID 参数设置 (0x8E18-0x97A8)，MENU==7 =================
       PID 槽：PID_MODE=0x10001708；显示缓冲 P/I=0x10001706/07；
       模式1-4 P/I 槽=0x10001709/0a、0b/0c、0d/0e、0f/10；
       增益子项=0x1000171a-1e。全程 ldrb/strb（byte 宽）。
       超时尾常量 0xc350（全函数唯一）。 */
    if (*MENU == 7) {
        volatile uint8_t  *m2   = (volatile uint8_t*)0x10001725; /* 光标/子项 MENU2 */
        volatile uint8_t  *m3   = (volatile uint8_t*)0x10001726; /* 编辑态标志 */
        volatile uint8_t  *pm   = (volatile uint8_t*)0x10001708; /* PID 模式 1-4 */
        volatile uint8_t  *pBuf = (volatile uint8_t*)0x10001706; /* P 显示缓冲 */
        volatile uint8_t  *iBuf = (volatile uint8_t*)0x10001707; /* I 显示缓冲 */
        volatile uint8_t  *p1   = (volatile uint8_t*)0x10001709; /* 模式1 P 槽 */
        volatile uint8_t  *i1   = (volatile uint8_t*)0x1000170a;
        volatile uint8_t  *p2   = (volatile uint8_t*)0x1000170b;
        volatile uint8_t  *i2   = (volatile uint8_t*)0x1000170c;
        volatile uint8_t  *p3   = (volatile uint8_t*)0x1000170d;
        volatile uint8_t  *i3   = (volatile uint8_t*)0x1000170e;
        volatile uint8_t  *p4   = (volatile uint8_t*)0x1000170f;
        volatile uint8_t  *i4   = (volatile uint8_t*)0x10001710;
        volatile uint8_t  *g1   = (volatile uint8_t*)0x1000171a;
        volatile uint8_t  *g2   = (volatile uint8_t*)0x1000171b;
        volatile uint8_t  *g3   = (volatile uint8_t*)0x1000171c;
        volatile uint8_t  *g4   = (volatile uint8_t*)0x1000171d;
        volatile uint8_t  *g5   = (volatile uint8_t*)0x1000171e;
        volatile uint32_t *tm   = (volatile uint32_t*)0x10001744; /* TIMEOUT */
        volatile uint32_t *tm3  = (volatile uint32_t*)0x10001758; /* TIMEOUT3 */

        /* key==1 编辑/浏览切换：m3 在 0/1 间翻转，随之调整 TIMEOUT3 相位 */
        if (key == 1) {
            *tm = 0;
            (*m3)++;
            if (*m3 > 1) *m3 = 0;
            if (*m3 == 0) *tm3 = 0xfa;   /* 浏览：下一拍 ++→0xfb 整页重绘 */
            if (*m3 == 1) *tm3 = 0x1f4;  /* 编辑：下一拍 ++→>0x1f4 清高亮 */
        }
        /* key==4 回主菜单：*MENU=2、*MENU2=4、画 case2 页标题、
         * 当前 PID 模式槽复制到显示缓冲 0x10001706/07 */
        else if (key == 4) {
            *tm = 0;
            *MENU = 2; *m2 = 4;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x5ffc, 0, 0, 1);
            disp_string(0x6010, 1, 0, 0);
            disp_string(0x6024, 2, 0, 0);
            disp_string(0x6038, 3, 0, 0);
            if (*pm == 1) { *pBuf = *p1; *iBuf = *i1; }
            if (*pm == 2) { *pBuf = *p2; *iBuf = *i2; }
            if (*pm == 3) { *pBuf = *p3; *iBuf = *i3; }
            if (*pm == 4) { *pBuf = *p4; *iBuf = *i4; }
        }
        /* key==2/3：浏览态(0)且 PIDMODE==4 时切换子项页（MENU2 0..8） */
        if ((key == 2 || key == 3) && *m3 == 0 && *pm == 4) {
            *tm = 0;
            if (key == 3) { (*m2)++; if (*m2 > 8) *m2 = 8; }
            if (key == 2) { if (*m2 > 0) (*m2)--; }
            /* 按 MENU2 区间画 4 行标题（col0，attr0） */
            if (*m2 < 4) {
                disp_string(0x65e4, 0, 0, 0); disp_string(0x65f8, 1, 0, 0);
                disp_string(0x660c, 2, 0, 0); disp_string(0x6620, 3, 0, 0);
            }
            if (*m2 >= 4 && *m2 < 8) {
                disp_string(0x9348, 0, 0, 0); disp_string(0x935c, 1, 0, 0);
                disp_string(0x9370, 2, 0, 0); disp_string(0x9384, 3, 0, 0);
            }
            if (*m2 >= 8 && *m2 < 0xc) {
                disp_string(0x9398, 0, 0, 0);
                disp_string(0x56d8, 1, 0, 0); disp_string(0x56d8, 2, 0, 0);
                disp_string(0x56d8, 3, 0, 0);
            }
            *tm3 = 0xfa;
        }
        /* key==2/0x16/3/0x21：编辑态(1)改值。key==3/0x21 降方向，key==2/0x16 升方向。 */
        if ((key == 2 || key == 0x16 || key == 3 || key == 0x21) && *m3 == 1) {
            if (key == 3 || key == 0x21) {
                /* 降方向 */
                *tm = 0;
                if (*m2 == 0) { (*pm)++; if (*pm >= 4) *pm = 4; }               /* 模式号 ++ clamp 4 */
                if (*m2 == 1 && *pm == 4) { if (*p4 > 1) (*p4)--; }             /* 模式4 P */
                if (*m2 == 2 && *pm == 4) { if (*i4 > 1) (*i4)--; }             /* 模式4 I */
                if (*m2 == 4) { if (*g1 > 1) (*g1)--; }
                if (*m2 == 5) { if (*g2 > 1) (*g2)--; }
                if (*m2 == 6) { if (*g3 > 1) (*g3)--; }
                if (*m2 == 7) { if (*g4 > 1) (*g4)--; }
                if (*m2 == 8) { if (*g5 > 1) (*g5)--; }
            } else {
                /* 升方向 */
                *tm = 0;
                if (*m2 == 0) { if (*pm > 1) (*pm)--; }                         /* 模式号 -- min 1 */
                if (*m2 == 1 && *pm == 4) { (*p4)++; if (*p4 >= 0x80) *p4 = 0x80; }
                if (*m2 == 2 && *pm == 4) { (*i4)++; if (*i4 >= 0x80) *i4 = 0x80; }
                if (*m2 == 4) { (*g1)++; if (*g1 >= 0xfa) *g1 = 0xfa; }
                if (*m2 == 5) { (*g2)++; if (*g2 > *g1) *g2 = *g1; }            /* 不超上一项 */
                if (*m2 == 6) { (*g3)++; if (*g3 >= 0xfa) *g3 = 0xfa; }
                if (*m2 == 7) { (*g4)++; if (*g4 > *g3) *g4 = *g3; }
                if (*m2 == 8) { (*g5)++; if (*g5 > *g4) *g5 = *g4; }
            }
            *tm3 = 0xfa; /* 改值后置回，下一拍 ++→0xfb 重绘 */
        }
        /* ---------- 刷新区：TIMEOUT3++ 计到 0xfb 按 MENU2 重绘 ---------- */
        (*tm3)++;
        if (*tm3 == 0xfb) {
            if (*m2 < 4) {
                /* 模式名 row0 col0xb：MENU2==0 反显(attr1)，否则常显(attr0) */
                if (*m2 == 0) {
                    if (*pm == 1) disp_string(0x6638, 0, 0xb, 1);
                    if (*pm == 2) disp_string(0x6648, 0, 0xb, 1);
                    if (*pm == 3) disp_string(0x6654, 0, 0xb, 1);
                    if (*pm == 4) disp_string(0x6664, 0, 0xb, 1);
                } else {
                    if (*pm == 1) disp_string(0x6638, 0, 0xb, 0);
                    if (*pm == 2) disp_string(0x6648, 0, 0xb, 0);
                    if (*pm == 3) disp_string(0x6654, 0, 0xb, 0);
                    if (*pm == 4) disp_string(0x6664, 0, 0xb, 0);
                }
                /* P 值 row1：MENU2==1 反显 */
                if (*m2 == 1) {
                    if (*pm == 1) disp_uint4(*p1, 1, 0xb, 1);
                    if (*pm == 2) disp_uint4(*p2, 1, 0xb, 1);
                    if (*pm == 3) disp_uint4(*p3, 1, 0xb, 1);
                    if (*pm == 4) disp_uint4(*p4, 1, 0xb, 1);
                } else {
                    if (*pm == 1) disp_uint4(*p1, 1, 0xb, 0);
                    if (*pm == 2) disp_uint4(*p2, 1, 0xb, 0);
                    if (*pm == 3) disp_uint4(*p3, 1, 0xb, 0);
                    if (*pm == 4) disp_uint4(*p4, 1, 0xb, 0);
                }
                /* I 值 row2：MENU2==2 反显 */
                if (*m2 == 2) {
                    if (*pm == 1) disp_uint4(*i1, 2, 0xb, 1);
                    if (*pm == 2) disp_uint4(*i2, 2, 0xb, 1);
                    if (*pm == 3) disp_uint4(*i3, 2, 0xb, 1);
                    if (*pm == 4) disp_uint4(*i4, 2, 0xb, 1);
                } else {
                    if (*pm == 1) disp_uint4(*i1, 2, 0xb, 0);
                    if (*pm == 2) disp_uint4(*i2, 2, 0xb, 0);
                    if (*pm == 3) disp_uint4(*i3, 2, 0xb, 0);
                    if (*pm == 4) disp_uint4(*i4, 2, 0xb, 0);
                }
            } else if (*m2 >= 4 && *m2 < 8) {
                /* 增益子项 row0-3（0x1000171a-1d） */
                if (*m2 == 4) disp_uint4(*g1, 0, 0xb, 1); else disp_uint4(*g1, 0, 0xb, 0);
                if (*m2 == 5) disp_uint4(*g2, 1, 0xb, 1); else disp_uint4(*g2, 1, 0xb, 0);
                if (*m2 == 6) disp_uint4(*g3, 2, 0xb, 1); else disp_uint4(*g3, 2, 0xb, 0);
                if (*m2 == 7) disp_uint4(*g4, 3, 0xb, 1); else disp_uint4(*g4, 3, 0xb, 0);
            } else if (*m2 >= 8 && *m2 < 0xc) {
                /* 增益子项 row0（0x1000171e） */
                if (*m2 == 8) disp_uint4(*g5, 0, 0xb, 1); else disp_uint4(*g5, 0, 0xb, 0);
            }
        }
        /* ---------- 超时清高亮：TIMEOUT3>0x1f4 清行；浏览态直接回主界面 ---------- */
        if (*tm3 > 0x1f4) {
            *tm3 = 0;
            if (*m3 == 0) return;
            /* 编辑态：按 MENU2 清当前行（0x5f98 空格串，col0xb） */
            if (*m2 == 0) disp_string(0x5f98, 0, 0xb, 0);
            if (*m2 == 1) disp_string(0x5f98, 1, 0xb, 0);
            if (*m2 == 2) disp_string(0x5f98, 2, 0xb, 0);
            if (*m2 == 3) disp_string(0x5f98, 3, 0xb, 0);
            if (*m2 == 4) disp_string(0x5f98, 0, 0xb, 0);
            if (*m2 == 5) disp_string(0x5f98, 1, 0xb, 0);
            if (*m2 == 6) disp_string(0x5f98, 2, 0xb, 0);
            if (*m2 == 7) disp_string(0x5f98, 3, 0xb, 0);
            if (*m2 == 8) disp_string(0x5f98, 0, 0xb, 0);
        }
        /* ---------- 超时尾（0xc350=50000） ---------- */
        (*tm)++;
        if (*tm >= 0xc350) {
            *tm = 0;
            *MENU = 1;
            disp_splash_screen();
        }
        return;
    }

    /* ================= case8 相位参数校准 (0x97a8-0x99da)，MENU==8 =================
       PHASE_OFF=0x10001624(word)、保存副本 0x10001628(word)、FAULT=0x1000161c(word)、
       RUN=0x10001620(byte，case8 局部运行标志，非 0x1000175e)、STATUS=0x100015cc(byte)、
       屏内置位 0x100015ce(byte)。 */
    if (*MENU == 8) {
        volatile uint8_t  *run_flag = (volatile uint8_t*)0x10001620; /* case8 局部 RUN */
        volatile uint8_t  *status8  = (volatile uint8_t*)0x100015cc;
        volatile uint8_t  *scr_set  = (volatile uint8_t*)0x100015ce;

        *scr_set = 1; /* 屏内置位 @0x97b0 */
        /* 相位偏移 增（key3/0x21 上限 0x2b0）/ 减（key2/0x16 下限 0x45） */
        if (key == 3 || key == 0x21) {
            *TIMEOUT = 0;
            (*PHASE_OFF)++;
            if (*PHASE_OFF > 0x2b0) *PHASE_OFF = 0x2b0;
            disp_offset(*PHASE_OFF, 2, 7, 1); /* 0x1260 */
        }
        if (key == 2 || key == 0x16) {
            *TIMEOUT = 0;
            if (*PHASE_OFF < 0x45) *PHASE_OFF = 0x45;
            (*PHASE_OFF)--;
            disp_offset(*PHASE_OFF, 2, 7, 1); /* 0x1260 */
        }
        /* 运行状态行显示（FAULT!=0 停机 / RUN 控制 STATUS 0/1/2） */
        if (*FAULT != 0) {
            *status8 = 0; disp_string(0x4334, 3, 0xa, 0);
        } else {
            if (*run_flag == 0 && *status8 != 1) {
                *status8 = 1; disp_string(0x4340, 3, 0xa, 0);
            }
            if (*run_flag == 1 && *status8 != 2) {
                *status8 = 2; disp_string(0x4348, 3, 0xa, 0);
            }
        }
        if (key == 5) { /* 启动 */
            *run_flag = 1; *TIMEOUT = 0;
        }
        if (key == 6) { /* 停机 */
            *run_flag = 0; *TIMEOUT = 0;
            gpio_outputs_set(); /* 0xe4fa */
        }
        run_stop_preset(); /* 0xf70a */
        if (key == 4) { /* SET 保存退出 */
            *TIMEOUT = 0;
            *MENU = 2; *MENU2 = 5;
            disp_clear();
            disp_string(0x5ffc, 0, 0, 0); disp_string(0x6010, 1, 0, 1);
            disp_string(0x6024, 2, 0, 0); disp_string(0x6038, 3, 0, 0);
            if (*PHASE_OFF != *(volatile uint32_t*)0x10001628) {
                /* 写 EEPROM reg 0xc9/0xca；0x1e38 = i2c_write_reg(data,reg) */
                *(volatile uint32_t*)0x10001628 = *PHASE_OFF;
                i2c_write_reg((uint16_t)*(volatile uint32_t*)0x10001628 >> 8, 0xc9);
                i2c_write_reg((uint8_t)*(volatile uint32_t*)0x10001628, 0xca);
            }
            gpio_outputs_set(); /* 0xe4fa */
            *run_flag = 0;
            run_stop_preset(); /* 0xf70a */
            *scr_set = 0;
            *status8 = 0;
            return;
        }
        /* 超时尾（0x3a98=15000） */
        (*TIMEOUT)++;
        if (*TIMEOUT >= 0x3a98) {
            *TIMEOUT = 0;
            *run_flag = 0;
            gpio_outputs_set(); /* 0xe4fa */
            *MENU = 1;
            disp_splash_screen(); /* 0x40b0 */
            *scr_set = 0;
            *status8 = 0;
        }
        return;
    }

    /* ================= caseB 运行时间查询 (0x99da-0x9ab6)，MENU==0xb =================
       时间变量（word）：HOUR_NOW=0x100015f8 / MIN_NOW=0x100015fc /
       HOUR_TOTAL=0x10001604 / MIN_TOTAL=0x10001608。 */
    if (*MENU == 0xb) {
        if (key == 4) { /* SET 退出回主菜单 */
            *TIMEOUT = 0;
            *MENU = 2; *MENU2 = 6;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x5ffc, 0, 0, 0); disp_string(0x6010, 1, 0, 0);
            disp_string(0x6024, 2, 0, 1); disp_string(0x6038, 3, 0, 0);
            return;
        }
        /* key==0x17 统计清零 → 4 个时间 word 归零并重显 */
        if (key == 0x17) {
            *HOUR_NOW = 0; *MIN_NOW = 0; *HOUR_TOTAL = 0; *MIN_TOTAL = 0;
            disp_uint5(*HOUR_NOW, 1, 3, 0); disp_uint2(*MIN_NOW, 1, 0xa, 0);
            disp_uint5(*HOUR_TOTAL, 3, 3, 0); disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
        }
        (*TIMEOUT)++;
        if (*TIMEOUT >= 0x1388) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case9 产品版本信息 (0x9ab6-0x9b44)，MENU==9 ================= */
    if (*MENU == 9) {
        if (key == 4) { /* SET 退出回主菜单 */
            *TIMEOUT = 0;
            *MENU = 2; *MENU2 = 7;
            param_sync_live_to_eeprom(); disp_clear();
            disp_string(0x5ffc, 0, 0, 0); disp_string(0x6010, 1, 0, 0);
            disp_string(0x6024, 2, 0, 0); disp_string(0x6038, 3, 0, 1);
            return;
        }
        (*TIMEOUT)++;
        if (*TIMEOUT >= 0x3a98) { *TIMEOUT = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case5A 电流手动平衡 (0x9b44-0x9d2a)，MENU==0x5a =================
       BAL_ANG=0x1000168c(byte)、EEPROM 影子 0x1000168d(byte)；cfg=0x10001620 运行标志。
       key5→运行 / key6→停机 + run_stop_preset()；保存写 EEPROM reg 0x1c。 */
    if (*MENU == 0x5a) {
        volatile uint8_t  *bal_ang = (volatile uint8_t*)0x1000168c;
        volatile uint8_t  *bal_eep = (volatile uint8_t*)0x1000168d;
        volatile uint8_t  *stat_l  = (volatile uint8_t*)0x100015cc;
        volatile uint32_t *timeout = (volatile uint32_t*)0x10001744;
        volatile uint8_t  *scr_set = (volatile uint8_t*)0x100015ce;

        *scr_set = 1;
        if (key == 2 || key == 0x16) { /* 增：上限 0xc7=199 */
            *timeout = 0;
            (*bal_ang)++;
            if (*bal_ang > 0xc7) *bal_ang = 0xc7;
            disp_signed_angle(*bal_ang, 2, 7, 1); /* 0x116c */
        }
        if (key == 3 || key == 0x21) { /* 减：下限 2 */
            *timeout = 0;
            if (*bal_ang < 2) *bal_ang = 2;
            (*bal_ang)--;
            disp_signed_angle(*bal_ang, 2, 7, 1); /* 0x116c */
        }

        /* 运行状态行：故障→0x4334；cfg==0→停 0x4340；cfg==1→运 0x4348 */
        if (*FAULT != 0) {
            disp_string(0x4334, 3, 0xa, 0);
        } else {
            if (*CFG == 0) {
                if (*stat_l != 1) { *stat_l = 1; disp_string(0x4340, 3, 0xa, 0); }
            }
            if (*CFG == 1) {
                if (*stat_l != 2) { *stat_l = 2; disp_string(0x4348, 3, 0xa, 0); }
            }
        }

        if (key == 5) { *CFG = 1; *timeout = 0; } /* 启动 */
        if (key == 6) { *CFG = 0; *timeout = 0; } /* 停机 */
        run_stop_preset(); /* 0xf70a */

        if (key == 4) { /* 保存并返回 */
            *timeout = 0;
            *MENU = 2; *MENU2 = 8;
            disp_clear();
            disp_string(0xa070, 0, 0, 1);
            disp_string(0xa080, 1, 0, 0); disp_string(0xa080, 2, 0, 0);
            disp_string(0xa080, 3, 0, 0);
            if (*bal_ang != *bal_eep) { /* 写 EEPROM reg 0x1c */
                *bal_eep = *bal_ang;
                i2c_write_reg(*bal_eep, 0x1c);
            }
            *CFG = 0;
            run_stop_preset(); /* 0xf70a */
            *scr_set = 0;
            return;
        }

        /* 超时尾（0x3a98=15000） */
        (*timeout)++;
        if (*timeout >= 0x3a98) {
            *timeout = 0;
            *CFG = 0;
            *MENU = 1;
            disp_splash_screen(); /* 0x40b0 */
            *scr_set = 0;
        }
        return;
    }

    /* ================= caseC 运行时间清零 (0x9d2a-0x9dc0)，MENU==0xc ================= */
    if (*MENU == 0xc) {
        volatile uint32_t *timeout = (volatile uint32_t*)0x10001744;

        if (key == 4) { /* 返回主菜单 */
            *MENU = 1; disp_splash_screen();
            return;
        }
        if (key == 0x17) { /* 统计清零 */
            *HOUR_NOW = 0; *MIN_NOW = 0; *HOUR_TOTAL = 0; *MIN_TOTAL = 0;
            disp_uint5(*HOUR_NOW, 1, 3, 0); disp_uint2(*MIN_NOW, 1, 0xa, 0);
            disp_uint5(*HOUR_TOTAL, 3, 3, 0); disp_uint2(*MIN_TOTAL, 3, 0xa, 0);
        }
        /* 超时尾（0x1388=5000） */
        (*timeout)++;
        if (*timeout >= 0x1388) { *timeout = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case14 运行状态监控 (0x9dc0-0x9fa6)，MENU==0x14 =================
       故障状态屏：TIMEOUT3>0xfa 时重绘标题+状态行；无故障提前 return（跳过超时尾）；
       16 故障位对应状态串（均 row2 col0）。 */
    if (*MENU == 0x14) {
        volatile uint32_t *timeout3 = (volatile uint32_t*)0x10001758;
        volatile uint32_t *timeout  = (volatile uint32_t*)0x10001744;

        if (key == 4) { /* 返回主菜单 */
            *MENU = 1; disp_splash_screen();
            return;
        }
        (*timeout3)++;
        if (*timeout3 > 0xfa) {
            *timeout3 = 0;
            disp_string(0xa0b0, 0, 4, 0); /* 标题 */
            if (*FAULT == 0) {
                disp_string(0xa0c0, 2, 0, 0); /* 正常 */
                return; /* 0x9e0e 提前返回（不走超时尾） */
            }
            /* 各故障位对应状态串（顺序 if，非 else-if，多个故障位可同时画） */
            if (*FAULT & 0x4)   disp_string(0xa0d4, 2, 0, 0);
            if (*FAULT & 0x2)   disp_string(0xa0d4, 2, 0, 0);
            if (*FAULT & 0x1)   disp_string(0xa0d4, 2, 0, 0);
            if (*FAULT & 0x8)   disp_string(0xa0e8, 2, 0, 0);
            if (*FAULT & 0x10)  disp_string(0xa0fc, 2, 0, 0);
            if (*FAULT & 0x20)  disp_string(0xa110, 2, 0, 0);
            if (*FAULT & 0x40)  disp_string(0xa124, 2, 0, 0);
            if (*FAULT & 0x80)  disp_string(0xa138, 2, 0, 0);
            if (*FAULT & 0x100) disp_string(0xa14c, 2, 0, 0);
            if (*FAULT & 0x200) disp_string(0xa160, 2, 0, 0);
            if (*FAULT & 0x400) disp_string(0xa174, 2, 0, 0);
            if (*FAULT & 0x800) disp_string(0xa188, 2, 0, 0);
            if (*FAULT & 0x1000) disp_string(0xa19c, 2, 0, 0);
            if (*FAULT & 0x4000) disp_string(0xa1b0, 2, 0, 0);
            if (*FAULT & 0x8000) disp_string(0xa1c4, 2, 0, 0);
            if (*FAULT & 0x2000) disp_string(0xa1d8, 2, 0, 0);
        }
        /* 超时尾（0x1388=5000） */
        (*timeout)++;
        if (*timeout >= 0x1388) { *timeout = 0; *MENU = 1; disp_splash_screen(); }
        return;
    }

    /* ================= case1E 运行主界面 (0x9fae-0xa854)，MENU==0x1e =================
       含完整 RUN/STOP 状态机（与 case1 同套门控：复位斜坡、RESET_MODE、ESTOP、
       急停、块A/块B、key5/key6、幅值块）。块尾不加 return，直接落入共享超时尾
       (0xa828-0xa854)：(*TIMEOUT)++; >=0x1388 回主屏。 */
    if (*MENU == 0x1e) {
        volatile uint32_t *t2  = (volatile uint32_t*)0x10001740; /* TIMEOUT2 */
        volatile uint32_t *t3  = (volatile uint32_t*)0x10001758; /* TIMEOUT3 */

        /* key==4 回主菜单 (0x9fb2) */
        if (key == 4) { *MENU = 1; disp_splash_screen(); *TIMEOUT = 0; return; }

        /* key==1 且停机 → 初始参数屏 (0x9fc4) */
        if (key == 1 && *CFG == 0) {
            disp_clear();
            *MENU = 0xa; *MENU2 = 0;
            *TIMEOUT = 0; *t2 = 0x3c; *IDLE = 0;
            disp_string(0x4cf4, 1, 0, 0);
            disp_string(0x4d04, 3, 7, 0);
            return;
        }

        /* ---- IDLE 周期刷新 + 状态/模式行 (0xa00c-0xa2c0)：每 0x15e 帧 ---- */
        (*IDLE)++;
        if (*IDLE >= 0x15e) {
            *IDLE = 0;
            disp_uint4(*(volatile uint32_t*)0x10001598, 0, 9, 0);
            disp_uint4(*(volatile uint32_t*)0x1000159c, 1, 9, 0);
            disp_uint4(*(volatile uint32_t*)0x100015a0, 2, 9, 0);
            if (*FAULT != 0) {
                *STATUS = 0;
                disp_string(0x4334, 3, 0xa, 0); /* 故障 */
            } else if (*CFG == 0 && *STATUS != 1) {
                *STATUS = 1;
                disp_string(0x4340, 3, 0xa, 0); /* 停机 */
            }
            if (*CTRL_MODE == 0) {
                if (*MODE_L != 1) { *MODE_L = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string(0x4354, 3, 0, 0); }
            }
            if (*CTRL_MODE == 1) {
                if (*MODE_L != 2) { *MODE_L = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string(0x435c, 3, 0, 0); }
            }
            if (*CTRL_MODE == 2) {
                if (*MODE_L != 3) { *MODE_L = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); disp_string(0x4364, 3, 0, 0); }
            }
        }

        /* ---- 复位去抖 P1.17 → 故障停机斜坡 (0xa2c4) ---- */
        *DB = debounce_p117();
        if (*FAULT != 0 && *DB == 2 && *RESET_MODE == 0) {
            *FAULT = 0; *CFG = 0; *STOP = 1; *RUN = 0;
            disp_string(0x4d6c, 3, 0xa, 0);
            *DELAY_OUT = 0;
            do { *DELAY_IN = 0; do { (*DELAY_IN)++; } while (*DELAY_IN < 0x7d0); wd_feed(); (*DELAY_OUT)++; } while (*DELAY_OUT < 0xbb8);
            disp_string(0x4d7c, 3, 0xa, 0);
            *DELAY_OUT = 0;
            do { *DELAY_IN = 0; do { (*DELAY_IN)++; } while (*DELAY_IN < 0x7d0); wd_feed(); (*DELAY_OUT)++; } while (*DELAY_OUT < 0xbb8);
            for (;;) { } /* 故障停机后锁定，等看门狗复位 */
        }
        if (*RESET_MODE == 1) {
            if (*DB != 2 && *MODE_L != 1) {
                *MODE_L2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
                disp_string(0x4354, 3, 0, 0);
            }
            if (*DB == 2 && *MODE_L != 2) {
                *MODE_L2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
                disp_string(0x435c, 3, 0, 0);
            }
        }
        if (*RESET_MODE == 2) {
            if (*DB != 2) *RESET2 = 0;
            if (*DB == 2) *RESET2 = 1;
        }

        /* ---- 急停去抖 P0.6 → 停机并锁定 (0xa42c) ---- */
        *DB = debounce_p06();
        if (*FAULT == 0 && *DB == 2 && *ESTOP == 0) {
            *RUN_REQ = 1; *CFG = 0; *STOP = 1; *RUN = 0;
            if (*STAT_FL == 0) { disp_string(0x4340, 3, 0xa, 0); *STAT_FL = 1; }
            return;
        }
        if (*ESTOP == 1) {
            if (*DB != 2 && *MODE_L != 1) {
                *MODE_L2 = 1; *CTRL_MODE = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
                disp_string(0x4354, 3, 0, 0);
            }
            if (*DB == 2 && *MODE_L != 2) {
                *MODE_L2 = 2; *CTRL_MODE = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
                disp_string(0x435c, 3, 0, 0);
            }
        }
        if (*ESTOP == 2) {
            if (*DB != 2) *RESET2 = 0;
            if (*DB == 2) *RESET2 = 1;
        }
        if (*RESET_MODE != 2 && *ESTOP != 2) *RESET2 = 0;

        *SCAN_RS = scan_run_stop();

        /* ---- 块A 持续运行 (0xa534)：run_en==1 且显示正常 → 强制 RUN ---- */
        if (*FAULT == 0 && *RUN == 0 && *RUN_EN == 1 && *DISP_SEL == 0) {
            *RUN = 1; *STOP = 0; *RUN_REQ = 0; *CFG = 1; *STAT_FL = 0;
            *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
            disp_string(0x4348, 3, 0xa, 0); /* 运行 */
        }
        /* ---- 块B 持续停机 (0xa584)：run_en==0 且显示正常 → 强制 STOP ---- */
        if (*FAULT == 0 && *STOP == 0 && *RUN_EN == 0 && *DISP_SEL == 0) {
            *STOP = 1; *RUN = 0; *CFG = 0;
            disp_string(0x4340, 3, 0xa, 0); /* 停机 */
        }
        if (*CFG == 0 && *DISP_SEL != 0) *RUN_EN = 0; /* 0xa5ba */

        /* ---- key5/扫描7 启动 (0xa5cc) ---- */
        if (*FAULT == 0 && *RUN == 0) {
            if (key == 5 || *SCAN_RS == 7) {
                if (*PAIR_MODE == 0 || (*SCAN_RS == 7 && *PAIR_MODE == 1 && *DISP_SEL != 0)) {
                    *RUN_EN = 1; *RUN = 1; *STOP = 0; *RUN_REQ = 0; *CFG = 1; *STAT_FL = 0;
                    *TICK = 0; *MIN_NOW = 0; *HOUR_NOW = 0;
                    disp_string(0x4348, 3, 0xa, 0);
                }
            }
        }
        /* ---- key6/扫描8 停机 (0xa6a2) ---- */
        if (*FAULT == 0 && *STOP == 0) {
            if (key == 6 || *SCAN_RS == 8) {
                if (*PAIR_MODE == 0 || (*SCAN_RS == 8 && *PAIR_MODE == 1 && *DISP_SEL != 0)) {
                    *RUN_EN = 0; *STOP = 1; *RUN = 0; *CFG = 0;
                    disp_string(0x4340, 3, 0xa, 0);
                }
            }
        }

        /* ---- 幅值/频率计算 (0xa6fa)：DISP_SEL 0=自动 1=显示保持 2=手动 ---- */
        if (*DISP_SEL == 0) {
            *TARGET = *HSRC;
            if (*CTRL_MODE == 0) *DCALC = (*HSRC * *GAIN0) / 1000;
            if (*CTRL_MODE == 1) *DCALC = (*HSRC * *GAIN1) / 1000;
            *V_AMP = *DCALC; *V_AMP2 = *DCALC;
        }
        if (*DISP_SEL == 1) *V_AMP = *V_AMP2;
        if (*DISP_SEL == 2) { /* 手动幅值 (0xa762) */
            if (key == 2 || key == 0x16) {
                (*MANUAL)++;
                if (*MANUAL >= 0x3e8) *MANUAL = 0x3e8;
                if (*MANUAL <= 0xa)  *MANUAL = 0xa;
                disp_fixed_1dec(*MANUAL, 0, 9, 0);
            }
            if (key == 3 || key == 0x21) {
                if (*MANUAL <= 0xa) *MANUAL = 1;
                (*MANUAL)--;
                disp_fixed_1dec(*MANUAL, 0, 9, 0);
            }
            *TARGET = *MANUAL;
            if (*CTRL_MODE == 0) *V5D4 = (*MANUAL * *GAIN0) / 1000;
            if (*CTRL_MODE == 1) *V5D4 = (*MANUAL * *GAIN1) / 1000;
            *V_AMP = *V5D4; *V_AMP2 = *V5D4;
        }

        /* ---- 共享超时尾 (0xa828-0xa854)：case1E 块尾不加 return 直接落入 ---- */
        (*TIMEOUT)++;
        if (*TIMEOUT >= 0x1388) {
            *TIMEOUT = 0;
            *MENU = 1;
            disp_splash_screen();
        }
        return;
    }
}
