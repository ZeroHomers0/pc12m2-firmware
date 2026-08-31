/* =============================================================================
 * PC12M-2 (12 相晶闸管移相触发板) / LPC1765FBD100 — 模块 08：UART3
 * 反编译源码导出（12p，基准 pc12m2_orig.bin SHA 2bc60868…bd271bd1）
 *
 * UART3 硬件：UART3 基址 0x4009C000（U3RBR/U3THR/U3DLL=+0x00、U3IER/U3DLM=+0x04、
 *   U3FCR=+0x08、U3LCR=+0x0C、U3LSR=+0x14）；FIO 池 0x2009C000（+0x20 FIO0DIR、
 *   +0x38 FIO1SET、+0x3C FIO1CLR，bit29=P1.29 RS485 方向）；PCONP=0x400FC0C4
 *   （bit25 UART3 上电）；PINSEL0=0x4002C000（+0/4 P0.0/P0.1 UART3 TXD/RXD）；
 *   NVIC ISER0=0xE000E100，UART3 IRQ=8（=0x100）。
 *
 * 波特率：LCR DLAB 置位（0x80..0x9B）写分频；分频值 = PCLK(0x16E360=1500000) /
 *   (波特率×查表系数/1000)。波特率索引 comm_baud_idx(0x100016F8 字)，系数随索引
 *   取 consts.h BAUD_FAC_0..7；波特率表 comm_baud_table=0x1000179C（字数组）。
 *   帧格式由 comm_div_sel(0x100016FC 字节)：0/1/2/3 → LCR(DLAB 开) 0x87/0x8B/0x9B/0x83，
 *   LCR(DLAB 关) 0x07/0x0B/0x1B/0x03。
 *
 * 协议：Modbus RTU，CRC16 查表（初值 0xFFFF 低位在前）。寄存器读/写映射见
 *   modbus_read_reg(0xAD04)/modbus_write_multi(0xB050) 与 08_modbus_dispatch.c(0xB3B2)。
 *
 * 函数地址（12p）：
 *   uart3_init              0x0000A994
 *   uart3_tx_byte           0x0000AB7C
 *   uart3_rx_timeout_monitor 0x0000ABC0
 *   comm_rx_frame(static)   0x0000AC40
 *   UART3_IRQHandler        0x0000AC78
 *   crc16                   0x0000ACD4
 *   modbus_read_reg         0x0000AD04
 *   modbus_write_multi      0x0000B050
 *   modbus_dispatch         → 08_modbus_dispatch.c（0x0000B3B2，本文件尾注释仅简述）
 * 导出：2026-08-31
 * ========================================================================== */

/* =============================================================================
 * 符号约定（12p verified 变量映射，见 globals.c / _pc12m2_verified_vars.json）：
 *   comm_state=0x10001770   comm_tx_param=0x10001774  comm_tx_data=0x10001773
 *   comm_tx_len=0x10001772   comm_tx_count=0x10001771  comm_frame_buf=0x10002340(TX)
 *   comm_quiet_timer=0x1000176C  comm_scan_timer=0x10001798  comm_rx_flag=0x100016FD
 *   comm_baud_idx=0x100016F8  comm_div_sel=0x100016FC  lookup_table=0x10002278(RX)
 *   out_param=0x1000161C  menu_state=0x100016F7  menu_state2=0x10001765
 *   g_reg_cur_idx=0x10001788  pid_kp2=0x10001706  pid_ki2=0x10001707
 *   g_cfg_pid_sel=0x10001708  comm_baud_table=uint32_t(0x1000179C,值型)
 *   无语义名槽位（0x10001640/0x1000164C/0x1000164D/0x100016B8/0x100016BC/0x100016C0/
 *   0x100016C4/0x100016C8/0x100016CC/0x100016D0/0x100016D4/0x10001650/0x10001598/
 *   0x1000159C/0x100015A0/0x10001728/0x10001690/0x10001698/0x100016A0/0x100016A8/
 *   0x100016B0/0x1000178C(scratch)）用裸 cast，地址为 12p 池槽→SRAM 映射确认值。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/globals.h"
#include "inc/consts.h"
#include <stdbool.h>

/* CRC16 查表（内嵌 const 数组，原 flash 0x111D8/0x112D8 —— 见 crc16_table.c） */
extern const uint8_t crc16_hi_tbl[256];
extern const uint8_t crc16_lo_tbl[256];

/* 跨模块：08_modbus_dispatch.c 引用（本文件不调用，签名以定义处为准） */

/* =============================================================================
 * 0x0000A994 —— UART3 初始化：FIO 方向/RS485 空闲、PCONP 上电、PINSEL 复用、
 *   波特率重算（DLAB 置位写 DLL/DLM）+ 帧格式 + FCR 清 FIFO + NVIC 使能 + IER。
 * 12p 要点（字节级核对）：
 *   · comm_div_sel 字节读、comm_baud_idx 字读；LCR 各分支为独立 if（cbnz/cmp+bne）。
 *   · DLM/DLL = (divisor>>8)/(divisor&0xff)，divisor 经 uxth 截 16 位。
 *   · NVIC_ISER0 = 0x100 为普通 str.w（非 |=）；U3IER |=1 再 |=2（分两条）。
 * ========================================================================== */
void uart3_init(void)
{
  volatile uint32_t *pinsel;
  volatile uint8_t *u3;
  volatile uint8_t *fio;
  uint32_t baud_idx;
  uint32_t baud;
  uint32_t factor;
  uint16_t divisor;
  uint8_t sel;

  fio = (volatile uint8_t *)0x2009c000;                   /* FIO 池 */
  *(volatile uint32_t *)(fio + 0x20) |= 0x20000000;       /* FIO0DIR P0.29 输出 */
  *(volatile uint32_t *)(fio + 0x3c) |= 0x20000000;       /* FIO1CLR P1.29 RS485 空闲 */
  REG32(0x400FC0C4UL) |= 0x2000000;                       /* PCONP UART3 上电 */
  pinsel = (volatile uint32_t *)0x4002c000;               /* PINSEL0 */
  *pinsel |= 2;                                           /* P0.0 UART3 TXD */
  *pinsel |= 8;                                           /* P0.1 UART3 RXD */

  u3 = (volatile uint8_t *)0x4009c000;                    /* UART3 */
  /* DLAB 置位写分频：LCR 按帧格式含 0x80 */
  sel = *(volatile uint8_t *)comm_div_sel;                /* 0x100016FC 字节 */
  if (sel == 0) {
    u3[0xc] = 0x87;
  }
  if (sel == 1) {
    u3[0xc] = 0x8b;
  }
  if (sel == 2) {
    u3[0xc] = 0x9b;
  }
  if (sel == 3) {
    u3[0xc] = 0x83;
  }
  /* 波特率：comm_baud_idx(0x100016F8) 字读；系数随索引选 BAUD_FAC_x；
     baud_tbl = comm_baud_table(0x1000179C) 字数组；
     divisor = PCLK(1500000) / (baud × factor / 1000)，uxth 截 16 位 */
  baud_idx = *comm_baud_idx;
  factor = BAUD_FAC_0;
  if (baud_idx < 3) {
    factor = BAUD_FAC_0;                                  /* 0x3BB */
  }
  if (baud_idx == 3) {
    factor = BAUD_FAC_3;                                  /* 0x3B6 */
  }
  if (baud_idx == 4) {
    factor = BAUD_FAC_4;                                  /* 0x3B1 */
  }
  if (baud_idx == 5) {
    factor = BAUD_FAC_5;                                  /* 0x3AA */
  }
  if (baud_idx == 6) {
    factor = BAUD_FAC_6;                                  /* 0x39D */
  }
  if (baud_idx == 7) {
    factor = BAUD_FAC_7;                                  /* 0x393 */
  }
  baud = *(volatile uint32_t *)(comm_baud_table + baud_idx * 4);
  divisor = (uint16_t)(1500000UL / ((uint32_t)(baud * factor) / 1000));
  u3[4] = (uint8_t)(divisor >> 8);                        /* U3DLM=分频高字节 */
  u3[0] = (uint8_t)divisor;                               /* U3DLL=分频低字节 */
  /* 清 DLAB：8N1 */
  sel = *(volatile uint8_t *)comm_div_sel;
  if (sel == 0) {
    u3[0xc] = 7;
  }
  if (sel == 1) {
    u3[0xc] = 0xb;
  }
  if (sel == 2) {
    u3[0xc] = 0x1b;
  }
  if (sel == 3) {
    u3[0xc] = 3;                                          /* strb comm_div_sel 值 */
  }
  u3[8] = 7;                                              /* U3FCR FIFO 使能+清 */
  NVIC_ISER0 = 0x100;                                     /* 普通写（非 |=） */
  *(volatile uint32_t *)(u3 + 4) |= 1;                    /* U3IER RBR 中断 */
  *(volatile uint32_t *)(u3 + 4) |= 2;                    /* U3IER THRE 中断 */
  return;
}

/* =============================================================================
 * 0x0000AB7C —— UART3 发送一帧（帧长 frame_len）：RS485 置发送方向、
 *   置 comm_state=6、comm_tx_param=0、comm_tx_data=frame_len，
 *   等 U3LSR THRE 后 U3THR = comm_frame_buf[0]（后续字节由 THRE 中断发）。
 * 12p 要点：comm_state/comm_tx_param/comm_tx_data 均字节写（strb）。
 * ========================================================================== */
void uart3_tx_byte(uint8_t frame_len)
{
  volatile uint8_t *u3 = (volatile uint8_t *)0x4009c000;   /* UART3 */
  volatile uint8_t *fio = (volatile uint8_t *)0x2009c000;  /* FIO 池 */
  volatile uint8_t *buf = (volatile uint8_t *)comm_frame_buf;  /* 0x10002340 TX */

  *(volatile uint32_t *)(fio + 0x38) |= 0x20000000;       /* FIO1SET P1.29 RS485 发送 */
  *(volatile uint8_t *)comm_state = 6;                    /* comm_state=6 */
  *(volatile uint8_t *)comm_tx_param = 0;                 /* comm_tx_param=0 */
  *(volatile uint8_t *)comm_tx_data = frame_len;          /* comm_tx_data=帧长 */
  while ((*(volatile uint8_t *)(u3 + 0x14) & 0x20) == 0) { }  /* 等 THRE */
  u3[0] = buf[0];                                         /* U3THR=frame[0] */
  return;
}

/* =============================================================================
 * 0x0000ABC0 —— 接收超时监控（主循环每 tick 调，12p）：
 *   · comm_rx_flag(0x100016FD 有符号字节)>0：comm_quiet_timer(0x1000176C 字)++；
 *     若 >0x7530 → 清零并 out_param(0x1000161C)|=0x8000（帧超时）。
 *   · comm_scan_timer(0x10001798 字)++；若 >0x12C(300) → 清零并重 uart3_init()。
 *   · comm_state==1（发送中）：comm_tx_count(0x10001771 字节)++；
 *     若 >0xA → 清零、comm_state=5、U3IER&=~1（关 RBR 中断，帧就绪）。
 * ========================================================================== */
void uart3_rx_timeout_monitor(void)
{
  volatile uint8_t *u3 = (volatile uint8_t *)0x4009c000;   /* UART3 */
  volatile uint8_t *state = (volatile uint8_t *)comm_state;   /* 0x10001770 */
  volatile uint8_t *tx_cnt = (volatile uint8_t *)comm_tx_count;  /* 0x10001771 */
  volatile uint8_t *rx_flag = (volatile uint8_t *)comm_rx_flag;  /* 0x100016FD */

  if ((int8_t)*rx_flag > 0) {                             /* 接收进行中（有符号） */
    *comm_quiet_timer = *comm_quiet_timer + 1;
    if (*comm_quiet_timer > 0x7530) {                     /* 帧超时 */
      *comm_quiet_timer = 0;
      *out_param = *out_param | 0x8000;
    }
  }
  *comm_scan_timer = *comm_scan_timer + 1;
  if (*comm_scan_timer > 0x12c) {                         /* 300 tick 无活动重初始化 */
    *comm_scan_timer = 0;
    uart3_init();
  }
  if (*state == 1) {                                      /* 发送中 */
    *tx_cnt = *tx_cnt + 1;
    if (*tx_cnt > 0xa) {
      *tx_cnt = 0;
      *state = 5;                                         /* 帧完整待处理 */
      *(volatile uint32_t *)(u3 + 4) &= ~1;               /* U3IER &= ~RBR */
    }
  }
  return;
}

/* =============================================================================
 * 0x0000AC40 —— UART3 RX 组帧子例程（12p；6p func_0x0000aed0 指令等价、地址平移）
 *   读 U3RBR 逐字节存入 RX 缓冲。comm_state(0x10001770) 为帧态：
 *   state==0 → 清 comm_tx_len(0x10001772，作 RX 索引) 并置 state=1；
 *   state==1 → 清 comm_tx_count(0x10001771，作帧间隙计数) 并
 *   rx_buf(0x10002278，verified 命名 lookup_table/查表区，地址复用)[rx_idx++] = U3RBR。
 *   与 6p func_0x0000aed0（state=0x10001790/gap=0x10001791/rx_idx=0x10001792/
 *   rx_buf=0x100022A4）指令等价、地址平移。仅 UART3 ISR(0xac78) 调用。 */
static void comm_rx_frame(void)
{
  volatile uint8_t *state = (volatile uint8_t *)comm_state;     /* 0x10001770 */
  volatile uint8_t *gap   = (volatile uint8_t *)comm_tx_count;  /* 0x10001771 */
  volatile uint8_t *idx   = (volatile uint8_t *)comm_tx_len;    /* 0x10001772 */
  volatile uint8_t *buf   = (volatile uint8_t *)lookup_table;   /* 0x10002278 */
  volatile uint8_t *uart3 = (volatile uint8_t *)0x4009c000;     /* UART3 */

  if (*state == 0) {
    *idx = 0;
    *state = 1;
  }
  if (*state == 1) {
    *gap = 0;
    buf[*idx] = uart3[0];          /* U3RBR */
    *idx = *idx + 1;
  }
  return;
}

/* =============================================================================
 * 0x0000AC78 —— UART3 中断：IIR 判因（12p；6p 0x0000AF08）
 *   U3IIR(+8)&0xe：因=4（RX 数据可用）→ comm_rx_frame() 组帧；
 *   因=2（THRE）→ comm_tx_param(0x10001774)++：未发完则 U3THR =
 *   comm_frame_buf(0x10002340)[param]；发完则 FIO1CLR P1.29 置位（RS485 释放）、
 *   comm_state(0x10001770)=0、U3IER(+4)|=1（RBR 中断恢复）。
 * 局部：param/len/buf/state = SRAM 变量字节别名；uart3 = UART3；fio = FIO 池。 */
void UART3_IRQHandler(void)
{
  volatile uint8_t *param = (volatile uint8_t *)comm_tx_param;   /* 0x10001774 发送索引 */
  volatile uint8_t *len   = (volatile uint8_t *)comm_tx_data;    /* 0x10001773 发送长度 */
  volatile uint8_t *buf   = (volatile uint8_t *)comm_frame_buf;  /* 0x10002340 发送缓冲 */
  volatile uint8_t *state = (volatile uint8_t *)comm_state;      /* 0x10001770 通讯状态 */
  volatile uint8_t *uart3 = (volatile uint8_t *)0x4009c000;      /* UART3 */
  volatile uint        fio = 0x2009c000;                         /* FIO 池 */
  uint iir_cause;

  iir_cause = *(volatile uint *)(uart3 + 8) & 0xe;               /* U3IIR 中断原因 */
  if (iir_cause == 4) {
    comm_rx_frame();                                             /* bl 0xac40 */
  }
  if (iir_cause == 2) {                                          /* THRE */
    *param = *param + 1;                                         /* comm_tx_param++ */
    if (*param < *len) {                                         /* 未发完 */
      *uart3 = buf[*param];                                      /* U3THR = frame[param] */
    }
    else {                                                       /* 发完 */
      *(volatile uint *)(fio + 0x3c) = *(volatile uint *)(fio + 0x3c) | 0x20000000;  /* FIO1CLR P1.29 */
      *state = 0;                                                /* comm_state = 0 */
      *(volatile uint *)(uart3 + 4) = *(volatile uint *)(uart3 + 4) | 1;            /* U3IER |= 1 */
    }
  }
  return;
}

/* =============================================================================
 * 0x0000ACD4 —— Modbus CRC16（查表，初值 0xFFFF，低位在前）
 *   状态：crc_hi=高字节、crc_lo=低字节，tbl_idx = 数据字节 ^ crc_lo；
 *   查表内嵌 crc16_hi_tbl/crc16_lo_tbl（原 flash 0x111D8/0x112D8，12p 池槽 0xADCC/0xADD0）。
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

/* =============================================================================
 * 0x0000AD04 —— Modbus 读保持寄存器（out_val=值槽、reg_addr=寄存器号&0xFFF）
 *   PID 前导：g_cfg_pid_sel(0x10001708 字节)==1..4 时，把所选参数银行 KP/KI
 *   （银行1 0x1709/0x170A、银行2 0x170B/0x170C、银行3 0x170D/0x170E、
 *   银行4 0x170F/0x1710）字节复制到活动槽 pid_kp2(0x1706)/pid_ki2(0x1707)。
 *   守卫：(uint8_t)*g_reg_cur_idx >= 0x3F → return 0（不写 out_val）。
 *   TBB 分派 idx0-62（63 项）。宽度要点（12p）：
 *   · B 目标 ldrb 零扩展 → `(uint32_t)*(volatile uint8_t *)PTR`（≠6p 字读！）；
 *   · W 目标 ldr → `*PTR`；
 *   · idx 0x1A-0x1F/0x24-0x25 为 `movs r3,#0; str r3,[out_val]`（写 0，非不写！）。
 *   返回恒 0。REG 表见下方注释（63 项，idx→(SRAM 地址, 宽度)）。
 * ========================================================================== */
uint32_t modbus_read_reg(uint32_t *out_val, uint32_t reg_addr)
{
  uint8_t sel;
  uint8_t idx;

  *g_reg_cur_idx = reg_addr & 0xfff;
  /* PID 前导：把所选参数银行 KP/KI 复制到活动槽（字节复制，sel 字节读） */
  sel = *(volatile uint8_t *)g_cfg_pid_sel;                 /* 0x10001708 */
  if (sel == 1) {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)0x10001709;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)0x1000170a;
  }
  if (sel == 2) {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)0x1000170b;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)0x1000170c;
  }
  if (sel == 3) {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)0x1000170d;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)0x1000170e;
  }
  if (sel == 4) {
    *(volatile uint8_t *)pid_kp2 = *(volatile uint8_t *)0x1000170f;
    *(volatile uint8_t *)pid_ki2 = *(volatile uint8_t *)0x10001710;
  }
  idx = (uint8_t)*g_reg_cur_idx;
  if (idx >= 0x3f) {
    return 0;                                               /* 守卫：idx>=0x3F 不写 out_val */
  }
  switch (idx) {
  case 0x00: *out_val = (uint32_t)*(volatile uint8_t *)gain_sel; break;        /* 0x162C B */
  case 0x01: *out_val = *gain_b; break;                                        /* 0x1634 W */
  case 0x02: *out_val = *gain_a; break;                                        /* 0x1630 W */
  case 0x03: *out_val = *gain_coef; break;                                     /* 0x1638 W */
  case 0x04: *out_val = *(volatile uint32_t *)0x10001640; break;               /* 0x1640 W */
  case 0x05: *out_val = *gain_c; break;                                        /* 0x163C W */
  case 0x06: *out_val = (uint32_t)*(volatile uint8_t *)startup_div; break;     /* 0x1644 B */
  case 0x07: *out_val = (uint32_t)*(volatile uint8_t *)stop_div; break;        /* 0x1645 B */
  case 0x08: *out_val = *llim_angle; break;                                    /* 0x1648 W */
  case 0x09: *out_val = (uint32_t)*(volatile uint8_t *)0x1000164c; break;      /* B */
  case 0x0a: *out_val = (uint32_t)*(volatile uint8_t *)0x1000164d; break;      /* B */
  case 0x0b: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_1; break;  /* 0x164E B */
  case 0x0c: *out_val = *(volatile uint32_t *)0x100016b8; break;               /* W */
  case 0x0d: *out_val = (uint32_t)*(volatile uint8_t *)0x100016bc; break;      /* B */
  case 0x0e: *out_val = *(volatile uint32_t *)0x100016c0; break;               /* W */
  case 0x0f: *out_val = (uint32_t)*(volatile uint8_t *)0x100016c4; break;      /* B */
  case 0x10: *out_val = *(volatile uint32_t *)0x100016c8; break;               /* W */
  case 0x11: *out_val = (uint32_t)*(volatile uint8_t *)0x100016cc; break;      /* B */
  case 0x12: *out_val = *(volatile uint32_t *)0x100016d0; break;               /* W */
  case 0x13: *out_val = (uint32_t)*(volatile uint8_t *)0x100016d4; break;      /* B */
  case 0x14: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_12; break; /* 0x16D5 B */
  case 0x15: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_13; break; /* 0x16D6 B */
  case 0x16: *out_val = (uint32_t)*(volatile uint8_t *)g_cfg_pid_sel; break;   /* 0x1708 B */
  case 0x17: *out_val = (uint32_t)*(volatile uint8_t *)pid_kp2; break;         /* 0x1706 B */
  case 0x18: *out_val = (uint32_t)*(volatile uint8_t *)pid_ki2; break;         /* 0x1707 B */
  case 0x19: *out_val = (uint32_t)*(volatile uint8_t *)trig_phase; break;      /* 0x168C B */
  case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
    *out_val = 0; break;                                    /* movs r3,#0; str [out_val] */
  case 0x20: *out_val = *(volatile uint32_t *)0x100015fc; break;               /* W */
  case 0x21: *out_val = *(volatile uint32_t *)0x100015f8; break;               /* W */
  case 0x22: *out_val = *(volatile uint32_t *)0x10001608; break;               /* W */
  case 0x23: *out_val = *(volatile uint32_t *)0x10001604; break;               /* W */
  case 0x24: case 0x25:
    *out_val = 0; break;                                    /* movs r3,#0; str [out_val] */
  case 0x26: *out_val = (uint32_t)*(volatile uint8_t *)menu_state2; break;     /* 0x1765 B */
  case 0x27: *out_val = *adc_conv_ch3; break;                                 /* 0x15A8 W */
  case 0x28: *out_val = *(volatile uint32_t *)0x10001598; break;               /* W */
  case 0x29: *out_val = *(volatile uint32_t *)0x1000159c; break;               /* W */
  case 0x2a: *out_val = *(volatile uint32_t *)0x100015a0; break;               /* W */
  case 0x2b: *out_val = *adc_conv_ch4; break;                                 /* 0x1594 W */
  case 0x2c: *out_val = *adc_conv_ch5; break;                                 /* 0x1590 W */
  case 0x2d: *out_val = *(volatile uint32_t *)0x10001728; break;               /* W */
  case 0x2e: *out_val = (uint32_t)*(volatile uint8_t *)menu_state; break;      /* 0x16F7 B */
  case 0x2f: *out_val = *comm_baud_idx; break;                                /* 0x16F8 W */
  case 0x30: *out_val = (uint32_t)*(volatile uint8_t *)comm_div_sel; break;    /* 0x16FC B */
  case 0x31: *out_val = (uint32_t)*(volatile uint8_t *)comm_rx_flag; break;    /* 0x16FD B */
  case 0x32: *out_val = *(volatile uint32_t *)0x10001690; break;               /* W */
  case 0x33: *out_val = *(volatile uint32_t *)0x10001698; break;               /* W */
  case 0x34: *out_val = *(volatile uint32_t *)0x100016a0; break;               /* W */
  case 0x35: *out_val = *(volatile uint32_t *)0x100016a8; break;               /* W */
  case 0x36: *out_val = *(volatile uint32_t *)0x100016b0; break;               /* W */
  case 0x37: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_2; break;  /* 0x164F B */
  case 0x38: *out_val = (uint32_t)*(volatile uint8_t *)0x10001650; break;      /* B */
  case 0x39: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_3; break;  /* 0x1651 B */
  case 0x3a: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_adc_cfg; break;  /* 0x1652 B */
  case 0x3b: *out_val = (uint32_t)*(volatile uint8_t *)eeprom_param_4; break;  /* 0x1653 B */
  case 0x3c: *out_val = *out_fine; break;                                     /* 0x1654 W */
  case 0x3d: *out_val = *ulim_angle; break;                                   /* 0x1658 W */
  case 0x3e: *out_val = *out_div; break;                                      /* 0x2034 W */
  default: break;                                                             /* idx>=0x3F 已守卫 */
  }
  return 0;
}

/* =============================================================================
 * 0x0000B050 —— Modbus 写保持寄存器（src_val=值指针、reg_idx=寄存器号&0xFFF）
 *   无 PID 前导。守卫：(uint8_t)*g_reg_cur_idx >= 0x3E → return 0。
 *   TBB 分派 idx0-61（62 项）。宽度要点（12p）：
 *   · B 目标 ldrb value; strb → `*(volatile uint8_t *)PTR = (uint8_t)*src_val`；
 *   · W 目标 ldr value; str → `*PTR = *src_val`；
 *   · idx 0x1A-0x1F/0x24-0x25/0x28-0x2D（写）→ 0x1000178C scratch（字写）；
 *   · idx 0x17/0x18 → 0x170F/0x1710（PID 银行4 KP/KI，≠READ 的活动槽）；
 *   · idx 0x27 → 0x15A8（adc_conv_ch3，与 READ 相同）。
 *   返回恒 0。
 * ========================================================================== */
uint32_t modbus_write_multi(uint32_t *src_val, uint32_t reg_idx)
{
  volatile uint32_t *reg_ofs;

  reg_ofs = g_reg_cur_idx;                                  /* 0x10001788 */
  *g_reg_cur_idx = reg_idx & 0xfff;
  if ((uint8_t)*reg_ofs >= 0x3e) {
    return 0;                                               /* 守卫：idx>=0x3E 不写 */
  }
  switch ((uint8_t)*reg_ofs) {
  case 0x00: *(volatile uint8_t *)gain_sel = (uint8_t)*src_val; break;          /* 0x162C B */
  case 0x01: *gain_b = *src_val; break;                                         /* 0x1634 W */
  case 0x02: *gain_a = *src_val; break;                                         /* 0x1630 W */
  case 0x03: *gain_coef = *src_val; break;                                      /* 0x1638 W */
  case 0x04: *(volatile uint32_t *)0x10001640 = *src_val; break;                /* W */
  case 0x05: *gain_c = *src_val; break;                                         /* 0x163C W */
  case 0x06: *(volatile uint8_t *)startup_div = (uint8_t)*src_val; break;       /* 0x1644 B */
  case 0x07: *(volatile uint8_t *)stop_div = (uint8_t)*src_val; break;          /* 0x1645 B */
  case 0x08: *llim_angle = *src_val; break;                                     /* 0x1648 W */
  case 0x09: *(volatile uint8_t *)0x1000164c = (uint8_t)*src_val; break;        /* B */
  case 0x0a: *(volatile uint8_t *)0x1000164d = (uint8_t)*src_val; break;        /* B */
  case 0x0b: *(volatile uint8_t *)eeprom_param_1 = (uint8_t)*src_val; break;    /* 0x164E B */
  case 0x0c: *(volatile uint32_t *)0x100016b8 = *src_val; break;                /* W */
  case 0x0d: *(volatile uint8_t *)0x100016bc = (uint8_t)*src_val; break;        /* B */
  case 0x0e: *(volatile uint32_t *)0x100016c0 = *src_val; break;                /* W */
  case 0x0f: *(volatile uint8_t *)0x100016c4 = (uint8_t)*src_val; break;        /* B */
  case 0x10: *(volatile uint32_t *)0x100016c8 = *src_val; break;                /* W */
  case 0x11: *(volatile uint8_t *)0x100016cc = (uint8_t)*src_val; break;        /* B */
  case 0x12: *(volatile uint32_t *)0x100016d0 = *src_val; break;                /* W */
  case 0x13: *(volatile uint8_t *)0x100016d4 = (uint8_t)*src_val; break;        /* B */
  case 0x14: *(volatile uint8_t *)eeprom_param_12 = (uint8_t)*src_val; break;   /* 0x16D5 B */
  case 0x15: *(volatile uint8_t *)eeprom_param_13 = (uint8_t)*src_val; break;   /* 0x16D6 B */
  case 0x16: *(volatile uint8_t *)g_cfg_pid_sel = (uint8_t)*src_val; break;     /* 0x1708 B */
  case 0x17: *(volatile uint8_t *)0x1000170f = (uint8_t)*src_val; break;        /* 银行4 KP B */
  case 0x18: *(volatile uint8_t *)0x10001710 = (uint8_t)*src_val; break;        /* 银行4 KI B */
  case 0x19: *(volatile uint8_t *)trig_phase = (uint8_t)*src_val; break;        /* 0x168C B */
  case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
  case 0x24: case 0x25:
  case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d:
    *(volatile uint32_t *)0x1000178c = *src_val; break;     /* scratch 字写 */
  case 0x20: *(volatile uint32_t *)0x100015fc = *src_val; break;                /* W */
  case 0x21: *(volatile uint32_t *)0x100015f8 = *src_val; break;                /* W */
  case 0x22: *(volatile uint32_t *)0x10001608 = *src_val; break;                /* W */
  case 0x23: *(volatile uint32_t *)0x10001604 = *src_val; break;                /* W */
  case 0x26: *(volatile uint8_t *)menu_state2 = (uint8_t)*src_val; break;       /* 0x1765 B */
  case 0x27: *adc_conv_ch3 = *src_val; break;                                  /* 0x15A8 W */
  case 0x2e: *(volatile uint8_t *)menu_state = (uint8_t)*src_val; break;        /* 0x16F7 B */
  case 0x2f: *comm_baud_idx = *src_val; break;                                 /* 0x16F8 W */
  case 0x30: *(volatile uint8_t *)comm_div_sel = (uint8_t)*src_val; break;      /* 0x16FC B */
  case 0x31: *(volatile uint8_t *)comm_rx_flag = (uint8_t)*src_val; break;      /* 0x16FD B */
  case 0x32: *(volatile uint32_t *)0x10001690 = *src_val; break;                /* W */
  case 0x33: *(volatile uint32_t *)0x10001698 = *src_val; break;                /* W */
  case 0x34: *(volatile uint32_t *)0x100016a0 = *src_val; break;                /* W */
  case 0x35: *(volatile uint32_t *)0x100016a8 = *src_val; break;                /* W */
  case 0x36: *(volatile uint32_t *)0x100016b0 = *src_val; break;                /* W */
  case 0x37: *(volatile uint8_t *)eeprom_param_2 = (uint8_t)*src_val; break;    /* 0x164F B */
  case 0x38: *(volatile uint8_t *)0x10001650 = (uint8_t)*src_val; break;        /* B */
  case 0x39: *(volatile uint8_t *)eeprom_param_3 = (uint8_t)*src_val; break;    /* 0x1651 B */
  case 0x3a: *(volatile uint8_t *)eeprom_adc_cfg = (uint8_t)*src_val; break;    /* 0x1652 B */
  case 0x3b: *(volatile uint8_t *)eeprom_param_4 = (uint8_t)*src_val; break;    /* 0x1653 B */
  case 0x3c: *out_fine = *src_val; break;                                      /* 0x1654 W */
  case 0x3d: *ulim_angle = *src_val; break;                                    /* 0x1658 W */
  default: break;                                                              /* idx>=0x3E 已守卫 */
  }
  return 0;
}

/* =============================================================================
 * modbus_dispatch(0x0000B3B2) —— Modbus RTU 从站帧解析与分发主处理
 * ★ 完整 C 实现见 src/08_modbus_dispatch.c（12p 重写）。本文件只承载
 *   uart3_init/uart3_tx_byte/uart3_rx_timeout_monitor/crc16/modbus_read_reg/
 *   modbus_write_multi 六函数（6p 时代的 dispatch 伪代码注释已随 12p 重写移除，
 *   权威实现与逐字节核对的流程说明在 08_modbus_dispatch.c 头部）。
 * ========================================================================== */
