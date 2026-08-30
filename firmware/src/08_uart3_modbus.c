/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 08：UART3（Modbus RTU 从站）+ CRC16
 *
 * UART3 硬件：g_uart3 = UART3 基址 0x4009C000（U3RBR/U3THR=+0x00、
 *   U3IER=+0x04、U3IIR/FCR=+0x08、U3LCR=+0x0C、U3LSR=+0x14、U3DLL=+0x00、
 *   U3DLM=+0x04）。DAT_0000b00c = FIO 池 0x2009C000（+0x20 FIO0DIR、+0x3c FIO1CLR，
 *   RS485 方向/空闲态）；DAT_0000b014 = SCB 0x400FC000（+0xc4 PCONP UART3 上电）；
 *   g_pinsel = PINSEL 0x4002C000（+0 引脚复用为 UART3 TXD/RXD）。
 *   L0 修正：上游反汇编注"B00C=SCB(0x400F4000)"有误，实为 FIO 池 0x2009C000。
 * 波特率：U3LCR=0x80.. 0x9B（DLAB 置位写分频），分频值 = PCLK /
 *   (波特率×查表系数/1000)，系数随 PCLK 表 0x1000B028（BAUD_FAC_0/BAUD_FAC_3/...）。
 * 协议：Modbus RTU，CRC16 查表（初值 0xFFFF 低位在前）。寄存器映射见
 *   MENU_PARAMETER_MAPPING.md §3；reg 0x0-0x3F 段地址 0x1000B4B8..0x1000B590，
 *   reg 0x2B-0x3D 段 0x1000B984..0x1000B9C4（按 0x1000B068 控制方式选择组）。
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · Modbus-RTU 协议逆向（帧/功能码/CRC/异常码）→ docs/uart3_protocol.md
 *   · 63 寄存器全表 / 读写不对称 → docs/PROGRESS_2026-08-20.md §4b、§4d
 *   · 通讯菜单参数 → docs/MENU_PARAMETER_MAPPING.md §3
 *   · 上位机集成示例 → APPLICATION_GUIDE_2026-08-21.md §二
 *   · 模块变量地址表 → DATA_SEGMENT_2026-08-21.md §5（flash 0x0000B00C 指针表）
 * ========================================================================== */

/* =============================================================================
 * src/08_uart3_modbus.c — 反编译模块 08（UART3 RS485 Modbus RTU 从站）可编译副本
 * 目标B 阶段4 修正：
 *   1) 补 include（types.h/reg.h/globals.h）。
 *   2) DAT_0000b00c 等 = UART3 指针表（flash 0xB00C-0xB094，初值 0x4009C000 基址），
 *      value 型下 DAT_x+off = 字节偏移 ✓（UART3 寄存器）。
 *   3) 跨模块函数 extern。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"
#include "inc/consts.h"
#include <stdbool.h>

/* CRC16 查表表（内嵌 const 数组，原 flash 0x11034/0x11134 —— 见 crc16_table.c，
 * bug #S9：原反编译用 0x11034/0x11134 悬空 flash 指针，GCC 重定位后读未编程区 0xFF） */
extern const uint8_t crc16_hi_tbl[256];
extern const uint8_t crc16_lo_tbl[256];

/* 0xAED0 RX 组帧子例程（骨架 stub 在 firmware/stub.c，W1 还原） */
void func_0x0000aed0(void);

/* 0x0000AC24 —— UART3 初始化：波特率重算 + 8N1 + 使能 TX/RX
 *   0x1000B01C=数据位(0/1/2/3→LCR 0x87/8B/9B/83 含 DLAB)；
 *   0x1000B024=波特率索引(0..7)→查表 0x1000B028 得系数，分频=PCLK/(波特率×系数/1000) */
void uart3_init(uint divisor)
{
  int fio;
  volatile uint32_t *pinsel;
  volatile uint8_t *uart3;

  fio = DAT_0000b00c;
  *(volatile uint *)(DAT_0000b00c + 0x20) = *(volatile uint *)(DAT_0000b00c + 0x20) | 0x20000000;
  *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x20000000;
  *(volatile uint *)(DAT_0000b014 + 0xc4) = *g_pconp | 0x2000000;   /* PCONP UART3 上电 */
  pinsel = g_pinsel;
  *g_pinsel = *g_pinsel | 2;                            /* PINSEL UART3 引脚 */
  *pinsel = *pinsel | 8;
  if (*g_uart_frame_sel == '\0') {
    g_uart3[0xc] = 0x87;                                   /* LCR：8 位+DLAB */
  }
  if (*g_uart_frame_sel == '\x01') {
    g_uart3[0xc] = 0x8b;
  }
  if (*g_uart_frame_sel == '\x02') {
    g_uart3[0xc] = 0x9b;
  }
  if (*g_uart_frame_sel == '\x03') {
    g_uart3[0xc] = 0x83;
  }
  uart3 = g_uart3;
  if (*g_baud_idx < 3) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_0) /
                            1000));
  }
  if (*g_baud_idx == 3) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_3) /
                            1000));
  }
  if (*g_baud_idx == 4) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_4) /
                            1000));
  }
  if (*g_baud_idx == 5) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_5) /
                            1000));
  }
  if (*g_baud_idx == 6) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_6) /
                            1000));
  }
  if (*g_baud_idx == 7) {
    divisor = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(volatile int *)(DAT_0000b028 + *g_baud_idx * 4) * BAUD_FAC_7) /
                            1000));
  }
  g_uart3[4] = (char)(divisor + ((uint)((int)divisor >> 0x1f) >> 0x18) >> 8);  /* DLM=高字节 */
  *uart3 = (char)divisor;                                         /* DLL=低字节 */
  if (*g_uart_frame_sel == '\0') {
    uart3[0xc] = 7;                                             /* LCR：8N1，清 DLAB */
  }
  if (*g_uart_frame_sel == '\x01') {
    g_uart3[0xc] = 0xb;
  }
  if (*g_uart_frame_sel == '\x02') {
    g_uart3[0xc] = 0x1b;
  }
  if (*g_uart_frame_sel == '\x03') {
    g_uart3[0xc] = 3;
  }
  g_uart3[8] = 7;                                           /* FCR：使能 FIFO+清 */
  uart3 = g_uart3;
  NVIC_ISER0 = 0x100;
  *(volatile uint *)(g_uart3 + 4) = *(volatile uint *)(g_uart3 + 4) | 1;  /* IER RBR 中断 */
  *(volatile uint *)(uart3 + 4) = *(volatile uint *)(uart3 + 4) | 2;              /* IER THRE 中断 */
  return;
}

/* 0x0000AE0C —— UART3 发送 1 字节（U3THR 写 + 等 THRE 位）
 *   0x1000B030=发送状态、0x1000B038=发送长度、0x1000B03C=发送缓冲 */
void uart3_tx_byte(undefined1 tx_byte)
{
  *(volatile uint *)(DAT_0000b00c + 0x38) = *(volatile uint *)(DAT_0000b00c + 0x38) | 0x20000000;
  *g_uart_tx_state = 6;
  *g_uart_tx_flag = 0;
  *g_uart_tx_len = tx_byte;
  do {
  } while ((g_uart3[0x14] & 0x20) == 0);                    /* 等 THRE */
  *g_uart3 = *g_uart_tx_buf;
  return;
}

/* 0x0000AE50 —— 接收超时监控（主循环每 tick 调）：
 *   0x1000B040=接收进行标志、0x1000B044=接收空闲计数（>0x7530 置 0x1000B048|0x8000 帧超时）、
 *   0x1000B04C=全局 tick 计数（>300 重新 uart3_init 防锁死）、
 *   0x1000B030=1 发送中：计数>10 → 状态=5、关 THRE 中断 */
void uart3_rx_timeout_monitor(void)
{
  volatile uint32_t *rx_gap_cnt;
  volatile uint32_t *tick_cnt;
  volatile uint8_t *tx_tick;

  rx_gap_cnt = g_uart_rx_timeout;
  if ((*g_comm_detect != '\0') &&
     (*g_uart_rx_timeout = *g_uart_rx_timeout + 1, 0x7530 < *rx_gap_cnt)) {
    *g_uart_rx_timeout = 0;
    *DAT_0000b048 = *DAT_0000b048 | 0x8000;
  }
  tick_cnt = g_uart_global_tick;
  *g_uart_global_tick = *g_uart_global_tick + 1;
  if (300 < *tick_cnt) {
    *tick_cnt = 0;
    uart3_init(0);
  }
  tx_tick = g_uart_tx_busy;
  if (*g_uart_tx_state == '\x01') {
    *g_uart_tx_busy = *g_uart_tx_busy + 1;
    if (10 < *tx_tick) {
      *tx_tick = 0;
      *g_uart_tx_state = '\x05';
      *(volatile uint *)(g_uart3 + 4) = *(volatile uint *)(g_uart3 + 4) & 0xfffffffe;
    }
  }
  return;
}

/* 0x0000AF08 —— UART3 中断：IIR 判因
 *   因=4（RX 数据可用）→ 子例程 0xAED0（接收组帧，未识别为函数，逐字节存入
 *     0x1000B054 缓冲，并置 0x1000B040=1 接收进行；长度满/收完置标志）
 *   因=2（THRE）→ 从 0x1000B03C 缓冲取下一字节发送，发完关 THRE 中断 */
void UART3_IRQHandler(void)
{
  volatile uint8_t *tx_idx;
  uint iir_cause;

  iir_cause = *(volatile uint *)(g_uart3 + 8) & 0xe;                 /* IIR 中断原因 */
  if (iir_cause == 4) {
    func_0x0000aed0();   /* RX 组帧子例程（extraout_r3 伪影已删：IIR 原因保持原值） */
  }
  tx_idx = g_uart_tx_flag;
  if (iir_cause == 2) {
    *g_uart_tx_flag = *g_uart_tx_flag + 1;                           /* 发送索引++ */
    if (*tx_idx < *g_uart_tx_len) {                               /* 未发完 */
      *g_uart3 = *(volatile undefined1 *)(g_uart_tx_buf + (uint)*g_uart_tx_flag);
    }
    else {                                                        /* 发完 */
      *(volatile uint *)(DAT_0000b00c + 0x3c) = *(volatile uint *)(DAT_0000b00c + 0x3c) | 0x20000000;
      *g_uart_tx_state = 0;
      *(volatile uint *)(g_uart3 + 4) = *(volatile uint *)(g_uart3 + 4) | 1;
    }
  }
  return;
}

/* 0x0000AF64 —— Modbus CRC16（查表，初值 0xFFFF，低位在前）
 *   状态：crc_hi=高字节、crc_lo=低字节，tbl_idx = 数据字节 ^ crc_lo；
 *   查表内嵌 crc16_hi_tbl/crc16_lo_tbl（原 flash 0x11034/0x11134）。
 *
 *   ※ 循环语义（A/B 差分 2026-08-23 实证，非 len-1！）：原码 0xAF84
 *     `movs r0,r4`(查 Z) → `sub.w r6,r4,#1`(**无 S 后缀，不置位**) → `uxtb r4,r6`(后减)。
 *     bne 用的 Z 来自 movs 测试【减前】计数器 —— 故 while(计数器!=0) 精确执行 len 次，
 *     处理全部 len 字节（标准 Modbus CRC）。旧读法把 sub.w 当置位 → 误判成 len-1，为本人 bug。 */
uint16_t crc16(uint8_t *data,uint16_t len)
{
  uint8_t tbl_idx;
  uint8_t crc_lo;
  uint8_t crc_hi;

  crc_hi = 0xff;
  crc_lo = 0xff;
  while (len != 0) {
    tbl_idx = *data ^ crc_lo;
    crc_lo = crc16_hi_tbl[tbl_idx] ^ crc_hi;
    crc_hi = crc16_lo_tbl[tbl_idx];
    data = data + 1;
    len = (uint16_t)(uint8_t)(len - 1);       /* uxtb 字节减；len>=256 处回绕语义保持一致 */
  }
  return crc_lo | crc_hi << 8;
}

/* 0x0000AF94 —— Modbus 读保持寄存器（reg_addr=寄存器号 & 0xFFF）
 *   0x1000B064=寄存器号、0x1000B068=控制方式组选择(1..4)，
 *   组基址表：组1→0x1000B06C/0x1000B074、组2→0x1000B07C/0x1000B080、
 *   组3→0x1000B084/0x1000B088、组4→0x1000B08C/0x1000B090；
 *   reg 0x00-0x3F 映射 0x1000B4B8..0x1000B590（0x1A-0x1F/0x24-0x25 等保留返回 0） */
undefined4 modbus_read_reg(uint *out_val,uint reg_addr)
{
  *g_reg_cur_idx = reg_addr & 0xfff;
  if (*g_cfg_pid_sel == '\x01') {
    *g_act_gain_a = *DAT_0000b06c;
    *g_act_gain_b = *DAT_0000b074;
  }
  if (*g_cfg_pid_sel == '\x02') {
    *g_act_gain_a = *DAT_0000b07c;
    *g_act_gain_b = *DAT_0000b080;
  }
  if (*g_cfg_pid_sel == '\x03') {
    *g_act_gain_a = *DAT_0000b084;
    *g_act_gain_b = *DAT_0000b088;
  }
  if (*g_cfg_pid_sel == '\x04') {
    *g_act_gain_a = *DAT_0000b08c;
    *g_act_gain_b = *DAT_0000b090;
  }
  switch((char)*g_reg_cur_idx) {
  case '\0':
    *out_val = (uint)*g_gain_sel;
    break;
  case '\x01':
    *out_val = *g_gain_a;
    break;
  case '\x02':
    *out_val = *g_gain_b;
    break;
  case '\x03':
    *out_val = *DAT_0000b4c4;
    break;
  case '\x04':
    *out_val = *DAT_0000b4c8;
    break;
  case '\x05':
    *out_val = *DAT_0000b4cc;
    break;
  case '\x06':
    *out_val = (uint)*DAT_0000b4d0;
    break;
  case '\a':
    *out_val = (uint)*DAT_0000b4d4;
    break;
  case '\b':
    *out_val = *DAT_0000b4d8;
    break;
  case '\t':
    *out_val = (uint)*g_out_fine;
    break;
  case '\n':
    *out_val = (uint)*DAT_0000b4e0;
    break;
  case '\v':
    *out_val = (uint)*DAT_0000b4e4;
    break;
  case '\f':
    *out_val = *DAT_0000b4e8;
    break;
  case '\r':
    *out_val = (uint)*DAT_0000b4ec;
    break;
  case '\x0e':
    *out_val = *DAT_0000b4f0;
    break;
  case '\x0f':
    *out_val = (uint)*DAT_0000b4f4;
    break;
  case '\x10':
    *out_val = *DAT_0000b4f8;
    break;
  case '\x11':
    *out_val = (uint)*DAT_0000b4fc;
    break;
  case '\x12':
    *out_val = *DAT_0000b500;
    break;
  case '\x13':
    *out_val = (uint)*DAT_0000b504;
    break;
  case '\x14':
    *out_val = (uint)*DAT_0000b508;
    break;
  case '\x15':
    *out_val = (uint)*DAT_0000b50c;
    break;
  case '\x16':
    *out_val = (uint)*g_cfg_pid_sel;
    break;
  case '\x17':
    *out_val = (uint)*g_act_gain_a;
    break;
  case '\x18':
    *out_val = (uint)*g_act_gain_b;
    break;
  case '\x19':
    *out_val = (uint)*g_phase_calib;
    break;
  case '\x1a':
    *out_val = 0;
    break;
  case '\x1b':
    *out_val = 0;
    break;
  case '\x1c':
    *out_val = 0;
    break;
  case '\x1d':
    *out_val = 0;
    break;
  case '\x1e':
    *out_val = 0;
    break;
  case '\x1f':
    *out_val = 0;
    break;
  case ' ':
    *out_val = *DAT_0000b520;
    break;
  case '!':
    *out_val = *DAT_0000b524;
    break;
  case '\"':
    *out_val = *DAT_0000b528;
    break;
  case '#':
    *out_val = *DAT_0000b52c;
    break;
  case '$':
    *out_val = 0;
    break;
  case '%':
    *out_val = 0;
    break;
  case '&':
    *out_val = (uint)*g_run_flag;
    break;
  case '\'':
    *out_val = *DAT_0000b534;
    break;
  case '(':
    *out_val = *DAT_0000b538;
    break;
  case ')':
    *out_val = *DAT_0000b53c;
    break;
  case '*':
    *out_val = *DAT_0000b540;
    break;
  case '+':
    *out_val = *DAT_0000b544;
    break;
  case ',':
    *out_val = *DAT_0000b548;
    break;
  case '-':
    *out_val = *DAT_0000b54c;
    break;
  case '.':
    *out_val = (uint)*g_slave_addr;
    break;
  case '/':
    *out_val = *g_baud_idx;
    break;
  case '0':
    *out_val = (uint)*g_uart_frame_sel;
    break;
  case '1':
    *out_val = (uint)*g_comm_detect;
    break;
  case '2':
    *out_val = *DAT_0000b560;
    break;
  case '3':
    *out_val = *DAT_0000b564;
    break;
  case '4':
    *out_val = *DAT_0000b568;
    break;
  case '5':
    *out_val = *DAT_0000b56c;
    break;
  case '6':
    *out_val = *DAT_0000b570;
    break;
  case '7':
    *out_val = (uint)*DAT_0000b574;
    break;
  case '8':
    *out_val = (uint)*DAT_0000b578;
    break;
  case '9':
    *out_val = (uint)*DAT_0000b57c;
    break;
  case ':':
    *out_val = (uint)*DAT_0000b580;
    break;
  case ';':
    *out_val = (uint)*g_out_phase;
    break;
  case '<':
    *out_val = *g_reg61_remote_en;
    break;
  case '=':
    *out_val = *g_reg62_start_phase;
    break;
  case '>':
    *out_val = *DAT_0000b590;
  }
  return 0;
}

/* 0x0000B2E0 —— Modbus 写保持寄存器（src_val=数据指针、reg_addr=寄存器号）
 *   0x1000B594=寄存器号；0x1A-0x1F/0x24-0x25/0x2B-0x2F/0x40-0x42 等映射到
 *   0x1000B5A0（保留/汇总区），0x2B-0x3D 段写 0x1000B984..0x1000B9C4 */
undefined4 modbus_write_multi(undefined4 *src_val,uint reg_addr)
{
  volatile uint32_t *reg_ofs;

  reg_ofs = g_reg_cur_idx;
  *g_reg_cur_idx = reg_addr & 0xfff;
  switch((char)*reg_ofs) {
  case '\0':
    *g_gain_sel = *(volatile undefined1 *)src_val;
    break;
  case '\x01':
    *g_gain_a = *src_val;
    break;
  case '\x02':
    *g_gain_b = *src_val;
    break;
  case '\x03':
    *DAT_0000b4c4 = *src_val;
    break;
  case '\x04':
    *DAT_0000b4c8 = *src_val;
    break;
  case '\x05':
    *DAT_0000b4cc = *src_val;
    break;
  case '\x06':
    *DAT_0000b4d0 = *(volatile undefined1 *)src_val;
    break;
  case '\a':
    *DAT_0000b4d4 = *(volatile undefined1 *)src_val;
    break;
  case '\b':
    *DAT_0000b4d8 = *src_val;
    break;
  case '\t':
    *g_out_fine = *(volatile undefined1 *)src_val;
    break;
  case '\n':
    *DAT_0000b4e0 = *(volatile undefined1 *)src_val;
    break;
  case '\v':
    *DAT_0000b4e4 = *(volatile undefined1 *)src_val;
    break;
  case '\f':
    *DAT_0000b4e8 = *src_val;
    break;
  case '\r':
    *DAT_0000b4ec = *(volatile undefined1 *)src_val;
    break;
  case '\x0e':
    *DAT_0000b4f0 = *src_val;
    break;
  case '\x0f':
    *DAT_0000b4f4 = *(volatile undefined1 *)src_val;
    break;
  case '\x10':
    *DAT_0000b4f8 = *src_val;
    break;
  case '\x11':
    *DAT_0000b4fc = *(volatile undefined1 *)src_val;
    break;
  case '\x12':
    *DAT_0000b500 = *src_val;
    break;
  case '\x13':
    *DAT_0000b504 = *(volatile undefined1 *)src_val;
    break;
  case '\x14':
    *DAT_0000b508 = *(volatile undefined1 *)src_val;
    break;
  case '\x15':
    *DAT_0000b50c = *(volatile undefined1 *)src_val;
    break;
  case '\x16':
    *g_cfg_pid_sel = *(volatile undefined1 *)src_val;
    break;
  case '\x17':
    *DAT_0000b598 = *(volatile undefined1 *)src_val;
    break;
  case '\x18':
    *DAT_0000b59c = *(volatile undefined1 *)src_val;
    break;
  case '\x19':
    *g_phase_calib = *(volatile undefined1 *)src_val;
    break;
  case '\x1a':
    *g_scratch = *src_val;
    break;
  case '\x1b':
    *g_scratch = *src_val;
    break;
  case '\x1c':
    *g_scratch = *src_val;
    break;
  case '\x1d':
    *g_scratch = *src_val;
    break;
  case '\x1e':
    *g_scratch = *src_val;
    break;
  case '\x1f':
    *g_scratch = *src_val;
    break;
  case ' ':
    *DAT_0000b520 = *src_val;
    break;
  case '!':
    *DAT_0000b524 = *src_val;
    break;
  case '\"':
    *DAT_0000b528 = *src_val;
    break;
  case '#':
    *DAT_0000b52c = *src_val;
    break;
  case '$':
    *g_scratch = *src_val;
    break;
  case '%':
    *g_scratch = *src_val;
    break;
  case '&':
    *g_run_flag = *(volatile undefined1 *)src_val;
    break;
  case '\'':
    *DAT_0000b534 = *src_val;
    break;
  case '(':
    *g_scratch = *src_val;
    break;
  case ')':
    *g_scratch = *src_val;
    break;
  case '*':
    *g_scratch = *src_val;
    break;
  case '+':
    *g_scratch = *src_val;
    break;
  case ',':
    *g_scratch = *src_val;
    break;
  case '-':
    *g_scratch = *src_val;
    break;
  case '.':
    *g_slave_addr = *(volatile undefined1 *)src_val;
    break;
  case '/':
    *g_baud_idx = *src_val;
    break;
  case '0':
    *g_uart_frame_sel = *(volatile undefined1 *)src_val;
    break;
  case '1':
    *g_comm_detect = *(volatile undefined1 *)src_val;
    break;
  case '2':
    *DAT_0000b998 = *src_val;
    break;
  case '3':
    *DAT_0000b99c = *src_val;
    break;
  case '4':
    *DAT_0000b9a0 = *src_val;
    break;
  case '5':
    *DAT_0000b9a4 = *src_val;
    break;
  case '6':
    *DAT_0000b9a8 = *src_val;
    break;
  case '7':
    *DAT_0000b9ac = *(volatile undefined1 *)src_val;
    break;
  case '8':
    *DAT_0000b9b0 = *(volatile undefined1 *)src_val;
    break;
  case '9':
    *DAT_0000b9b4 = *(volatile undefined1 *)src_val;
    break;
  case ':':
    *DAT_0000b9b8 = *(volatile undefined1 *)src_val;
    break;
  case ';':
    *g_out_phase = *(volatile undefined1 *)src_val;
    break;
  case '<':
    *g_reg61_remote_en = *src_val;
    break;
  case '=':
    *g_reg62_start_phase = *src_val;
  }
  return 0;
}

/* =============================================================================
 * modbus_dispatch（0x0000B642，函数体 0xB642-0xE573，约 12049B / 5161 指令）
 * —— Modbus RTU 从站帧解析与分发（写寄存器主处理）
 *
 * ★ 说明：该函数为本固件最大函数之一，C 语言反编译结果超过 MCP 5s 传输上限
 *   （连续 6 次超时），因此改为「反汇编精读还原」：完整反汇编已另存为
 *   evidence/reverse/disassembly/08_modbus_dispatch_asm.txt（5161 条指令），此处给出流程还原、
 *   关键数据区与代表性代码段。寄存器读/写值映射见 modbus_read_reg 与
 *   modbus_write_multi（reg 0x00-0x3F → 0x1000B4B8..0x1000B590；
 *   reg 0x2B-0x3D → 0x1000B984..0x1000B9C4）。
 *
 * 调用关系：crc16(0xAF64)×237、uart3_tx_byte(0xAE0C)×118、
 *   param_sync_live_to_eeprom(0x35F2)×51、i2c_write_reg(0x1E88)×2、
 *   out_relay_p020(0x10588)×2、modbus_write_multi(0xB2E0)×1、modbus_read_reg(0xAF94)×1
 *   （×N 为该函数内对子例程的调用次数）
 *
 * 数据区（0x1000B988 起）：
 *   0x1000B988 从站地址           0x1000B9C8 接收状态（0=空闲 / 1=首字节已收帧未完 / 5=完整帧待处理）
 *   0x1000B9CC/0xD0 计数          0x1000B9D4 接收缓冲区（帧[0..N]）
 *   0x1000B9D8 FIO 池指针         0x1000B9DC UART3 寄存器指针
 *   0x1000B9E0 发送缓冲区         0x1000B9E4 接收帧长度
 *   0x1000B9E8 CRC 计算缓存       0x1000B9EC 接收帧 CRC 两字节缓存
 *   0x1000B9F0 写寄存器 16 位值缓存 0x1000B9F4 控制方式模式值
 *   0x1000B9F8..0x1000BE2x 写分支目标参数变量（每组分支一个）
 * 导出：2026-08-21
 * ========================================================================== */

/* ---------------------------------------------------------------------------
 * 一、流程还原（伪代码）
 * ---------------------------------------------------------------------------
 * modbus_dispatch(param) {
 *   1) 帧态门控（2026-08-21 复核）：*0xB9C8==1 → 只清 *0xB9CC 计数后 return 0
 *      （首字节已收、帧未完，不解析）；*0xB9C8!=5 → return 0（非完整帧态一律忽略）
 *      状态机：0=空闲 / 1=首字节已收 / 5=完整帧含 CRC（由 UART3 接收侧设置，处理完清 0）
 *      *0xB9D0 = 0；*0xB9CC = 0;
 *
 *   2) 从站地址匹配：帧[0]（=*0xB9D4[0]）对比 *0xB988（本站地址）
 *        不匹配 → FIO4CLR P4.29 置位；*0xB9C8 = 0；
 *                UART3 寄存器 +4 |= 1（清/屏蔽接收）；return 0;
 *        匹配   → 继续
 *
 *   3) 功能码分发：帧[1]
 *        若 != 0x03 且 != 0x06 且 != 0x10：                 // 非法功能
 *          异常响应 [地址, func|0x80, 0x01, CRC16]
 *        否则 → 4)
 *
 *   4) CRC 校验：crc16(帧, 长度-2) → 0xB9E8；
 *        与帧末两字节（0xB9EC）比对
 *        不匹配 → 异常响应 [地址, func|0x80, 0x04, CRC16]    // 异常码4
 *        匹配   → 5)
 *
 *   5) 功能码 0x06（写单寄存器）：
 *        值 = 帧[4]<<8 | 帧[5]；存 0xB9F0
 *        分支匹配寄存器号 = 帧[2]<<8 | 帧[3]：
 *        · 0x1001（控制方式组）：
 *            值 < 3 → 0xB9F4 = 值；param_sync_live_to_eeprom()；
 *                响应 [地址,0x06,0x10,0x01,val_hi,val_lo,CRC16]（8B）
 *            值 ≥ 3 → 异常响应 [地址,0x86,0x03]
 *        · 0x1002（0xB848 分支）：值 < 0x1771(6001) 且 > 9 →
 *            存 0xB9F8；param_sync()；响应帧同上
 *        · 写分支结构（51 个）：寄存器号高字节固定 0x10、低字节 0x01..0x33 递增；
 *          每分支 = 上下限范围校验 → 值存参数槽（0x1000B9F8 起顺序排列）
 *          → param_sync_live_to_eeprom() → 8 字节响应 [地址,0x06,0x10,reg,val_hi,val_lo,CRC]
 *          已确证分支：0x1001=控制方式（值<3）、0x1002（10..6000）、
 *          值越界 → 异常响应 [地址,0x86,0x03]（异常码 3=非法数据值）
 *          全量寄存器号→参数/范围映射见 docs/uart3_protocol.md 与 docs/PROGRESS_2026-08-20.md §4b
 *
 *   6) 功能码 0x10（写多寄存器，0xE19E 区）：
 *        检查地址 0x1000/数量，0xE2D0 调 modbus_write_multi(0xB2E0)；
 *        按数量循环写并计数，完成发响应 [地址,0x10,0x10,数量,CRC16]
 *
 *   7) 功能码 0x03（读保持寄存器，0xE49E 区）：
 *        按起始寄存器循环调 modbus_read_reg(0xAF94) 读值，
 *        组响应帧 [地址,0x03,字节数,数据...,CRC16]，uart3_tx_byte 发送
 *
 *   8) 帧尾（0xE546-0xE573）：
 *        异常/完成路径 → FIO4CLR P4.29 置位、*0xE5A0 = 0、
 *        UART3 +4 |= 1；return 0;
 * }
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * 二、关键代码段精读（真实反汇编）
 * ---------------------------------------------------------------------------
 * 入口（0xB642）：
 *   push {r4,lr}                      ; r4 = param
 *   ldr r0,[0xB9C8]; ldrb r0,[r0,#0]  ; 接收状态
 *   cmp r0,#1; bne 0xB656
 *   movs r0,#0; ldr r1,[0xB9CC]; str r0,[r1,#0]; pop {r4,pc}   ; 单字节态返回
 *   ldr r0,[0xB9C8]; ldrb r0,[r0,#0]; cmp r0,#5; bne.w 0xE570 ; 非完整帧 → 尾
 *   movs r0,#0; ldr r1,[0xB9D0]; str r0,[r1,#0]                ; 清计数
 *   ldr r1,[0xB9CC]; str r0,[r1,#0]
 *
 * 地址匹配（0xB674）：
 *   ldr r0,[0xB9D4]; ldrb r0,[r0,#0]       ; 帧[0]=从站地址
 *   ldr r1,[0xB988]; ldrb r1,[r1,#0]       ; 本站地址
 *   cmp r0,r1; beq 0xB698                  ; 匹配 → 功能码分发
 *   ldr r0,[0xB9D8]; ldr r0,[r0,#0x3c]; orr r0,r0,#0x20000000
 *   ldr r1,[0xB9D8]; str r0,[r1,#0x3c]     ; FIO4CLR P4.29（错误指示）
 *   movs r0,#0; ldr r1,[0xB9C8]; strb r0,[r1,#0]   ; 清接收状态
 *   ldr r0,[0xB9DC]; ldr r0,[r0,#4]; orr r0,r0,#1; str r0,[r1,#4]  ; UART3 IER
 *
 * 功能码分发（0xB698）：
 *   ldrb r0,[r0,#1]; cmp r0,#3; beq 0xB6E8  ; 0x03 读
 *   cmp r0,#6; beq 0xB6E8                   ; 0x06 写单
 *   cmp r0,#0x10; beq 0xB6E8                ; 0x10 写多
 *   ldrb r0,[r0,#0]                        ; 其他功能码 → 异常响应
 *   ldr r1,[0xB9E0]; strb r0,[r1,#0]        ; TX[0]=地址
 *   ldrb r0,[r0,#1]; adds r0,#0x80; strb r0,[r1,#1]   ; TX[1]=func|0x80
 *   movs r0,#1; strb r0,[r1,#2]             ; TX[2]=异常码 0x01
 *   movs r1,#3; ldr r0,[0xB9E0]; bl 0xAF64  ; crc16(TX,3)
 *
 * CRC 校验（0xB6E8 区）：
 *   ldrb r0,[r0,#0]; subs r0,#2; uxtb r1,r0 ; len-2
 *   ldr r0,[0xB9D4]; bl 0xAF64              ; crc16(帧, len-2)
 *   ldr r1,[0xB9E8]; str r0,[r1,#0]         ; 存计算 CRC
 *   ldrh r0,[r0,#0]; ubfx r1,r0,#8,#8       ; CRC 高字节
 *   ldrb r0,[r0,#0]; subs r0,#1; ldr r2,[0xB9EC]; strb r1,[r2,r0]  ; 比对帧末
 *   ldr r0,[0xB9E8]; ldrb r1,[r0,#0]; ldrb r0,[r0,#0]; subs r0,#2; strb r1,[r2,r0]
 *   ; 两字节比对，不匹配 → beq 跳异常响应 [地址,func|0x80,0x04]
 *
 * 0x06 写单寄存器 - 0x1001 控制方式组（0xB77E）：
 *   ldrb r0,[r0,#1]; cmp r0,#6; bne.w 0xE19E      ; 非 0x06 → 0x10 处理
 *   ldrb r0,[r0,#2]; cmp r0,#0x10; bne 0xB848     ; 非 0x10 高字节 → 下一分支
 *   ldrb r0,[r0,#3]; cmp r0,#1; bne 0xB848        ; 非 0x01 低字节 → 下一分支
 *   ldrb r1,[r0,#5]; ldrb r0,[r0,#4]; add.w r0,r1,r0,lsl#8  ; 值=帧[4..5]
 *   ldr r1,[0xB9F0]; str r0,[r1,#0]               ; 值 → 0xB9F0
 *   cmp r0,#3; bcs 0xB814                         ; 值≥3 → 异常响应 0x86/0x03
 *   ldrb r0,[r0,#0]; ldr r1,[0xB9F4]; strb r0,[r1,#0]   ; 0xB9F4=值
 *   bl 0x35F2                                     ; param_sync_live_to_eeprom()
 *   ; 组装 8 字节响应：[地址,0x06,0x10,0x01,val_hi,val_lo,CRC_lo,CRC_hi]
 *   ldr r0,[0xB988]; ldrb r0,[r0,#0]; ldr r1,[0xB9E0]; strb r0,[r1,#0]
 *   movs r0,#6; strb r0,[r1,#1]; movs r0,#0x10; strb r0,[r1,#2]
 *   movs r0,#1; strb r0,[r1,#3]
 *   ; (0xB9F4 值高低字节拆分) → TX[4],TX[5]
 *   bl 0xAF64 ; crc16(TX,6) → TX[6]；再次 crc16 → TX[7]
 *   movs r0,#8; bl 0xAE0C                     ; uart3_tx_byte 8 字节响应
 *   movs r0,#0; b 0xB654                      ; return 0
 *
 * 通用写分支（0xB848 = 寄存器 0x1002）：
 *   ldrb r0,[r0,#2]; cmp r0,#0x10; bne 0xB900 ; 寄存器高字节匹配
 *   ldrb r0,[r0,#3]; cmp r0,#2; bne 0xB900    ; 低字节
 *   ...值=帧[4..5] → 0xB9F0
 *   cmp r0,#0x1771; bcs 0xB8CC                ; 值≥6001 越界 → 异常
 *   cmp r0,#9; bls 0xB8CC                     ; 值≤9 越界 → 异常
 *   ldr r0,[0xB9F0]; ldr r0,[r0,#0]; ldr r1,[0xB9F8]; str r0,[r1,#0]  ; 存参数
 *   bl 0x35F2                                 ; param_sync() → EEPROM
 *   ; 响应帧同上（值原样回送）
 *
 * 0x10 写多寄存器（0xE19E 起 / 0xE2D0 调用 modbus_write_multi）：
 *   ldrb r0,[r0,#1]; cmp r0,#0x10; bne 0xE29A
 *   ldrb r0,[r0,#2]; cmp r0,#0x10; bne 0xE29C
 *   ldrb r0,[r0,#3]; ldr r1,[0xE38C]; str r0,[r1,#0]    ; 寄存器号
 *   subs r0,#1; ldr r1,[0xE390]; str r0,[r1,#0]         ; 数量-1（循环计数）
 *   ; 校验寄存器号 ∈ [1,0x3E] 否则异常响应 0x90/0x02
 *   ...
 *   0xE2D0: bl 0xB2E0                                    ; modbus_write_multi()
 *   ; 循环写，完成响应 [地址,0x10,0x10,数量,CRC16]
 *
 * 0x03 读保持寄存器（0xE49E 起）：
 *   bl 0xAF94                                    ; modbus_read_reg(参数区, 寄存器号)
 *   ; 读回值高低字节放入 TX[3..N]（0xE584=发送缓冲、0xE594=长度计数、0xE58C=长度）
 *   ; 循环直到读够数量
 *   bl 0xAF64 ; crc16(TX,长度)；两字节 CRC → TX 末尾
 *   movs r0,#长度+2; bl 0xAE0C                   ; 发送
 *   ; 非法范围 → 异常响应 [地址,0x83,0x02,CRC16]
 *
 * 帧尾（0xE546-0xE573）：
 *   bl 0xAE0C                                    ; 异常帧发送
 *   movs r0,#0; b 0xDD5C
 *   0xE54E: ldr r0,[0xE59C]; ldr r0,[r0,#0x3c]; orr r0,r0,#0x20000000  ; FIO4CLR
 *   0xE55A: strb r0,[0xE5A0]; ldr r0,[0xE5A4]; orr r0,[r0,#4],#1      ; UART3 IER
 *   0xE570: movs r0,#0; b 0xDD90                ; 非完整帧直接返回
 * --------------------------------------------------------------------------- */
