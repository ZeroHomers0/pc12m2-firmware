/* =============================================================================
 * PC12M-2（12 相）LPC1765 反编译源码导出 — 模块 06：参数系统（EEPROM 装载 / live→EEPROM 同步）
 * 固件：pc12m2_orig.bin（12 相孪生板；6p 参照：PC6M-10 06_param_system.c）
 *
 * 存储：AT24C02C EEPROM @0x53（byte 寻址，见 04_i2c.c），双银行备份：
 *   银行 A 魔数 reg5/6 == 'U'(0x55)
 *       有效 → 从芯片 regs 0x0A..0x9A 读入影子区 0x1000165C..（44 项，if 分支）
 *       无效 → 用影子区默认值整组回写（else 分支，58 次写）+ reg5/6=0x55
 *   银行 B 魔数 reg7/8 == 'f'(0x66)
 *       有效 → 从芯片 regs 0x1F..0xD4 读入影子区 0x1000167F..（13 项，if 分支）
 *       无效 → 用影子区默认值整组回写（else 分支，21 次写）+ reg7/8=0x66
 *   随后 shadow→live 拷贝 56 项（0x1000165C→0x1000162C … 0x10001628→0x10001624），
 *   再按 0x10001708（控制方式）选择活动增益对 → pid_kp2(0x10001706)/pid_ki2(0x10001707)
 *
 * 16 位参数 EEPROM 字节序（读回与写回对称）：
 *   读回：*(volatile uint*)slot = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100
 *        （rd_lo 来自小 reg、rd_hi 来自大 reg → rd_lo 为高字节、rd_hi 为低字节）
 *   写回：*slot >> 8 → 小 reg 号，低字节 (char)*slot → 大 reg 号
 * 单字节参数：有符号提取 (char)*(volatile uint8_t*)slot 传给 i2c_write_reg
 *        （内部 i2c_write_byte 做 data=(data<<1)&0xFF 掩码，见 04_i2c.c）。
 *
 * param_sync_live_to_eeprom(0x3534)：live(0x1000162C..) 与 EEPROM 缓存副本(0x1000165C..)
 *   逐参数比对（57 组），不一致则更新 shadow 并写回芯片对应寄存器
 *   （16 位分高低两字节）。方向 live→shadow。
 * 导出：2026-08-30
 *
 * 交叉引用：
 *   · 57 组 live↔shadow↔EEPROM 同步全表 → docs/PROGRESS_*.md（12p 参数系统）
 *   · 双银行魔数 'U'(0x55) / 'f'(0x66) → docs/i2c_param_sync.md
 *   · 16 位参数高低字节序 → 04_i2c.c（i2c_write_byte data=(data<<1)&0xFF）
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

/* 0x0000258C —— 上电装载配置（main 启动序列 load_config()）
 *   · 银行 A：reg5/6 任一 == 'U' 则整组从 EEPROM 读入影子区；
 *     否则以影子区默认值整组回写并置魔数 0x55
 *   · 银行 B：reg7/8 任一 == 'f' 则整组读入；否则回写默认 + 置魔数 0x66
 *   · shadow→live 拷贝 + 增益对选择
 * 局部（i2c_read_reg 逐字节读回）：
 *   rd_lo / rd_hi —— 读回字节临时值；16 位参数各占一个 EEPROM 字节，
 *     拼合成 *(volatile uint*)slot = (rd_hi<<0)|(rd_lo<<8)；单字节参数仅用 rd_lo */
void load_config(void)
{
  uint8_t rd_hi;
  uint8_t rd_lo;

  rd_lo = 0;
  rd_hi = 0;
  i2c_read_reg(&rd_lo,5);
  i2c_read_reg(&rd_hi,6);
  if (((char)rd_lo == 'U') || ((char)rd_hi == 'U')) {
    /* —— 银行 A 有效：从 EEPROM 读入影子区 0x1000165C.. —— */
    i2c_read_reg(&rd_lo,0xa);
    *(volatile uint8_t *)DAT_000029bc = rd_lo;
    i2c_read_reg(&rd_lo,0xb);
    i2c_read_reg(&rd_hi,0xc);
    *(volatile uint *)DAT_000029c0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd);
    i2c_read_reg(&rd_hi,0xe);
    *(volatile uint *)DAT_000029c4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xf);
    *(volatile uint8_t *)DAT_000029c8 = rd_lo;
    i2c_read_reg(&rd_lo,0x10);
    i2c_read_reg(&rd_hi,0x11);
    *(volatile uint *)DAT_000029cc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x12);
    *(volatile uint8_t *)eeprom_param_5 = rd_lo;
    i2c_read_reg(&rd_lo,0x13);
    i2c_read_reg(&rd_hi,0x14);
    *(volatile uint *)DAT_000029d4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x15);
    *(volatile uint8_t *)DAT_000029d8 = rd_lo;
    i2c_read_reg(&rd_lo,0x16);
    i2c_read_reg(&rd_hi,0x17);
    *(volatile uint *)DAT_000029dc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x18);
    i2c_read_reg(&rd_hi,0x19);
    *(volatile uint *)DAT_000029e0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x1a);
    *(volatile uint8_t *)DAT_000029e4 = rd_lo;
    i2c_read_reg(&rd_lo,0x1b);
    *(volatile uint8_t *)eeprom_param_6 = rd_lo;
    i2c_read_reg(&rd_lo,0x1c);
    *(volatile uint8_t *)eeprom_param_11 = rd_lo;
    i2c_read_reg(&rd_lo,0x1d);
    i2c_read_reg(&rd_hi,0x1e);
    *(volatile uint *)DAT_000029f0 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x32);
    i2c_read_reg(&rd_hi,0x33);
    *(volatile uint *)DAT_000029f4 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x34);
    *(volatile uint8_t *)DAT_000029f8 = rd_lo;
    i2c_read_reg(&rd_lo,0x35);
    i2c_read_reg(&rd_hi,0x36);
    *(volatile uint *)DAT_000029fc = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x37);
    *(volatile uint8_t *)DAT_00002a00 = rd_lo;
    i2c_read_reg(&rd_lo,0x38);
    i2c_read_reg(&rd_hi,0x39);
    *(volatile uint *)DAT_00002a04 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3a);
    *(volatile uint8_t *)DAT_00002a08 = rd_lo;
    i2c_read_reg(&rd_lo,0x3b);
    i2c_read_reg(&rd_hi,0x3c);
    *(volatile uint *)DAT_00002a0c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3d);
    *(volatile uint8_t *)DAT_00002a10 = rd_lo;
    i2c_read_reg(&rd_lo,0x3e);
    *(volatile uint8_t *)eeprom_param_14 = rd_lo;
    i2c_read_reg(&rd_lo,0x3f);
    *(volatile uint8_t *)eeprom_param_15 = rd_lo;
    i2c_read_reg(&rd_lo,0x5a);
    *(volatile uint8_t *)DAT_00002a1c = rd_lo;
    i2c_read_reg(&rd_lo,0x5b);
    *(volatile uint8_t *)DAT_00002a20 = rd_lo;
    i2c_read_reg(&rd_lo,0x5c);
    *(volatile uint8_t *)DAT_00002a24 = rd_lo;
    i2c_read_reg(&rd_lo,0x5d);
    *(volatile uint8_t *)DAT_00002a28 = rd_lo;
    i2c_read_reg(&rd_lo,0x5e);
    *(volatile uint8_t *)DAT_00002a2c = rd_lo;
    i2c_read_reg(&rd_lo,0x5f);
    *(volatile uint8_t *)DAT_00002a30 = rd_lo;
    i2c_read_reg(&rd_lo,0x60);
    *(volatile uint8_t *)DAT_00002a34 = rd_lo;
    i2c_read_reg(&rd_lo,0x61);
    *(volatile uint8_t *)DAT_00002a38 = rd_lo;
    i2c_read_reg(&rd_lo,0x62);
    *(volatile uint8_t *)DAT_00002a3c = rd_lo;
    i2c_read_reg(&rd_lo,0x6e);
    *(volatile uint8_t *)DAT_00002a40 = rd_lo;
    i2c_read_reg(&rd_lo,0x6f);
    *(volatile uint8_t *)DAT_00002a44 = rd_lo;
    i2c_read_reg(&rd_lo,0x70);
    *(volatile uint8_t *)DAT_00002a48 = rd_lo;
    i2c_read_reg(&rd_lo,0x71);
    *(volatile uint8_t *)DAT_00002a4c = rd_lo;
    i2c_read_reg(&rd_lo,0x72);
    *(volatile uint8_t *)DAT_00002a50 = rd_lo;
    i2c_read_reg(&rd_lo,0x64);
    *(volatile uint8_t *)DAT_00002a54 = rd_lo;
    i2c_read_reg(&rd_lo,0x65);
    i2c_read_reg(&rd_hi,0x66);
    *(volatile uint *)DAT_00002a58 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x67);
    *(volatile uint8_t *)DAT_00002a5c = rd_lo;
    i2c_read_reg(&rd_lo,0x68);
    *(volatile uint8_t *)DAT_00002a60 = rd_lo;
    i2c_read_reg(&rd_lo,0x97);
    i2c_read_reg(&rd_hi,0x98);
    *(volatile uint *)DAT_00002a64 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x99);
    i2c_read_reg(&rd_hi,0x9a);
    *(volatile uint *)DAT_00002a68 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 A 无魔数：用影子区默认值整组回写（58 次写）+ 置魔数 0x55 —— */
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000029bc,0xa);
    i2c_write_reg(*(volatile uint *)DAT_000029c0 >> 8,0xb);
    i2c_write_reg((char)*(volatile uint *)DAT_000029c0,0xc);
    i2c_write_reg(*(volatile uint *)DAT_000029c4 >> 8,0xd);
    i2c_write_reg((char)*(volatile uint *)DAT_000029c4,0xe);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000029c8,0xf);
    i2c_write_reg(*(volatile uint *)DAT_000029cc >> 8,0x10);
    i2c_write_reg((char)*(volatile uint *)DAT_000029cc,0x11);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_5,0x12);
    i2c_write_reg(*(volatile uint *)DAT_000029d4 >> 8,0x13);
    i2c_write_reg((char)*(volatile uint *)DAT_000029d4,0x14);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000029d8,0x15);
    i2c_write_reg(*(volatile uint *)DAT_000029dc >> 8,0x16);
    i2c_write_reg((char)*(volatile uint *)DAT_000029dc,0x17);
    i2c_write_reg(*(volatile uint *)DAT_000029e0 >> 8,0x18);
    i2c_write_reg((char)*(volatile uint *)DAT_000029e0,0x19);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000029e4,0x1a);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_6,0x1b);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_11,0x1c);
    i2c_write_reg(*(volatile uint *)DAT_000029f0 >> 8,0x1d);
    i2c_write_reg((char)*(volatile uint *)DAT_000029f0,0x1e);
    i2c_write_reg(*(volatile uint *)DAT_000029f4 >> 8,0x32);
    i2c_write_reg((char)*(volatile uint *)DAT_000029f4,0x33);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000029f8,0x34);
    i2c_write_reg(*(volatile uint *)DAT_000029fc >> 8,0x35);
    i2c_write_reg((char)*(volatile uint *)DAT_000029fc,0x36);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a00,0x37);
    i2c_write_reg(*(volatile uint *)DAT_00002a04 >> 8,0x38);
    i2c_write_reg((char)*(volatile uint *)DAT_00002a04,0x39);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a08,0x3a);
    i2c_write_reg(*(volatile uint *)DAT_00002a0c >> 8,0x3b);
    i2c_write_reg((char)*(volatile uint *)DAT_00002a0c,0x3c);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a10,0x3d);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_14,0x3e);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_15,0x3f);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a1c,0x5a);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a20,0x5b);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a24,0x5c);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a28,0x5d);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a2c,0x5e);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a30,0x5f);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a34,0x60);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a38,0x61);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a3c,0x62);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a40,0x6e);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a44,0x6f);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a48,0x70);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a4c,0x71);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a50,0x72);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a54,0x64);
    i2c_write_reg(*(volatile uint *)DAT_00002a58 >> 8,0x65);
    i2c_write_reg((char)*(volatile uint *)DAT_00002a58,0x66);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a5c,0x67);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00002a60,0x68);
    i2c_write_reg(*(volatile uint *)DAT_00002a64 >> 8,0x97);
    i2c_write_reg((char)*(volatile uint *)DAT_00002a64,0x98);
    i2c_write_reg(*(volatile uint *)DAT_00002a68 >> 8,0x99);
    i2c_write_reg((char)*(volatile uint *)DAT_00002a68,0x9a);
    i2c_write_reg(0x55,5);
    i2c_write_reg(0x55,6);
  }
  i2c_read_reg(&rd_lo,7);
  i2c_read_reg(&rd_hi,8);
  if (((char)rd_lo == 'f') || ((char)rd_hi == 'f')) {
    /* —— 银行 B 有效：从 EEPROM 读入影子区 0x1000167F.. —— */
    i2c_read_reg(&rd_lo,0x1f);
    *(volatile uint8_t *)eeprom_param_7 = rd_lo;
    i2c_read_reg(&rd_lo,0x20);
    *(volatile uint8_t *)eeprom_param_8 = rd_lo;
    i2c_read_reg(&rd_lo,0x21);
    *(volatile uint8_t *)eeprom_param_9 = rd_lo;
    i2c_read_reg(&rd_lo,0x22);
    *(volatile uint8_t *)DAT_00003314 = rd_lo;
    i2c_read_reg(&rd_lo,0x23);
    *(volatile uint8_t *)eeprom_param_10 = rd_lo;
    i2c_read_reg(&rd_lo,0x24);
    i2c_read_reg(&rd_hi,0x25);
    *(volatile uint *)DAT_0000331c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x26);
    i2c_read_reg(&rd_hi,0x27);
    *(volatile uint *)DAT_00003320 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xc9);
    i2c_read_reg(&rd_hi,0xca);
    *(volatile uint *)out_freq_adj_shadow = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcb);
    i2c_read_reg(&rd_hi,0xcc);
    *(volatile uint *)DAT_00003328 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcd);
    i2c_read_reg(&rd_hi,0xce);
    *(volatile uint *)DAT_0000332c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcf);
    i2c_read_reg(&rd_hi,0xd0);
    *(volatile uint *)DAT_00003330 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd1);
    i2c_read_reg(&rd_hi,0xd2);
    *(volatile uint *)DAT_00003334 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd3);
    i2c_read_reg(&rd_hi,0xd4);
    *(volatile uint *)DAT_00003338 = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 B 无魔数：用影子区默认值整组回写（21 次写）+ 置魔数 0x66 —— */
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_7,0x1f);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_8,0x20);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_9,0x21);
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003314,0x22);
    i2c_write_reg((char)*(volatile uint8_t *)eeprom_param_10,0x23);
    i2c_write_reg(*(volatile uint *)DAT_0000331c >> 8,0x24);
    i2c_write_reg((char)*(volatile uint *)DAT_0000331c,0x25);
    i2c_write_reg(*(volatile uint *)DAT_00003320 >> 8,0x26);
    i2c_write_reg((char)*(volatile uint *)DAT_00003320,0x27);
    i2c_write_reg(*(volatile uint *)out_freq_adj_shadow >> 8,0xc9);
    i2c_write_reg((char)*(volatile uint *)out_freq_adj_shadow,0xca);
    i2c_write_reg(*(volatile uint *)DAT_00003328 >> 8,0xcb);
    i2c_write_reg((char)*(volatile uint *)DAT_00003328,0xcc);
    i2c_write_reg(*(volatile uint *)DAT_0000332c >> 8,0xcd);
    i2c_write_reg((char)*(volatile uint *)DAT_0000332c,0xce);
    i2c_write_reg(*(volatile uint *)DAT_00003330 >> 8,0xcf);
    i2c_write_reg((char)*(volatile uint *)DAT_00003330,0xd0);
    i2c_write_reg(*(volatile uint *)DAT_00003334 >> 8,0xd1);
    i2c_write_reg((char)*(volatile uint *)DAT_00003334,0xd2);
    i2c_write_reg(*(volatile uint *)DAT_00003338 >> 8,0xd3);
    i2c_write_reg((char)*(volatile uint *)DAT_00003338,0xd4);
    i2c_write_reg(0x66,7);
    i2c_write_reg(0x66,8);
  }
  /* —— shadow→live 拷贝 56 项（影子区默认值 → 活动参数区）—— */
  *(volatile uint8_t *)DAT_00003340 = *(volatile uint8_t *)DAT_0000333c;
  *(volatile uint *)DAT_00003348 = *(volatile uint *)DAT_00003344;
  *(volatile uint *)gain_c = *(volatile uint *)DAT_0000334c;
  *startup_div = *(volatile uint8_t *)DAT_00003354;
  *(volatile uint *)DAT_00003360 = *(volatile uint *)DAT_0000335c;
  *(volatile uint8_t *)stop_div = *(volatile uint8_t *)DAT_00003364;
  *(volatile uint *)llim_angle = *(volatile uint *)DAT_0000336c;
  *counter2 = *(volatile uint8_t *)DAT_00003374;
  *(volatile uint *)gain_a = *(volatile uint *)DAT_0000337c;
  *(volatile uint *)gain_b = *(volatile uint *)DAT_00003384;
  *DAT_00003390 = *(volatile uint8_t *)DAT_0000338c;
  *DAT_00003398 = *(volatile uint8_t *)DAT_00003394;
  *(volatile uint8_t *)trig_phase = *(volatile uint8_t *)DAT_0000339c;
  *(volatile uint *)DAT_000033a8 = *(volatile uint *)DAT_000033a4;
  *eeprom_param_2 = *(volatile uint8_t *)eeprom_param_7;
  *(volatile uint8_t *)DAT_000033b0 = *(volatile uint8_t *)DAT_00003314;
  *(volatile uint8_t *)eeprom_param_4 = *(volatile uint8_t *)eeprom_param_10;
  *(volatile uint8_t *)eeprom_param_3 = *(volatile uint8_t *)eeprom_param_8;
  *DAT_000033bc = *(volatile uint8_t *)eeprom_param_9;
  *(volatile uint *)DAT_000033c0 = *(volatile uint *)DAT_00003328;
  *(volatile uint *)DAT_000033c4 = *(volatile uint *)DAT_0000332c;
  *(volatile uint *)DAT_000033c8 = *(volatile uint *)DAT_00003330;
  *(volatile uint *)DAT_000033cc = *(volatile uint *)DAT_00003334;
  *(volatile uint *)DAT_000033d0 = *(volatile uint *)DAT_00003338;
  *(volatile uint *)out_fine = *(volatile uint *)DAT_0000331c;
  *(volatile uint *)ulim_angle = *(volatile uint *)DAT_00003320;
  *(volatile uint *)DAT_000033e0 = *(volatile uint *)DAT_000033dc;
  *DAT_000033e8 = *(volatile uint8_t *)DAT_000033e4;
  *(volatile uint *)DAT_000033f0 = *(volatile uint *)DAT_000033ec;
  *DAT_000033f8 = *(volatile uint8_t *)DAT_000033f4;
  *(volatile uint *)DAT_00003400 = *(volatile uint *)DAT_000033fc;
  *DAT_00003408 = *(volatile uint8_t *)DAT_00003404;
  *(volatile uint *)DAT_00003410 = *(volatile uint *)DAT_0000340c;
  *DAT_00003418 = *(volatile uint8_t *)DAT_00003414;
  *(volatile uint8_t *)eeprom_param_12 = *(volatile uint8_t *)DAT_0000341c;   /* 0x100016D5 ← 0x100016F5 */
  *(volatile uint8_t *)eeprom_param_13 = *(volatile uint8_t *)DAT_00003424;   /* 0x100016D6 ← 0x100016F6 */
  *(volatile uint8_t *)DAT_00003430 = *(volatile uint8_t *)DAT_0000342c;
  *comm_param_1 = *(volatile uint8_t *)DAT_00003434;
  *(volatile uint8_t *)comm_param_2 = *(volatile uint8_t *)DAT_0000343c;
  *(volatile uint8_t *)comm_param_3 = *(volatile uint8_t *)DAT_00003444;
  *(volatile uint8_t *)DAT_00003450 = *(volatile uint8_t *)DAT_0000344c;
  *(volatile uint8_t *)DAT_00003458 = *(volatile uint8_t *)DAT_00003454;
  *(volatile uint8_t *)DAT_00003460 = *(volatile uint8_t *)DAT_0000345c;
  *DAT_00003468 = *(volatile uint8_t *)DAT_00003464;
  *(volatile uint8_t *)DAT_00003470 = *(volatile uint8_t *)DAT_0000346c;
  *(volatile uint8_t *)cl_thresh_hi = *(volatile uint8_t *)DAT_00003870;   /* dump 连写两次，C 写一次 */
  *(volatile uint8_t *)cl_gain_big = *(volatile uint8_t *)DAT_00003878;
  *(volatile uint8_t *)cl_gain_mid = *(volatile uint8_t *)DAT_00003880;
  *(volatile uint8_t *)cl_gain_small = *(volatile uint8_t *)DAT_00003888;
  *(volatile uint8_t *)menu_state = *(volatile uint8_t *)DAT_00003890;
  *(volatile uint *)comm_baud_idx = *(volatile uint *)DAT_00003898;
  *(volatile uint8_t *)comm_div_sel = *(volatile uint8_t *)DAT_000038a0;
  *(volatile uint8_t *)comm_rx_flag = *(volatile uint8_t *)DAT_000038a8;
  *(volatile uint *)DAT_000038b4 = *(volatile uint *)DAT_000038b0;
  *(volatile uint *)DAT_000038bc = *(volatile uint *)DAT_000038b8;
  *(volatile uint *)DAT_000038c4 = *(volatile uint *)DAT_000038c0;
  /* —— 按 0x10001708（控制方式）选择活动增益对 → pid_kp2(0x10001706)/pid_ki2(0x10001707) —— */
  if (*(volatile uint8_t *)DAT_000038c8 == '\x01') {
    *(volatile uint8_t *)pid_kp2 = *DAT_000038cc;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)DAT_000038d4;
  }
  if (*(volatile uint8_t *)DAT_000038c8 == '\x02') {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)DAT_000038dc;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)DAT_000038e0;
  }
  if (*(volatile uint8_t *)DAT_000038c8 == '\x03') {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)DAT_000038e4;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)DAT_000038e8;
  }
  if (*(volatile uint8_t *)DAT_000038c8 == '\x04') {
    *(volatile uint8_t *)pid_kp2 = *DAT_000038ec;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)DAT_000038f0;
  }
  return;
}


/* 0x00003534 —— 参数 live→EEPROM 同步（param_sync_live_to_eeprom() 调用；
 *   主循环/菜单修改参数时也会触发）
 *   结构：对每个参数比对 live(0x1000162C..) 与 EEPROM 缓存副本(0x1000165C..)，
 *   不等则更新 shadow 并 i2c_write_reg 写回芯片（16 位参数分高低字节：
 *     *reg >> 8 写小寄存器号，低字节 (char)*reg 写下一大寄存器号）。
 *   寄存器号序列（节选）：
 *     0x0A..0x1F 基本参数 | 0x20..0x27 | 0x32..0x3F | 0x5A..0x62 | 0x64..0x68
 *     | 0x6E..0x72 保护参数 | 0x97..0x9A | 0xC9..0xD4 通讯参数
 *   方向 live→shadow。57 组。 */
void param_sync_live_to_eeprom(void)
{
  if (*(volatile uint8_t *)DAT_000038f4 != *(volatile uint8_t *)DAT_000038f8) {
    *(volatile uint8_t *)DAT_000038f8 = *(volatile uint8_t *)DAT_000038f4;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_000038f8,0xa);
  }
  if (*(volatile uint *)DAT_000038fc != *(volatile uint *)DAT_00003900) {
    *(volatile uint *)DAT_00003900 = *(volatile uint *)DAT_000038fc;
    i2c_write_reg(*(volatile uint *)DAT_00003900 >> 8,0xb);
    i2c_write_reg((char)*(volatile uint *)DAT_00003900,0xc);
  }
  if (*(volatile uint *)DAT_00003904 != *(volatile uint *)DAT_00003908) {
    *(volatile uint *)DAT_00003908 = *(volatile uint *)DAT_00003904;
    i2c_write_reg(*(volatile uint *)DAT_00003908 >> 8,0xd);
    i2c_write_reg((char)*(volatile uint *)DAT_00003908,0xe);
  }
  if (*(volatile uint8_t *)DAT_0000390c != *(volatile uint8_t *)DAT_00003910) {
    *(volatile uint8_t *)DAT_00003910 = *(volatile uint8_t *)DAT_0000390c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003910,0xf);
  }
  if (*(volatile uint *)DAT_00003914 != *(volatile uint *)DAT_00003918) {
    *(volatile uint *)DAT_00003918 = *(volatile uint *)DAT_00003914;
    i2c_write_reg(*(volatile uint *)DAT_00003918 >> 8,0x10);
    i2c_write_reg((char)*(volatile uint *)DAT_00003918,0x11);
  }
  if (*(volatile uint8_t *)DAT_0000391c != *(volatile uint8_t *)DAT_00003920) {
    *(volatile uint8_t *)DAT_00003920 = *(volatile uint8_t *)DAT_0000391c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003920,0x12);
  }
  if (*(volatile uint *)DAT_00003924 != *(volatile uint *)DAT_00003928) {
    *(volatile uint *)DAT_00003928 = *(volatile uint *)DAT_00003924;
    i2c_write_reg(*(volatile uint *)DAT_00003928 >> 8,0x13);
    i2c_write_reg((char)*(volatile uint *)DAT_00003928,0x14);
  }
  if (*(volatile uint8_t *)DAT_0000392c != *(volatile uint8_t *)DAT_00003930) {
    *(volatile uint8_t *)DAT_00003930 = *(volatile uint8_t *)DAT_0000392c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003930,0x15);
  }
  if (*(volatile uint *)DAT_00003934 != *(volatile uint *)DAT_00003938) {
    *(volatile uint *)DAT_00003938 = *(volatile uint *)DAT_00003934;
    i2c_write_reg(*(volatile uint *)DAT_00003938 >> 8,0x16);
    i2c_write_reg((char)*(volatile uint *)DAT_00003938,0x17);
  }
  if (*(volatile uint *)DAT_0000393c != *(volatile uint *)DAT_00003940) {
    *(volatile uint *)DAT_00003940 = *(volatile uint *)DAT_0000393c;
    i2c_write_reg(*(volatile uint *)DAT_00003940 >> 8,0x18);
    i2c_write_reg((char)*(volatile uint *)DAT_00003940,0x19);
  }
  if (*(volatile uint8_t *)DAT_00003944 != *(volatile uint8_t *)DAT_00003948) {
    *(volatile uint8_t *)DAT_00003948 = *(volatile uint8_t *)DAT_00003944;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003948,0x1a);
  }
  if (*(volatile uint8_t *)DAT_0000394c != *(volatile uint8_t *)DAT_00003950) {
    *(volatile uint8_t *)DAT_00003950 = *(volatile uint8_t *)DAT_0000394c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003950,0x1b);
  }
  if (*(volatile uint8_t *)DAT_00003954 != *(volatile uint8_t *)DAT_00003958) {
    *(volatile uint8_t *)DAT_00003958 = *(volatile uint8_t *)DAT_00003954;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003958,0x1c);
  }
  if (*(volatile uint *)DAT_0000395c != *(volatile uint *)DAT_00003960) {
    *(volatile uint *)DAT_00003960 = *(volatile uint *)DAT_0000395c;
    i2c_write_reg(*(volatile uint *)DAT_00003960 >> 8,0x1d);
    i2c_write_reg((char)*(volatile uint *)DAT_00003960,0x1e);
  }
  if (*(volatile uint8_t *)DAT_00003964 != *(volatile uint8_t *)DAT_00003968) {
    *(volatile uint8_t *)DAT_00003968 = *(volatile uint8_t *)DAT_00003964;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003968,0x1f);
  }
  if (*(volatile uint8_t *)DAT_0000396c != *(volatile uint8_t *)DAT_00003970) {
    *(volatile uint8_t *)DAT_00003970 = *(volatile uint8_t *)DAT_0000396c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003970,0x20);
  }
  if (*(volatile uint8_t *)DAT_00003974 != *(volatile uint8_t *)DAT_00003978) {
    *(volatile uint8_t *)DAT_00003978 = *(volatile uint8_t *)DAT_00003974;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003978,0x21);
  }
  if (*(volatile uint8_t *)DAT_0000397c != *(volatile uint8_t *)DAT_00003980) {
    *(volatile uint8_t *)DAT_00003980 = *(volatile uint8_t *)DAT_0000397c;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003980,0x22);
  }
  if (*(volatile uint8_t *)DAT_00003984 != *(volatile uint8_t *)DAT_00003988) {
    *(volatile uint8_t *)DAT_00003988 = *(volatile uint8_t *)DAT_00003984;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003988,0x23);
  }
  if (*(volatile uint *)DAT_0000398c != *(volatile uint *)DAT_00003990) {
    *(volatile uint *)DAT_00003990 = *(volatile uint *)DAT_0000398c;
    i2c_write_reg(*(volatile uint *)DAT_00003990 >> 8,0x24);
    i2c_write_reg((char)*(volatile uint *)DAT_00003990,0x25);
  }
  if (*(volatile uint *)DAT_00003d98 != *(volatile uint *)DAT_00003d9c) {
    *(volatile uint *)DAT_00003d9c = *(volatile uint *)DAT_00003d98;
    i2c_write_reg(*(volatile uint *)DAT_00003d9c >> 8,0x26);
    i2c_write_reg((char)*(volatile uint *)DAT_00003d9c,0x27);
  }
  if (*(volatile uint *)DAT_00003da0 != *(volatile uint *)DAT_00003da4) {
    *(volatile uint *)DAT_00003da4 = *(volatile uint *)DAT_00003da0;
    i2c_write_reg(*(volatile uint *)DAT_00003da4 >> 8,0x32);
    i2c_write_reg((char)*(volatile uint *)DAT_00003da4,0x33);
  }
  if (*(volatile uint8_t *)DAT_00003da8 != *(volatile uint8_t *)DAT_00003dac) {
    *(volatile uint8_t *)DAT_00003dac = *(volatile uint8_t *)DAT_00003da8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003dac,0x34);
  }
  if (*(volatile uint *)DAT_00003db0 != *(volatile uint *)DAT_00003db4) {
    *(volatile uint *)DAT_00003db4 = *(volatile uint *)DAT_00003db0;
    i2c_write_reg(*(volatile uint *)DAT_00003db4 >> 8,0x35);
    i2c_write_reg((char)*(volatile uint *)DAT_00003db4,0x36);
  }
  if (*(volatile uint8_t *)DAT_00003db8 != *(volatile uint8_t *)DAT_00003dbc) {
    *(volatile uint8_t *)DAT_00003dbc = *(volatile uint8_t *)DAT_00003db8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003dbc,0x37);
  }
  if (*(volatile uint *)DAT_00003dc0 != *(volatile uint *)DAT_00003dc4) {
    *(volatile uint *)DAT_00003dc4 = *(volatile uint *)DAT_00003dc0;
    i2c_write_reg(*(volatile uint *)DAT_00003dc4 >> 8,0x38);
    i2c_write_reg((char)*(volatile uint *)DAT_00003dc4,0x39);
  }
  if (*(volatile uint8_t *)DAT_00003dc8 != *(volatile uint8_t *)DAT_00003dcc) {
    *(volatile uint8_t *)DAT_00003dcc = *(volatile uint8_t *)DAT_00003dc8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003dcc,0x3a);
  }
  if (*(volatile uint *)DAT_00003dd0 != *(volatile uint *)DAT_00003dd4) {
    *(volatile uint *)DAT_00003dd4 = *(volatile uint *)DAT_00003dd0;
    i2c_write_reg(*(volatile uint *)DAT_00003dd4 >> 8,0x3b);
    i2c_write_reg((char)*(volatile uint *)DAT_00003dd4,0x3c);
  }
  if (*(volatile uint8_t *)DAT_00003dd8 != *(volatile uint8_t *)DAT_00003ddc) {
    *(volatile uint8_t *)DAT_00003ddc = *(volatile uint8_t *)DAT_00003dd8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003ddc,0x3d);
  }
  if (*(volatile uint8_t *)DAT_00003de0 != *(volatile uint8_t *)DAT_00003de4) {
    *(volatile uint8_t *)DAT_00003de4 = *(volatile uint8_t *)DAT_00003de0;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003de4,0x3e);
  }
  if (*(volatile uint8_t *)DAT_00003de8 != *(volatile uint8_t *)DAT_00003dec) {
    *(volatile uint8_t *)DAT_00003dec = *(volatile uint8_t *)DAT_00003de8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003dec,0x3f);
  }
  if (*(volatile uint8_t *)DAT_00003df0 != *(volatile uint8_t *)DAT_00003df4) {
    *(volatile uint8_t *)DAT_00003df4 = *(volatile uint8_t *)DAT_00003df0;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003df4,0x5a);
  }
  if (*(volatile uint8_t *)DAT_00003df8 != *(volatile uint8_t *)DAT_00003dfc) {
    *(volatile uint8_t *)DAT_00003dfc = *(volatile uint8_t *)DAT_00003df8;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003dfc,0x5b);
  }
  if (*(volatile uint8_t *)DAT_00003e00 != *(volatile uint8_t *)DAT_00003e04) {
    *(volatile uint8_t *)DAT_00003e04 = *(volatile uint8_t *)DAT_00003e00;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e04,0x5c);
  }
  if (*(volatile uint8_t *)DAT_00003e08 != *(volatile uint8_t *)DAT_00003e0c) {
    *(volatile uint8_t *)DAT_00003e0c = *(volatile uint8_t *)DAT_00003e08;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e0c,0x5d);
  }
  if (*(volatile uint8_t *)DAT_00003e10 != *(volatile uint8_t *)DAT_00003e14) {
    *(volatile uint8_t *)DAT_00003e14 = *(volatile uint8_t *)DAT_00003e10;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e14,0x5e);
  }
  if (*(volatile uint8_t *)DAT_00003e18 != *(volatile uint8_t *)DAT_00003e1c) {
    *(volatile uint8_t *)DAT_00003e1c = *(volatile uint8_t *)DAT_00003e18;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e1c,0x5f);
  }
  if (*(volatile uint8_t *)DAT_00003e20 != *(volatile uint8_t *)DAT_00003e24) {
    *(volatile uint8_t *)DAT_00003e24 = *(volatile uint8_t *)DAT_00003e20;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e24,0x60);
  }
  if (*(volatile uint8_t *)DAT_00003e28 != *(volatile uint8_t *)DAT_00003e2c) {
    *(volatile uint8_t *)DAT_00003e2c = *(volatile uint8_t *)DAT_00003e28;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e2c,0x61);
  }
  if (*(volatile uint8_t *)DAT_00003e30 != *(volatile uint8_t *)DAT_00003e34) {
    *(volatile uint8_t *)DAT_00003e34 = *(volatile uint8_t *)DAT_00003e30;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e34,0x62);
  }
  if (*(volatile uint8_t *)DAT_00003e38 != *(volatile uint8_t *)DAT_00003e3c) {
    *(volatile uint8_t *)DAT_00003e3c = *(volatile uint8_t *)DAT_00003e38;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e3c,0x6e);
  }
  if (*(volatile uint8_t *)cl_thresh_lo != *(volatile uint8_t *)DAT_00003e44) {
    *(volatile uint8_t *)DAT_00003e44 = *(volatile uint8_t *)cl_thresh_lo;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e44,0x6f);
  }
  if (*(volatile uint8_t *)DAT_00003e48 != *(volatile uint8_t *)DAT_00003e4c) {
    *(volatile uint8_t *)DAT_00003e4c = *(volatile uint8_t *)DAT_00003e48;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e4c,0x70);
  }
  if (*(volatile uint8_t *)DAT_00003e50 != *(volatile uint8_t *)DAT_00003e54) {
    *(volatile uint8_t *)DAT_00003e54 = *(volatile uint8_t *)DAT_00003e50;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00003e54,0x71);
  }
  if (*(volatile uint8_t *)DAT_00004260 != *(volatile uint8_t *)DAT_00004264) {
    *(volatile uint8_t *)DAT_00004264 = *(volatile uint8_t *)DAT_00004260;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00004264,0x72);
  }
  if (*(volatile uint8_t *)DAT_00004268 != *(volatile uint8_t *)DAT_0000426c) {
    *(volatile uint8_t *)DAT_0000426c = *(volatile uint8_t *)DAT_00004268;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_0000426c,0x64);
  }
  if (*(volatile uint *)DAT_00004270 != *(volatile uint *)DAT_00004274) {
    *(volatile uint *)DAT_00004274 = *(volatile uint *)DAT_00004270;
    i2c_write_reg(*(volatile uint *)DAT_00004274 >> 8,0x65);
    i2c_write_reg((char)*(volatile uint *)DAT_00004274,0x66);
  }
  if (*(volatile uint8_t *)DAT_00004278 != *(volatile uint8_t *)DAT_0000427c) {
    *(volatile uint8_t *)DAT_0000427c = *(volatile uint8_t *)DAT_00004278;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_0000427c,0x67);
  }
  if (*(volatile uint8_t *)DAT_00004280 != *(volatile uint8_t *)DAT_00004284) {
    *(volatile uint8_t *)DAT_00004284 = *(volatile uint8_t *)DAT_00004280;
    i2c_write_reg((char)*(volatile uint8_t *)DAT_00004284,0x68);
  }
  if (*(volatile uint *)DAT_00004288 != *(volatile uint *)DAT_0000428c) {
    *(volatile uint *)DAT_0000428c = *(volatile uint *)DAT_00004288;
    i2c_write_reg(*(volatile uint *)DAT_0000428c >> 8,0x97);
    i2c_write_reg((char)*(volatile uint *)DAT_0000428c,0x98);
  }
  if (*(volatile uint *)DAT_00004290 != *(volatile uint *)DAT_00004294) {
    *(volatile uint *)DAT_00004294 = *(volatile uint *)DAT_00004290;
    i2c_write_reg(*(volatile uint *)DAT_00004294 >> 8,0x99);
    i2c_write_reg((char)*(volatile uint *)DAT_00004294,0x9a);
  }
  if (*(volatile uint *)DAT_00004298 != *(volatile uint *)DAT_0000429c) {
    *(volatile uint *)DAT_0000429c = *(volatile uint *)DAT_00004298;
    i2c_write_reg(*(volatile uint *)DAT_0000429c >> 8,0xc9);
    i2c_write_reg((char)*(volatile uint *)DAT_0000429c,0xca);
  }
  if (*(volatile uint *)DAT_000042a0 != *(volatile uint *)DAT_000042a4) {
    *(volatile uint *)DAT_000042a4 = *(volatile uint *)DAT_000042a0;
    i2c_write_reg(*(volatile uint *)DAT_000042a4 >> 8,0xcb);
    i2c_write_reg((char)*(volatile uint *)DAT_000042a4,0xcc);
  }
  if (*(volatile uint *)DAT_000042a8 != *(volatile uint *)DAT_000042ac) {
    *(volatile uint *)DAT_000042ac = *(volatile uint *)DAT_000042a8;
    i2c_write_reg(*(volatile uint *)DAT_000042ac >> 8,0xcd);
    i2c_write_reg((char)*(volatile uint *)DAT_000042ac,0xce);
  }
  if (*(volatile uint *)DAT_000042b0 != *(volatile uint *)DAT_000042b4) {
    *(volatile uint *)DAT_000042b4 = *(volatile uint *)DAT_000042b0;
    i2c_write_reg(*(volatile uint *)DAT_000042b4 >> 8,0xcf);
    i2c_write_reg((char)*(volatile uint *)DAT_000042b4,0xd0);
  }
  if (*(volatile uint *)DAT_000042b8 != *(volatile uint *)DAT_000042bc) {
    *(volatile uint *)DAT_000042bc = *(volatile uint *)DAT_000042b8;
    i2c_write_reg(*(volatile uint *)DAT_000042bc >> 8,0xd1);
    i2c_write_reg((char)*(volatile uint *)DAT_000042bc,0xd2);
  }
  if (*(volatile uint *)DAT_000042c0 != *(volatile uint *)DAT_000042c4) {
    *(volatile uint *)DAT_000042c4 = *(volatile uint *)DAT_000042c0;
    i2c_write_reg(*(volatile uint *)DAT_000042c4 >> 8,0xd3);
    i2c_write_reg((char)*(volatile uint *)DAT_000042c4,0xd4);
  }
  return;
}
