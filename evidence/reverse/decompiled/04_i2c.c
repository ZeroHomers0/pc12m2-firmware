/* =============================================================================
 * LPC1765FBD100 (ST33C / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 04：I2C GPIO 位带模拟（访问芯片@0x53 AT24C02C）
 *
 * 硬件：SDA=P0.10（FIO0 bit10=0x400）、SCL=P0.11（FIO0 bit11=0x800）
 *   固件用位带（FIO0SET/CLR）模拟 I2C，无硬件 I2C0/I2C2 控制器引用
 * 器件：7-bit 地址 0x53，写 0xA6 / 读 0xA7（byte 寻址，24C02 类 EEPROM）
 *   i2c_write_reg = [0xA6][reg][val]+STOP；i2c_read_reg = [0xA6][reg]→restart→[0xA7]读 1 字节
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · 芯片@0x53 = AT24C02C（U6）精确证实 → docs/HARDWARE_VERIFICATION_2026-08-20.md §一.2、§六.A
 *   · 参数存储架构（live/shadow/双银行/61 组同步）→ docs/i2c_param_sync.md、06_param_system.c
 *   · 换 EEPROM / 参数备份 → APPLICATION_GUIDE_2026-08-21.md §五.3
 * ========================================================================== */

/* 0x00001C40 —— I2C GPIO 初始化：P0.10(SDA)/P0.11(SCL) 置输出（FIODIR）
 *   DAT_00001EFC = 0x2009C000（FIO0 池基址）；[6]=FIO0SET(+0x18)、[7]=FIO0CLR(+0x1C) */
void i2c_gpio_init(void)
{
  uint *puVar1;

  puVar1 = DAT_00001efc;
  *DAT_00001efc = *DAT_00001efc | 0x800;    /* P0.11 SCL 输出 */
  *puVar1 = *puVar1 | 0x400;                /* P0.10 SDA 输出 */
  puVar1[6] = puVar1[6] | 0x800;            /* SCL 初始高 */
  puVar1[6] = puVar1[6] | 0x400;            /* SDA 初始高 */
  return;
}

/* 0x00001C6C —— 短延时（5 空循环） */
void i2c_delay_short(void)
{
  undefined4 local_8;

  for (local_8 = 5; local_8 != 0; local_8 = local_8 + -1) {
  }
  return;
}

/* 0x00001C82 —— 延时（param_1 × 10000 空循环；EEPROM 写周期等待用） */
void i2c_delay(uint param_1)
{
  undefined4 local_c;
  undefined4 local_8;

  for (local_8 = 0; local_8 < param_1; local_8 = local_8 + 1) {
    for (local_c = 0; local_c < 10000; local_c = local_c + 1) {
    }
  }
  return;
}

/* 0x00001CAE —— START 条件：SDA=1 → SCL=1 → SDA=0 → SCL=0 */
undefined4 i2c_start(void)
{
  uint *puVar1;

  *DAT_00001efc = *DAT_00001efc | 0x400;    /* SDA=1 */
  i2c_delay_short();
  puVar1 = DAT_00001efc;
  DAT_00001efc[6] = DAT_00001efc[6] | 0x400;
  puVar1[6] = puVar1[6] | 0x800;            /* SCL=1 */
  i2c_delay_short();
  DAT_00001efc[7] = DAT_00001efc[7] | 0x400;  /* SDA=0 */
  i2c_delay_short();
  DAT_00001efc[7] = DAT_00001efc[7] | 0x800;  /* SCL=0 */
  i2c_delay_short();
  return 1;
}

/* 0x00001CFE —— STOP 条件：SCL=1 → SDA=1 */
void i2c_stop(void)
{
  uint *puVar1;

  *DAT_00001efc = *DAT_00001efc | 0x400;    /* SDA=1 */
  i2c_delay_short();
  puVar1 = DAT_00001efc;
  DAT_00001efc[7] = DAT_00001efc[7] | 0x400;  /* SDA=0 */
  puVar1[6] = puVar1[6] | 0x800;            /* SCL=1 */
  i2c_delay_short();
  DAT_00001efc[6] = DAT_00001efc[6] | 0x400;  /* SDA=1 */
  i2c_delay_short();
  return;
}

/* 0x00001D3C —— 发送 1 字节（MSB 先）+ 读 ACK（SDA 输入位=0 → ACK）
 *   返回 ACK 状态（0x10001F00 由调用者记录） */
undefined4 i2c_write_byte(undefined4 param_1)
{
  uint *puVar1;
  byte bVar2;
  uint uVar3;
  uint extraout_r1;
  uint extraout_r1_00;
  uint extraout_r1_01;
  undefined4 extraout_r1_02;
  undefined4 extraout_r2;

  uVar3 = *DAT_00001efc;
  *DAT_00001efc = uVar3 | 0x400;            /* SDA=1 */
  i2c_delay_short(uVar3 | 0x400,param_1);
  uVar3 = extraout_r1;
  for (bVar2 = 0; puVar1 = DAT_00001efc, bVar2 < 8; bVar2 = bVar2 + 1) {
    if ((uVar3 & 0x80) == 0) {
      DAT_00001efc[7] = DAT_00001efc[7] | 0x400;   /* 位=0 → SDA=0 */
    }
    else {
      DAT_00001efc[6] = DAT_00001efc[6] | 0x400;   /* 位=1 → SDA=1 */
    }
    DAT_00001efc[6] = DAT_00001efc[6] | 0x800;     /* SCL=1 */
    i2c_delay_short();
    DAT_00001efc[7] = DAT_00001efc[7] | 0x800;     /* SCL=0 */
    i2c_delay_short(extraout_r1_00 << 0x19,(extraout_r1_00 & 0x7f) << 1);
    uVar3 = extraout_r1_01;
  }
  *DAT_00001efc = *DAT_00001efc & 0xfffffbff;   /* SDA 方向输入（读 ACK） */
  puVar1[6] = puVar1[6] | 0x400;
  i2c_delay_short();
  DAT_00001efc[6] = DAT_00001efc[6] | 0x800;    /* SCL=1 */
  i2c_delay_short();
  puVar1 = DAT_00001efc + 5;
  uVar3 = DAT_00001efc[7];
  DAT_00001efc[7] = uVar3 | 0x800;              /* SCL=0 */
  i2c_delay_short(uVar3 | 0x800,extraout_r1_02,(*puVar1 & 0x400) != 0);  /* 读 SDA */
  *DAT_00001efc = *DAT_00001efc | 0x400;        /* SDA 方向输出 */
  return extraout_r2;
}

/* 0x00001DFA —— 接收 1 字节（MSB 先）+ 发 NACK；返回读到的字节 */
undefined4 i2c_read_byte(void)
{
  uint *puVar1;
  uint uVar2;
  uint uVar3;
  uint extraout_r1;
  undefined4 extraout_r1_00;
  int extraout_r2;

  puVar1 = DAT_00001efc;
  *DAT_00001efc = *DAT_00001efc & 0xfffffbff;   /* SDA 输入 */
  uVar2 = puVar1[6] | 0x400;
  puVar1[6] = uVar2;
  i2c_delay_short(uVar2,0);
  uVar2 = 0;
  while (uVar2 < 8) {
    i2c_delay_short();
    DAT_00001efc[7] = DAT_00001efc[7] | 0x800;  /* SCL=0 */
    i2c_delay_short();
    DAT_00001efc[6] = DAT_00001efc[6] | 0x800;  /* SCL=1 */
    i2c_delay_short();
    uVar2 = (extraout_r1 & 0x7f) * 2;           /* 移位累加 */
    uVar3 = DAT_00001efc[5];                    /* 读 SDA 引脚 */
    if ((uVar3 & 0x400) != 0) {
      uVar3 = uVar2 + 1;
      uVar2 = uVar3;
    }
    i2c_delay_short(uVar3,uVar2);
    uVar2 = extraout_r2 + 1U & 0xff;
  }
  i2c_delay_short();
  DAT_00001efc[7] = DAT_00001efc[7] | 0x800;    /* SCL=0 */
  i2c_delay_short();
  *DAT_00001efc = *DAT_00001efc | 0x400;        /* SDA 输出（NACK） */
  i2c_delay_short();
  return extraout_r1_00;
}

/* 0x00001E88 —— 写芯片寄存器：[0xA6][reg][val] + STOP + 写周期等待
 *   param_1=数据、param_2=寄存器号；ACK 记录到 0x10001F00 */
void i2c_write_reg(undefined4 param_1,undefined4 param_2)
{
  undefined1 uVar1;

  i2c_start();
  uVar1 = i2c_write_byte(0xa6);      /* 器件地址写（0x53<<1） */
  *DAT_00001f00 = uVar1;
  uVar1 = i2c_write_byte(param_2);   /* 寄存器号 */
  *DAT_00001f00 = uVar1;
  uVar1 = i2c_write_byte(param_1);   /* 数据 */
  *DAT_00001f00 = uVar1;
  i2c_stop();
  i2c_delay(5);                      /* EEPROM 写周期 */
  return;
}

/* 0x00001EBC —— 读芯片寄存器：[0xA6][reg] → restart → [0xA7] 读 1 字节 → STOP
 *   param_1=输出缓冲区、param_2=寄存器号 */
void i2c_read_reg(undefined1 *param_1,undefined4 param_2)
{
  undefined1 uVar1;

  i2c_start();
  uVar1 = i2c_write_byte(0xa6);      /* 器件地址写 */
  *DAT_00001f00 = uVar1;
  uVar1 = i2c_write_byte(param_2);   /* 寄存器号 */
  *DAT_00001f00 = uVar1;
  i2c_start();                       /* 重复 START */
  uVar1 = i2c_write_byte(0xa7);      /* 器件地址读 */
  *DAT_00001f00 = uVar1;
  uVar1 = i2c_read_byte(0);
  *param_1 = uVar1;                  /* 读 1 字节 */
  i2c_stop();
  i2c_delay(2);
  return;
}
