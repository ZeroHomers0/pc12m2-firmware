/* =============================================================================
 * stub.c — 反编译伪代码函数的编译占位（firmware/stub.c）
 *
 * 反编译源码中两个超大函数（state_machine 0x458C / modbus_dispatch 0xB642）
 * 无合法 C 函数体（见 08_modbus_dispatch.c 流程还原注释），gcc 直接编译会失败。
 * 二者现已由 src 侧完整还原：
 *   modbus_dispatch(0xB642) → src/08_modbus_dispatch.c（W1a 已完成）
 *   state_machine(0x458C)   → src/07_state_machine.c（W1b 进行中）
 * 故本文件不再提供二者的占位定义，只保留无法纳入 src 联编的辅助子例程。
 *
 * freq_adjust_sync（0xAB48，07 联锁屏频率调节）有完整可编译实现，从根目录
 *   07_state_machine.c 迁入（非 stub），保证联锁分支行为保真。
 * ========================================================================== */
#include <stdint.h>
#include "inc/types.h"
#include "inc/globals.h"

/* 08 UART3 RX 组帧子例程（0xAED0..0xAF06）。 */
void func_0x0000aed0(void)
{
  volatile uint8_t *state = (volatile uint8_t *)0x10001790;
  volatile uint8_t *gap = (volatile uint8_t *)0x10001791;
  volatile uint8_t *rx_idx = (volatile uint8_t *)0x10001792;
  volatile uint8_t *rx_buf = (volatile uint8_t *)0x100022A4;

  if (*state == 0) {
    *rx_idx = 0;
    *state = 1;
  }
  if (*state == 1) {
    *gap = 0;
    rx_buf[*rx_idx] = *(volatile uint8_t *)0x4009C000;
    *rx_idx = (uint8_t)(*rx_idx + 1);
  }
}

/* 0x0000A856 —— 联锁/错误屏频率调节 + EEPROM 写回（12p；6p 0xAB48。07 迁入，完整实现） */
undefined4 debounce_p09(void);
void i2c_write_reg(undefined4 param_1, undefined4 param_2);
void disp_offset(uint param_1, undefined4 param_2, int param_3, undefined4 param_4);
void disp_string(int param_1, undefined4 param_2, uint param_3, undefined4 param_4);

void freq_adjust_sync(int param_1)
{
  volatile uint32_t *puVar1;
  int iVar2;

  iVar2 = debounce_p09();
  if ((iVar2 == 1) && (*out_freq_adj != *out_freq_adj_shadow)) {
    /* shadow = live；16 位 EEPROM 写回：高字节→reg 0xc9、低字节→reg 0xca（12p 字节序） */
    *out_freq_adj_shadow = *out_freq_adj;
    i2c_write_reg((ushort)*out_freq_adj_shadow >> 8,0xc9);
    i2c_write_reg((char)*out_freq_adj_shadow,0xca);
  }
  puVar1 = out_freq_adj;
  if ((param_1 == 2) || (param_1 == 0x16)) {
    *out_freq_adj = *out_freq_adj + 1;
    if (0x2b0 < *puVar1) {
      *puVar1 = 0x2b0;
    }
    disp_offset(*out_freq_adj,2,7,1);
  }
  if ((param_1 == 3) || (param_1 == 0x21)) {
    if (*out_freq_adj < 0x45) {
      *out_freq_adj = 0x45;
    }
    *out_freq_adj = *out_freq_adj - 1;
    disp_offset(*out_freq_adj,2,7,1);
  }
  if (param_1 == 5) {
    *g_cfg_word = 1;                               /* strb → 0x10001620：运行标志 */
    disp_string((int)0x4348,3,0xb,0);    /* 0x4348："运行"（GBK d4cb d0d0） */
  }
  if (param_1 == 6) {
    *g_cfg_word = 0;                               /* strb → 0x10001620：停止标志 */
    disp_string((int)0xa98c,3,0xb,0);    /* 0xa98c："停止"（GBK cda3 d6b9，adr 取址） */
  }
  return;
}
