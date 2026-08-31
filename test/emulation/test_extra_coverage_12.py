# -*- coding: utf-8 -*-
# =============================================================================
# test_extra_coverage_12.py — PC12M-2 查漏补缺回归测试（移植 6p emulation 覆盖组）
#
# 背景：PC6M-10/test/emulation 有 22 个 A/B 测试；P5 只覆盖了其中一部分
# （静态+状态机+Modbus 读写矩阵等）。本文件把 12p 尚未覆盖的组移植为 12p 地址：
#   · adc_wait_done          —— AD0GDR/AD0DR0 播不同结果，抓"错读 +16 偏移"回归
#   · adc_scan_sequence      —— 连续多拍扫描 + cfg/gain_sel/标定除数边界
#   · input_scan             —— 全 6 位引脚组合 × 计数初值差分 + 关键按键事件
#   · uart_rx_sequence       —— 经 UART3_IRQHandler(IIR=4) 多字节组帧状态转移/索引回绕
#   · modbus_dispatch        —— 读/写/异常/CRC 错/站址不匹配帧全流程 A/B
#   · eeprom_sync_matrix     —— param_sync 逐字节/批量扰动（i2c_write_reg 捕获）
#   · interrupt_sequence     —— 多 ISR 按先后顺序连续执行（含 EINT1/2/3）
#   · control_multitick      —— 状态机+输出级在持久 RAM 上连续多拍
#   · case3_edit             —— menu=3 case3 编辑键（menu2=2..15，P5 只覆盖 0/1/3）
#
# 全部 A/B 差分：OLD(BIN) vs NEW(ELF) 同一初态执行，比较 R0 + SRAM 快照 +
# （需要时）外设/GPIO 写迹或 i2c 写序列。
#
# 用法：PC12M-2 仓库根目录 `python test/emulation/test_extra_coverage_12.py`
# 依赖：tools/verification/verify_firmware_equivalence_12.py（machine/run/snapshot/SYMS）
# =============================================================================
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verification"))
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

from unicorn import UcError, UC_HOOK_CODE, UC_HOOK_MEM_WRITE  # noqa: E402
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,  # noqa: E402
                               UC_ARM_REG_R1)

from verify_firmware_equivalence_12 import (machine, run, snapshot, SYMS,  # noqa: E402
                                            ORIGINAL, STOP)

# ── 12p OLD(BIN) 入口 → NEW(ELF) 符号（P5 反汇编确认）────────────────────
OLD = {
    "adc0_wait_done": 0x1f56, "adc0_start": 0x1f30, "adc0_scan_channels": 0x1f6c,
    "input_scan_state": 0x15ae,
    "i2c_write_reg": 0x1e38, "param_sync_live_to_eeprom": 0x3534,
    "modbus_dispatch": 0xb3b2, "uart3_tx_byte": 0xab7c,
    "UART3_IRQHandler": 0xac78,
    "TIMER0_IRQHandler": 0x29a, "TIMER1_IRQHandler": 0xfb0c, "TIMER2_IRQHandler": 0xfae8,
    "EINT1_IRQHandler": 0xf748, "EINT2_IRQHandler": 0xf76a, "EINT3_IRQHandler": 0xf78c,
    "state_machine": 0x4464, "output_stage": 0xe70c,
    "disp_string": 0xcec, "disp_uint4": 0xe80, "disp_number3": 0xdf2,
    "disp_clear": 0x942, "disp_splash_screen": 0x40b0,
}
NEW = {k: SYMS[k] for k in OLD}

ADC_BASE = 0x40034000        # LPC1765 ADC0 外设块（12p machine() 未映射，需补）
AD0CR = ADC_BASE + 0x00
AD0GDR = ADC_BASE + 0x04
AD0DR0 = ADC_BASE + 0x10
TRAP_PTR = 0x10002000
TRAP_VAL = 0x80000000 | (0x777 << 4)

U3RBR = 0x4009C000
U3IIR = 0x4009C008

_passed = _failed = 0


def _check(name, cond, detail=""):
    global _passed, _failed
    _passed += bool(cond)
    _failed += not cond
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", name,
                           ("  " + detail) if detail else ""))


def map_adc(uc):
    try:
        uc.mem_map(ADC_BASE, 0x1000)
    except Exception:
        pass


def call(uc, entry, max_insn=2_000_000):
    """单次调用：重置 LR=STOP+1 再跑（重复调用时 OLD/NEW 均需重置）。"""
    uc.reg_write(UC_ARM_REG_LR, STOP + 1)
    run(uc, entry, max_insn=max_insn)


def add_skip(uc, address):
    """entry 处 PC=LR 跳过函数体（用于待 hook 外设时序的函数）。"""
    def skip(machine_, a, s, u):
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)


# ── 1) adc_wait_done（AD0GDR/AD0DR0 不同结果，抓 +16 偏移回归）────────────────
def _adc_wait_run(is_new, gdr, dr0):
    uc = machine(is_new)
    map_adc(uc)
    uc.mem_write(AD0CR, struct.pack("<I", TRAP_PTR))        # bug 版会当指针解引用
    uc.mem_write(TRAP_PTR + 16, struct.pack("<I", TRAP_VAL))
    uc.mem_write(AD0GDR, struct.pack("<I", gdr))
    uc.mem_write(AD0DR0, struct.pack("<I", dr0))
    call(uc, OLD["adc0_wait_done"] if not is_new else NEW["adc0_wait_done"])
    return uc.reg_read(UC_ARM_REG_R0)


def test_adc_wait_done():
    cases = [
        ("DONE+0xABC/0xDEF", 0x80000000 | (0xABC << 4), 0x80000000 | (0xDEF << 4)),
        ("DONE+0x000/0xFFF", 0x80000000 | (0x000 << 4), 0x80000000 | (0xFFF << 4)),
        ("DONE+0xFFF/0x001", 0x80000000 | (0xFFF << 4), 0x80000000 | (0x001 << 4)),
        ("DONE+0x555/0x2AA", 0x80000000 | (0x555 << 4), 0x80000000 | (0x2AA << 4)),
    ]
    print("== adc_wait_done ==")
    for label, gdr, dr0 in cases:
        ro = _adc_wait_run(False, gdr, dr0)
        rn = _adc_wait_run(True, gdr, dr0)
        exp = (gdr >> 4) & 0xFFF
        _check("%s A/B 等价" % label, ro == exp and rn == exp and ro == rn,
               "期望=0x%03X 原=0x%03X 新=0x%03X" % (exp, ro, rn))
    return 0


# ── 2) adc_scan_channels（连续多拍 + cfg/gain_sel/除数边界）──────────────────
def _adc_scan_run(is_new, samples, calls, overrides=()):
    uc = machine(is_new)
    map_adc(uc)
    for address, data in overrides:
        uc.mem_write(address, data)
    cursor = [0]

    def start_hook(machine_, a, s, u):
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    def wait_hook(machine_, a, s, u):
        v = samples[cursor[0] % len(samples)] & 0xFFF
        cursor[0] += 1
        machine_.reg_write(UC_ARM_REG_R0, v)
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    s_addr = NEW["adc0_start"] if is_new else OLD["adc0_start"]
    w_addr = NEW["adc0_wait_done"] if is_new else OLD["adc0_wait_done"]
    # 只注册本侧地址：12p OLD/NEW 布局不同，OLD 地址落在 NEW 函数体中段会误触发
    uc.hook_add(UC_HOOK_CODE, start_hook, begin=s_addr, end=s_addr + 1)
    uc.hook_add(UC_HOOK_CODE, wait_hook, begin=w_addr, end=w_addr + 1)
    for _ in range(calls):
        call(uc, OLD["adc0_scan_channels"] if not is_new else NEW["adc0_scan_channels"])
    return (bytes(uc.mem_read(0x10000000, 0x4000)),
            bytes(uc.mem_read(ADC_BASE, 0x40)), cursor[0])


def test_adc_scan():
    print("== adc_scan_channels ==")
    cases = [
        ("零值连续12拍", [0], 12, ()),
        ("六通道固定梯度连续12拍", [100, 300, 500, 700, 900, 1100], 12, ()),
        ("边界与交错样本连续24拍", [0, 1, 9, 10, 2047, 4095, 123, 3960], 24, ()),
    ]
    for cfg in (0, 1):
        for gain_sel in (0, 1, 2):
            cases.append(("cfg=%d/gain_sel=%d" % (cfg, gain_sel), [9, 10, 2047, 4095, 800, 1000], 18,
                          ((0x10001620, bytes((cfg,))), (0x1000162C, bytes((gain_sel,))))))
    for divisor in (1, 10, 0xFFFF, 0xFFFFFFFF):
        packed = divisor.to_bytes(4, "little")
        overrides = tuple((a, packed) for a in
                          (0x10001690, 0x10001698, 0x100016A0, 0x100016A8, 0x100016B0))
        cases.append(("标定除数边界0x%X" % divisor, [0, 1, 4095, 3960, 10, 2047], 18, overrides))
    failed = 0
    for name, samples, calls, overrides in cases:
        old = _adc_scan_run(False, samples, calls, overrides)
        new = _adc_scan_run(True, samples, calls, overrides)
        ok = old == new
        if not ok:
            diff = next((i for i, (a, b) in enumerate(zip(old[0], new[0])) if a != b), -1)
            _check("%s A/B 等价" % name, False,
                   "RAM首差异=0x%08X conversions=原%d/新%d" % (
                       0x10000000 + diff, old[2], new[2]))
            failed += 1
        else:
            _check("%s A/B 等价" % name, True, "conversions=%d" % old[2])
    return failed


# ── 3) input_scan_state（全 6 位引脚组合 × 计数初值 + 关键事件）──────────────
FIO_BASE = 0x2009C000
FIO1PIN = FIO_BASE + 0x34
FIO0PIN = FIO_BASE + 0x14
FIO3PIN = FIO_BASE + 0x74
CNT = 0x10001588
LATCH = 0x10001570
SLOW = 0x10001571


def _input_scan_run(is_new, p1, p0, p3, counter, latch, slow):
    uc = machine(is_new)
    uc.mem_write(CNT, struct.pack("<I", counter))
    uc.mem_write(LATCH, bytes((latch,)))
    uc.mem_write(SLOW, bytes((slow,)))
    uc.mem_write(FIO1PIN, struct.pack("<I", p1))
    uc.mem_write(FIO0PIN, struct.pack("<I", p0))
    uc.mem_write(FIO3PIN, struct.pack("<I", p3))
    call(uc, OLD["input_scan_state"] if not is_new else NEW["input_scan_state"])
    return uc.reg_read(UC_ARM_REG_R0), snapshot(uc)


def test_input_scan():
    print("== input_scan_state ==")
    P1M = 0x80000 | 0x40000
    P0M = 0x40000000 | 0x20000000
    P3M = 0x2000000 | 0x4000000

    def combo(m):
        p1 = (0x80000 if m & 1 else 0) | (0x40000 if m & 2 else 0)
        p0 = (0x40000000 if m & 4 else 0) | (0x20000000 if m & 8 else 0)
        p3 = (0x2000000 if m & 16 else 0) | (0x4000000 if m & 32 else 0)
        return p1, p0, p3

    combos = [combo(m) for m in range(64)]
    counters = [0, 1, 0x18, 0x19, 0x1a, 0xf5, 0xf9, 0xfa]
    sweep = 0
    for p1, p0, p3 in combos:
        for c in counters:
            ro, so = _input_scan_run(False, p1, p0, p3, c, 0, 0)
            rn, sn = _input_scan_run(True, p1, p0, p3, c, 0, 0)
            if not (ro == rn and so == sn):
                _check("组合 p1=0x%08X p0=0x%08X p3=0x%08X cnt=0x%X" % (p1, p0, p3, c),
                       False, "原=0x%X 新=0x%X" % (ro, rn))
                return 1
            sweep += 1
    _check("引脚组合×计数差分 %d 例" % sweep, True)

    ev_cases = [
        ("0x16 快加 A=1,B=0,P0.30/29,P3.25/26 就绪", 0x80000, 0x60000000, 0x6000000, 0x16),
        ("0x21 快减 A=1,B=1,P0.30=0,P0.29=1",        0xC0000, 0x20000000, 0x6000000, 0x21),
        ("0x17 慢减 A=1,B=0,P0.30=0,P0.29=1(计数0x1D)", 0x80000, 0x20000000, 0x6000000, 0x17),
        ("0x0E 组合 A=0,B=1,P0.30=1,P0.29=0(计数0x1D)", 0x40000, 0x40000000, 0x6000000, 0x0E),
        ("0x0B 慢加 A=0,B=1,P0.30/29=1(慢计数9)",      0x40000, 0x60000000, 0x6000000, 0x0B),
    ]
    for name, p1, p0, p3, ev in ev_cases:
        slow = 0x1D if ev in (0x17, 0x0E) else (0x09 if ev == 0x0B else 0)
        ro, so = _input_scan_run(False, p1, p0, p3, 0xFA, 0, slow)
        rn, sn = _input_scan_run(True, p1, p0, p3, 0xFA, 0, slow)
        _check("%s → 0x%02X" % (name, ev),
               ro == ev and rn == ev and so == sn,
               "原=0x%02X 新=0x%02X" % (ro, rn))
    return 0


# ── 4) uart_rx_sequence（经 UART3_IRQHandler IIR=4 组帧）───────────────────
RX_STATE = 0x10001770        # (state, gap, idx)
FRAME = 0x10002278            # rx_buf


def _uart_rx_run(is_new, initial, data):
    uc = machine(is_new)
    uc.mem_write(RX_STATE, bytes(initial))
    uc.mem_write(FRAME, bytes((0xCC,)) * 256)
    uc.mem_write(U3IIR, struct.pack("<I", 4))
    for value in data:
        uc.mem_write(U3RBR, bytes((value,)))
        call(uc, OLD["UART3_IRQHandler"] if not is_new else NEW["UART3_IRQHandler"])
    return bytes(uc.mem_read(RX_STATE, 3)), bytes(uc.mem_read(FRAME, 256)), snapshot(uc)


def test_uart_rx():
    print("== uart_rx_sequence ==")
    cases = [
        ("空闲态首字节后进入接收态", (0, 7, 19), [0xA5]),
        ("接收态连续8字节", (1, 9, 3), list(range(0x10, 0x18))),
        ("非接收态不改变缓冲区", (5, 11, 23), [0x55, 0xAA]),
        ("索引254跨越255并回绕", (1, 13, 254), [0x31, 0x32, 0x33]),
        ("索引255单步回绕到0", (1, 15, 255), [0x7E]),
    ]
    failed = 0
    for name, initial, data in cases:
        old = _uart_rx_run(False, initial, data)
        new = _uart_rx_run(True, initial, data)
        ok = old[0] == new[0] and old[1] == new[1] and old[2] == new[2]
        _check("%s A/B 等价" % name, ok, "state=%s" % old[0].hex())
        failed += not ok
    return failed


# ── 5) modbus_dispatch（读/写/异常/CRC 错/站址不匹配）──────────────────────
# 12p CRC16 表 @0x111D8/0x112D8（P5 确认；6p 为 0x11034/0x11134）
def crc16_fw(data, length):
    hi = ORIGINAL[0x111D8:0x111D8 + 256]
    lo = ORIGINAL[0x112D8:0x112D8 + 256]
    ch = 0xff
    cl = 0xff
    for i in range(length):
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]
    return (cl | (ch << 8)) & 0xffff


FRAME_BUF = 0x10002278       # 接收帧（dispatch 读取）
TXBUF = 0x10002340           # 发送缓冲
RX_LEN = 0x10001772
SLAVE_ADDR = 0x100016F7


def _dispatch_run(is_new, frame, slave=0x01, gain_b=0x56):
    uc = machine(is_new)
    tx_count = []
    i2c_writes = []

    def hook_tx(machine_, a, s, u):
        tx_count.append(machine_.reg_read(UC_ARM_REG_R0) & 0xff)
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    def hook_i2c(machine_, a, s, u):
        i2c_writes.append((machine_.reg_read(UC_ARM_REG_R1) & 0xFF,
                           machine_.reg_read(UC_ARM_REG_R0) & 0xFF))
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    tx_addr = NEW["uart3_tx_byte"] if is_new else OLD["uart3_tx_byte"]
    i2c_addr = NEW["i2c_write_reg"] if is_new else OLD["i2c_write_reg"]
    # 只注册本侧地址（见 _adc_scan_run 注释）
    uc.hook_add(UC_HOOK_CODE, hook_tx, begin=tx_addr, end=tx_addr + 1)
    uc.hook_add(UC_HOOK_CODE, hook_i2c, begin=i2c_addr, end=i2c_addr + 1)
    for i, b in enumerate(frame):
        uc.mem_write(FRAME_BUF + i, bytes((b,)))
    uc.mem_write(RX_LEN, bytes((len(frame),)))
    uc.mem_write(RX_STATE, bytes((5,)))
    uc.mem_write(SLAVE_ADDR, bytes((slave,)))
    uc.mem_write(0x10001634, struct.pack("<I", gain_b))
    try:
        call(uc, OLD["modbus_dispatch"] if not is_new else NEW["modbus_dispatch"],
             max_insn=2_000_000)
        n = tx_count[0] if tx_count else -1
        txb = bytes(uc.mem_read(TXBUF, max(n, 0)))
        return ("OK", n, txb, snapshot(uc), tuple(i2c_writes))
    except UcError as ex:
        return ("UC_ERR", ex.errno)


def test_modbus_dispatch():
    print("== modbus_dispatch ==")

    def framed(body):
        c = crc16_fw(body, len(body))
        return body + bytes((c & 0xff, c >> 8))

    G = 0x56
    # 场景1：合法读 站1 读 reg 0x1001 数量1 → 7 字节响应
    req = bytes((0x01, 0x03, 0x10, 0x01, 0x00, 0x01))
    frame_ok = framed(req)
    o = _dispatch_run(False, frame_ok, gain_b=G)
    n = _dispatch_run(True, frame_ok, gain_b=G)
    _check("读请求帧 CRC 自洽(0x0AD1)", crc16_fw(req, 6) == 0x0AD1,
           "0x%04X" % crc16_fw(req, 6))
    _check("读分支 原/新 各发 7 字节", o[1] == 7 and n[1] == 7, "原=%d 新=%d" % (o[1], n[1]))
    _check("读分支 A/B TXBUF 一致", o == n, "原=%s" % o[2].hex().upper())

    # 场景2：CRC 错帧 → 异常 5B [01 83 04]
    frame_bad = req + bytes((req[0],))  # 占位，实际构造坏 CRC
    frame_bad = req + bytes(((crc16_fw(req, 6) & 0xff) ^ 0xFF, crc16_fw(req, 6) >> 8))
    o = _dispatch_run(False, frame_bad, gain_b=G)
    n = _dispatch_run(True, frame_bad, gain_b=G)
    _check("CRC 错帧 → 异常 5B [01 83 04] 原", o[1] == 5 and o[2][:3] == bytes((1, 0x83, 4)),
           o[2].hex().upper())
    _check("CRC 错帧 → 异常 5B [01 83 04] 新", n[1] == 5 and n[2][:3] == bytes((1, 0x83, 4)),
           n[2].hex().upper())
    _check("CRC 错帧 A/B 一致", o == n)

    # 场景3：站址不匹配 → 不发
    o = _dispatch_run(False, frame_ok, slave=0x02, gain_b=G)
    n = _dispatch_run(True, frame_ok, slave=0x02, gain_b=G)
    _check("站址不匹配 原/新 不发送", o[1] == -1 and n[1] == -1, "原=%s 新=%s" % (o, n))
    _check("站址不匹配 A/B 一致", o == n)

    # 场景4：0x06 单写 reg 0x1001=2 → 8 字节回显 + param_sync/i2c
    req06 = bytes((0x01, 0x06, 0x10, 0x01, 0x00, 0x02))
    frame06 = framed(req06)
    o = _dispatch_run(False, frame06, gain_b=G)
    n = _dispatch_run(True, frame06, gain_b=G)
    _check("0x06 原/新 发 8 字节回显", o[1] == 8 and n[1] == 8, "原=%d 新=%d" % (o[1], n[1]))
    _check("0x06 完整帧 A/B 一致(i2c写%d条)" % len(o[4]), o == n,
           "原=%s" % o[2].hex().upper())

    # 场景5：0x10 多写 → 8 字节确认
    req10 = bytes((0x01, 0x10, 0x10, 0x01, 0x00, 0x01, 0x02, 0x00, 0x35))
    frame10 = framed(req10)
    o = _dispatch_run(False, frame10, gain_b=G)
    n = _dispatch_run(True, frame10, gain_b=G)
    _check("0x10 原/新 发 8 字节确认", o[1] == 8 and n[1] == 8, "原=%d 新=%d" % (o[1], n[1]))
    _check("0x10 完整帧 A/B 一致", o == n, "原=%s" % o[2].hex().upper())

    # 场景6：异常/边界/短帧 A/B 一致
    abnormal = [
        ("0x06 值越界", framed(bytes((1, 6, 0x10, 1, 0, 3)))),
        ("0x06 不存在寄存器", framed(bytes((1, 6, 0x10, 0x3F, 0, 1)))),
        ("0x10 数量为0", framed(bytes((1, 0x10, 0x10, 1, 0, 0, 0)))),
        ("0x10 数量超限", framed(bytes((1, 0x10, 0x10, 1, 0, 0x3F, 0x7E)))),
        ("0x10 字节数不匹配", framed(bytes((1, 0x10, 0x10, 1, 0, 2, 2, 0, 1)))),
        ("不支持功能码", framed(bytes((1, 4, 0x10, 1, 0, 1)))),
        ("1字节短帧", bytes((1,))),
        ("2字节短帧", bytes((1, 3))),
    ]
    for name, bad_frame in abnormal:
        o = _dispatch_run(False, bad_frame, gain_b=G)
        n = _dispatch_run(True, bad_frame, gain_b=G)
        _check("%s A/B结果一致" % name, o == n, "原=%s" % str(o))
    return 0


# ── 6) eeprom_sync_matrix（param_sync 逐字节/批量扰动）────────────────────
def _eeprom_run(is_new, mutations):
    uc = machine(is_new)
    for address, value in mutations:
        uc.mem_write(address, bytes((value & 0xFF,)))
    writes = []

    def write_hook(machine_, a, s, u):
        writes.append((machine_.reg_read(UC_ARM_REG_R1) & 0xFF,
                       machine_.reg_read(UC_ARM_REG_R0) & 0xFF))
        machine_.reg_write(UC_ARM_REG_PC, machine_.reg_read(UC_ARM_REG_LR))

    w_addr = NEW["i2c_write_reg"] if is_new else OLD["i2c_write_reg"]
    # 只注册本侧地址（见 _adc_scan_run 注释）
    uc.hook_add(UC_HOOK_CODE, write_hook, begin=w_addr, end=w_addr + 1)
    call(uc, OLD["param_sync_live_to_eeprom"] if not is_new else NEW["param_sync_live_to_eeprom"],
         max_insn=2_000_000)
    return bytes(uc.mem_read(0x10001000, 0x800)), tuple(writes)


def test_eeprom_sync():
    print("== eeprom_sync_matrix ==")
    image = (ROOT / "firmware" / "assets" / "ram_data_image.bin").read_bytes()
    cases = []
    for address in range(0x10001620, 0x10001730):
        old = image[address - 0x10000000]
        cases.append(("byte@0x%08X" % address, [(address, old ^ 0x5A)]))
    for seed in range(8):
        muts = []
        for i, address in enumerate(range(0x10001620, 0x10001730, 7)):
            muts.append((address, (image[address - 0x10000000] + seed * 29 + i + 1) & 0xFF))
        cases.append(("batch%d" % seed, muts))
    failed = 0
    for name, mutations in cases:
        old = _eeprom_run(False, mutations)
        new = _eeprom_run(True, mutations)
        if old != new:
            _check("%s A/B 等价" % name, False, "原写=%s 新写=%s" % (old[1][:8], new[1][:8]))
            failed += 1
    _check("EEPROM同步扰动矩阵 %d 例" % len(cases), failed == 0,
           "%d/%d" % (len(cases) - failed, len(cases)))
    return failed


# ── 7) interrupt_sequence（多 ISR 顺序执行，含 EINT）───────────────────────
ISR_RANGES = ((0x2009C000, 0x2009CFFF), (0x40008000, 0x40008FFF),
              (0x40004000, 0x40004FFF), (0x40090000, 0x40090FFF),
              (0x4009C000, 0x4009CFFF), (0x400FC000, 0x400FDFFF),
              (0xE000E000, 0xE000FFFF))


def _isr_seq_run(is_new, sequence):
    uc = machine(is_new)
    # 12p EINT/TIMER 相关种子（P5 已用 0x10001FD9/0x10001764 播种 T0）
    uc.mem_write(0x10001FE0, struct.pack("<I", 0))        # EINT3 过零计数
    uc.mem_write(0x1000204E, b"\x00")                      # EINT 状态
    uc.mem_write(0x1000204D, b"\x00")
    uc.mem_write(0x10002098, b"\x00")                      # EINT2_TICK
    uc.mem_write(0x10002099, b"\x00")                      # EINT3_TICK
    uc.mem_write(0x1000209A, b"\x00")                      # ADC_TICK
    uc.mem_write(0x10001FDA, b"\x00")                      # EINT3 计数
    uc.mem_write(0x10001FD9, b"\x50")                      # phase_cnt
    uc.mem_write(0x10001764, b"\x01")                      # tick_countdown
    uc.mem_write(0x10001770, bytes((0, 7, 0)))             # RX state/gap/idx
    for offset in range(0, 0xA0, 4):
        uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x13570000 ^ offset))
    trace = []
    callback = lambda machine_, access, address, size, value, user: trace.append(
        (address, size, value))
    for begin, end in ISR_RANGES:
        uc.hook_add(UC_HOOK_MEM_WRITE, callback, begin=begin, end=end)
    rx_count = 0
    for name in sequence:
        if name == "RX":
            uc.mem_write(U3IIR, struct.pack("<I", 4))
            uc.mem_write(U3RBR, bytes(((0xA5 + rx_count) & 0xFF,)))
            rx_count += 1
            name = "UART3_IRQHandler"     # RX 虚拟名 → 真 ISR
        uc.reg_write(UC_ARM_REG_LR, STOP + 1)
        run(uc, OLD[name] if not is_new else NEW[name], max_insn=2_000_000)
    return bytes(uc.mem_read(0x10000000, 0x2200)), trace


def test_interrupt_sequence():
    print("== interrupt_sequence ==")
    cases = [
        ("过零链正序", ("EINT1_IRQHandler", "TIMER2_IRQHandler", "TIMER1_IRQHandler",
                      "EINT2_IRQHandler", "EINT3_IRQHandler"), True),
        ("过零链逆序", ("EINT3_IRQHandler", "EINT2_IRQHandler", "TIMER1_IRQHandler",
                      "TIMER2_IRQHandler", "EINT1_IRQHandler"), False),
        ("节拍夹在触发中断之间", ("EINT1_IRQHandler", "TIMER0_IRQHandler", "TIMER2_IRQHandler",
                          "TIMER1_IRQHandler", "TIMER0_IRQHandler", "EINT3_IRQHandler"), True),
        ("UART与定时器交错", ("RX", "TIMER0_IRQHandler", "TIMER1_IRQHandler",
                        "RX", "TIMER2_IRQHandler", "TIMER0_IRQHandler"), False),
        ("重复完整序列", ("TIMER0_IRQHandler", "EINT1_IRQHandler", "EINT2_IRQHandler",
                      "EINT3_IRQHandler", "TIMER2_IRQHandler", "TIMER1_IRQHandler") * 4, True),
    ]
    failed = 0
    for name, sequence, strict_mmio in cases:
        old = _isr_seq_run(False, sequence)
        new = _isr_seq_run(True, sequence)
        ram_ok = old[0] == new[0]
        mmio_ok = old[1] == new[1]
        ok = ram_ok and (mmio_ok or not strict_mmio)
        scope = "RAM+MMIO" if strict_mmio else "RAM（MMIO模型受限）"
        _check("%s %s handlers=%d" % (name, scope, len(sequence)), ok,
               "writes=%d" % len(old[1]))
        if not ok and old[0] != new[0]:
            diff = next(i for i, (a, b) in enumerate(zip(old[0], new[0])) if a != b)
            print("    RAM首差异：0x%08X 原=0x%02X 新=0x%02X" % (
                0x10000000 + diff, old[0][diff], new[0][diff]))
        if not ok and old[1] != new[1]:
            pos = next((i for i, (a, b) in enumerate(zip(old[1], new[1])) if a != b),
                       min(len(old[1]), len(new[1])))
            print("    MMIO首差异：index=%d 原=%s 新=%s" % (
                pos, old[1][max(0, pos - 2):pos + 3], new[1][max(0, pos - 2):pos + 3]))
        failed += not ok
    return failed


# ── 8) control_multitick（状态机+输出级多拍）───────────────────────────────
def _multitick_prepare(is_new):
    uc = machine(is_new)
    disp = [OLD["disp_string"], OLD["disp_uint4"], OLD["disp_number3"],
            OLD["disp_clear"], OLD["disp_splash_screen"]]
    if is_new:
        disp = [NEW["disp_string"], NEW["disp_uint4"], NEW["disp_number3"],
                NEW["disp_clear"], NEW["disp_splash_screen"]]
    for address in disp:
        add_skip(uc, address)
    # 标准状态机播种（P5 验证）+ 输出级输入
    uc.mem_write(0x10001724, bytes((1, 0, 0)))             # MENU=1 MENU2=0 MENU3=0
    uc.mem_write(0x10001690, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    uc.mem_write(0x10001658, struct.pack("<I", 90))        # 起始相位
    uc.mem_write(0x10002051, b"\x09")                      # startup_count=9
    uc.mem_write(0x100015CE, b"\x00")                      # scr_set=0
    uc.mem_write(0x100016D5, b"\x01")                      # FAULT_DET_EN=1（故障刺激生效）
    uc.mem_write(0x1000161C, struct.pack("<I", 0))         # FAULT=0
    uc.mem_write(0x10001728, struct.pack("<I", 0))         # FAULT_CODE=0
    uc.mem_write(0x1000175E, b"\x00")                      # RUN=0
    uc.mem_write(0x1000175F, b"\x00")                      # STOP=0
    return uc


def _multitick_execute(is_new, stimuli):
    uc = _multitick_prepare(is_new)
    state = OLD["state_machine"] if not is_new else NEW["state_machine"]
    output = OLD["output_stage"] if not is_new else NEW["output_stage"]
    snapshots = []
    for key, run_flag, fault in stimuli:
        uc.mem_write(0x1000175E, bytes((run_flag & 0xFF,)))
        uc.mem_write(0x1000175F, bytes(((1 - (run_flag & 1)) & 0xFF,)))
        uc.mem_write(0x1000161C, struct.pack("<I", fault))
        uc.reg_write(UC_ARM_REG_R0, key)
        call(uc, state, max_insn=6_000_000)
        call(uc, output, max_insn=2_000_000)
        snapshots.append(bytes(uc.mem_read(0x10000000, 0x2200)))
    return snapshots


def test_control_multitick():
    print("== control_multitick ==")
    cases = [
        ("空闲连续30拍", [(0, 0, 0)] * 30),
        ("运行/停止与故障交错", [(0, i % 2, (0, 8, 0x10, 0x20)[i % 4]) for i in range(32)]),
        ("主界面按键序列", [((0, 1, 2, 3, 4, 5, 6, 0x16, 0x17, 0x21)[i % 10], 0, 0)
                          for i in range(30)]),
    ]
    failed = 0
    for name, stimuli in cases:
        old = _multitick_execute(False, stimuli)
        new = _multitick_execute(True, stimuli)
        ok = old == new
        _check("%s ticks=%d" % (name, len(stimuli)), ok)
        if not ok:
            tick = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
            diff = next(i for i, (a, b) in enumerate(zip(old[tick], new[tick])) if a != b)
            print("    首差异：tick=%d, addr=0x%08X, 原=0x%02X, 新=0x%02X" % (
                tick + 1, 0x10000000 + diff, old[tick][diff], new[tick][diff]))
        failed += not ok
    return failed


# ── 9) case3_edit（menu=3 编辑键 menu2=2..15，P5 只覆盖 0/1/3）──────────────
def _case3_run(is_new, item, key):
    uc = machine(is_new)
    # 标准播种（P5）：含全部参数、MENU 由 0x10001724 读取
    from verify_firmware_equivalence_12 import _seed_state_machine
    _seed_state_machine(uc, 3, item, 1)
    uc.mem_write(0x10001758, struct.pack("<I", 0xFB))      # TIMEOUT3
    uc.reg_write(UC_ARM_REG_R0, key)
    run(uc, OLD["state_machine"] if not is_new else NEW["state_machine"],
        max_insn=6_000_000)
    return snapshot(uc)


def test_case3_edit():
    print("== case3_edit ==")
    failed = 0
    for item in range(2, 16):
        for key in (2, 3, 0x16, 0x21):
            old = _case3_run(False, item, key)
            new = _case3_run(True, item, key)
            if old != new:
                diff = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
                _check("item%02d key=0x%X A/B 等价" % (item, key), False,
                       "首差异=0x%08X 原=0x%02X 新=0x%02X" % (
                           0x10000000 + diff, old[diff], new[diff]))
                failed += 1
            else:
                _check("item%02d key=0x%X A/B 等价" % (item, key), True)
    return failed


def main():
    global _passed, _failed
    failed = 0
    failed += test_adc_wait_done()
    failed += test_adc_scan()
    failed += test_input_scan()
    failed += test_uart_rx()
    failed += test_modbus_dispatch()
    failed += test_eeprom_sync()
    failed += test_interrupt_sequence()
    failed += test_control_multitick()
    failed += test_case3_edit()
    print()
    print("  通过 %d/%d" % (_passed, _passed + _failed))
    return 1 if (failed or _failed) else 0


if __name__ == "__main__":
    sys.exit(main())
