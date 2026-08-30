#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
# w8_relay_ab.py — 1-10 继电器/状态脚误吸合：原 BIN vs 编译固件 A/B 差分 + 上电预期
#
# 背景：state_machine 有两条吸合路径（07_state_machine.c）：
#   · 375 行  if (*RUN != 0) fio0_pin22_ctrl(1)      —— RLY1 运行继电器（设计意图）
#   · 397 行  if (*SYNC_2C != 1) { RLY3/RLY2/RLY1 }  —— 锁机强制三继电器全吸合
# 其中 SYNC_2C(0x1000172C) = 锁机标志（globals.c DAT_00000750=0x1000172C；
#   1=放行 / 0=锁机，README §关键语义澄清）。main() 认证段 01_startup.c:291
#   强制置 1（抄板永久放行）→ 真实上电后 397 行恒不触发。
#
# 目的：A/B 差分验证——同一预置（无故障停机运行态）下，原 BIN(0x458C) 与
#   编译固件(lookup state_machine) 对 FIO0SET/CLR（P0.20/21/22 继电器）的
#   写序列完全一致；并按 SYNC_2C=1/0 分别断言上电预期（不吸合/吸合）。
#
# 注意：Unicorn 不模拟 FIO0SET→FIO0PIN 硬件联动，且 out_relay 用 RMW
#   （读 SET/CLR → OR → 写回），故以【FIO0SET/FIO0CLR 写序列】为准判定。
#   每 bit 最后一次写定吸合态：SET 写 1=吸合、CLR 写 1=断开。
#
# 继电器映射（HARDWARE_VERIFICATION_2026-08-20.md）：
#   P0.20=RLY3 备用 / P0.21=RLY2 报警 / P0.22=RLY1 运行（ULN2003A 高电平吸合）
#   FIO0 池 0x2009C000：+0x18 SET / +0x1C CLR
# =============================================================================
import os, sys, struct

try:
    sys.stdout.reconfigure(encoding='utf-8')   # Windows 控制台 GBK 乱码防护
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'test', 'support'))
from unicorn import *
from unicorn.arm_const import *
from unicorn_harness import (parse_sections, lookup, FW, ROOT, FLASH_BASE,
                             FLASH_MAP, load_original, load_firmware, ORIG_BIN)

# ── 地址与符号 ────────────────────────────────────────────────────────────────
SM_ORIG = 0x458C                 # 原 BIN state_machine（反汇编 0000458c）
SM_NEW  = lookup('state_machine')  # 编译固件（map 解析）
SYNC_2C = 0x1000172C             # 锁机标志（= DAT_00000750 / DAT_000108a4）
FIO0_SET = 0x2009C018            # FIO0 SET
FIO0_CLR = 0x2009C01C            # FIO0 CLR
RELAY_BITS = [(0x100000, 'RLY3/P0.20'), (0x200000, 'RLY2/P0.21'), (0x400000, 'RLY1/P0.22')]
STOP = 0xFF000000

EXTRA_MAPS = [
    (0x20000000, 0x0007C000),   # SRAM1 以下未用区
    (0x20080000, 0x00020000),   # SRAM2 + AHB GPIO 池（0x2009C000 FIO）
    (0x40000000, 0x00100000),   # APB0/APB1 + SCB
    (0x50000000, 0x00100000),   # AHB 外设
    (0xE0000000, 0x00100000),   # NVIC/SCS
]

# 无故障停机运行态（同 w8_stack_watermark，SYNC_2C 地址按 globals.c 修正）
PRESET = [
    (0x1000172C, 1, 4),   # SYNC_2C = 1 放行（case 按需覆盖）
    (0x10001624, 0, 4),   # FAULT = 0
    (0x10001628, 0, 1),   # RUN = 0
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
    (0x10002076, 0, 1),   # input_state = 0
    (0x10002000, 0, 4),   # input_locked = 0
    (0x10002075, 0, 1),   # mode_byte = 0
    (0x10001ffa, 0, 1),   # debounce_count = 0
    (0x40034004, 0x80000100, 4),  # AD0GDR：DONE
    (0x4009c014, 0x00000020, 4),  # UART3 ULSR：THRE
]


def build_emu(loader):
    e = loader()
    for base, length in EXTRA_MAPS:
        e.mem_map(base, length, UC_PROT_ALL)
    return e


def run_case(loader, sm_addr, sync_val, run=0, key=0):
    """跑 state_machine 一帧，hook FIO0SET/CLR 写。返回 (writes, err)。"""
    e = build_emu(loader)
    for addr, val, width in PRESET:
        e.mem_write(addr, int(val).to_bytes(width, 'little'))
    e.mem_write(SYNC_2C, struct.pack('<I', sync_val))   # 场景锁机标志
    e.mem_write(0x10001628, struct.pack('<I', run))     # RUN 场景值
    writes = []
    def hw(uc, access, address, size, value, ud):
        if address in (FIO0_SET, FIO0_CLR):
            writes.append((address, value))
    h = e.hook_add(UC_HOOK_MEM_WRITE, hw)
    e.reg_write(UC_ARM_REG_R0, key)
    e.reg_write(UC_ARM_REG_LR, STOP)
    err = None
    try:
        e.emu_start(sm_addr | 1, STOP, count=2_000_000)
    except UcError as ex:
        err = '%s' % ex
    e.hook_del(h)
    return writes, err


def relay_final(writes):
    """每 bit 最后一次写定吸合态：SET=1→吸合 / CLR=1→断开 / 无写→保持断开。
    返回完整三 bit 状态（无写 bit 记 '断开'），便于与预期表精确比较。"""
    last = dict((n, '断开') for _, n in RELAY_BITS)   # 默认全断开（无写=保持）
    for addr, value in writes:
        for bit, name in RELAY_BITS:
            if value & bit:
                last[name] = '吸合' if addr == FIO0_SET else '断开'
    return last


def fmt_writes(writes):
    if not writes:
        return '  （无 FIO0 继电器写）'
    out = []
    for addr, value in writes:
        tag = 'SET' if addr == FIO0_SET else 'CLR'
        bits = '+'.join(n for b, n in RELAY_BITS if value & b) or '—'
        out.append('  %s 0x%08X  →  %s' % (tag, value, bits))
    return '\n'.join(out)


def main():
    print('=' * 74)
    print('W8 1-10 继电器误吸合：A/B 差分（原 BIN 0x%X vs 编译固件 0x%X）'
          % (SM_ORIG, SM_NEW))
    print('=' * 74)

    cases = [
        ('上电放行 SYNC_2C=1 RUN=0', dict(sync_val=1, run=0),
         {'RLY3/P0.20': '断开', 'RLY2/P0.21': '断开', 'RLY1/P0.22': '断开'}),
        ('锁机     SYNC_2C=0 RUN=0', dict(sync_val=0, run=0),
         {'RLY3/P0.20': '吸合', 'RLY2/P0.21': '吸合', 'RLY1/P0.22': '吸合'}),
        ('运行态   SYNC_2C=1 RUN=1', dict(sync_val=1, run=1),
         {'RLY3/P0.20': '断开', 'RLY2/P0.21': '断开', 'RLY1/P0.22': '吸合'}),
    ]

    all_pass = True
    for label, kw, expect in cases:
        w_o, err_o = run_case(load_original, SM_ORIG, **kw)
        w_n, err_n = run_case(load_firmware, SM_NEW, **kw)
        f_o, f_n = relay_final(w_o), relay_final(w_n)
        ab_same = (f_o == f_n) and (err_o is None) == (err_n is None)
        exp_ok = (f_o == expect) and (f_n == expect)
        ok = ab_same and exp_ok and not err_o and not err_n
        all_pass &= ok
        print('\n[%s]' % label)
        print('  %-14s err=%s' % ('原 BIN', err_o or '-'))
        print('  %-14s err=%s' % ('编译固件', err_n or '-'))
        print('  %-14s → %s' % ('原 BIN 末态', f_o or '（全断开）'))
        print('  %-14s → %s' % ('编译末态', f_n or '（全断开）'))
        print('  预期：%s' % expect)
        print('  A/B 一致=%s  符合预期=%s  判定=%s'
              % ('是' if ab_same else '否', '是' if exp_ok else '否',
                 'PASS' if ok else 'FAIL'))
        if not ab_same or not exp_ok:
            print('  —— 写序列明细（调试）——')
            print('  【原 BIN】')
            print(fmt_writes(w_o))
            print('  【编译固件】')
            print(fmt_writes(w_n))

    print('\n' + '=' * 74)
    verdict = 'PASS' if all_pass else 'FAIL'
    print('判定：%s —— 全部场景 A/B 一致且符合上电预期 → 1-10 上电无危险动作' % verdict)
    print('上电预期：SYNC_2C=1（main 认证强制放行）→ 397 行不触发；RUN=0 出厂')
    print('  停机态 → 三继电器全断开，无危险动作。RLY1 仅在用户启动(RUN=1)时吸合。')
    return 0 if all_pass else 1


if __name__ == '__main__':
    sys.exit(main())
