/* =============================================================================
 * reg.h — LPC1765 外设寄存器（firmware/inc）
 *
 * 依据 LPC176x/5x 用户手册 + 反编译代码实际访问偏移（01/05/08/09 模块核实）。
 * 访问宽度：反编译为 uint*（32 位）用 REG32；为 char* / byte（LCR/DLL/DLM 等）
 * 用 REG8。volatile 全程保留（外设/MRAM 直接访问）。
 * ========================================================================== */
#ifndef REG_H
#define REG_H

#include <stdint.h>

#define REG32(addr)  (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG8(addr)   (*(volatile uint8_t  *)(uintptr_t)(addr))

/* ================= FIO 池 0x2009C000 =================
 * DIR: FIO0..4 = +0x00/+0x20/+0x40/+0x60/+0x80
 * SET/CLR: FIO0 = +0x18/+0x1C，FIO1 = +0x38/+0x3C
 * +0x54 FIO2PIN +0x58 FIO2SET +0x5C FIO2CLR
 * +0x80 FIO3DIR                                                       */
#define FIO_BASE        0x2009C000UL
#define FIO0SET         REG32(FIO_BASE + 0x18)
#define FIO0CLR         REG32(FIO_BASE + 0x1C)
#define FIO0DIR         REG32(FIO_BASE + 0x00)
#define FIO1SET         REG32(FIO_BASE + 0x38)
#define FIO1CLR         REG32(FIO_BASE + 0x3C)
#define FIO1DIR         REG32(FIO_BASE + 0x20)
#define FIO2DIR         REG32(FIO_BASE + 0x40)
#define FIO2PIN         REG32(FIO_BASE + 0x54)
#define FIO2SET         REG32(FIO_BASE + 0x58)
#define FIO2CLR         REG32(FIO_BASE + 0x5C)
#define FIO3DIR         REG32(FIO_BASE + 0x60)
#define FIO4DIR         REG32(FIO_BASE + 0x80)

/* ================= TIMER 通用结构（TIMER0/1/2/3 同布局） ================= */
typedef struct {
    volatile uint32_t IR;    /* +0x00 中断寄存器 */
    volatile uint32_t TCR;   /* +0x04 计数控制 */
    volatile uint32_t TC;    /* +0x08 定时器计数器 */
    volatile uint32_t PR;    /* +0x0C 预分频寄存器 */
    volatile uint32_t PC;    /* +0x10 预分频计数器 */
    volatile uint32_t MCR;   /* +0x14 匹配控制 */
    volatile uint32_t MR0;   /* +0x18 */
    volatile uint32_t MR1;   /* +0x1C */
    volatile uint32_t MR2;   /* +0x20 */
    volatile uint32_t MR3;   /* +0x24 */
} LPC_TIM_TypeDef;

#define TIMER0           ((LPC_TIM_TypeDef *)0x40004000UL)
#define TIMER1           ((LPC_TIM_TypeDef *)0x40008000UL)
#define TIMER2           ((LPC_TIM_TypeDef *)0x40090000UL)

#define TMR_IR           TIMER0->IR
#define TMR_TCR          TIMER0->TCR
#define TMR_MCR          TIMER0->MCR
#define TMR_MR0          TIMER0->MR0

/* ================= WDT 0x40000000 =================
 * +0x00 WDMOD +0x04 WDTC +0x08 WDFEED +0x0C WDTV                */
#define WDT_BASE        0x40000000UL
#define WDMOD           REG8(WDT_BASE + 0x00)
#define WDTC            REG32(WDT_BASE + 0x04)
#define WDFEED          REG8(WDT_BASE + 0x08)
#define WDTV            REG32(WDT_BASE + 0x0C)

/* ================= UART3 0x4009C000 =================
 * +0x00 RBR/THR/DLL +0x04 IER/DLM +0x08 IIR/FCR
 * +0x0C LCR +0x10 MCR +0x14 LSR +0x18 MSR +0x1C SCR          */
#define UART3_BASE      0x4009C000UL
#define U3RBR           REG8(UART3_BASE + 0x00)
#define U3THR           REG8(UART3_BASE + 0x00)
#define U3DLL           REG8(UART3_BASE + 0x00)
#define U3DLM           REG8(UART3_BASE + 0x04)
#define U3IER           REG32(UART3_BASE + 0x04)   /* DLAB=0 时读改写此 8 位(反汇编 word 置位) */
#define U3IIR           REG8(UART3_BASE + 0x08)    /* 反汇编 ldrb：只读 8 位 */
#define U3FCR           REG8(UART3_BASE + 0x08)    /* 反汇编 strb：只写 8 位 */
#define U3LCR           REG8(UART3_BASE + 0x0C)
#define U3LSR           REG8(UART3_BASE + 0x14)    /* 反汇编 ldrb：只读 8 位 */

/* ================= ADC0 0x40034000 =================
 * +0x00 ADCR +0x04 ADGDR +0x08 ADGSR +0x0C ADSTAT
 * +0x10..0x2C ADDR0..7                                              */
#define ADC0_BASE       0x40034000UL
#define AD0CR           REG32(ADC0_BASE + 0x00)
#define AD0GDR          REG32(ADC0_BASE + 0x04)

/* ================= SCB 0x400FC000 =================
 * +0xC4 PCONP +0x140 EXTINT +0x148 EXTMODE +0x14C EXTPOLAR
 * +0x1A8 PCLKSEL0 +0x1AC PCLKSEL1                                  */
#define SCB_BASE        0x400FC000UL
#define PCONP           REG32(SCB_BASE + 0xC4)
#define EXTINT          REG32(SCB_BASE + 0x140)
#define EXTMODE         REG32(SCB_BASE + 0x148)
#define EXTPOLAR        REG32(SCB_BASE + 0x14C)
#define PCLKSEL0        REG32(SCB_BASE + 0x1A8)
#define PCLKSEL1        REG32(SCB_BASE + 0x1AC)

/* ================= PINSEL 0x4002C000 =================
 * +0x00 PINSEL0 +0x04 PINSEL1 +0x08 PINSEL2 +0x0C PINSEL3
 * +0x10 PINSEL4 +0x14 PINSEL5 +0x18 PINSEL6 +0x1C PINSEL7
 * +0x20 PINSEL8 +0x24 PINSEL9 +0x28 PINSEL10 +0x2C PINSEL11         */
#define PINSEL_BASE     0x4002C000UL
#define PINSEL0         REG32(PINSEL_BASE + 0x00)
#define PINSEL1         REG32(PINSEL_BASE + 0x04)
#define PINSEL2         REG32(PINSEL_BASE + 0x08)
#define PINSEL3         REG32(PINSEL_BASE + 0x0C)
#define PINSEL4         REG32(PINSEL_BASE + 0x10)

/* ================= NVIC 0xE000E100 ================= */
#define NVIC_ISER0      REG32(0xE000E100UL)

#endif /* REG_H */
