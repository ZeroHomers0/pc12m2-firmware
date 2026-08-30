/* =============================================================================
 * 08_modbus_dispatch.c — modbus_dispatch(0xB642) C 级还原
 * 目标B W1a：替换 firmware/stub.c 占位。依据 evidence/reverse/disassembly/08_modbus_dispatch_asm.txt
 *   （5161 条指令）逐段还原，数据地址全部以反汇编字面量 SRAM 值为准。
 *
 * 函数：0x0000B642-0xE573（Modbus RTU 从站帧解析与分发主处理）
 * 调用点：main() 主循环 modbus_dispatch(0)
 * 调用关系：crc16(0xAF64)、uart3_tx_byte(0xAE0C)、param_sync_live_to_eeprom(0x35F2)、
 *   i2c_write_reg(0x1E88)×2、out_relay_p020(0x10588)×2、modbus_write_multi(0xB2E0)、
 *   modbus_read_reg(0xAF94)
 *
 * 关键 SRAM（真实地址，反汇编字面量确认）：
 *   0x100022A4 接收帧缓冲       0x1000236C 发送缓冲(TX)
 *   0x100016FF 本站从站地址     0x10001790 接收/发送状态（0 空闲/1 首字节/5 完整帧）
 *   0x10001792 接收帧长度       0x100017F0 写寄存器 16 位值缓存(word)
 *   0x100017F4 CRC 计算缓存     0x10002754 帧末 CRC 暂存
 *   0x2009C000 FIO 池(+0x3C FIO4CLR)   0x4009C000 UART3(+0x04 IER)
 *   0x1000178C/0x100017B8 计数
 *   0x100017A4/0x10001798/0x1000179C/0x100017A0/0x100017A8/0x100017B0/0x100017B4
 *     —— 0x10 写多/0x03 读区工作变量
 *
 * 写分支（0x06 单写）53 个：寄存器号 0x1001-0x103E（缺 0x101E-0x1020、0x1029-0x102E、
 * 0x103F；0x101E/0x1F/0x20/0x29-0x2E 无写分支，0x103F 不存在）。每分支 =
 *   值缓存 store → 范围校验 → 参数槽 store（word 或 byte）→ param_sync →
 *   8 字节响应 [地址,0x06,0x10,reg,val_hi,val_lo,CRC16]。
 *   特殊分支：0x1001 控制方式(byte 0x10001634,值<3)、0x1017 控制方式+增益组复制、
 *   0x1018/0x1019 第4组增益(控制方式==4 即时同步)、0x1021-0x1024 仅允许写 0、
 *   0x1025/0x1026 无 param_sync 特殊命令、0x102F 从站地址、0x103D 远程使能。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* ---- 依赖函数前向声明（签名与定义模块核实一致；形参名语义化，无行为影响） ---- */
uint16_t crc16(uint8_t *data, uint16_t len);                   /* 08_uart3_modbus.c */
void uart3_tx_byte(uint8_t tx_byte);                           /* 08_uart3_modbus.c */
uint32_t modbus_read_reg(uint *out_val, uint reg_addr);        /* 08_uart3_modbus.c */
uint32_t modbus_write_multi(uint32_t *src_val, uint reg_addr); /* 08_uart3_modbus.c */
void param_sync_live_to_eeprom(void);                          /* 06_param_system.c */
void i2c_write_reg(uint32_t data, uint32_t reg_addr);          /* 04_i2c.c */
void out_relay_p020(int on);                                   /* 10_relay_led.c */

/* ---- 数据指针（真实 SRAM 地址，volatile 因被 ISR 写入） ---- */
#define RX_STATE   ((volatile uint8_t *)0x10001790)   /* 接收/发送状态 */
#define RX_LEN     ((volatile uint8_t *)0x10001792)   /* 接收帧长度 */
#define SLAVE_ADDR ((volatile uint8_t *)0x100016FF)   /* 本站从站地址 */
#define VALCACHE   ((volatile uint32_t *)0x100017F0)  /* 写寄存器值缓存(word) */
#define FRAME      ((uint8_t *)0x100022A4)            /* 接收帧缓冲 */
#define TXBUF      ((uint8_t *)0x1000236C)            /* 发送缓冲 */
#define FIO4CLR    (*(volatile uint32_t *)0x2009C03C) /* FIO4 清零（P4.29 错误指示） */
#define UART3_IER  (*(volatile uint32_t *)0x4009C004) /* UART3 中断使能 */

/* =============================================================================
 * modbus_dispatch(0x0000B642)
 * 流程（对照 evidence/reverse/disassembly/08_modbus_dispatch_asm.txt）：
 *   1) 帧态门控：状态==1 → 只清计数返回；==5 → 继续；否则返回
 *   2) 从站地址匹配：帧[0]==本站地址？不匹配 → FIO4CLR P4.29 + 清状态 + IER + 返回
 *   3) 功能码分发：非 0x03/0x06/0x10 → 异常响应 [地址,func|0x80,0x01]
 *   4) CRC 校验：crc16(帧, len-2) vs 帧末两字节；不匹配 → 异常 [..,0x04]
 *   5) 0x06 写单：值=帧[4..5] → 值缓存 → 53 分支
 *   6) 0x10 写多：modbus_write_multi 循环 → 响应 [地址,0x10,0x10,起始reg,数量,CRC16]
 *   7) 0x03 读：modbus_read_reg 循环 → 响应 [地址,0x03,字节数,数据..,CRC16]
 *   8) 兜底帧尾：FIO4CLR P4.29 + 清状态 + UART3 IER
 *   入口参数 arg：保留未用（主循环调用传 0；帧数据全部读自全局 SRAM，见 #define 数据指针）。
 * ========================================================================== */
void modbus_dispatch(int arg)
{
  volatile uint8_t *const rx_state = RX_STATE;
  volatile uint8_t *const rx_len = RX_LEN;
  volatile uint8_t *const slave = SLAVE_ADDR;
  volatile uint32_t *const valcache = VALCACHE;
  uint8_t *const frame = FRAME;
  uint8_t *const tx = TXBUF;
  uint16_t crc;
  uint32_t v;
  uint32_t reg, cnt;
  uint32_t i;
  (void)arg;

  /* 1) 帧态门控 */
  if (*rx_state == 1) {
    *(volatile uint32_t *)0x100017B8 = 0;
    return;
  }
  if (*rx_state != 5) {
    return;
  }
  *(volatile uint32_t *)0x1000178C = 0;
  *(volatile uint32_t *)0x100017B8 = 0;

  /* 2) 从站地址匹配 */
  if (frame[0] != *slave) {
    FIO4CLR |= 0x20000000;
    *rx_state = 0;
    UART3_IER |= 1;
    return;
  }

  /* 3) 功能码分发（非法功能码） */
  if (frame[1] != 0x03 && frame[1] != 0x06 && frame[1] != 0x10) {
    tx[0] = *slave; tx[1] = frame[1] | 0x80; tx[2] = 0x01;
    crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* 4) CRC 校验（帧末两字节 vs 计算值） */
  v = (uint32_t)*rx_len;
  crc = crc16((uint8_t *)frame, v - 2);
  if ((uint8_t)(crc & 0xff) != frame[v - 2] || (uint8_t)(crc >> 8) != frame[v - 1]) {
    tx[0] = *slave; tx[1] = frame[1] | 0x80; tx[2] = 0x04;
    crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* ================= 5) 0x06 写单寄存器（53 分支） ================= */
  if (frame[1] == 0x06 && frame[2] == 0x10) {
    v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];   /* 原 BIN 0xB798: frame[4]<<8|frame[5] 大端 */
    *valcache = v;
    switch (frame[3]) {
    case 0x01:                    /* 0x1001 控制方式（byte 参数，值<3） */
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x10001634 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x02:                    /* 0x1002 值 10..6000 → word 0x1000163C */
      if (v > 6000 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x1000163C = v;
      param_sync_live_to_eeprom();
      break;
    case 0x03:                    /* 0x1003 → word 0x10001638 */
      if (v > 6000 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x10001638 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x04:                    /* 0x1004 → word 0x10001640 */
      if (v > 6000 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x10001640 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x05:                    /* 0x1005 → word 0x10001648 */
      if (v > 6000 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x10001648 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x06:                    /* 0x1006 → word 0x10001644 */
      if (v > 6000 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x10001644 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x07:                    /* 0x1007 byte 值<201 → 0x1000164C */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x1000164C = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x08:                    /* 0x1008 byte 值<201 → 0x1000164D */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x1000164D = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x09:                    /* 0x1009 word 值<181 → 0x10001650 */
      if (v >= 181) { goto bad_value; }
      *(volatile uint32_t *)0x10001650 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x0A:                    /* 0x100A byte 40..160 → 0x10001654 */
      if (v >= 161 || v <= 39) { goto bad_value; }
      *(volatile uint8_t *)0x10001654 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x0B:                    /* 0x100B byte 值<3 → 0x10001655 */
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x10001655 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x0C:                    /* 0x100C byte 值<2 → 0x10001656 */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x10001656 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x0D:                    /* 0x100D word 值<6001 → 0x100016C0 */
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016C0 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x0E:                    /* 0x100E byte 值<201 → 0x100016C4 */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016C4 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x0F:                    /* 0x100F word 值<6001 → 0x100016C8 */
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016C8 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x10:                    /* 0x1010 byte 值<201 → 0x100016CC */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016CC = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x11:                    /* 0x1011 word 值<6001 → 0x100016D0 */
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016D0 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x12:                    /* 0x1012 byte 值<201 → 0x100016D4 */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016D4 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x13:                    /* 0x1013 word 值<6001 → 0x100016D8 */
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016D8 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x14:                    /* 0x1014 byte 值<201 → 0x100016DC */
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016DC = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x15:                    /* 0x1015 byte 值<2 → 0x100016DD */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x100016DD = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x16:                    /* 0x1016 byte 值<61 → 0x100016DE */
      if (v >= 61) { goto bad_value; }
      *(volatile uint8_t *)0x100016DE = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x17:                    /* 0x1017 控制方式（1..4）+ 对应增益组复制到活动槽 */
      if (v >= 5 || v == 0) { goto bad_value; }
      *(volatile uint8_t *)0x10001710 = (uint8_t)v;
      if (v == 1) { *(volatile uint8_t *)0x1000170E = *(volatile uint8_t *)0x10001711;
                    *(volatile uint8_t *)0x1000170F = *(volatile uint8_t *)0x10001712; }
      if (v == 2) { *(volatile uint8_t *)0x1000170E = *(volatile uint8_t *)0x10001713;
                    *(volatile uint8_t *)0x1000170F = *(volatile uint8_t *)0x10001714; }
      if (v == 3) { *(volatile uint8_t *)0x1000170E = *(volatile uint8_t *)0x10001715;
                    *(volatile uint8_t *)0x1000170F = *(volatile uint8_t *)0x10001716; }
      if (v == 4) { *(volatile uint8_t *)0x1000170E = *(volatile uint8_t *)0x10001717;
                    *(volatile uint8_t *)0x1000170F = *(volatile uint8_t *)0x10001718; }
      param_sync_live_to_eeprom();
      break;
    case 0x18:                    /* 0x1018 第4组增益1（1..128）；控制方式==4 即时同步 */
      if (v >= 129 || v == 0) { goto bad_value; }
      *(volatile uint8_t *)0x10001717 = (uint8_t)v;
      if (*(volatile uint8_t *)0x10001710 == 4) {
        *(volatile uint8_t *)0x1000170E = *(volatile uint8_t *)0x10001717;
      }
      param_sync_live_to_eeprom();
      break;
    case 0x19:                    /* 0x1019 第4组增益2（1..128）；控制方式==4 即时同步 */
      if (v >= 129 || v == 0) { goto bad_value; }
      *(volatile uint8_t *)0x10001718 = (uint8_t)v;
      if (*(volatile uint8_t *)0x10001710 == 4) {
        *(volatile uint8_t *)0x1000170F = *(volatile uint8_t *)0x10001718;
      }
      param_sync_live_to_eeprom();
      break;
    case 0x1A:                    /* 0x101A byte 2..199 → 0x10001694（cmp 0xc8;bcs 上限 0xC8） */
      if (v >= 200 || v <= 1) { goto bad_value; }
      *(volatile uint8_t *)0x10001694 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x1B:                    /* 0x101B word 值<4501 → 0x10001698 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x10001698 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x1C:                    /* 0x101C word 值<4501 → 0x100016A0 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016A0 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x1D:                    /* 0x101D word 值<4501 → 0x100016A8 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016A8 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x21:                    /* 0x1021 仅允许写 0 → word 0x100015FC */
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x100015FC = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x22:                    /* 0x1022 仅允许写 0 → word 0x100015F8 */
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x100015F8 = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x23:                    /* 0x1023 仅允许写 0 → word 0x10001608 */
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x10001608 = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x24:                    /* 0x1024 仅允许写 0 → word 0x10001604 */
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x10001604 = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x25:                    /* 0x1025 特殊：仅允许写 0 → word 0x10001624（无 param_sync） */
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x10001624 = 0;
      break;
    case 0x26:                    /* 0x1026 特殊：仅允许写 0 → i2c_write_reg(0,5/6)（无 param_sync） */
      if (v != 0) { goto bad_value; }
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      break;
    case 0x27:                    /* 0x1027 byte 值<2 → 0x10001785 */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x10001785 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x28:                    /* 0x1028 word 无范围 → 0x10001788 */
      *(volatile uint32_t *)0x10001788 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x2F:                    /* 0x102F 从站地址（1..247）→ byte 0x100016FF（即本站地址） */
      if (v >= 248 || v == 0) { goto bad_value; }
      *slave = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x30:                    /* 0x1030 word 值<8 → 0x10001700 */
      if (v >= 8) { goto bad_value; }
      *(volatile uint32_t *)0x10001700 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x31:                    /* 0x1031 byte 值<4 → 0x10001704 */
      if (v >= 4) { goto bad_value; }
      *(volatile uint8_t *)0x10001704 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x32:                    /* 0x1032 byte 值<2 → 0x10001705 */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x10001705 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x33:                    /* 0x1033 word 值<4501 → 0x10001698（同 0x101B 槽） */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x10001698 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x34:                    /* 0x1034 → 0x100016A0 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016A0 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x35:                    /* 0x1035 → 0x100016A8 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016A8 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x36:                    /* 0x1036 → 0x100016B0 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016B0 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x37:                    /* 0x1037 → 0x100016B8 */
      if (v >= 4501) { goto bad_value; }
      *(volatile uint32_t *)0x100016B8 = v;
      param_sync_live_to_eeprom();
      break;
    case 0x38:                    /* 0x1038 byte 值<3 → 0x10001657 */
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x10001657 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x39:                    /* 0x1039 byte 值<3 → 0x10001658 */
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x10001658 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x3A:                    /* 0x103A byte 值<2 → 0x10001659 */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x10001659 = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x3B:                    /* 0x103B byte 值<2 → 0x1000165A */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x1000165A = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x3C:                    /* 0x103C byte 值<2 → 0x1000165B */
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)0x1000165B = (uint8_t)v;
      param_sync_live_to_eeprom();
      break;
    case 0x3D:                    /* 0x103D 远程使能：值<101；非 0 → out_relay_p020(1)，0 → (0) */
      if (v >= 101) { goto bad_value; }
      if (v != 0) { out_relay_p020(1); }
      else { out_relay_p020(0); }
      *(volatile uint32_t *)0x1000165C = v;
      param_sync_live_to_eeprom();
      break;
    case 0x3E:                    /* 0x103E word 值<181 → 0x10001660 */
      if (v >= 181) { goto bad_value; }
      *(volatile uint32_t *)0x10001660 = v;
      param_sync_live_to_eeprom();
      break;
    default:
      goto bad_address;           /* 原 BIN 0xE16A：未匹配寄存器 → [地址,0x86,0x02] */
    }
    /* 正常写响应：[地址,0x06,0x10,reg,val_hi,val_lo,CRC16] */
    tx[0] = *slave; tx[1] = 0x06; tx[2] = 0x10; tx[3] = frame[3];
    tx[4] = (uint8_t)(v >> 8); tx[5] = (uint8_t)(v & 0xff);
    crc = crc16((uint8_t *)tx, 6); tx[6] = crc & 0xff; tx[7] = crc >> 8;
    uart3_tx_byte(8);
    return;

  bad_address:                    /* 非法寄存器地址 → 异常响应 [地址,0x86,0x02] */
    tx[0] = *slave; tx[1] = 0x86; tx[2] = 0x02;
    crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
    uart3_tx_byte(5);
    return;

  bad_value:                      /* 值越界 → 异常响应 [地址,0x86,0x03] */
    tx[0] = *slave; tx[1] = 0x86; tx[2] = 0x03;
    crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* ================= 6) 0x10 写多寄存器 ================= */
  if (frame[1] == 0x10 && frame[2] == 0x10) {
    reg = (uint32_t)frame[3];
    cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];   /* 原 BIN 0xE202: frame[4]<<8|frame[5] 大端 */
    if (reg == 0 || reg > 0x3E) {
      tx[0] = *slave; tx[1] = 0x90; tx[2] = 0x02;      /* 非法地址 */
      crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    if (cnt == 0 || cnt > 0x3E || frame[6] != (cnt << 1)) {
      tx[0] = *slave; tx[1] = 0x90; tx[2] = 0x03;      /* 非法数量/字节数 */
      crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    for (i = 0; i < cnt; i++) {
      v = ((uint32_t)frame[7 + i * 2] << 8) | (uint32_t)frame[8 + i * 2];  /* 原 BIN 0xE2C2 大端 */
      *(volatile uint32_t *)0x100017A4 = v;
      modbus_write_multi((uint32_t *)0x100017A4, reg - 1 + i);  /* 内部表 0 基（reg1→case0） */
    }
    /* 响应：[地址,0x10,0x10,起始reg,数量_hi,数量_lo,CRC16] */
    tx[0] = *slave; tx[1] = 0x10; tx[2] = 0x10; tx[3] = (uint8_t)reg;
    tx[4] = (uint8_t)(cnt >> 8); tx[5] = (uint8_t)(cnt & 0xff);
    crc = crc16((uint8_t *)tx, 6); tx[6] = crc & 0xff; tx[7] = crc >> 8;
    uart3_tx_byte(8);
    return;
  }

  /* ================= 7) 0x03 读保持寄存器 ================= */
  if (frame[1] == 0x03 && frame[2] == 0x10) {
    reg = (uint32_t)frame[3];
    cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];   /* 原 BIN 0xE408: frame[4]<<8|frame[5] 大端 */
    if (reg == 0 || reg > 0x3F) {
      tx[0] = *slave; tx[1] = 0x83; tx[2] = 0x02;
      crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    if (cnt == 0 || (reg - 1 + cnt) > 0x3F) {   /* asm: cnt+(reg-1)>0x3F → 越界 */
      tx[0] = *slave; tx[1] = 0x83; tx[2] = 0x03;
      crc = crc16((uint8_t *)tx, 3); tx[3] = crc & 0xff; tx[4] = crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    tx[0] = *slave; tx[1] = 0x03; tx[2] = (uint8_t)(cnt << 1);
    for (i = 0; i < cnt; i++) {
      /* modbus_read_reg 把数据写入 *out_val(0x100017A4)，返回值恒为 0。
       * 原机码 bl 后用 ldrh r0,[rx] 回读 *out_val 取数据，不用返回值。 */
      modbus_read_reg((uint *)0x100017A4, reg - 1 + i);  /* 内部表 0 基 */
      v = (uint32_t)*(uint16_t *)0x100017A4;
      tx[3 + i * 2] = (uint8_t)(v >> 8);
      tx[4 + i * 2] = (uint8_t)(v & 0xff);
    }
    crc = crc16((uint8_t *)tx, 3 + cnt * 2);
    tx[3 + cnt * 2] = crc & 0xff;
    tx[4 + cnt * 2] = crc >> 8;
    uart3_tx_byte(5 + cnt * 2);
    return;
  }

                                  /* 8) 兜底帧尾：结构不符（帧[2]!=0x10 等） */
  FIO4CLR |= 0x20000000;
  *rx_state = 0;
  UART3_IER |= 1;
  return;
}
