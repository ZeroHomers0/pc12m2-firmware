/* =============================================================================
 * consts.h — 语义化常量表（firmware/inc）
 *
 * 集中承载 DATA_SEGMENT_2026-08-31.md 已由十二相反汇编确证的魔数，
 * 让源码里的裸数字（0x18BD、0x2C88…）有名字。全部值经 evidence/reverse/disassembly/functions
 * 逐地址核对，语义源自文档与函数上下文。后续常量表落地（任务#4）
 * 在此追加定义并逐步替换源码引用。
 * ========================================================================== */
#ifndef CONSTS_H
#define CONSTS_H

/* ================= SCR 移相触发角 ================= */
#define ANGLE_FULL      0xB4    /* 180°：触发角公式 180° - 当前角 */
#define ANGLE_SCALE     0x18BD  /* 6333(×100)：角度系数，角×6333/100 得 PWM 值 */
#define TRIG_PERIOD     0x2C88  /* 周期基量：触发周期/扫描步进基准 */
#define SOFT_START_INIT 0x1771  /* 6001：软起动斜坡累加初始值 */
#define RANGE_MAX       0x1771  /* 6001：闭环增益/参数量程上限（12_closed_loop 增益钳位） */
#define TRIG_WINDOW     0x36    /* 触发窗口 MR0 */
#define HZ_DIV          0x32    /* 50：频率换算除数 */
#define MR0_50HZ        0x488   /* TIMER1 显示扫描 MR0（50Hz '2'） */
#define MR0_60HZ        0x261   /* TIMER1 显示扫描 MR0（60Hz '<'） */

/* ================= 波特率分频因子（UART3，按档 0..7） ================= */
#define BAUD_FAC_0      0x3BB
#define BAUD_FAC_3      0x3B6
#define BAUD_FAC_4      0x3B1
#define BAUD_FAC_5      0x3AA
#define BAUD_FAC_6      0x39D
#define BAUD_FAC_7      0x393

/* ================= PID 闭环 ================= */
#define PID_CLAMP_HI    0x5CC60  /* 积分累加钳位上界 */
#define PID_CLAMP_LO    0x116520 /* 积分累加钳位下界 */
#define PID_DIV_MODE2   0x46     /* 控制模式 2 的除数 */

/* ================= EEPROM 双银行 magic ================= */
#define EEPROM_MAGIC_U  0x55     /* 'U'：银行 A 校验 */
#define EEPROM_MAGIC_f  0x66     /* 'f'：银行 B 校验 */
#define AUTH_CHAL       0x55     /* 认证挑战固定字节 */

#endif /* CONSTS_H */
