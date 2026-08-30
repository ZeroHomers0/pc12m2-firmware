/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 08：UART3（Modbus RTU 从站）+ CRC16
 *
 * UART3 硬件：DAT_0000B020 = UART3 基址 0x4009C000（U3RBR/U3THR=+0x00、
 *   U3IER=+0x04、U3IIR/FCR=+0x08、U3LCR=+0x0C、U3LSR=+0x14、U3DLL=+0x00、
 *   U3DLM=+0x04）；DAT_0000B00C = SCB(0x400F4000)（UART3 时钟/引脚相关，
 *   实际为 FIO2 或 SCB；经 PCLKSEL/PCONP 置位）
 * 波特率：U3LCR=0x80.. 0x9B（DLAB 置位写分频），分频值 = PCLK /
 *   (波特率×查表系数/1000)，系数随 PCLK 表 0x1000B028（0x3BB/0x3B6/...）。
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

/* 0x0000AC24 —— UART3 初始化：波特率重算 + 8N1 + 使能 TX/RX
 *   0x1000B01C=数据位(0/1/2/3→LCR 0x87/8B/9B/83 含 DLAB)；
 *   0x1000B024=波特率索引(0..7)→查表 0x1000B028 得系数，分频=PCLK/(波特率×系数/1000) */
void uart3_init(uint param_1)
{
  int iVar1;
  uint *puVar2;
  undefined1 *puVar3;

  iVar1 = DAT_0000b00c;
  *(uint *)(DAT_0000b00c + 0x20) = *(uint *)(DAT_0000b00c + 0x20) | 0x20000000;
  *(uint *)(iVar1 + 0x3c) = *(uint *)(iVar1 + 0x3c) | 0x20000000;
  *(uint *)(DAT_0000b014 + 0xc4) = *DAT_0000b010 | 0x2000000;   /* PCONP UART3 上电 */
  puVar2 = DAT_0000b018;
  *DAT_0000b018 = *DAT_0000b018 | 2;                            /* PCLKSEL UART3 */
  *puVar2 = *puVar2 | 8;
  if (*DAT_0000b01c == '\0') {
    DAT_0000b020[0xc] = 0x87;                                   /* LCR：8 位+DLAB */
  }
  if (*DAT_0000b01c == '\x01') {
    DAT_0000b020[0xc] = 0x8b;
  }
  if (*DAT_0000b01c == '\x02') {
    DAT_0000b020[0xc] = 0x9b;
  }
  if (*DAT_0000b01c == '\x03') {
    DAT_0000b020[0xc] = 0x83;
  }
  puVar3 = DAT_0000b020;
  if (*DAT_0000b024 < 3) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x3bb) /
                            1000));
  }
  if (*DAT_0000b024 == 3) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x3b6) /
                            1000));
  }
  if (*DAT_0000b024 == 4) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x3b1) /
                            1000));
  }
  if (*DAT_0000b024 == 5) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x3aa) /
                            1000));
  }
  if (*DAT_0000b024 == 6) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x39d) /
                            1000));
  }
  if (*DAT_0000b024 == 7) {
    param_1 = (uint)(ushort)((ulonglong)DAT_0000b02c /
                            ((ulonglong)(uint)(*(int *)(DAT_0000b028 + *DAT_0000b024 * 4) * 0x393) /
                            1000));
  }
  DAT_0000b020[4] = (char)(param_1 + ((uint)((int)param_1 >> 0x1f) >> 0x18) >> 8);  /* DLM=高字节 */
  *puVar3 = (char)param_1;                                        /* DLL=低字节 */
  if (*DAT_0000b01c == '\0') {
    puVar3[0xc] = 7;                                             /* LCR：8N1，清 DLAB */
  }
  if (*DAT_0000b01c == '\x01') {
    DAT_0000b020[0xc] = 0xb;
  }
  if (*DAT_0000b01c == '\x02') {
    DAT_0000b020[0xc] = 0x1b;
  }
  if (*DAT_0000b01c == '\x03') {
    DAT_0000b020[0xc] = 3;
  }
  DAT_0000b020[8] = 7;                                           /* FCR：使能 FIFO+清 */
  puVar3 = DAT_0000b020;
  uRame000e100 = 0x100;
  *(uint *)(DAT_0000b020 + 4) = *(uint *)(DAT_0000b020 + 4) | 1;  /* IER RBR 中断 */
  *(uint *)(puVar3 + 4) = *(uint *)(puVar3 + 4) | 2;             /* IER THRE 中断 */
  return;
}

/* 0x0000AE0C —— UART3 发送 1 字节（U3THR 写 + 等 THRE 位）
 *   0x1000B030=发送状态、0x1000B038=发送长度、0x1000B03C=发送缓冲 */
void uart3_tx_byte(undefined1 param_1)
{
  *(uint *)(DAT_0000b00c + 0x38) = *(uint *)(DAT_0000b00c + 0x38) | 0x20000000;
  *DAT_0000b030 = 6;
  *DAT_0000b034 = 0;
  *DAT_0000b038 = param_1;
  do {
  } while ((DAT_0000b020[0x14] & 0x20) == 0);                    /* 等 THRE */
  *DAT_0000b020 = *DAT_0000b03c;
  return;
}

/* 0x0000AE50 —— 接收超时监控（主循环每 tick 调）：
 *   0x1000B040=接收进行标志、0x1000B044=接收空闲计数（>0x7530 置 0x1000B048|0x8000 帧超时）、
 *   0x1000B04C=全局 tick 计数（>300 重新 uart3_init 防锁死）、
 *   0x1000B030=1 发送中：计数>10 → 状态=5、关 THRE 中断 */
void uart3_rx_timeout_monitor(void)
{
  int *piVar1;
  uint *puVar2;
  byte *pbVar3;

  piVar1 = DAT_0000b044;
  if ((*DAT_0000b040 != '\0') &&
     (*DAT_0000b044 = *DAT_0000b044 + 1, &DAT_00007530 < (undefined4 *)*piVar1)) {
    *DAT_0000b044 = 0;
    *DAT_0000b048 = *DAT_0000b048 | 0x8000;
  }
  puVar2 = DAT_0000b04c;
  *DAT_0000b04c = *DAT_0000b04c + 1;
  if (300 < *puVar2) {
    *puVar2 = 0;
    uart3_init();
  }
  pbVar3 = DAT_0000b050;
  if (*DAT_0000b030 == '\x01') {
    *DAT_0000b050 = *DAT_0000b050 + 1;
    if (10 < *pbVar3) {
      *pbVar3 = 0;
      *DAT_0000b030 = '\x05';
      *(uint *)(DAT_0000b020 + 4) = *(uint *)(DAT_0000b020 + 4) & 0xfffffffe;
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
  byte *pbVar1;
  uint uVar2;
  uint extraout_r3;

  uVar2 = *(uint *)(DAT_0000b020 + 8) & 0xe;                     /* IIR 中断原因 */
  if (uVar2 == 4) {
    func_0x0000aed0();                                            /* RX 组帧子例程 */
    uVar2 = extraout_r3;
  }
  pbVar1 = DAT_0000b034;
  if (uVar2 == 2) {
    *DAT_0000b034 = *DAT_0000b034 + 1;                           /* 发送索引++ */
    if (*pbVar1 < *DAT_0000b038) {                               /* 未发完 */
      *DAT_0000b020 = *(undefined1 *)(DAT_0000b03c + (uint)*DAT_0000b034);
    }
    else {                                                        /* 发完 */
      *(uint *)(DAT_0000b00c + 0x3c) = *(uint *)(DAT_0000b00c + 0x3c) | 0x20000000;
      *DAT_0000b030 = 0;
      *(uint *)(DAT_0000b020 + 4) = *(uint *)(DAT_0000b020 + 4) | 1;
    }
  }
  return;
}

/* 0x0000AF64 —— Modbus CRC16（查表，初值 0xFFFF）
 *   DAT_0000B05C/0x1000B060 = CRC 高/低字节查表基址 */
uint crc16(byte *param_1,uint param_2)
{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  bool bVar4;

  uVar3 = 0xff;
  uVar2 = 0xff;
  while (bVar4 = param_2 != 0, param_2 = param_2 - 1 & 0xff, bVar4) {
    uVar1 = *param_1 ^ uVar2;
    uVar2 = *(byte *)(DAT_0000b05c + uVar1) ^ uVar3;
    uVar3 = (uint)*(byte *)(DAT_0000b060 + uVar1);
    param_1 = param_1 + 1;
  }
  return uVar2 | uVar3 << 8;
}

/* 0x0000AF94 —— Modbus 读保持寄存器（param_2=寄存器号 & 0xFFF）
 *   0x1000B064=寄存器号、0x1000B068=控制方式组选择(1..4)，
 *   组基址表：组1→0x1000B06C/0x1000B074、组2→0x1000B07C/0x1000B080、
 *   组3→0x1000B084/0x1000B088、组4→0x1000B08C/0x1000B090；
 *   reg 0x00-0x3F 映射 0x1000B4B8..0x1000B590（0x1A-0x1F/0x24-0x25 等保留返回 0） */
undefined4 modbus_read_reg(uint *param_1,uint param_2)
{
  *DAT_0000b064 = param_2 & 0xfff;
  if (*DAT_0000b068 == '\x01') {
    *DAT_0000b070 = *DAT_0000b06c;
    *DAT_0000b078 = *DAT_0000b074;
  }
  if (*DAT_0000b068 == '\x02') {
    *DAT_0000b070 = *DAT_0000b07c;
    *DAT_0000b078 = *DAT_0000b080;
  }
  if (*DAT_0000b068 == '\x03') {
    *DAT_0000b070 = *DAT_0000b084;
    *DAT_0000b078 = *DAT_0000b088;
  }
  if (*DAT_0000b068 == '\x04') {
    *DAT_0000b070 = *DAT_0000b08c;
    *DAT_0000b078 = *DAT_0000b090;
  }
  switch((char)*DAT_0000b064) {
  case '\0':
    *param_1 = (uint)*DAT_0000b4b8;
    break;
  case '\x01':
    *param_1 = *DAT_0000b4bc;
    break;
  case '\x02':
    *param_1 = *DAT_0000b4c0;
    break;
  case '\x03':
    *param_1 = *DAT_0000b4c4;
    break;
  case '\x04':
    *param_1 = *DAT_0000b4c8;
    break;
  case '\x05':
    *param_1 = *DAT_0000b4cc;
    break;
  case '\x06':
    *param_1 = (uint)*DAT_0000b4d0;
    break;
  case '\a':
    *param_1 = (uint)*DAT_0000b4d4;
    break;
  case '\b':
    *param_1 = *DAT_0000b4d8;
    break;
  case '\t':
    *param_1 = (uint)*DAT_0000b4dc;
    break;
  case '\n':
    *param_1 = (uint)*DAT_0000b4e0;
    break;
  case '\v':
    *param_1 = (uint)*DAT_0000b4e4;
    break;
  case '\f':
    *param_1 = *DAT_0000b4e8;
    break;
  case '\r':
    *param_1 = (uint)*DAT_0000b4ec;
    break;
  case '\x0e':
    *param_1 = *DAT_0000b4f0;
    break;
  case '\x0f':
    *param_1 = (uint)*DAT_0000b4f4;
    break;
  case '\x10':
    *param_1 = *DAT_0000b4f8;
    break;
  case '\x11':
    *param_1 = (uint)*DAT_0000b4fc;
    break;
  case '\x12':
    *param_1 = *DAT_0000b500;
    break;
  case '\x13':
    *param_1 = (uint)*DAT_0000b504;
    break;
  case '\x14':
    *param_1 = (uint)*DAT_0000b508;
    break;
  case '\x15':
    *param_1 = (uint)*DAT_0000b50c;
    break;
  case '\x16':
    *param_1 = (uint)*DAT_0000b510;
    break;
  case '\x17':
    *param_1 = (uint)*DAT_0000b514;
    break;
  case '\x18':
    *param_1 = (uint)*DAT_0000b518;
    break;
  case '\x19':
    *param_1 = (uint)*DAT_0000b51c;
    break;
  case '\x1a':
    *param_1 = 0;
    break;
  case '\x1b':
    *param_1 = 0;
    break;
  case '\x1c':
    *param_1 = 0;
    break;
  case '\x1d':
    *param_1 = 0;
    break;
  case '\x1e':
    *param_1 = 0;
    break;
  case '\x1f':
    *param_1 = 0;
    break;
  case ' ':
    *param_1 = *DAT_0000b520;
    break;
  case '!':
    *param_1 = *DAT_0000b524;
    break;
  case '\"':
    *param_1 = *DAT_0000b528;
    break;
  case '#':
    *param_1 = *DAT_0000b52c;
    break;
  case '$':
    *param_1 = 0;
    break;
  case '%':
    *param_1 = 0;
    break;
  case '&':
    *param_1 = (uint)*DAT_0000b530;
    break;
  case '\'':
    *param_1 = *DAT_0000b534;
    break;
  case '(':
    *param_1 = *DAT_0000b538;
    break;
  case ')':
    *param_1 = *DAT_0000b53c;
    break;
  case '*':
    *param_1 = *DAT_0000b540;
    break;
  case '+':
    *param_1 = *DAT_0000b544;
    break;
  case ',':
    *param_1 = *DAT_0000b548;
    break;
  case '-':
    *param_1 = *DAT_0000b54c;
    break;
  case '.':
    *param_1 = (uint)*DAT_0000b550;
    break;
  case '/':
    *param_1 = *DAT_0000b554;
    break;
  case '0':
    *param_1 = (uint)*DAT_0000b558;
    break;
  case '1':
    *param_1 = (uint)*DAT_0000b55c;
    break;
  case '2':
    *param_1 = *DAT_0000b560;
    break;
  case '3':
    *param_1 = *DAT_0000b564;
    break;
  case '4':
    *param_1 = *DAT_0000b568;
    break;
  case '5':
    *param_1 = *DAT_0000b56c;
    break;
  case '6':
    *param_1 = *DAT_0000b570;
    break;
  case '7':
    *param_1 = (uint)*DAT_0000b574;
    break;
  case '8':
    *param_1 = (uint)*DAT_0000b578;
    break;
  case '9':
    *param_1 = (uint)*DAT_0000b57c;
    break;
  case ':':
    *param_1 = (uint)*DAT_0000b580;
    break;
  case ';':
    *param_1 = (uint)*DAT_0000b584;
    break;
  case '<':
    *param_1 = *DAT_0000b588;
    break;
  case '=':
    *param_1 = *DAT_0000b58c;
    break;
  case '>':
    *param_1 = *DAT_0000b590;
  }
  return 0;
}

/* 0x0000B2E0 —— Modbus 写保持寄存器（param_1=数据指针、param_2=寄存器号）
 *   0x1000B594=寄存器号；0x1A-0x1F/0x24-0x25/0x2B-0x2F/0x40-0x42 等映射到
 *   0x1000B5A0（保留/汇总区），0x2B-0x3D 段写 0x1000B984..0x1000B9C4 */
undefined4 modbus_write_multi(undefined4 *param_1,uint param_2)
{
  uint *puVar1;

  puVar1 = DAT_0000b594;
  *DAT_0000b594 = param_2 & 0xfff;
  switch((char)*puVar1) {
  case '\0':
    *DAT_0000b4b8 = *(undefined1 *)param_1;
    break;
  case '\x01':
    *DAT_0000b4bc = *param_1;
    break;
  case '\x02':
    *DAT_0000b4c0 = *param_1;
    break;
  case '\x03':
    *DAT_0000b4c4 = *param_1;
    break;
  case '\x04':
    *DAT_0000b4c8 = *param_1;
    break;
  case '\x05':
    *DAT_0000b4cc = *param_1;
    break;
  case '\x06':
    *DAT_0000b4d0 = *(undefined1 *)param_1;
    break;
  case '\a':
    *DAT_0000b4d4 = *(undefined1 *)param_1;
    break;
  case '\b':
    *DAT_0000b4d8 = *param_1;
    break;
  case '\t':
    *DAT_0000b4dc = *(undefined1 *)param_1;
    break;
  case '\n':
    *DAT_0000b4e0 = *(undefined1 *)param_1;
    break;
  case '\v':
    *DAT_0000b4e4 = *(undefined1 *)param_1;
    break;
  case '\f':
    *DAT_0000b4e8 = *param_1;
    break;
  case '\r':
    *DAT_0000b4ec = *(undefined1 *)param_1;
    break;
  case '\x0e':
    *DAT_0000b4f0 = *param_1;
    break;
  case '\x0f':
    *DAT_0000b4f4 = *(undefined1 *)param_1;
    break;
  case '\x10':
    *DAT_0000b4f8 = *param_1;
    break;
  case '\x11':
    *DAT_0000b4fc = *(undefined1 *)param_1;
    break;
  case '\x12':
    *DAT_0000b500 = *param_1;
    break;
  case '\x13':
    *DAT_0000b504 = *(undefined1 *)param_1;
    break;
  case '\x14':
    *DAT_0000b508 = *(undefined1 *)param_1;
    break;
  case '\x15':
    *DAT_0000b50c = *(undefined1 *)param_1;
    break;
  case '\x16':
    *DAT_0000b510 = *(undefined1 *)param_1;
    break;
  case '\x17':
    *DAT_0000b598 = *(undefined1 *)param_1;
    break;
  case '\x18':
    *DAT_0000b59c = *(undefined1 *)param_1;
    break;
  case '\x19':
    *DAT_0000b51c = *(undefined1 *)param_1;
    break;
  case '\x1a':
    *DAT_0000b5a0 = *param_1;
    break;
  case '\x1b':
    *DAT_0000b5a0 = *param_1;
    break;
  case '\x1c':
    *DAT_0000b5a0 = *param_1;
    break;
  case '\x1d':
    *DAT_0000b5a0 = *param_1;
    break;
  case '\x1e':
    *DAT_0000b5a0 = *param_1;
    break;
  case '\x1f':
    *DAT_0000b5a0 = *param_1;
    break;
  case ' ':
    *DAT_0000b520 = *param_1;
    break;
  case '!':
    *DAT_0000b524 = *param_1;
    break;
  case '\"':
    *DAT_0000b528 = *param_1;
    break;
  case '#':
    *DAT_0000b52c = *param_1;
    break;
  case '$':
    *DAT_0000b5a0 = *param_1;
    break;
  case '%':
    *DAT_0000b5a0 = *param_1;
    break;
  case '&':
    *DAT_0000b530 = *(undefined1 *)param_1;
    break;
  case '\'':
    *DAT_0000b534 = *param_1;
    break;
  case '(':
    *DAT_0000b5a0 = *param_1;
    break;
  case ')':
    *DAT_0000b5a0 = *param_1;
    break;
  case '*':
    *DAT_0000b5a0 = *param_1;
    break;
  case '+':
    *DAT_0000b984 = *param_1;
    break;
  case ',':
    *DAT_0000b984 = *param_1;
    break;
  case '-':
    *DAT_0000b984 = *param_1;
    break;
  case '.':
    *DAT_0000b988 = *(undefined1 *)param_1;
    break;
  case '/':
    *DAT_0000b98c = *param_1;
    break;
  case '0':
    *DAT_0000b990 = *(undefined1 *)param_1;
    break;
  case '1':
    *DAT_0000b994 = *(undefined1 *)param_1;
    break;
  case '2':
    *DAT_0000b998 = *param_1;
    break;
  case '3':
    *DAT_0000b99c = *param_1;
    break;
  case '4':
    *DAT_0000b9a0 = *param_1;
    break;
  case '5':
    *DAT_0000b9a4 = *param_1;
    break;
  case '6':
    *DAT_0000b9a8 = *param_1;
    break;
  case '7':
    *DAT_0000b9ac = *(undefined1 *)param_1;
    break;
  case '8':
    *DAT_0000b9b0 = *(undefined1 *)param_1;
    break;
  case '9':
    *DAT_0000b9b4 = *(undefined1 *)param_1;
    break;
  case ':':
    *DAT_0000b9b8 = *(undefined1 *)param_1;
    break;
  case ';':
    *DAT_0000b9bc = *(undefined1 *)param_1;
    break;
  case '<':
    *DAT_0000b9c0 = *param_1;
    break;
  case '=':
    *DAT_0000b9c4 = *param_1;
  }
  return 0;
}

/* =============================================================================
 * modbus_dispatch（0x0000B642，函数体 0xB642-0xE573，约 12049B / 5161 指令）
 * —— Modbus RTU 从站帧解析与分发（写寄存器主处理）
 *
 * ★ 说明：该函数为本固件最大函数之一，C 语言反编译结果超过 MCP 5s 传输上限
 *   （连续 6 次超时），因此改为「反汇编精读还原」：完整反汇编已另存为
 *   decompiled/08_modbus_dispatch_asm.txt（5161 条指令），此处给出流程还原、
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
