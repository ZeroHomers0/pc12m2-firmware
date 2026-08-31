/* =============================================================================
 * 08_modbus_dispatch.c — modbus_dispatch(0x0000B3B2) 12p 完整还原（PC12M-2）
 * 依据 evidence/reverse/disassembly/functions/0000b3b2_FUN_0000b3b2.txt
 * （9215 行）逐块字节级核对。本函数为 Modbus RTU 从站帧解析与分发主处理。
 *
 * 函数：0x0000B3B2-0xE2CE（12p，基准 pc12m2_orig.bin）
 * 调用点：main() 主循环 modbus_dispatch()（见 01_startup.c）
 * 调用关系：crc16(0xACD4)、uart3_tx_byte(0xAB7C)、modbus_read_reg(0xAD04)、
 *   modbus_write_multi(0xB050)、param_sync_live_to_eeprom(0x3534)、
 *   i2c_write_reg(0x1E38)×2
 *
 * 12p 关键 SRAM（反汇编池槽引用确认）：
 *   0x10001770 comm_state（帧态：0 空闲/1 收帧/5 完整帧，字节）
 *   0x1000176C comm_quiet_timer（字）  0x10001798 comm_scan_timer（字）
 *   0x10001772 comm_tx_len（接收帧长度，字节）
 *   0x100016F7 menu_state（本站从站地址，字节）
 *   0x10002278 lookup_table（接收帧缓冲）  0x10002340 comm_frame_buf（发送缓冲）
 *   0x100017D0 写单值缓存(word)    0x100017D4 protocol_work_2（CRC 字）
 *   0x10002728 buf_data_1（CRC 双缓冲暂存）  0x10001784 读/写多值槽(word)
 *   0x10001790 disp_param（起始寄存器）  0x10001788 g_reg_cur_idx
 *   0x10001780 frame_count（数量 Q）    0x10001778 menu_param_3（N）
 *   0x1000177C menu_param_4（字节数）   0x10001794 menu_index（循环计数）
 *   0x2009C03C FIO1CLR（P1.29 RS485）  0x4009C004 UART3 IER
 *
 * 流程（对照 dump）：
 *   1) 帧态门控：state==1 → 清 scan_timer 返回；!=5 → 返回；==5 → 清双计时器
 *   2) 从站地址匹配：帧[0]==本站地址？不匹配 → FIO1CLR P1.29 + 清态 + IER + 返回
 *   3) 功能码分发：非 0x03/0x06/0x10 → 异常 [地址,func|0x80,0x01]
 *   4) CRC 校验（12p 双缓冲）：crc16(帧,len-2)→0x100017D4(word)，
 *      buf_data_1[len-1]/[len-2]=crc 高/低字节，再与帧末两字节比较；不匹配 → 异常 0x04
 *   5) 0x06 写单：线性 if 链（非 TBB）。入口 frame[2]==0x10；frame[3] 匹配 53 块；
 *      每块 = valcache(0x100017D0) 存 → 边界 → 参数槽存(字/字节) → param_sync →
 *      8 字节响应 [地址,0x06,0x10,reg,val_hi,val_lo,CRC16]。无块匹配 → [0x86,0x02]。
 *      reg37/38：仅 v==0，响应后自旋死循环（0xD028/0xD0F8，不复位）；
 *      reg38 无存储，响应后 i2c_write_reg(0,5/6) 再自旋。
 *   6) 0x10 写多：frame[2]!=0x10 → [0x90,0x02]；start_reg==0||>0x3E → [0x90,0x02]；
 *      Q==0||>0x3E 或 frame[6]!=Q*2 → [0x90,0x03]；循环 modbus_write_multi；
 *      响应 [地址,0x10,0x10,start_reg,Q_hi,Q_lo,CRC16]。
 *   7) 0x03 读：frame[2]!=0x10 → [0x83,0x02]；start_reg==0||>0x3F → [0x83,0x02]；
 *      N==0||end_reg>0x3F → [0x83,0x03]。
 *      ★ N+2 怪癖（忠实复现）：byte_count 先写 TX[2]=2N 再 +=3 → 0x1000177C=2N+3；
 *      循环 while(menu_index<2N+3) 步进 2 → 迭代 N+2 次（读 N+2 个寄存器，
 *      前 N 个值入响应，第 N/N+1 个被 CRC 覆盖/超出发送长度）；CRC 长度=2N+3，
 *      send(2N+5)。
 *   8) 帧[1]!=3 防御重置（死代码，func 已被 3) 过滤）：
 *      FIO1CLR P1.29 + 清态 + U3IER + 返回。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/globals.h"

/* ---- 依赖函数前向声明（签名与定义模块核实一致） ---- */
uint16_t crc16(uint8_t *data, uint16_t len);                   /* 08_uart3_modbus.c */
void uart3_tx_byte(uint8_t frame_len);                         /* 08_uart3_modbus.c */
uint32_t modbus_read_reg(uint32_t *out_val, uint32_t reg_addr);/* 08_uart3_modbus.c */
uint32_t modbus_write_multi(uint32_t *src_val, uint32_t reg_idx);/* 08_uart3_modbus.c */
void param_sync_live_to_eeprom(void);                          /* 06_param_system.c */
void i2c_write_reg(uint32_t data, uint32_t reg_addr);          /* 04_i2c.c */

/* ---- 数据指针（12p 真实 SRAM 地址，volatile 因被 ISR 写入） ---- */
#define RX_STATE   ((volatile uint8_t *)comm_state)            /* 0x10001770 帧态 */
#define RX_LEN     ((volatile uint8_t *)comm_tx_len)           /* 0x10001772 接收帧长 */
#define SLAVE_ADDR ((volatile uint8_t *)menu_state)            /* 0x100016F7 从站地址 */
#define FRAME      ((uint8_t *)lookup_table)                   /* 0x10002278 接收帧 */
#define TXBUF      ((uint8_t *)comm_frame_buf)                 /* 0x10002340 发送缓冲 */
#define VALCACHE   ((volatile uint32_t *)0x100017d0)           /* 写单值缓存(word) */
#define CRCWORD    ((volatile uint32_t *)0x100017d4)           /* protocol_work_2 */
#define CRCCOPY    ((uint8_t *)0x10002728)                     /* buf_data_1 */
#define VALSLOT    ((volatile uint32_t *)0x10001784)           /* 读/写多值槽(word) */
#define DISP_REG   ((volatile uint32_t *)0x10001790)           /* disp_param 起始寄存器 */
#define REG_CUR    ((volatile uint32_t *)g_reg_cur_idx)        /* 0x10001788 */
#define FRAME_CNT  ((volatile uint32_t *)0x10001780)           /* frame_count 数量 */
#define MENU_P3    ((volatile uint32_t *)menu_param_3)         /* 0x10001778 数量 N */
#define MENU_P4    ((volatile uint32_t *)menu_param_4)         /* 0x1000177C 字节数 */
#define MENU_IDX   ((volatile uint32_t *)menu_index)           /* 0x10001794 循环计数 */
#define FIO1CLR    (*(volatile uint32_t *)0x2009c03c)          /* P1.29 RS485 方向 */
#define UART3_IER  (*(volatile uint32_t *)0x4009c004)          /* UART3 中断使能 */

/* =============================================================================
 * 异常响应：TX[0..4] = [从站地址, func, 异常码, CRC16_lo, CRC16_hi]，send 5 字节。
 * 12p 各异常路径内联构建，此处辅助函数产生相同 SRAM 写序（crc16 为纯函数）。
 * ========================================================================== */
static void modbus_send_exception(uint8_t func, uint8_t code)
{
  uint16_t crc;
  uint8_t *tx = TXBUF;

  tx[0] = *SLAVE_ADDR;
  tx[1] = func;
  tx[2] = code;
  crc = crc16((uint8_t *)tx, 3);
  tx[3] = (uint8_t)crc;
  tx[4] = (uint8_t)(crc >> 8);
  uart3_tx_byte(5);
}

/* =============================================================================
 * PID 参数银行复制（reg23-25 写单触发，读 g_cfg_pid_sel=0x10001708 字节决定）：
 *   sel==1 → 0x1709/0x170A → 活动槽 pid_kp2/pid_ki2(0x1706/0x1707)
 *   sel==2 → 0x170B/0x170C    sel==3 → 0x170D/0x170E    sel==4 → 0x170F/0x1710
 * 与 08_uart3_modbus.c::modbus_read_reg 的 PID 前导同一套复制（字节复制）。
 * ========================================================================== */
static void pid_bank_copy(void)
{
  uint8_t sel = *(volatile uint8_t *)0x10001708;               /* g_cfg_pid_sel */

  if (sel == 1) {
    *(volatile uint8_t *)0x10001706 = *(volatile uint8_t *)0x10001709;
    *(volatile uint8_t *)0x10001707 = *(volatile uint8_t *)0x1000170a;
  }
  if (sel == 2) {
    *(volatile uint8_t *)0x10001706 = *(volatile uint8_t *)0x1000170b;
    *(volatile uint8_t *)0x10001707 = *(volatile uint8_t *)0x1000170c;
  }
  if (sel == 3) {
    *(volatile uint8_t *)0x10001706 = *(volatile uint8_t *)0x1000170d;
    *(volatile uint8_t *)0x10001707 = *(volatile uint8_t *)0x1000170e;
  }
  if (sel == 4) {
    *(volatile uint8_t *)0x10001706 = *(volatile uint8_t *)0x1000170f;
    *(volatile uint8_t *)0x10001707 = *(volatile uint8_t *)0x10001710;
  }
}

/* =============================================================================
 * modbus_dispatch(0x0000B3B2) — 完整 12p 还原。返回恒 0（12p 各出口 movs r0,#0）。
 * 帧数据全部读自全局 SRAM（RX 缓冲 0x10002278），详见头部数据指针注释。
 * ========================================================================== */
void modbus_dispatch(void)
{
  uint8_t *const frame = FRAME;                    /* 0x10002278 接收帧 */
  uint8_t *const tx = TXBUF;                       /* 0x10002340 发送缓冲 */
  uint16_t crc;
  uint32_t v;
  uint32_t reg, cnt;
  uint32_t i;
  uint16_t len;

  /* 1) 帧态门控（0xB3B6-0xB3D8） */
  if (*RX_STATE == 1) {
    *comm_scan_timer = 0;                          /* 仅清 scan_timer 返回 */
    return;
  }
  if (*RX_STATE != 5) {
    return;
  }
  *comm_quiet_timer = 0;                           /* state==5：清双计时器 */
  *comm_scan_timer = 0;

  /* 2) 从站地址匹配（0xB3DA-0xB406） */
  if (frame[0] != *SLAVE_ADDR) {
    FIO1CLR |= 0x20000000;
    *RX_STATE = 0;
    UART3_IER |= 1;
    return;
  }

  /* 3) 功能码分发：非 0x03/0x06/0x10 → 异常 0x01（0xB408-0xB456） */
  if (frame[1] != 0x03 && frame[1] != 0x06 && frame[1] != 0x10) {
    modbus_send_exception((uint8_t)(frame[1] | 0x80), 0x01);
    return;
  }

  /* 4) CRC 校验（0xB458-0xB4EC，12p 双缓冲） */
  len = (uint16_t)*RX_LEN;
  crc = crc16((uint8_t *)frame, len - 2);
  *CRCWORD = crc;                                  /* protocol_work_2 = crc(word) */
  CRCCOPY[len - 1] = (uint8_t)(crc >> 8);          /* buf_data_1[len-1] = crc 高 */
  CRCCOPY[len - 2] = (uint8_t)crc;                 /* buf_data_1[len-2] = crc 低 */
  if (CRCCOPY[len - 1] != frame[len - 1] || CRCCOPY[len - 2] != frame[len - 2]) {
    modbus_send_exception((uint8_t)(frame[1] | 0x80), 0x04);
    return;
  }

  /* ================= 5) 0x06 写单寄存器（线性 if 链，53 块） ================= */
  if (frame[1] == 0x06) {
    uint8_t e_hi, e_lo;

    if (frame[2] != 0x10) {
      goto bad_address;                            /* frame[2]!=0x10 → 0x86/0x02 */
    }
    switch (frame[3]) {
    case 0x01:                                     /* 0x1001 控制方式：B 0x162C */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)gain_sel = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;                                    /* B-echo：(int32)(uint8)v>>8 = 0 */
      e_lo = (uint8_t)*(volatile uint8_t *)gain_sel;
      break;
    case 0x02:                                     /* 0x1002 → W 0x1634 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001 || v <= 9) { goto bad_value; }
      *gain_b = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*gain_b >> 8);    /* W-echo：ldrh 回读 */
      e_lo = (uint8_t)*gain_b;
      break;
    case 0x03:                                     /* 0x1003 → W 0x1630 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001 || v <= 9) { goto bad_value; }
      *gain_a = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*gain_a >> 8);
      e_lo = (uint8_t)*gain_a;
      break;
    case 0x04:                                     /* 0x1004 → W 0x1638 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001 || v <= 9) { goto bad_value; }
      *gain_coef = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*gain_coef >> 8);
      e_lo = (uint8_t)*gain_coef;
      break;
    case 0x05:                                     /* 0x1005 → W 0x1640 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001 || v <= 9) { goto bad_value; }
      *(volatile uint32_t *)0x10001640 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001640 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001640;
      break;
    case 0x06:                                     /* 0x1006 → W 0x163C */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001 || v <= 9) { goto bad_value; }
      *gain_c = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*gain_c >> 8);
      e_lo = (uint8_t)*gain_c;
      break;
    case 0x07:                                     /* 0x1007 → B 0x1644 启动相移 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)startup_div = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)startup_div;
      break;
    case 0x08:                                     /* 0x1008 → B 0x1645 停止相移 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)stop_div = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)stop_div;
      break;
    case 0x09:                                     /* 0x1009 → W 0x1648 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 181) { goto bad_value; }
      *llim_angle = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*llim_angle >> 8);
      e_lo = (uint8_t)*llim_angle;
      break;
    case 0x0a:                                     /* 0x100A → B 0x164C 40..160 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 161 || v <= 39) { goto bad_value; }
      *(volatile uint8_t *)0x1000164c = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x1000164c;
      break;
    case 0x0b:                                     /* 0x100B → B 0x164D */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x1000164d = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x1000164d;
      break;
    case 0x0c:                                     /* 0x100C → B 0x164E */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_1 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_1;
      break;
    case 0x0d:                                     /* 0x100D → W 0x16B8 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016b8 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016b8 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016b8;
      break;
    case 0x0e:                                     /* 0x100E → B 0x16BC */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016bc = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x100016bc;
      break;
    case 0x0f:                                     /* 0x100F → W 0x16C0 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016c0 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016c0 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016c0;
      break;
    case 0x10:                                     /* 0x1010 → B 0x16C4 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016c4 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x100016c4;
      break;
    case 0x11:                                     /* 0x1011 → W 0x16C8 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016c8 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016c8 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016c8;
      break;
    case 0x12:                                     /* 0x1012 → B 0x16CC */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016cc = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x100016cc;
      break;
    case 0x13:                                     /* 0x1013 → W 0x16D0 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 6001) { goto bad_value; }
      *(volatile uint32_t *)0x100016d0 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016d0 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016d0;
      break;
    case 0x14:                                     /* 0x1014 → B 0x16D4 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 201) { goto bad_value; }
      *(volatile uint8_t *)0x100016d4 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x100016d4;
      break;
    case 0x15:                                     /* 0x1015 → B 0x16D5 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_12 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_12;
      break;
    case 0x16:                                     /* 0x1016 → B 0x16D6 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 61) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_13 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_13;
      break;
    case 0x17:                                     /* 0x1017 控制方式：B 0x1708 + PID 银行复制 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v == 0 || v >= 5) { goto bad_value; }
      *(volatile uint8_t *)g_cfg_pid_sel = (uint8_t)v;
      pid_bank_copy();                             /* 按新 g_cfg_pid_sel 复制 */
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)g_cfg_pid_sel;
      break;
    case 0x18:                                     /* 0x1018 第4组KP：B 0x170F + 银行复制 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v == 0 || v >= 129) { goto bad_value; }
      *(volatile uint8_t *)0x1000170f = (uint8_t)v;
      pid_bank_copy();                             /* 若 g_cfg_pid_sel==4 即时同步 */
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x1000170f;
      break;
    case 0x19:                                     /* 0x1019 第4组KI：B 0x1710 + 银行复制 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v == 0 || v >= 129) { goto bad_value; }
      *(volatile uint8_t *)0x10001710 = (uint8_t)v;
      pid_bank_copy();
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x10001710;
      break;
    case 0x1a:                                     /* 0x101A → B 0x168C 触发相 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 200 || v <= 1) { goto bad_value; }
      *(volatile uint8_t *)trig_phase = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)trig_phase;
      break;
    case 0x1b:                                     /* 0x101B → W 0x1690 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x10001690 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001690 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001690;
      break;
    case 0x1c:                                     /* 0x101C → W 0x1698 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x10001698 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001698 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001698;
      break;
    case 0x1d:                                     /* 0x101D → W 0x16A0 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x100016a0 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016a0 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016a0;
      break;
    case 0x21:                                     /* 0x1021 仅允许写 0 → W 0x15FC */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x100015fc = 0;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = 0;
      break;
    case 0x22:                                     /* 0x1022 仅允许写 0 → W 0x15F8 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x100015f8 = 0;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = 0;
      break;
    case 0x23:                                     /* 0x1023 仅允许写 0 → W 0x1608 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x10001608 = 0;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = 0;
      break;
    case 0x24:                                     /* 0x1024 仅允许写 0 → W 0x1604 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x10001604 = 0;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = 0;
      break;
    case 0x25:                                     /* 0x1025 特殊命令：仅 v==0，写 out_param 后自旋 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      *(volatile uint32_t *)0x1000161c = 0;        /* out_param = 0 */
      tx[0] = *SLAVE_ADDR; tx[1] = 0x06; tx[2] = 0x10; tx[3] = 0x25;
      tx[4] = 0; tx[5] = 0;
      crc = crc16((uint8_t *)tx, 6); tx[6] = (uint8_t)crc; tx[7] = (uint8_t)(crc >> 8);
      uart3_tx_byte(8);
      for (;;) { }                                 /* 0xD028 自旋死循环（不复位） */
      /* 不可达 */
    case 0x26:                                     /* 0x1026 特殊命令：仅 v==0，i2c 清两寄存器后自旋 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v != 0) { goto bad_value; }
      tx[0] = *SLAVE_ADDR; tx[1] = 0x06; tx[2] = 0x10; tx[3] = 0x26;
      tx[4] = 0; tx[5] = 0;
      crc = crc16((uint8_t *)tx, 6); tx[6] = (uint8_t)crc; tx[7] = (uint8_t)(crc >> 8);
      uart3_tx_byte(8);
      i2c_write_reg(0, 5);                         /* 0x1E38 两次 */
      i2c_write_reg(0, 6);
      for (;;) { }                                 /* 0xD0F8 自旋死循环 */
      /* 不可达 */
    case 0x27:                                     /* 0x1027 → B 0x1765 远程使能 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)menu_state2 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)menu_state2;
      break;
    case 0x28:                                     /* 0x1028 → W 0x1768 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v > 1000) { goto bad_value; }            /* cmp.w #0x3E8; bhi */
      *(volatile uint32_t *)0x10001768 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001768 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001768;
      break;
    case 0x2f:                                     /* 0x102F 从站地址：B 0x16F7 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v == 0 || v >= 248) { goto bad_value; }
      *SLAVE_ADDR = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*SLAVE_ADDR;
      break;
    case 0x30:                                     /* 0x1030 → W 0x16F8 波特率索引 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 8) { goto bad_value; }
      *comm_baud_idx = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*comm_baud_idx >> 8);
      e_lo = (uint8_t)*comm_baud_idx;
      break;
    case 0x31:                                     /* 0x1031 → B 0x16FC 帧格式 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4) { goto bad_value; }
      *(volatile uint8_t *)comm_div_sel = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)comm_div_sel;
      break;
    case 0x32:                                     /* 0x1032 → B 0x16FD 通讯开关 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)comm_rx_flag = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)comm_rx_flag;
      break;
    case 0x33:                                     /* 0x1033 → W 0x1690（同 0x101B 槽） */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x10001690 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001690 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001690;
      break;
    case 0x34:                                     /* 0x1034 → W 0x1698 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x10001698 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x10001698 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x10001698;
      break;
    case 0x35:                                     /* 0x1035 → W 0x16A0 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x100016a0 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016a0 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016a0;
      break;
    case 0x36:                                     /* 0x1036 → W 0x16A8 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x100016a8 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016a8 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016a8;
      break;
    case 0x37:                                     /* 0x1037 → W 0x16B0 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 4501 || v <= 3499) { goto bad_value; }
      *(volatile uint32_t *)0x100016b0 = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*(volatile uint32_t *)0x100016b0 >> 8);
      e_lo = (uint8_t)*(volatile uint32_t *)0x100016b0;
      break;
    case 0x38:                                     /* 0x1038 → B 0x164F */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_2 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_2;
      break;
    case 0x39:                                     /* 0x1039 → B 0x1650 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 3) { goto bad_value; }
      *(volatile uint8_t *)0x10001650 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)0x10001650;
      break;
    case 0x3a:                                     /* 0x103A → B 0x1651 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_3 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_3;
      break;
    case 0x3b:                                     /* 0x103B → B 0x1652 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)eeprom_adc_cfg = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_adc_cfg;
      break;
    case 0x3c:                                     /* 0x103C → B 0x1653 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 2) { goto bad_value; }
      *(volatile uint8_t *)eeprom_param_4 = (uint8_t)v;
      param_sync_live_to_eeprom();
      e_hi = 0;
      e_lo = (uint8_t)*(volatile uint8_t *)eeprom_param_4;
      break;
    case 0x3d:                                     /* 0x103D → W 0x1654 微调 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 101) { goto bad_value; }
      *out_fine = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*out_fine >> 8);
      e_lo = (uint8_t)*out_fine;
      break;
    case 0x3e:                                     /* 0x103E → W 0x1658 */
      v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
      *VALCACHE = v;
      if (v >= 181) { goto bad_value; }
      *ulim_angle = v;
      param_sync_live_to_eeprom();
      e_hi = (uint8_t)((uint16_t)*ulim_angle >> 8);
      e_lo = (uint8_t)*ulim_angle;
      break;
    default:
      goto bad_address;                            /* 无匹配寄存器 → [0x86,0x02] */
    }
    /* 正常写单响应（0xB52E-0xB582 各块内联等价）：[地址,0x06,0x10,reg,val_hi,val_lo,CRC16] */
    tx[0] = *SLAVE_ADDR; tx[1] = 0x06; tx[2] = 0x10; tx[3] = frame[3];
    tx[4] = e_hi; tx[5] = e_lo;
    crc = crc16((uint8_t *)tx, 6); tx[6] = (uint8_t)crc; tx[7] = (uint8_t)(crc >> 8);
    uart3_tx_byte(8);
    return;
  }

  /* ================= 6) 0x10 写多寄存器 ================= */
  if (frame[1] == 0x10) {
    if (frame[2] != 0x10) {                        /* 0xD5B8 → [0x90,0x02] */
      modbus_send_exception(0x90, 0x02);
      return;
    }
    reg = (uint32_t)frame[3];                      /* start_reg */
    *DISP_REG = reg;                               /* disp_param = start_reg */
    *REG_CUR = reg - 1;                            /* g_reg_cur_idx = start_reg-1 */
    if (reg == 0 || reg > 0x3e) {                  /* 0xDF1C-0xDF28 */
      modbus_send_exception(0x90, 0x02);
      return;
    }
    cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];   /* Q */
    *FRAME_CNT = cnt;                              /* frame_count = Q */
    if (cnt == 0 || cnt > 0x3e) {                  /* 0xDF70-0xDF78 */
      modbus_send_exception(0x90, 0x03);
      return;
    }
    if (frame[6] != (uint8_t)(cnt << 1)) {         /* 0xDFAE-0xDFC0：byte_count != Q*2 */
      modbus_send_exception(0x90, 0x03);
      return;
    }
    *MENU_P4 = (uint32_t)frame[6];                 /* menu_param_4 = byte_count */
    *MENU_IDX = 0;                                 /* menu_index = 0（循环计数） */
    while (*MENU_IDX < *FRAME_CNT) {               /* 0xE044-0xE04E */
      i = *MENU_IDX;
      v = ((uint32_t)frame[7 + i * 2] << 8) | (uint32_t)frame[8 + i * 2];  /* 大端 */
      *VALSLOT = v;                                /* 0x10001784 = 值 */
      modbus_write_multi((uint32_t *)0x10001784, *REG_CUR);
      *MENU_IDX = i + 1;
      *REG_CUR = *REG_CUR + 1;
    }
    /* 响应（0xE050-0xE094）：[地址,0x10,0x10,start_reg,Q_hi,Q_lo,CRC16] */
    tx[0] = *SLAVE_ADDR; tx[1] = 0x10; tx[2] = 0x10; tx[3] = (uint8_t)reg;
    tx[4] = (uint8_t)(cnt >> 8); tx[5] = (uint8_t)(cnt & 0xff);
    crc = crc16((uint8_t *)tx, 6); tx[6] = (uint8_t)crc; tx[7] = (uint8_t)(crc >> 8);
    uart3_tx_byte(8);
    return;
  }

  /* ================= 7) 0x03 读保持寄存器 ================= */
  if (frame[1] != 0x03) {                          /* 0xE0CA 防御重置（死代码，func 已过滤） */
    FIO1CLR |= 0x20000000;
    *RX_STATE = 0;
    UART3_IER |= 1;
    return;
  }
  if (frame[2] != 0x10) {                          /* 0xE0D2 → [0x83,0x02] */
    modbus_send_exception(0x83, 0x02);
    return;
  }
  reg = (uint32_t)frame[3];                        /* start_reg */
  *DISP_REG = reg;                                 /* disp_param = start_reg */
  *REG_CUR = reg - 1;                              /* g_reg_cur_idx = start_reg-1 */
  if (reg == 0 || reg > 0x3f) {                    /* 0xE122-0xE12E */
    modbus_send_exception(0x83, 0x02);
    return;
  }
  cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];   /* N */
  *MENU_P3 = cnt;                                  /* menu_param_3 = N */
  *MENU_P4 = cnt << 1;                             /* menu_param_4 = byte_count = 2N */
  *FRAME_CNT = *REG_CUR + cnt;                     /* frame_count = end_reg = start_reg-1+N */
  if (cnt == 0 || *FRAME_CNT > 0x3f) {             /* 0xE18A-0xE196 */
    modbus_send_exception(0x83, 0x03);
    return;
  }
  tx[0] = *SLAVE_ADDR; tx[1] = 0x03;
  tx[2] = (uint8_t)*MENU_P4;                       /* TX[2] = 2N（+=3 前快照） */
  *MENU_P4 = *MENU_P4 + 3;                         /* 2N+3（N+2 怪癖） */
  *MENU_IDX = 0;
  while (*MENU_IDX < *MENU_P4) {                   /* 0xE1F4-0xE23A：N+2 次迭代 */
    modbus_read_reg((uint32_t *)0x10001784, *REG_CUR);
    v = (uint32_t)*(volatile uint16_t *)0x10001784;   /* ldrh 回读值槽 */
    tx[*MENU_IDX + 3] = (uint8_t)(v >> 8);
    tx[*MENU_IDX + 4] = (uint8_t)v;
    *MENU_IDX = *MENU_IDX + 2;
    *REG_CUR = *REG_CUR + 1;
  }
  crc = crc16((uint8_t *)tx, (uint16_t)*MENU_P4);  /* 长度=2N+3 */
  tx[*MENU_P4] = (uint8_t)crc;
  tx[*MENU_P4 + 1] = (uint8_t)(crc >> 8);
  uart3_tx_byte((uint8_t)(*MENU_P4 + 2));          /* 2N+5 */
  return;

  /* 非法寄存器地址（无匹配块/帧[2]!=0x10）→ [地址,0x86,0x02]（0xDEC6） */
bad_address:
  modbus_send_exception(0x86, 0x02);
  return;

  /* 值越界 → [地址,0x86,0x03]（各块内联 0xB584 等） */
bad_value:
  modbus_send_exception(0x86, 0x03);
  return;
}
