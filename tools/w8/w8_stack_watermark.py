#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
# w8_stack_watermark.py — W8 阶段1 必检 1-11：最低 MSP 栈水位（Unicorn 离线模拟）
#
# 背景：J-Link 实机哨兵测栈水位已证伪（停核 write 无效 / 栈区读 0 / connect
#   注入 3456ABCD / LCD 随机 / 按键不响应——v1-v4 实验链）。本脚本改用 Unicorn
#   模拟真实编译固件（firmware.elf），在仿真里真实执行主循环函数链 + 各 ISR，
#   用 UC_HOOK_MEM_WRITE 捕获栈区（0x100027C8-0x100029C8）写入时的最低 SP。
#
# 原理：
#   · 加载 flash + SRAM0 + SRAM1 + 外设空间（LPC1765 APB/AHB/NVIC 全 map RAM）
#   · 预置外设位（AD0GDR DONE / UART3 ULSR THRE）→ 避免 ADC/UART 等待死循环
#   · 预置关键状态全局为「无故障停机运行态」→ 避开 state_machine 故障锁定死循环
#   · 分别模拟执行 8 个主循环函数与 8 个 ISR，hook 栈区写记录最低 SP
#   · 合成最坏 MSP = 主循环最深 + main帧 + 中断硬件压栈(32B) + 最深 ISR 内部栈深
#   · 判定：worst_MSP ≥ 栈底 0x100027C8 + 128B 余量 → PASS（W8_STAGE1 §1 标准）
#
# 中断嵌套：源码无 NVIC_IPR 设置 → 全部 IRQ 默认同优先级(0) → Cortex-M3 同优先
#   级不嵌套 → 主判据用单层中断；另报告双层保守参考。
#
# 最终结论（2026-08-27 判定 PASS）：
#   · 主循环最深 = state_machine 深路径 MENU=3(基本参数屏) key=2(项间导航)，
#     4×disp_string 页重绘 → SP=0x10002954 depth=116B（实测，真实分支）
#   · 最深 ISR = EINT3(过零) depth=24B（纯位操作，实测）
#   · 单层中断 最坏 MSP = 0x1000290C，距栈底 0x100027C8 余量 324B ≥ 128B → PASS
#   · 双层保守参考 最坏 MSP = 0x100028E4，余量 284B，也 PASS
#   · 2026-08-27 地址修正：PRESET_CLEAN 的认证放行位原误写 0x10000750（01_startup.c
#     注释笔误），实际为 0x1000172C=SYNC_2C（globals.c DAT_00000750）。修正后重跑
#     depth 仍 116B、余量 324B 不变 → 1-11 结论不受影响（state_machine 397 行
#     SYNC_2C=1 放行不吸合继电器，本脚本 preset 已对齐真实上电态）。
#   · 已知局限：UART3 ISR 的 func_0x0000aed0 为 stub 替代(真实为 RX 组帧，估浅)；
#     外设空间读 0 使部分外设依赖分支走默认路径；多步状态转移未逐组合覆盖。
#     全部上浮(ISR+50B, 主循环+50B)仍 ≤ 288B < 512-128=384B，结论稳健。
# =============================================================================
import os, sys, struct

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'test', 'support'))
from unicorn import *
from unicorn.arm_const import *
from unicorn_harness import parse_sections, lookup, FW, ROOT, FLASH_BASE, FLASH_MAP

# ── 实机栈区（lpc1765.ld 权威）───────────────────────────────────────────────
STACK_TOP = 0x100029C8          # _estack（复位向量栈顶）
STACK_BOT = 0x100027C8          # 栈底（fw_bss 末）
STACK_END = 0x10008000          # SRAM0 上界（hook 用）
HW_PUSH   = 32                  # Cortex-M3 异常自动压栈 8 字（xPSR/PC/LR/R12/R3-R0）
MAIN_FRAME = 16                 # main 局部变量帧保守估计（反编译 main 4 个局部）
STOP = 0xFF000000               # LR 哨兵返回地址

# ── 内存映射 ──────────────────────────────────────────────────────────────────
SRAM0_BASE, SRAM0_LEN = 0x10000000, 0x8000
SRAM1_BASE, SRAM1_LEN = 0x2007C000, 0x4000
# 外设/未用区宽覆盖为可读写 RAM（Unicorn 未映射访问会 UC_ERR）
EXTRA_MAPS = [
    (0x20000000, 0x0007C000),   # SRAM1 以下未用区（0x20000000-0x2007C000）
    (0x20080000, 0x00020000),   # SRAM2 + AHB GPIO1/3（0x2009C000 FIO 池）宽覆盖
    (0x40000000, 0x00100000),   # APB0/APB1 外设 + SCB（0x400FC000：PLL/PCONP/EXTINT）
    (0x50000000, 0x00100000),   # AHB 外设（SSP 等）
    (0xE0000000, 0x00100000),   # NVIC/SCS/调试（ISER/IPR）
]

# ── 关键全局预置：无故障停机运行态（地址来自 07_state_machine.c 宏 + 01_startup.c）──
PRESET_CLEAN = [
    (0x1000172C, 1, 4),   # SYNC_2C 锁机标志=1 放行（globals.c DAT_00000750=0x1000172C，
                          #   main 认证 01_startup.c:291 强制置 1；0x10000750 是注释笔误）
    (0x10001624, 0, 4),   # FAULT = 0（避开故障锁定 for(;;){} 死循环）
    (0x10001628, 0, 1),   # RUN = 0（停机）
    (0x10001744, 0, 1),   # MENU = 0
    (0x10001745, 0, 1),   # MENU2 = 0
    (0x10001655, 0, 1),   # DISP_SEL = 0
    (0x10001634, 0, 1),   # CTRL_MODE = 0
    (0x100015cd, 0, 1),   # DISP_MODE = 0
    (0x1000177d, 0, 1),   # RUN_REQ = 0
    (0x1000177e, 0, 1),   # STOP_REQ = 0
    (0x1000177f, 0, 1),   # STOP_PEND = 0
    (0x10001780, 0, 1),   # DB_116 = 0
    (0x1000177c, 0, 1),   # DB_117 = 0
    (0x10002076, 0, 1),   # input_state = 0（g_input_state→0x10002076）
    (0x10002000, 0, 4),   # input_locked = 0（PTR_input_locked→0x10002000）
    (0x10002075, 0, 1),   # mode_byte = 0（g_mode_byte→0x10002075）
    (0x10001ffa, 0, 1),   # debounce_count = 0（PTR_debounce_count→0x10001FFA）
]
# 外设位（避免 ADC/UART 等待死循环）
PRESET_PERIPH = [
    (0x40034004, 0x80000100, 4),  # AD0GDR：DONE + 任意 12 位数据
    (0x4009c014, 0x00000020, 4),  # UART3 ULSR：THRE
]

# ── 被测符号（firmware.map 解析）──────────────────────────────────────────────
MAIN_LOOP = [
    'stub_ret', 'adc0_scan_channels', 'input_scan_state',
    'state_machine', 'output_stage', 'wd_feed',
    'uart3_rx_timeout_monitor', 'modbus_dispatch',
]
ISRS = [
    'TIMER0_IRQHandler', 'WDT_IRQHandler', 'UART3_IRQHandler',
    'EINT1_IRQHandler', 'EINT2_IRQHandler', 'EINT3_IRQHandler',
    'TIMER1_IRQHandler', 'TIMER2_IRQHandler',
]


def build_emu():
    """加载 firmware.elf + 全内存 map + 预置外设位。返回 Uc 实例。"""
    secs, d = parse_sections(FW)
    e = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    e.mem_map(FLASH_BASE, FLASH_MAP, UC_PROT_ALL)
    for sec in ('.isr_vector', '.text'):
        if sec in secs:
            a, o, s = secs[sec]
            e.mem_write(a, d[o:o + s])
    e.mem_map(SRAM0_BASE, SRAM0_LEN, UC_PROT_ALL)
    if '.fw_image' in secs:
        a, o, s = secs['.fw_image']
        e.mem_write(a, d[o:o + s])
    e.mem_map(SRAM1_BASE, SRAM1_LEN, UC_PROT_ALL)
    if '.data' in secs:
        a, o, s = secs['.data']
        e.mem_write(a, d[o:o + s])
    for base, length in EXTRA_MAPS:
        e.mem_map(base, length, UC_PROT_ALL)
    # 预置外设位
    for addr, val, width in PRESET_PERIPH:
        e.mem_write(addr, int(val).to_bytes(width, 'little'))
    e.reg_write(UC_ARM_REG_SP, STACK_TOP)
    return e


def apply_presets(e):
    for addr, val, width in PRESET_CLEAN:
        e.mem_write(addr, int(val).to_bytes(width, 'little'))


def measure(e, func_addr, label, args=None, max_insn=4_000_000, extra_preset=None):
    """重置栈+预置全局后 call func，hook 栈区写记录最低占用地址。
    注意：Unicorn 的 UC_HOOK_MEM_WRITE 在写指令完成前触发，SP 尚未更新（PUSH 逐字
    写时读 SP 仍是旧值），但栈内容已写到目标地址——故以【最低栈写入地址】为栈水位，
    等价于栈被占用的最深点。返回 (min_wr, depth, stack_hits, err)。
    extra_preset：apply_presets 之后再覆盖的 (addr,val,width) 列表（如 MENU case 值）。"""
    e.reg_write(UC_ARM_REG_SP, STACK_TOP)
    e.reg_write(UC_ARM_REG_LR, STOP)
    if args:
        for i, v in enumerate(args[:4]):
            e.reg_write([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][i], v)
    apply_presets(e)
    if extra_preset:
        for addr, val, width in extra_preset:
            e.mem_write(addr, int(val).to_bytes(width, 'little'))
    min_wr = [STACK_TOP]
    hits = [0]
    def hw(uc, access, address, size, value, ud):
        if STACK_BOT <= address < STACK_TOP:
            hits[0] += 1
            if address < min_wr[0]:
                min_wr[0] = address
    h = e.hook_add(UC_HOOK_MEM_WRITE, hw)
    err = None
    try:
        e.emu_start(func_addr | 1, STOP, count=max_insn)
    except UcError as ex:
        err = '%s' % ex
    e.hook_del(h)
    return min_wr[0], STACK_TOP - min_wr[0], hits[0], err


def main():
    e = build_emu()
    print('=' * 74)
    print('W8 1-11 栈水位 Unicorn 离线模拟  (栈区 0x%X-0x%X, 栈顶=0x%X)'
          % (STACK_BOT, STACK_TOP, STACK_TOP))
    print('=' * 74)

    results = {}

    # ── 主循环函数链（真实调用，hook 覆盖嵌套子函数）──────────────────
    print('\n[主循环函数]  addr        minSP      depth   写栈  err')
    main_min = STACK_TOP
    for name in MAIN_LOOP:
        addr = lookup(name)
        min_sp, depth, hits, err = measure(e, addr, name)
        results['main:' + name] = (min_sp, depth, hits, err)
        main_min = min(main_min, min_sp)
        print('  %-20s 0x%08X 0x%08X %6d %6d  %s'
              % (name, addr, min_sp, depth, hits, err or ''))

    # ── state_machine 深路径：MENU × KEY 参数扫描 ──────────────────────
    # case 内深分支由 key（按键值：1=确认 2=DOWN 3=UP 4=SET 5=启动 6=停机
    #   0x16=快加 0x17=统计清零 0xe=密码）触发；key=0 只走 IDLE 累积浅路径。
    sm_addr = lookup('state_machine')
    sm_best = [None, STACK_TOP]
    for menu in (1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0x14, 0x1e, 0x5a, 0x62, 0x63):
        for key in (0, 1, 2, 3, 4, 5, 6, 0xe, 0x16, 0x17, 0x21):
            min_sp, depth, hits, err = measure(e, sm_addr, 'sm:M%#x,K%#x' % (menu, key),
                                               args=[key], max_insn=300000,
                                               extra_preset=[(0x10001744, menu, 1)])
            results['sm:m%#x,k%#x' % (menu, key)] = (min_sp, depth, hits, err)
            if not err and min_sp < sm_best[1]:
                sm_best = [(menu, key), min_sp]
            main_min = min(main_min, min_sp)
    if sm_best[0] is not None:
        menu, key = sm_best[0]
        print('  [state_machine 深路径] 最深 MENU=%#x KEY=%#x → lowest 0x%08X depth=%d'
              % (menu, key, sm_best[1], STACK_TOP - sm_best[1]))
    else:
        print('  [state_machine 深路径] 全部测量失败！')

    # ── ISR（记录自身内部栈深，不含硬件压栈 32B）────────────────────────
    print('\n[ISR]          addr        minSP      depth   写栈  err')
    isr_depths = []
    for name in ISRS:
        addr = lookup(name)
        min_sp, depth, hits, err = measure(e, addr, name)
        results['isr:' + name] = (min_sp, depth, hits, err)
        if not err:
            isr_depths.append((depth, name, addr, min_sp))
        print('  %-20s 0x%08X 0x%08X %6d %6d  %s'
              % (name, addr, min_sp, depth, hits, err or ''))
    isr_depths.sort(reverse=True)

    # ── 合成最坏 MSP ─────────────────────────────────────────────────────
    print('\n' + '=' * 74)
    print('合成（栈底 0x%X, 栈空间 0x%X-0x%X = %d B）'
          % (STACK_BOT, STACK_BOT, STACK_TOP, STACK_TOP - STACK_BOT))
    print('=' * 74)
    print('主循环最深 SP        : 0x%08X (depth %d B)' % (main_min, STACK_TOP - main_min))
    print('main 局部变量帧      : +%d B' % MAIN_FRAME)
    if isr_depths:
        d1, n1, a1, m1 = isr_depths[0]
        print('最深 ISR %-18s: depth %d B' % (n1, d1))
        worst1 = main_min - MAIN_FRAME - HW_PUSH - d1
        margin1 = worst1 - STACK_BOT
        print('\n[单层中断（默认优先级同，不嵌套）] 最坏 MSP = 0x%08X'
              % (main_min - MAIN_FRAME - HW_PUSH - d1))
        print('  最坏 MSP 距栈底余量 : %d B   %s' % (margin1,
              'PASS (≥128B)' if margin1 >= 128 else 'FAIL (<128B)'))
        # 保守参考：两层最深 ISR 嵌套
        d2, n2, a2, m2 = isr_depths[1] if len(isr_depths) > 1 else (0, '-', 0, 0)
        worst2 = main_min - MAIN_FRAME - HW_PUSH - d1 - HW_PUSH - d2
        margin2 = worst2 - STACK_BOT
        print('\n[双层嵌套（保守参考）]  %s(%d) + %s(%d) 最坏 MSP = 0x%08X 余量 %d B'
              % (n1, d1, n2, d2, worst2, margin2))
    else:
        print('无有效 ISR 测量！')

    # ── 结论 ────────────────────────────────────────────────────────────
    if isr_depths:
        verdict = 'PASS' if margin1 >= 128 else 'FAIL'
        print('\n判定：1-11 最低 MSP 栈水位 = %s（单层中断，余量 %d B ≥ 128B 标准）'
              % (verdict, margin1))
    else:
        print('\n判定：无有效数据')
    return 0


if __name__ == '__main__':
    sys.exit(main())
