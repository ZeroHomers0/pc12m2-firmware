/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 06：参数系统（EEPROM 装载 / live→EEPROM 同步）
 *
 * 存储：AT24C02C EEPROM @0x53（byte 寻址，见 04_i2c.c），双银行备份：
 *   银行 A 魔数 reg5/6 == 'U'(0x55)
 *       有效 → 从芯片 regs 0x0A..0x9C 读入 live 区 0x10002A0C..0x10002ABC
 *       无效 → 用默认区 0x10002EC4..0x10002F68 回写芯片 + reg5/6=0x55
 *   银行 B 魔数 reg7/8 == 'f'(0x66)
 *       有效 → 从芯片 regs 0x1F..0xBF 读入 live 区 0x10003398..0x100033D4
 *       无效 → 用默认区回写芯片 + reg7/8=0x66
 *   随后 shadow→live 拷贝（0x100033D8→0x100033DC … 0x10003970→0x10003978），
 *   再按 cfg_1710(0x100038C4，控制方式) 选择活动增益对 → 0x10003980/0x10003984
 *
 * param_sync_live_to_eeprom：live(0x10003988..) 与 EEPROM 缓存副本(0x1000398C..)
 *   逐参数比对，不一致即写回芯片对应寄存器（16 位分高低两字节）。
 *   寄存器号→语义对应关系见 MENU_PARAMETER_MAPPING.md / load_config 地址映射。
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · 61 组 live↔shadow↔EEPROM 同步全表 → docs/PROGRESS_2026-08-20.md §2
 *   · 双银行魔数 'U'(0x55) / 'f'(0x66) → docs/i2c_param_sync.md
 *   · 参数组 / 活动增益对 → docs/PLAN.md「关键符号速查」
 * ========================================================================== */

/* =============================================================================
 * src/06_param_system.c — 反编译模块 06（参数系统：live↔shadow↔EEPROM 同步）可编译副本
 * 目标B 阶段4：补 include。&DAT_x 地址伪影/符号语义按需修正。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"

/* 跨模块前向声明：i2c_read_reg/i2c_write_reg 定义在 04_i2c.c */
void i2c_write_reg(undefined4 data,undefined4 reg_addr);
void i2c_read_reg(undefined1 *out_buf,undefined4 reg_addr);

/* 0x000025DC —— 上电装载配置（main 启动序列第 11 步 load_config()）
 *   · 银行 A：reg5/6 任一 == 'U' 则整组从 EEPROM 读入 live；
 *     否则以 0x10002EC4.. 默认值整组回写并置魔数 0x55
 *   · 银行 B：reg7/8 任一 == 'f' 则整组读入；否则回写默认 + 置魔数 0x66
 *   · shadow→live 拷贝 + 增益对选择
 * 局部（i2c_read_reg 逐字节读回）：
 *   rd_lo / rd_hi —— 读回字节临时值；16 位参数各占一个 EEPROM 字节，
 *     拼合成 *(uint*)DAT_x = (rd_hi<<8)|rd_lo；单字节参数仅用 rd_lo
 *   dst_shadow —— 银行 B shadow→live 拷贝目的指针（0x1000390C） */
void load_config(void)
{
  volatile uint8_t *dst_shadow;
  uint8_t rd_hi;
  uint8_t rd_lo;

  rd_lo = 0;
  rd_hi = 0;
  i2c_read_reg(&rd_lo,5);
  i2c_read_reg(&rd_hi,6);
  if (((char)rd_lo == 'U') || ((char)rd_hi == 'U')) {
    i2c_read_reg(&rd_lo,10);
    *DAT_00002a0c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0xb);
    i2c_read_reg(&rd_hi,0xc);
    *(volatile uint *)DAT_00002a10 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd);
    i2c_read_reg(&rd_hi,0xe);
    *(volatile uint *)DAT_00002a14 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xf);
    *DAT_00002a18 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x10);
    i2c_read_reg(&rd_hi,0x11);
    *DAT_00002a1c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x12);
    *DAT_00002a20 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x13);
    i2c_read_reg(&rd_hi,0x14);
    *DAT_00002a24 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x15);
    *DAT_00002a28 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x16);
    i2c_read_reg(&rd_hi,0x17);
    *DAT_00002a2c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x18);
    i2c_read_reg(&rd_hi,0x19);
    *DAT_00002a30 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x1a);
    *DAT_00002a34 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1b);
    *DAT_00002a38 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1c);
    *DAT_00002a3c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1d);
    i2c_read_reg(&rd_hi,0x1e);
    *DAT_00002a40 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x32);
    i2c_read_reg(&rd_hi,0x33);
    *DAT_00002a44 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x34);
    *DAT_00002a48 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x35);
    i2c_read_reg(&rd_hi,0x36);
    *DAT_00002a4c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x37);
    *DAT_00002a50 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x38);
    i2c_read_reg(&rd_hi,0x39);
    *DAT_00002a54 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3a);
    *DAT_00002a58 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3b);
    i2c_read_reg(&rd_hi,0x3c);
    *DAT_00002a5c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3d);
    *DAT_00002a60 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3e);
    *DAT_00002a64 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3f);
    *DAT_00002a68 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5a);
    *DAT_00002a6c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5b);
    *DAT_00002a70 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5c);
    *DAT_00002a74 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5d);
    *DAT_00002a78 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5e);
    *DAT_00002a7c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5f);
    *DAT_00002a80 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x60);
    *DAT_00002a84 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x61);
    *DAT_00002a88 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x62);
    *DAT_00002a8c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x6e);
    *DAT_00002a90 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x6f);
    *DAT_00002a94 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x70);
    *DAT_00002a98 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x71);
    *DAT_00002a9c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x72);
    *DAT_00002aa0 = (char)rd_lo;
    i2c_read_reg(&rd_lo,100);
    *DAT_00002aa4 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x65);
    i2c_read_reg(&rd_hi,0x66);
    *DAT_00002aa8 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x67);
    *DAT_00002aac = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x68);
    *DAT_00002ab0 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x97);
    i2c_read_reg(&rd_hi,0x98);
    *DAT_00002ab4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x99);
    i2c_read_reg(&rd_hi,0x9a);
    *DAT_00002ab8 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x9b);
    i2c_read_reg(&rd_hi,0x9c);
    *DAT_00002abc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 A 无魔数：用默认区整组回写 —— */
    i2c_write_reg(*DAT_00002a0c,10);
    i2c_write_reg(*DAT_00002a10 >> 8,0xb);
    i2c_write_reg((char)*DAT_00002a10,0xc);
    i2c_write_reg(*DAT_00002a14 >> 8,0xd);
    i2c_write_reg((char)*DAT_00002a14,0xe);
    i2c_write_reg(*DAT_00002ec4,0xf);
    i2c_write_reg(*DAT_00002ec8 >> 8,0x10);
    i2c_write_reg((char)*DAT_00002ec8,0x11);
    i2c_write_reg(*DAT_00002ecc,0x12);
    i2c_write_reg(*DAT_00002ed0 >> 8,0x13);
    i2c_write_reg((char)*DAT_00002ed0,0x14);
    i2c_write_reg(*DAT_00002ed4,0x15);
    i2c_write_reg(*DAT_00002ed8 >> 8,0x16);
    i2c_write_reg((char)*DAT_00002ed8,0x17);
    i2c_write_reg(*DAT_00002edc >> 8,0x18);
    i2c_write_reg((char)*DAT_00002edc,0x19);
    i2c_write_reg(*DAT_00002ee0,0x1a);
    i2c_write_reg(*DAT_00002ee4,0x1b);
    i2c_write_reg(*DAT_00002ee8,0x1c);
    i2c_write_reg(*DAT_00002eec >> 8,0x1d);
    i2c_write_reg((char)*DAT_00002eec,0x1e);
    i2c_write_reg(*DAT_00002ef0 >> 8,0x32);
    i2c_write_reg((char)*DAT_00002ef0,0x33);
    i2c_write_reg(*DAT_00002ef4,0x34);
    i2c_write_reg(*DAT_00002ef8 >> 8,0x35);
    i2c_write_reg((char)*DAT_00002ef8,0x36);
    i2c_write_reg(*DAT_00002efc,0x37);
    i2c_write_reg(*DAT_00002f00 >> 8,0x38);
    i2c_write_reg((char)*DAT_00002f00,0x39);
    i2c_write_reg(*DAT_00002f04,0x3a);
    i2c_write_reg(*DAT_00002f08 >> 8,0x3b);
    i2c_write_reg((char)*DAT_00002f08,0x3c);
    i2c_write_reg(*DAT_00002f0c,0x3d);
    i2c_write_reg(*DAT_00002f10,0x3e);
    i2c_write_reg(*DAT_00002f14,0x3f);
    i2c_write_reg(*DAT_00002f18,0x5a);
    i2c_write_reg(*DAT_00002f1c,0x5b);
    i2c_write_reg(*DAT_00002f20,0x5c);
    i2c_write_reg(*DAT_00002f24,0x5d);
    i2c_write_reg(*DAT_00002f28,0x5e);
    i2c_write_reg(*DAT_00002f2c,0x5f);
    i2c_write_reg(*DAT_00002f30,0x60);
    i2c_write_reg(*DAT_00002f34,0x61);
    i2c_write_reg(*DAT_00002f38,0x62);
    i2c_write_reg(*DAT_00002f3c,0x6e);
    i2c_write_reg(*DAT_00002f40,0x6f);
    i2c_write_reg(*DAT_00002f44,0x70);
    i2c_write_reg(*DAT_00002f48,0x71);
    i2c_write_reg(*DAT_00002f4c,0x72);
    i2c_write_reg(*DAT_00002f50,100);
    i2c_write_reg(*DAT_00002f54 >> 8,0x65);
    i2c_write_reg((char)*DAT_00002f54,0x66);
    i2c_write_reg(*DAT_00002f58,0x67);
    i2c_write_reg(*DAT_00002f5c,0x68);
    i2c_write_reg(*DAT_00002f60 >> 8,0x97);
    i2c_write_reg((char)*DAT_00002f60,0x98);
    i2c_write_reg(*DAT_00002f64 >> 8,0x99);
    i2c_write_reg((char)*DAT_00002f64,0x9a);
    i2c_write_reg(*DAT_00002f68 >> 8,0x9b);
    i2c_write_reg((char)*DAT_00002f68,0x9c);
    i2c_write_reg(0x55,5);
    i2c_write_reg(0x55,6);
  }
  i2c_read_reg(&rd_lo,7);
  i2c_read_reg(&rd_hi,8);
  if (((char)rd_lo == 'f') || ((char)rd_hi == 'f')) {
    i2c_read_reg(&rd_lo,0x1f);
    *DAT_00003398 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x20);
    *DAT_0000339c = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x21);
    *DAT_000033a0 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x22);
    *DAT_000033a4 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x23);
    *DAT_000033a8 = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x24);
    i2c_read_reg(&rd_hi,0x25);
    *(volatile uint *)DAT_000033ac = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x26);
    i2c_read_reg(&rd_hi,0x27);
    *(volatile uint *)DAT_000033b0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xc9);
    i2c_read_reg(&rd_hi,0xca);
    *(volatile uint *)DAT_000033b4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcb);
    i2c_read_reg(&rd_hi,0xcc);
    *(volatile uint *)DAT_000033b8 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcd);
    i2c_read_reg(&rd_hi,0xce);
    *(volatile uint *)DAT_000033bc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcf);
    i2c_read_reg(&rd_hi,0xd0);
    *(volatile uint *)DAT_000033c0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd1);
    i2c_read_reg(&rd_hi,0xd2);
    *(volatile uint *)DAT_000033c4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd3);
    i2c_read_reg(&rd_hi,0xd4);
    *(volatile uint *)DAT_000033c8 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xba);
    i2c_read_reg(&rd_hi,0xbb);
    *DAT_000033cc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xbc);
    i2c_read_reg(&rd_hi,0xbd);
    *(volatile uint *)DAT_000033d0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xbe);
    i2c_read_reg(&rd_hi,0xbf);
    *(volatile uint *)DAT_000033d4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 B 无魔数：用默认区整组回写 —— */
    i2c_write_reg(*DAT_00003398,0x1f);
    i2c_write_reg(*DAT_0000339c,0x20);
    i2c_write_reg(*DAT_000033a0,0x21);
    i2c_write_reg(*DAT_000033a4,0x22);
    i2c_write_reg(*DAT_000033a8,0x23);
    i2c_write_reg(*DAT_000033ac >> 8,0x24);
    i2c_write_reg((char)*DAT_000033ac,0x25);
    i2c_write_reg(*DAT_000033b0 >> 8,0x26);
    i2c_write_reg((char)*DAT_000033b0,0x27);
    i2c_write_reg(*DAT_000033b4 >> 8,0xc9);
    i2c_write_reg((char)*DAT_000033b4,0xca);
    i2c_write_reg(*DAT_000033b8 >> 8,0xcb);
    i2c_write_reg((char)*DAT_000033b8,0xcc);
    i2c_write_reg(*DAT_000033bc >> 8,0xcd);
    i2c_write_reg((char)*DAT_000033bc,0xce);
    i2c_write_reg(*DAT_000033c0 >> 8,0xcf);
    i2c_write_reg((char)*DAT_000033c0,0xd0);
    i2c_write_reg(*DAT_000033c4 >> 8,0xd1);
    i2c_write_reg((char)*DAT_000033c4,0xd2);
    i2c_write_reg(*DAT_000033c8 >> 8,0xd3);
    i2c_write_reg((char)*DAT_000033c8,0xd4);
    i2c_write_reg(*DAT_000033d0 >> 8,0xbc);
    i2c_write_reg((char)*DAT_000033d0,0xbd);
    i2c_write_reg(*DAT_000033d4 >> 8,0xbe);
    i2c_write_reg((char)*DAT_000033d4,0xbf);
    i2c_write_reg(0x66,7);
    i2c_write_reg(0x66,8);
  }
  /* —— shadow→live 拷贝（银行 B 默认区 → 活动参数区）—— */
  *g_gain_sel = *DAT_000033d8;
  *DAT_000033e4 = *DAT_000033e0;
  *DAT_000033ec = *DAT_000033e8;
  *DAT_000033f4 = *DAT_000033f0;
  *DAT_000033fc = *DAT_000033f8;
  *DAT_00003404 = *DAT_00003400;
  *DAT_0000340c = *DAT_00003408;
  *g_out_fine = *DAT_00003410;
  *g_gain_b = *DAT_00003418;
  *g_gain_a = *DAT_00003420;
  *DAT_0000342c = *DAT_00003428;
  *DAT_00003434 = *DAT_00003430;
  *g_phase_calib = *DAT_00003438;
  *DAT_00003444 = *DAT_00003440;
  *DAT_00003448 = *DAT_00003398;      /* 保护参数首字节 */
  *DAT_0000344c = *DAT_000033a4;
  *g_out_phase = *DAT_000033a8;
  *DAT_00003454 = *DAT_0000339c;
  *DAT_00003458 = *DAT_000033a0;
  *DAT_0000345c = *(volatile undefined4 *)DAT_000033b8;
  *DAT_00003460 = *(volatile undefined4 *)DAT_000033bc;
  *DAT_00003464 = *(volatile undefined4 *)DAT_000033c0;
  *DAT_00003468 = *(volatile undefined4 *)DAT_000033c4;
  *DAT_0000346c = *(volatile undefined4 *)DAT_000033c8;
  *g_reg61_remote_en = *(volatile undefined4 *)DAT_000033ac;
  *g_reg62_start_phase = *(volatile undefined4 *)DAT_000033b0;
  *DAT_0000347c = *DAT_00003478;
  *DAT_0000387c = *DAT_00003878;      /* PID / 通讯区 */
  *DAT_00003884 = *DAT_00003880;
  *DAT_0000388c = *DAT_00003888;
  *DAT_00003894 = *DAT_00003890;
  *DAT_0000389c = *DAT_00003898;
  *DAT_000038a4 = *DAT_000038a0;
  *DAT_000038ac = *DAT_000038a8;
  *DAT_000038b4 = *DAT_000038b0;
  *DAT_000038bc = *DAT_000038b8;
  *g_cfg_pid_sel = *DAT_000038c0;
  *DAT_000038cc = *DAT_000038c8;
  *DAT_000038d4 = *DAT_000038d0;
  *DAT_000038dc = *DAT_000038d8;
  *DAT_000038e4 = *DAT_000038e0;
  *DAT_000038ec = *DAT_000038e8;
  *DAT_000038f4 = *DAT_000038f0;
  *DAT_000038fc = *DAT_000038f8;
  *DAT_00003904 = *DAT_00003900;
  dst_shadow = g_cl_thresh_hi;
  *g_cl_thresh_hi = *DAT_00003908;
  *dst_shadow = *DAT_00003908;
  *g_cl_gain_big = *DAT_00003910;
  *g_cl_gain_mid = *DAT_00003918;
  *g_cl_gain_small = *DAT_00003920;
  *g_slave_addr = *DAT_00003928;
  *g_baud_idx = *DAT_00003930;
  *g_uart_frame_sel = *DAT_00003938;
  *g_comm_detect = *DAT_00003940;
  *DAT_0000394c = *DAT_00003948;
  *DAT_00003954 = *DAT_00003950;
  *DAT_0000395c = *DAT_00003958;
  *DAT_00003964 = *DAT_00003960;
  *DAT_0000396c = *DAT_00003968;
  *DAT_00003974 = *DAT_00003970;
  *DAT_0000397c = *DAT_00003978;
  /* —— 按控制方式选择活动增益对 → 0x10003980(gain_a 电压量程)/0x10003984(gain_b 电流量程) —— */
  if (*g_cfg_pid_sel == '\x01') {
    *g_act_gain_a = *DAT_000038cc;
    *g_act_gain_b = *DAT_000038d4;
  }
  if (*g_cfg_pid_sel == '\x02') {
    *g_act_gain_a = *DAT_000038dc;
    *g_act_gain_b = *DAT_000038e4;
  }
  if (*g_cfg_pid_sel == '\x03') {
    *g_act_gain_a = *DAT_000038ec;
    *g_act_gain_b = *DAT_000038f4;
  }
  if (*g_cfg_pid_sel == '\x04') {
    *g_act_gain_a = *DAT_000038fc;
    *g_act_gain_b = *DAT_00003904;
  }
  return;
}


/* 0x000035F2 —— 参数 live→EEPROM 同步（认证通过后 param_sync_live_to_eeprom() 调用；
 *   主循环/菜单修改参数时也会触发）
 *   结构：对每个参数比对 live(0x10003988..) 与 EEPROM 缓存副本(0x1000398C..)，
 *   不等则更新缓存并 i2c_write_reg 写回芯片（16 位参数分高低字节：
 *     *reg >> 8 写高字节，低字节写下一寄存器 (char)*reg）。
 *   寄存器号序列（节选）：
 *     0x0A..0x1F 基本参数 | 0x20..0x3F | 0x5A..0x62 | 0x64..0x68 | 0x6E..0x72
 *     | 0x97..0x9C | 0xBA..0xBF 保护参数 | 0xC9..0xD4 PID/通讯参数
 *   局部：shadow = 当前参数的 EEPROM 缓存副本指针（16 位参数为 *(int*)shadow） */
void param_sync_live_to_eeprom(void)
{
  volatile uint8_t *shadow;

  shadow = DAT_0000398c;
  if (*g_gain_sel != *DAT_0000398c) {
    *DAT_0000398c = *g_gain_sel;
    i2c_write_reg(*shadow,10);
  }
  if (*DAT_00003990 != *(volatile int *)DAT_00003994) {
    *(volatile int *)DAT_00003994 = *DAT_00003990;
    i2c_write_reg(*DAT_00003994 >> 8,0xb);
    i2c_write_reg((char)*DAT_00003994,0xc);
  }
  if (*DAT_00003998 != *(volatile int *)DAT_0000399c) {
    *(volatile int *)DAT_0000399c = *DAT_00003998;
    i2c_write_reg(*DAT_0000399c >> 8,0xd);
    i2c_write_reg((char)*DAT_0000399c,0xe);
  }
  shadow = DAT_000039a4;
  if (*DAT_000039a0 != *DAT_000039a4) {
    *DAT_000039a4 = *DAT_000039a0;
    i2c_write_reg(*shadow,0xf);
  }
  if (*DAT_000039a8 != *(volatile int *)DAT_000039ac) {
    *(volatile int *)DAT_000039ac = *DAT_000039a8;
    i2c_write_reg(*DAT_000039ac >> 8,0x10);
    i2c_write_reg((char)*DAT_000039ac,0x11);
  }
  shadow = DAT_000039b4;
  if (*DAT_000039b0 != *DAT_000039b4) {
    *DAT_000039b4 = *DAT_000039b0;
    i2c_write_reg(*shadow,0x12);
  }
  if (*DAT_000039b8 != *(volatile int *)DAT_000039bc) {
    *(volatile int *)DAT_000039bc = *DAT_000039b8;
    i2c_write_reg(*DAT_000039bc >> 8,0x13);
    i2c_write_reg((char)*DAT_000039bc,0x14);
  }
  shadow = DAT_000039c4;
  if (*g_out_fine != *DAT_000039c4) {
    *DAT_000039c4 = *g_out_fine;
    i2c_write_reg(*shadow,0x15);
  }
  if (*g_gain_b != *(volatile int *)DAT_000039cc) {
    *(volatile int *)DAT_000039cc = *g_gain_b;
    i2c_write_reg(*DAT_000039cc >> 8,0x16);
    i2c_write_reg((char)*DAT_000039cc,0x17);
  }
  if (*g_gain_a != *(volatile int *)DAT_000039d4) {
    *(volatile int *)DAT_000039d4 = *g_gain_a;
    i2c_write_reg(*DAT_000039d4 >> 8,0x18);
    i2c_write_reg((char)*DAT_000039d4,0x19);
  }
  shadow = DAT_000039dc;
  if (*DAT_000039d8 != *DAT_000039dc) {
    *DAT_000039dc = *DAT_000039d8;
    i2c_write_reg(*shadow,0x1a);
  }
  shadow = DAT_000039e4;
  if (*DAT_000039e0 != *DAT_000039e4) {
    *DAT_000039e4 = *DAT_000039e0;
    i2c_write_reg(*shadow,0x1b);
  }
  shadow = DAT_000039ec;
  if (*g_phase_calib != *DAT_000039ec) {
    *DAT_000039ec = *g_phase_calib;
    i2c_write_reg(*shadow,0x1c);
  }
  if (*DAT_000039f0 != *(volatile int *)DAT_000039f4) {
    *(volatile int *)DAT_000039f4 = *DAT_000039f0;
    i2c_write_reg(*DAT_000039f4 >> 8,0x1d);
    i2c_write_reg((char)*DAT_000039f4,0x1e);
  }
  shadow = DAT_000039fc;
  if (*DAT_000039f8 != *DAT_000039fc) {
    *DAT_000039fc = *DAT_000039f8;
    i2c_write_reg(*shadow,0x1f);
  }
  shadow = DAT_00003e04;
  if (*DAT_00003e00 != *DAT_00003e04) {
    *DAT_00003e04 = *DAT_00003e00;
    i2c_write_reg(*shadow,0x20);
  }
  shadow = DAT_00003e0c;
  if (*DAT_00003e08 != *DAT_00003e0c) {
    *DAT_00003e0c = *DAT_00003e08;
    i2c_write_reg(*shadow,0x21);
  }
  shadow = DAT_00003e14;
  if (*DAT_00003e10 != *DAT_00003e14) {
    *DAT_00003e14 = *DAT_00003e10;
    i2c_write_reg(*shadow,0x22);
  }
  shadow = DAT_00003e1c;
  if (*g_out_phase != *DAT_00003e1c) {
    *DAT_00003e1c = *g_out_phase;
    i2c_write_reg(*shadow,0x23);
  }
  if (*g_reg61_remote_en != *(volatile int *)DAT_00003e24) {
    *(volatile int *)DAT_00003e24 = *g_reg61_remote_en;
    i2c_write_reg(*DAT_00003e24 >> 8,0x24);
    i2c_write_reg((char)*DAT_00003e24,0x25);
  }
  if (*g_reg62_start_phase != *(volatile int *)DAT_00003e2c) {
    *(volatile int *)DAT_00003e2c = *g_reg62_start_phase;
    i2c_write_reg(*DAT_00003e2c >> 8,0x26);
    i2c_write_reg((char)*DAT_00003e2c,0x27);
  }
  if (*DAT_00003e30 != *(volatile int *)DAT_00003e34) {
    *(volatile int *)DAT_00003e34 = *DAT_00003e30;
    i2c_write_reg(*DAT_00003e34 >> 8,0x32);
    i2c_write_reg((char)*DAT_00003e34,0x33);
  }
  shadow = DAT_00003e3c;
  if (*DAT_00003e38 != *DAT_00003e3c) {
    *DAT_00003e3c = *DAT_00003e38;
    i2c_write_reg(*shadow,0x34);
  }
  if (*DAT_00003e40 != *(volatile int *)DAT_00003e44) {
    *(volatile int *)DAT_00003e44 = *DAT_00003e40;
    i2c_write_reg(*DAT_00003e44 >> 8,0x35);
    i2c_write_reg((char)*DAT_00003e44,0x36);
  }
  shadow = DAT_00003e4c;
  if (*DAT_00003e48 != *DAT_00003e4c) {
    *DAT_00003e4c = *DAT_00003e48;
    i2c_write_reg(*shadow,0x37);
  }
  if (*DAT_00003e50 != *(volatile int *)DAT_00003e54) {
    *(volatile int *)DAT_00003e54 = *DAT_00003e50;
    i2c_write_reg(*DAT_00003e54 >> 8,0x38);
    i2c_write_reg((char)*DAT_00003e54,0x39);
  }
  shadow = DAT_00003e5c;
  if (*DAT_00003e58 != *DAT_00003e5c) {
    *DAT_00003e5c = *DAT_00003e58;
    i2c_write_reg(*shadow,0x3a);
  }
  if (*DAT_00003e60 != *(volatile int *)DAT_00003e64) {
    *(volatile int *)DAT_00003e64 = *DAT_00003e60;
    i2c_write_reg(*DAT_00003e64 >> 8,0x3b);
    i2c_write_reg((char)*DAT_00003e64,0x3c);
  }
  shadow = DAT_00003e6c;
  if (*DAT_00003e68 != *DAT_00003e6c) {
    *DAT_00003e6c = *DAT_00003e68;
    i2c_write_reg(*shadow,0x3d);
  }
  shadow = DAT_00003e74;
  if (*DAT_00003e70 != *DAT_00003e74) {
    *DAT_00003e74 = *DAT_00003e70;
    i2c_write_reg(*shadow,0x3e);
  }
  shadow = DAT_00003e7c;
  if (*DAT_00003e78 != *DAT_00003e7c) {
    *DAT_00003e7c = *DAT_00003e78;
    i2c_write_reg(*shadow,0x3f);
  }
  shadow = DAT_00003e84;
  if (*g_cfg_pid_sel != *DAT_00003e84) {
    *DAT_00003e84 = *g_cfg_pid_sel;
    i2c_write_reg(*shadow,0x5a);
  }
  shadow = DAT_00003e8c;
  if (*DAT_00003e88 != *DAT_00003e8c) {
    *DAT_00003e8c = *DAT_00003e88;
    i2c_write_reg(*shadow,0x5b);
  }
  shadow = DAT_00003e94;
  if (*DAT_00003e90 != *DAT_00003e94) {
    *DAT_00003e94 = *DAT_00003e90;
    i2c_write_reg(*shadow,0x5c);
  }
  shadow = DAT_00003e9c;
  if (*DAT_00003e98 != *DAT_00003e9c) {
    *DAT_00003e9c = *DAT_00003e98;
    i2c_write_reg(*shadow,0x5d);
  }
  shadow = DAT_00003ea4;
  if (*DAT_00003ea0 != *DAT_00003ea4) {
    *DAT_00003ea4 = *DAT_00003ea0;
    i2c_write_reg(*shadow,0x5e);
  }
  shadow = DAT_00003eac;
  if (*DAT_00003ea8 != *DAT_00003eac) {
    *DAT_00003eac = *DAT_00003ea8;
    i2c_write_reg(*shadow,0x5f);
  }
  shadow = DAT_00003eb4;
  if (*DAT_00003eb0 != *DAT_00003eb4) {
    *DAT_00003eb4 = *DAT_00003eb0;
    i2c_write_reg(*shadow,0x60);
  }
  shadow = DAT_00003ebc;
  if (*DAT_00003eb8 != *DAT_00003ebc) {
    *DAT_00003ebc = *DAT_00003eb8;
    i2c_write_reg(*shadow,0x61);
  }
  shadow = DAT_000042c4;
  if (*DAT_000042c0 != *DAT_000042c4) {
    *DAT_000042c4 = *DAT_000042c0;
    i2c_write_reg(*shadow,0x62);
  }
  shadow = DAT_000042cc;
  if (*g_cl_thresh_hi != *DAT_000042cc) {
    *DAT_000042cc = *g_cl_thresh_hi;
    i2c_write_reg(*shadow,0x6e);
  }
  shadow = DAT_000042d4;
  if (*g_cl_thresh_lo != *DAT_000042d4) {
    *DAT_000042d4 = *g_cl_thresh_lo;
    i2c_write_reg(*shadow,0x6f);
  }
  shadow = DAT_000042dc;
  if (*g_cl_gain_big != *DAT_000042dc) {
    *DAT_000042dc = *g_cl_gain_big;
    i2c_write_reg(*shadow,0x70);
  }
  shadow = DAT_000042e4;
  if (*g_cl_gain_mid != *DAT_000042e4) {
    *DAT_000042e4 = *g_cl_gain_mid;
    i2c_write_reg(*shadow,0x71);
  }
  shadow = DAT_000042ec;
  if (*g_cl_gain_small != *DAT_000042ec) {
    *DAT_000042ec = *g_cl_gain_small;
    i2c_write_reg(*shadow,0x72);
  }
  shadow = DAT_000042f4;
  if (*g_slave_addr != *DAT_000042f4) {
    *DAT_000042f4 = *g_slave_addr;
    i2c_write_reg(*shadow,100);
  }
  if (*g_baud_idx != *(volatile int *)DAT_000042fc) {
    *(volatile int *)DAT_000042fc = *g_baud_idx;
    i2c_write_reg(*DAT_000042fc >> 8,0x65);
    i2c_write_reg((char)*DAT_000042fc,0x66);
  }
  shadow = DAT_00004304;
  if (*g_uart_frame_sel != *DAT_00004304) {
    *DAT_00004304 = *g_uart_frame_sel;
    i2c_write_reg(*shadow,0x67);
  }
  shadow = DAT_0000430c;
  if (*g_comm_detect != *DAT_0000430c) {
    *DAT_0000430c = *g_comm_detect;
    i2c_write_reg(*shadow,0x68);
  }
  if (*DAT_00004310 != *(volatile int *)DAT_00004314) {
    *(volatile int *)DAT_00004314 = *DAT_00004310;
    i2c_write_reg(*DAT_00004314 >> 8,0x97);
    i2c_write_reg((char)*DAT_00004314,0x98);
  }
  if (*DAT_00004318 != *(volatile int *)DAT_0000431c) {
    *(volatile int *)DAT_0000431c = *DAT_00004318;
    i2c_write_reg(*DAT_0000431c >> 8,0x99);
    i2c_write_reg((char)*DAT_0000431c,0x9a);
  }
  if (*DAT_00004320 != *(volatile int *)DAT_00004324) {
    *(volatile int *)DAT_00004324 = *DAT_00004320;
    i2c_write_reg(*DAT_00004324 >> 8,0x9b);
    i2c_write_reg((char)*DAT_00004324,0x9c);
  }
  if (*DAT_00004328 != *(volatile int *)DAT_0000432c) {
    *(volatile int *)DAT_0000432c = *DAT_00004328;
    i2c_write_reg(*DAT_0000432c >> 8,0xc9);
    i2c_write_reg((char)*DAT_0000432c,0xca);
  }
  if (*DAT_00004330 != *(volatile int *)DAT_00004334) {
    *(volatile int *)DAT_00004334 = *DAT_00004330;
    i2c_write_reg(*DAT_00004334 >> 8,0xcb);
    i2c_write_reg((char)*DAT_00004334,0xcc);
  }
  if (*DAT_00004338 != *(volatile int *)DAT_0000433c) {
    *(volatile int *)DAT_0000433c = *DAT_00004338;
    i2c_write_reg(*DAT_0000433c >> 8,0xcd);
    i2c_write_reg((char)*DAT_0000433c,0xce);
  }
  if (*DAT_00004340 != *(volatile int *)DAT_00004344) {
    *(volatile int *)DAT_00004344 = *DAT_00004340;
    i2c_write_reg(*DAT_00004344 >> 8,0xcf);
    i2c_write_reg((char)*DAT_00004344,0xd0);
  }
  if (*DAT_00004348 != *(volatile int *)DAT_0000434c) {
    *(volatile int *)DAT_0000434c = *DAT_00004348;
    i2c_write_reg(*DAT_0000434c >> 8,0xd1);
    i2c_write_reg((char)*DAT_0000434c,0xd2);
  }
  if (*DAT_00004350 != *(volatile int *)DAT_00004354) {
    *(volatile int *)DAT_00004354 = *DAT_00004350;
    i2c_write_reg(*DAT_00004354 >> 8,0xd3);
    i2c_write_reg((char)*DAT_00004354,0xd4);
  }
  if (*DAT_00004358 != *(volatile int *)DAT_0000435c) {
    *(volatile int *)DAT_0000435c = *DAT_00004358;
    i2c_write_reg(*DAT_0000435c >> 8,0xba);
    i2c_write_reg((char)*DAT_0000435c,0xbb);
  }
  if (*DAT_00004360 != *(volatile int *)DAT_00004364) {
    *(volatile int *)DAT_00004364 = *DAT_00004360;
    i2c_write_reg(*DAT_00004364 >> 8,0xbc);
    i2c_write_reg((char)*DAT_00004364,0xbd);
  }
  if (*DAT_00004368 != *(volatile int *)DAT_0000436c) {
    *(volatile int *)DAT_0000436c = *DAT_00004368;
    i2c_write_reg(*DAT_0000436c >> 8,0xbe);
    i2c_write_reg((char)*DAT_0000436c,0xbf);
  }
  return;
}
