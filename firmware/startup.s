/* =============================================================================
 * startup.s — LPC1765 Cortex-M3 启动（firmware/startup.s）
 *
 * GCC 方案替代原 IAR 启动链（Reset→iar_program_start→iar_data_init→main）。
 * 顺序：
 *   1. 拷贝原始固件 SRAM 镜像（firmware/assets/ram_data_image.bin，VMA=0x10000000，LMA=flash）
 *      —— 恢复反编译代码绝对指针指向的 .data 初始值
 *   2. 清零原始 .bss 区（0x1000213C-0x100029C8）
 *   3. 拷贝本固件 C 代码 .data + 清零 .bss（链接器符号）
 *   4. 调 main()（main 自带 SystemInit→…→主循环）
 * ========================================================================== */
    .syntax unified
    .cpu cortex-m3
    .thumb

/* ── 初始栈顶：原始复位向量 0x100029C8 ── */

/* ═══════════════ 向量表（flash 0x0）═══════════════ */
    .section .isr_vector,"a",%progbits
    .type   g_pfnVectors, %object

g_pfnVectors:
    .word   _estack
    .word   Reset_Handler
    .word   NMI_Handler
    .word   HardFault_Handler
    .word   MemManage_Handler
    .word   BusFault_Handler
    .word   UsageFault_Handler
    .word   _vector_checksum            /* LPC Boot ROM 用户代码校验 */
    .word   0
    .word   0
    .word   0
    .word   SVC_Handler
    .word   DebugMon_Handler
    .word   0
    .word   PendSV_Handler
    .word   SysTick_Handler
/* IRQ0-34（LPC1765 向量顺序） */
    .word   WDT_IRQHandler              /* IRQ0  */
    .word   TIMER0_IRQHandler           /* IRQ1  */
    .word   TIMER1_IRQHandler           /* IRQ2  */
    .word   TIMER2_IRQHandler           /* IRQ3  */
    .word   TIMER3_IRQHandler           /* IRQ4  */
    .word   UART0_IRQHandler            /* IRQ5  */
    .word   UART1_IRQHandler            /* IRQ6  */
    .word   UART2_IRQHandler            /* IRQ7  */
    .word   UART3_IRQHandler            /* IRQ8  */
    .word   PWM1_IRQHandler             /* IRQ9  */
    .word   I2C0_IRQHandler             /* IRQ10 */
    .word   I2C1_IRQHandler             /* IRQ11 */
    .word   I2C2_IRQHandler             /* IRQ12 */
    .word   SPI_IRQHandler              /* IRQ13 */
    .word   SSP0_IRQHandler             /* IRQ14 */
    .word   SSP1_IRQHandler             /* IRQ15 */
    .word   PLL0_IRQHandler             /* IRQ16 */
    .word   RTC_IRQHandler              /* IRQ17 */
    .word   EINT0_IRQHandler            /* IRQ18 */
    .word   EINT1_IRQHandler            /* IRQ19 */
    .word   EINT2_IRQHandler            /* IRQ20 */
    .word   EINT3_IRQHandler            /* IRQ21 */
    .word   ADC0_IRQHandler             /* IRQ22 */
    .word   BOD_IRQHandler              /* IRQ23 */
    .word   USB_IRQHandler              /* IRQ24 */
    .word   CAN_IRQHandler              /* IRQ25 */
    .word   DMA_IRQHandler              /* IRQ26 */
    .word   I2S_IRQHandler              /* IRQ27 */
    .word   ETHERNET_IRQHandler         /* IRQ28 */
    .word   RIT_IRQHandler              /* IRQ29 */
    .word   MCPWM_IRQHandler            /* IRQ30 */
    .word   QEI_IRQHandler              /* IRQ31 */
    .word   PLL1_IRQHandler             /* IRQ32 */
    .word   USBActivity_IRQHandler      /* IRQ33 */
    .word   CANActivity_IRQHandler      /* IRQ34 */
    .size   g_pfnVectors, . - g_pfnVectors

/* ═══════════════ Code Read Protection word（flash 0x2FC）═══════════════ */
/* LPC17xx 把 flash 偏移 0x2FC 的 32 位字当代码读保护控制字。必须保持
   0xFFFFFFFF（无保护）；否则 Boot ROM 按未定义值启用保护（至少 CRP1），
   SWD 调试和 ISP 对 Flash 的读写都会受限。链接脚本（lpc1765.ld）把本段
   固定放在 0x2FC..0x2FF，使 .text 从 0x300 开始，普通代码绕开该地址。 */
    .section .crp,"a",%progbits
    .global   _crp_word
    .type     _crp_word, %object
_crp_word:
    .word     0xFFFFFFFF
    .size     _crp_word, . - _crp_word

/* ═══════════════ Reset_Handler ═══════════════ */
    .section .text.Reset_Handler,"ax",%progbits
    .global Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* 1. 拷贝原始 SRAM 镜像 flash→RAM */
    ldr     r0, =_ram_image_lma_start
    ldr     r1, =_ram_image_vma_start
    ldr     r2, =_ram_image_vma_end
    bl      copy_loop
    /* 2. 清零原始 .bss 区 */
    ldr     r0, =_ram_bss_start
    ldr     r1, =_ram_bss_end
    bl      zero_loop
    /* 3a. 拷贝本固件 .data */
    ldr     r0, =_sidata
    ldr     r1, =_sdata
    ldr     r2, =_edata
    bl      copy_loop
    /* 3b. 清零本固件 .bss */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    bl      zero_loop
    /* 4. 进入 main */
    bl      main
    b       .

/* ── 工具函数（r0=LMA/src、r1=VMA/dst、r2=end）── */
copy_loop:
    cmp     r1, r2
    beq     copy_done
    ldrb    r3, [r0], #1
    strb    r3, [r1], #1
    b       copy_loop
copy_done:
    bx      lr

zero_loop:
    cmp     r0, r1
    beq     zero_done
    movs    r3, #0
    strb    r3, [r0], #1
    b       zero_loop
zero_done:
    bx      lr

    .size   Reset_Handler, . - Reset_Handler

/* ═══════════════ 弱默认中断处理（vectors.c 强定义覆盖）═══════════════ */
    .macro  weak_handler name
    .weak   \name
    .type   \name, %function
\name:
    b       .
    .size   \name, . - \name
    .endm

    weak_handler NMI_Handler
    weak_handler HardFault_Handler
    weak_handler MemManage_Handler
    weak_handler BusFault_Handler
    weak_handler UsageFault_Handler
    weak_handler SVC_Handler
    weak_handler DebugMon_Handler
    weak_handler PendSV_Handler
    weak_handler SysTick_Handler
    weak_handler TIMER3_IRQHandler
    weak_handler UART0_IRQHandler
    weak_handler UART1_IRQHandler
    weak_handler UART2_IRQHandler
    weak_handler PWM1_IRQHandler
    weak_handler I2C0_IRQHandler
    weak_handler I2C1_IRQHandler
    weak_handler I2C2_IRQHandler
    weak_handler SPI_IRQHandler
    weak_handler SSP0_IRQHandler
    weak_handler SSP1_IRQHandler
    weak_handler PLL0_IRQHandler
    weak_handler RTC_IRQHandler
    weak_handler EINT0_IRQHandler
    weak_handler ADC0_IRQHandler
    weak_handler BOD_IRQHandler
    weak_handler ETHERNET_IRQHandler
    weak_handler USB_IRQHandler
    weak_handler CAN_IRQHandler
    weak_handler DMA_IRQHandler
    weak_handler I2S_IRQHandler
    weak_handler RIT_IRQHandler
    weak_handler MCPWM_IRQHandler
    weak_handler QEI_IRQHandler
    weak_handler PLL1_IRQHandler
    weak_handler USBActivity_IRQHandler
    weak_handler CANActivity_IRQHandler
    weak_handler WDT_IRQHandler
    weak_handler TIMER0_IRQHandler
    weak_handler TIMER1_IRQHandler
    weak_handler TIMER2_IRQHandler
    weak_handler UART3_IRQHandler
    weak_handler EINT1_IRQHandler
    weak_handler EINT2_IRQHandler
    weak_handler EINT3_IRQHandler

    .end
