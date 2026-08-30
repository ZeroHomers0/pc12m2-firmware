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

/* 0x000025DC —— 上电装载配置（main 启动序列第 11 步 load_config()）
 *   · 银行 A：reg5/6 任一 == 'U' 则整组从 EEPROM 读入 live；
 *     否则以 0x10002EC4.. 默认值整组回写并置魔数 0x55
 *   · 银行 B：reg7/8 任一 == 'f' 则整组读入；否则回写默认 + 置魔数 0x66
 *   · shadow→live 拷贝 + 增益对选择 */
void load_config(void)
{
  undefined1 *puVar1;
  uint local_10;
  uint local_c;

  local_c = 0;
  local_10 = 0;
  i2c_read_reg(&local_c,5);
  i2c_read_reg(&local_10,6);
  if (((char)local_c == 'U') || ((char)local_10 == 'U')) {
    i2c_read_reg(&local_c,10);
    *DAT_00002a0c = (char)local_c;
    i2c_read_reg(&local_c,0xb);
    i2c_read_reg(&local_10,0xc);
    *(uint *)DAT_00002a10 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xd);
    i2c_read_reg(&local_10,0xe);
    *(uint *)DAT_00002a14 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xf);
    *DAT_00002a18 = (char)local_c;
    i2c_read_reg(&local_c,0x10);
    i2c_read_reg(&local_10,0x11);
    *DAT_00002a1c = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x12);
    *DAT_00002a20 = (char)local_c;
    i2c_read_reg(&local_c,0x13);
    i2c_read_reg(&local_10,0x14);
    *DAT_00002a24 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x15);
    *DAT_00002a28 = (char)local_c;
    i2c_read_reg(&local_c,0x16);
    i2c_read_reg(&local_10,0x17);
    *DAT_00002a2c = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x18);
    i2c_read_reg(&local_10,0x19);
    *DAT_00002a30 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x1a);
    *DAT_00002a34 = (char)local_c;
    i2c_read_reg(&local_c,0x1b);
    *DAT_00002a38 = (char)local_c;
    i2c_read_reg(&local_c,0x1c);
    *DAT_00002a3c = (char)local_c;
    i2c_read_reg(&local_c,0x1d);
    i2c_read_reg(&local_10,0x1e);
    *DAT_00002a40 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x32);
    i2c_read_reg(&local_10,0x33);
    *DAT_00002a44 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x34);
    *DAT_00002a48 = (char)local_c;
    i2c_read_reg(&local_c,0x35);
    i2c_read_reg(&local_10,0x36);
    *DAT_00002a4c = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x37);
    *DAT_00002a50 = (char)local_c;
    i2c_read_reg(&local_c,0x38);
    i2c_read_reg(&local_10,0x39);
    *DAT_00002a54 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x3a);
    *DAT_00002a58 = (char)local_c;
    i2c_read_reg(&local_c,0x3b);
    i2c_read_reg(&local_10,0x3c);
    *DAT_00002a5c = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x3d);
    *DAT_00002a60 = (char)local_c;
    i2c_read_reg(&local_c,0x3e);
    *DAT_00002a64 = (char)local_c;
    i2c_read_reg(&local_c,0x3f);
    *DAT_00002a68 = (char)local_c;
    i2c_read_reg(&local_c,0x5a);
    *DAT_00002a6c = (char)local_c;
    i2c_read_reg(&local_c,0x5b);
    *DAT_00002a70 = (char)local_c;
    i2c_read_reg(&local_c,0x5c);
    *DAT_00002a74 = (char)local_c;
    i2c_read_reg(&local_c,0x5d);
    *DAT_00002a78 = (char)local_c;
    i2c_read_reg(&local_c,0x5e);
    *DAT_00002a7c = (char)local_c;
    i2c_read_reg(&local_c,0x5f);
    *DAT_00002a80 = (char)local_c;
    i2c_read_reg(&local_c,0x60);
    *DAT_00002a84 = (char)local_c;
    i2c_read_reg(&local_c,0x61);
    *DAT_00002a88 = (char)local_c;
    i2c_read_reg(&local_c,0x62);
    *DAT_00002a8c = (char)local_c;
    i2c_read_reg(&local_c,0x6e);
    *DAT_00002a90 = (char)local_c;
    i2c_read_reg(&local_c,0x6f);
    *DAT_00002a94 = (char)local_c;
    i2c_read_reg(&local_c,0x70);
    *DAT_00002a98 = (char)local_c;
    i2c_read_reg(&local_c,0x71);
    *DAT_00002a9c = (char)local_c;
    i2c_read_reg(&local_c,0x72);
    *DAT_00002aa0 = (char)local_c;
    i2c_read_reg(&local_c,100);
    *DAT_00002aa4 = (char)local_c;
    i2c_read_reg(&local_c,0x65);
    i2c_read_reg(&local_10,0x66);
    *DAT_00002aa8 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x67);
    *DAT_00002aac = (char)local_c;
    i2c_read_reg(&local_c,0x68);
    *DAT_00002ab0 = (char)local_c;
    i2c_read_reg(&local_c,0x97);
    i2c_read_reg(&local_10,0x98);
    *DAT_00002ab4 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x99);
    i2c_read_reg(&local_10,0x9a);
    *DAT_00002ab8 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x9b);
    i2c_read_reg(&local_10,0x9c);
    *DAT_00002abc = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
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
  i2c_read_reg(&local_c,7);
  i2c_read_reg(&local_10,8);
  if (((char)local_c == 'f') || ((char)local_10 == 'f')) {
    i2c_read_reg(&local_c,0x1f);
    *DAT_00003398 = (char)local_c;
    i2c_read_reg(&local_c,0x20);
    *DAT_0000339c = (char)local_c;
    i2c_read_reg(&local_c,0x21);
    *DAT_000033a0 = (char)local_c;
    i2c_read_reg(&local_c,0x22);
    *DAT_000033a4 = (char)local_c;
    i2c_read_reg(&local_c,0x23);
    *DAT_000033a8 = (char)local_c;
    i2c_read_reg(&local_c,0x24);
    i2c_read_reg(&local_10,0x25);
    *(uint *)DAT_000033ac = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0x26);
    i2c_read_reg(&local_10,0x27);
    *(uint *)DAT_000033b0 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xc9);
    i2c_read_reg(&local_10,0xca);
    *(uint *)DAT_000033b4 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xcb);
    i2c_read_reg(&local_10,0xcc);
    *(uint *)DAT_000033b8 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xcd);
    i2c_read_reg(&local_10,0xce);
    *(uint *)DAT_000033bc = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xcf);
    i2c_read_reg(&local_10,0xd0);
    *(uint *)DAT_000033c0 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xd1);
    i2c_read_reg(&local_10,0xd2);
    *(uint *)DAT_000033c4 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xd3);
    i2c_read_reg(&local_10,0xd4);
    *(uint *)DAT_000033c8 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xba);
    i2c_read_reg(&local_10,0xbb);
    *DAT_000033cc = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xbc);
    i2c_read_reg(&local_10,0xbd);
    *(uint *)DAT_000033d0 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
    i2c_read_reg(&local_c,0xbe);
    i2c_read_reg(&local_10,0xbf);
    *(uint *)DAT_000033d4 = (local_10 & 0xff) + (local_c & 0xff) * 0x100;
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
  *DAT_000033dc = *DAT_000033d8;
  *DAT_000033e4 = *DAT_000033e0;
  *DAT_000033ec = *DAT_000033e8;
  *DAT_000033f4 = *DAT_000033f0;
  *DAT_000033fc = *DAT_000033f8;
  *DAT_00003404 = *DAT_00003400;
  *DAT_0000340c = *DAT_00003408;
  *DAT_00003414 = *DAT_00003410;
  *DAT_0000341c = *DAT_00003418;
  *DAT_00003424 = *DAT_00003420;
  *DAT_0000342c = *DAT_00003428;
  *DAT_00003434 = *DAT_00003430;
  *DAT_0000343c = *DAT_00003438;
  *DAT_00003444 = *DAT_00003440;
  *DAT_00003448 = *DAT_00003398;      /* 保护参数首字节 */
  *DAT_0000344c = *DAT_000033a4;
  *DAT_00003450 = *DAT_000033a8;
  *DAT_00003454 = *DAT_0000339c;
  *DAT_00003458 = *DAT_000033a0;
  *DAT_0000345c = *(undefined4 *)DAT_000033b8;
  *DAT_00003460 = *(undefined4 *)DAT_000033bc;
  *DAT_00003464 = *(undefined4 *)DAT_000033c0;
  *DAT_00003468 = *(undefined4 *)DAT_000033c4;
  *DAT_0000346c = *(undefined4 *)DAT_000033c8;
  *DAT_00003470 = *(undefined4 *)DAT_000033ac;
  *DAT_00003474 = *(undefined4 *)DAT_000033b0;
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
  *DAT_000038c4 = *DAT_000038c0;
  *DAT_000038cc = *DAT_000038c8;
  *DAT_000038d4 = *DAT_000038d0;
  *DAT_000038dc = *DAT_000038d8;
  *DAT_000038e4 = *DAT_000038e0;
  *DAT_000038ec = *DAT_000038e8;
  *DAT_000038f4 = *DAT_000038f0;
  *DAT_000038fc = *DAT_000038f8;
  *DAT_00003904 = *DAT_00003900;
  puVar1 = DAT_0000390c;
  *DAT_0000390c = *DAT_00003908;
  *puVar1 = *DAT_00003908;
  *DAT_00003914 = *DAT_00003910;
  *DAT_0000391c = *DAT_00003918;
  *DAT_00003924 = *DAT_00003920;
  *DAT_0000392c = *DAT_00003928;
  *DAT_00003934 = *DAT_00003930;
  *DAT_0000393c = *DAT_00003938;
  *DAT_00003944 = *DAT_00003940;
  *DAT_0000394c = *DAT_00003948;
  *DAT_00003954 = *DAT_00003950;
  *DAT_0000395c = *DAT_00003958;
  *DAT_00003964 = *DAT_00003960;
  *DAT_0000396c = *DAT_00003968;
  *DAT_00003974 = *DAT_00003970;
  *DAT_0000397c = *DAT_00003978;
  /* —— 按控制方式选择活动增益对 → 0x10003980(gain_a 电压量程)/0x10003984(gain_b 电流量程) —— */
  if (*DAT_000038c4 == '\x01') {
    *DAT_00003980 = *DAT_000038cc;
    *DAT_00003984 = *DAT_000038d4;
  }
  if (*DAT_000038c4 == '\x02') {
    *DAT_00003980 = *DAT_000038dc;
    *DAT_00003984 = *DAT_000038e4;
  }
  if (*DAT_000038c4 == '\x03') {
    *DAT_00003980 = *DAT_000038ec;
    *DAT_00003984 = *DAT_000038f4;
  }
  if (*DAT_000038c4 == '\x04') {
    *DAT_00003980 = *DAT_000038fc;
    *DAT_00003984 = *DAT_00003904;
  }
  return;
}


/* 0x000035F2 —— 参数 live→EEPROM 同步（认证通过后 param_sync_live_to_eeprom() 调用；
 *   主循环/菜单修改参数时也会触发）
 *   结构：对每个参数比对 live(0x10003988..) 与 EEPROM 缓存副本(0x1000398C..)，
 *   不等则更新缓存并 i2c_write_reg 写回芯片（16 位参数分高低字节）。
 *   寄存器号序列（节选）：
 *     0x0A..0x1F 基本参数 | 0x20..0x3F | 0x5A..0x62 | 0x64..0x68 | 0x6E..0x72
 *     | 0x97..0x9C | 0xBA..0xBF 保护参数 | 0xC9..0xD4 PID/通讯参数 */
void param_sync_live_to_eeprom(void)
{
  char *pcVar1;

  pcVar1 = DAT_0000398c;
  if (*DAT_00003988 != *DAT_0000398c) {
    *DAT_0000398c = *DAT_00003988;
    i2c_write_reg(*pcVar1,10);
  }
  if (*DAT_00003990 != *(int *)DAT_00003994) {
    *(int *)DAT_00003994 = *DAT_00003990;
    i2c_write_reg(*DAT_00003994 >> 8,0xb);
    i2c_write_reg((char)*DAT_00003994,0xc);
  }
  if (*DAT_00003998 != *(int *)DAT_0000399c) {
    *(int *)DAT_0000399c = *DAT_00003998;
    i2c_write_reg(*DAT_0000399c >> 8,0xd);
    i2c_write_reg((char)*DAT_0000399c,0xe);
  }
  pcVar1 = DAT_000039a4;
  if (*DAT_000039a0 != *DAT_000039a4) {
    *DAT_000039a4 = *DAT_000039a0;
    i2c_write_reg(*pcVar1,0xf);
  }
  if (*DAT_000039a8 != *(int *)DAT_000039ac) {
    *(int *)DAT_000039ac = *DAT_000039a8;
    i2c_write_reg(*DAT_000039ac >> 8,0x10);
    i2c_write_reg((char)*DAT_000039ac,0x11);
  }
  pcVar1 = DAT_000039b4;
  if (*DAT_000039b0 != *DAT_000039b4) {
    *DAT_000039b4 = *DAT_000039b0;
    i2c_write_reg(*pcVar1,0x12);
  }
  if (*DAT_000039b8 != *(int *)DAT_000039bc) {
    *(int *)DAT_000039bc = *DAT_000039b8;
    i2c_write_reg(*DAT_000039bc >> 8,0x13);
    i2c_write_reg((char)*DAT_000039bc,0x14);
  }
  pcVar1 = DAT_000039c4;
  if (*DAT_000039c0 != *DAT_000039c4) {
    *DAT_000039c4 = *DAT_000039c0;
    i2c_write_reg(*pcVar1,0x15);
  }
  if (*DAT_000039c8 != *(int *)DAT_000039cc) {
    *(int *)DAT_000039cc = *DAT_000039c8;
    i2c_write_reg(*DAT_000039cc >> 8,0x16);
    i2c_write_reg((char)*DAT_000039cc,0x17);
  }
  if (*DAT_000039d0 != *(int *)DAT_000039d4) {
    *(int *)DAT_000039d4 = *DAT_000039d0;
    i2c_write_reg(*DAT_000039d4 >> 8,0x18);
    i2c_write_reg((char)*DAT_000039d4,0x19);
  }
  pcVar1 = DAT_000039dc;
  if (*DAT_000039d8 != *DAT_000039dc) {
    *DAT_000039dc = *DAT_000039d8;
    i2c_write_reg(*pcVar1,0x1a);
  }
  pcVar1 = DAT_000039e4;
  if (*DAT_000039e0 != *DAT_000039e4) {
    *DAT_000039e4 = *DAT_000039e0;
    i2c_write_reg(*pcVar1,0x1b);
  }
  pcVar1 = DAT_000039ec;
  if (*DAT_000039e8 != *DAT_000039ec) {
    *DAT_000039ec = *DAT_000039e8;
    i2c_write_reg(*pcVar1,0x1c);
  }
  if (*DAT_000039f0 != *(int *)DAT_000039f4) {
    *(int *)DAT_000039f4 = *DAT_000039f0;
    i2c_write_reg(*DAT_000039f4 >> 8,0x1d);
    i2c_write_reg((char)*DAT_000039f4,0x1e);
  }
  pcVar1 = DAT_000039fc;
  if (*DAT_000039f8 != *DAT_000039fc) {
    *DAT_000039fc = *DAT_000039f8;
    i2c_write_reg(*pcVar1,0x1f);
  }
  pcVar1 = DAT_00003e04;
  if (*DAT_00003e00 != *DAT_00003e04) {
    *DAT_00003e04 = *DAT_00003e00;
    i2c_write_reg(*pcVar1,0x20);
  }
  pcVar1 = DAT_00003e0c;
  if (*DAT_00003e08 != *DAT_00003e0c) {
    *DAT_00003e0c = *DAT_00003e08;
    i2c_write_reg(*pcVar1,0x21);
  }
  pcVar1 = DAT_00003e14;
  if (*DAT_00003e10 != *DAT_00003e14) {
    *DAT_00003e14 = *DAT_00003e10;
    i2c_write_reg(*pcVar1,0x22);
  }
  pcVar1 = DAT_00003e1c;
  if (*DAT_00003e18 != *DAT_00003e1c) {
    *DAT_00003e1c = *DAT_00003e18;
    i2c_write_reg(*pcVar1,0x23);
  }
  if (*DAT_00003e20 != *(int *)DAT_00003e24) {
    *(int *)DAT_00003e24 = *DAT_00003e20;
    i2c_write_reg(*DAT_00003e24 >> 8,0x24);
    i2c_write_reg((char)*DAT_00003e24,0x25);
  }
  if (*DAT_00003e28 != *(int *)DAT_00003e2c) {
    *(int *)DAT_00003e2c = *DAT_00003e28;
    i2c_write_reg(*DAT_00003e2c >> 8,0x26);
    i2c_write_reg((char)*DAT_00003e2c,0x27);
  }
  if (*DAT_00003e30 != *(int *)DAT_00003e34) {
    *(int *)DAT_00003e34 = *DAT_00003e30;
    i2c_write_reg(*DAT_00003e34 >> 8,0x32);
    i2c_write_reg((char)*DAT_00003e34,0x33);
  }
  pcVar1 = DAT_00003e3c;
  if (*DAT_00003e38 != *DAT_00003e3c) {
    *DAT_00003e3c = *DAT_00003e38;
    i2c_write_reg(*pcVar1,0x34);
  }
  if (*DAT_00003e40 != *(int *)DAT_00003e44) {
    *(int *)DAT_00003e44 = *DAT_00003e40;
    i2c_write_reg(*DAT_00003e44 >> 8,0x35);
    i2c_write_reg((char)*DAT_00003e44,0x36);
  }
  pcVar1 = DAT_00003e4c;
  if (*DAT_00003e48 != *DAT_00003e4c) {
    *DAT_00003e4c = *DAT_00003e48;
    i2c_write_reg(*pcVar1,0x37);
  }
  if (*DAT_00003e50 != *(int *)DAT_00003e54) {
    *(int *)DAT_00003e54 = *DAT_00003e50;
    i2c_write_reg(*DAT_00003e54 >> 8,0x38);
    i2c_write_reg((char)*DAT_00003e54,0x39);
  }
  pcVar1 = DAT_00003e5c;
  if (*DAT_00003e58 != *DAT_00003e5c) {
    *DAT_00003e5c = *DAT_00003e58;
    i2c_write_reg(*pcVar1,0x3a);
  }
  if (*DAT_00003e60 != *(int *)DAT_00003e64) {
    *(int *)DAT_00003e64 = *DAT_00003e60;
    i2c_write_reg(*DAT_00003e64 >> 8,0x3b);
    i2c_write_reg((char)*DAT_00003e64,0x3c);
  }
  pcVar1 = DAT_00003e6c;
  if (*DAT_00003e68 != *DAT_00003e6c) {
    *DAT_00003e6c = *DAT_00003e68;
    i2c_write_reg(*pcVar1,0x3d);
  }
  pcVar1 = DAT_00003e74;
  if (*DAT_00003e70 != *DAT_00003e74) {
    *DAT_00003e74 = *DAT_00003e70;
    i2c_write_reg(*pcVar1,0x3e);
  }
  pcVar1 = DAT_00003e7c;
  if (*DAT_00003e78 != *DAT_00003e7c) {
    *DAT_00003e7c = *DAT_00003e78;
    i2c_write_reg(*pcVar1,0x3f);
  }
  pcVar1 = DAT_00003e84;
  if (*DAT_00003e80 != *DAT_00003e84) {
    *DAT_00003e84 = *DAT_00003e80;
    i2c_write_reg(*pcVar1,0x5a);
  }
  pcVar1 = DAT_00003e8c;
  if (*DAT_00003e88 != *DAT_00003e8c) {
    *DAT_00003e8c = *DAT_00003e88;
    i2c_write_reg(*pcVar1,0x5b);
  }
  pcVar1 = DAT_00003e94;
  if (*DAT_00003e90 != *DAT_00003e94) {
    *DAT_00003e94 = *DAT_00003e90;
    i2c_write_reg(*pcVar1,0x5c);
  }
  pcVar1 = DAT_00003e9c;
  if (*DAT_00003e98 != *DAT_00003e9c) {
    *DAT_00003e9c = *DAT_00003e98;
    i2c_write_reg(*pcVar1,0x5d);
  }
  pcVar1 = DAT_00003ea4;
  if (*DAT_00003ea0 != *DAT_00003ea4) {
    *DAT_00003ea4 = *DAT_00003ea0;
    i2c_write_reg(*pcVar1,0x5e);
  }
  pcVar1 = DAT_00003eac;
  if (*DAT_00003ea8 != *DAT_00003eac) {
    *DAT_00003eac = *DAT_00003ea8;
    i2c_write_reg(*pcVar1,0x5f);
  }
  pcVar1 = DAT_00003eb4;
  if (*DAT_00003eb0 != *DAT_00003eb4) {
    *DAT_00003eb4 = *DAT_00003eb0;
    i2c_write_reg(*pcVar1,0x60);
  }
  pcVar1 = DAT_00003ebc;
  if (*DAT_00003eb8 != *DAT_00003ebc) {
    *DAT_00003ebc = *DAT_00003eb8;
    i2c_write_reg(*pcVar1,0x61);
  }
  pcVar1 = DAT_000042c4;
  if (*DAT_000042c0 != *DAT_000042c4) {
    *DAT_000042c4 = *DAT_000042c0;
    i2c_write_reg(*pcVar1,0x62);
  }
  pcVar1 = DAT_000042cc;
  if (*DAT_000042c8 != *DAT_000042cc) {
    *DAT_000042cc = *DAT_000042c8;
    i2c_write_reg(*pcVar1,0x6e);
  }
  pcVar1 = DAT_000042d4;
  if (*DAT_000042d0 != *DAT_000042d4) {
    *DAT_000042d4 = *DAT_000042d0;
    i2c_write_reg(*pcVar1,0x6f);
  }
  pcVar1 = DAT_000042dc;
  if (*DAT_000042d8 != *DAT_000042dc) {
    *DAT_000042dc = *DAT_000042d8;
    i2c_write_reg(*pcVar1,0x70);
  }
  pcVar1 = DAT_000042e4;
  if (*DAT_000042e0 != *DAT_000042e4) {
    *DAT_000042e4 = *DAT_000042e0;
    i2c_write_reg(*pcVar1,0x71);
  }
  pcVar1 = DAT_000042ec;
  if (*DAT_000042e8 != *DAT_000042ec) {
    *DAT_000042ec = *DAT_000042e8;
    i2c_write_reg(*pcVar1,0x72);
  }
  pcVar1 = DAT_000042f4;
  if (*DAT_000042f0 != *DAT_000042f4) {
    *DAT_000042f4 = *DAT_000042f0;
    i2c_write_reg(*pcVar1,100);
  }
  if (*DAT_000042f8 != *(int *)DAT_000042fc) {
    *(int *)DAT_000042fc = *DAT_000042f8;
    i2c_write_reg(*DAT_000042fc >> 8,0x65);
    i2c_write_reg((char)*DAT_000042fc,0x66);
  }
  pcVar1 = DAT_00004304;
  if (*DAT_00004300 != *DAT_00004304) {
    *DAT_00004304 = *DAT_00004300;
    i2c_write_reg(*pcVar1,0x67);
  }
  pcVar1 = DAT_0000430c;
  if (*DAT_00004308 != *DAT_0000430c) {
    *DAT_0000430c = *DAT_00004308;
    i2c_write_reg(*pcVar1,0x68);
  }
  if (*DAT_00004310 != *(int *)DAT_00004314) {
    *(int *)DAT_00004314 = *DAT_00004310;
    i2c_write_reg(*DAT_00004314 >> 8,0x97);
    i2c_write_reg((char)*DAT_00004314,0x98);
  }
  if (*DAT_00004318 != *(int *)DAT_0000431c) {
    *(int *)DAT_0000431c = *DAT_00004318;
    i2c_write_reg(*DAT_0000431c >> 8,0x99);
    i2c_write_reg((char)*DAT_0000431c,0x9a);
  }
  if (*DAT_00004320 != *(int *)DAT_00004324) {
    *(int *)DAT_00004324 = *DAT_00004320;
    i2c_write_reg(*DAT_00004324 >> 8,0x9b);
    i2c_write_reg((char)*DAT_00004324,0x9c);
  }
  if (*DAT_00004328 != *(int *)DAT_0000432c) {
    *(int *)DAT_0000432c = *DAT_00004328;
    i2c_write_reg(*DAT_0000432c >> 8,0xc9);
    i2c_write_reg((char)*DAT_0000432c,0xca);
  }
  if (*DAT_00004330 != *(int *)DAT_00004334) {
    *(int *)DAT_00004334 = *DAT_00004330;
    i2c_write_reg(*DAT_00004334 >> 8,0xcb);
    i2c_write_reg((char)*DAT_00004334,0xcc);
  }
  if (*DAT_00004338 != *(int *)DAT_0000433c) {
    *(int *)DAT_0000433c = *DAT_00004338;
    i2c_write_reg(*DAT_0000433c >> 8,0xcd);
    i2c_write_reg((char)*DAT_0000433c,0xce);
  }
  if (*DAT_00004340 != *(int *)DAT_00004344) {
    *(int *)DAT_00004344 = *DAT_00004340;
    i2c_write_reg(*DAT_00004344 >> 8,0xcf);
    i2c_write_reg((char)*DAT_00004344,0xd0);
  }
  if (*DAT_00004348 != *(int *)DAT_0000434c) {
    *(int *)DAT_0000434c = *DAT_00004348;
    i2c_write_reg(*DAT_0000434c >> 8,0xd1);
    i2c_write_reg((char)*DAT_0000434c,0xd2);
  }
  if (*DAT_00004350 != *(int *)DAT_00004354) {
    *(int *)DAT_00004354 = *DAT_00004350;
    i2c_write_reg(*DAT_00004354 >> 8,0xd3);
    i2c_write_reg((char)*DAT_00004354,0xd4);
  }
  if (*DAT_00004358 != *(int *)DAT_0000435c) {
    *(int *)DAT_0000435c = *DAT_00004358;
    i2c_write_reg(*DAT_0000435c >> 8,0xba);
    i2c_write_reg((char)*DAT_0000435c,0xbb);
  }
  if (*DAT_00004360 != *(int *)DAT_00004364) {
    *(int *)DAT_00004364 = *DAT_00004360;
    i2c_write_reg(*DAT_00004364 >> 8,0xbc);
    i2c_write_reg((char)*DAT_00004364,0xbd);
  }
  if (*DAT_00004368 != *(int *)DAT_0000436c) {
    *(int *)DAT_0000436c = *DAT_00004368;
    i2c_write_reg(*DAT_0000436c >> 8,0xbe);
    i2c_write_reg((char)*DAT_0000436c,0xbf);
  }
  return;
}
