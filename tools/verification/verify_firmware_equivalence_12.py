# -*- coding: utf-8 -*-
"""12p A/B 等价性验证（PC12M-2）—— Unicorn 直接执行原 BIN vs 新 ELF 关键函数差分。

对照原固件 backup/pc12m2_orig.bin 与新编译 firmware/firmware.bin，逐函数：
  · 函数入口（12p 地址表，P5 阶段从反汇编/源码确认）
  · R0-R3 实参 → 返回值 + SRAM0 末态 + 外设/GPIO 写迹
  · 覆盖：向量表、状态机矩阵、输出级矩阵、TIMER0/1/2+WDT+UART3 ISR、
    Modbus 读写矩阵、CRC 矩阵、闭环 integral/wrapper、认证、去抖/扫描、
    显示矩阵（hook 跳过 disp 体）+ 显示全执行（真实渲染 GPIO 写迹）、继电器直连。

12p SRAM0 布局：data_image 0x10000000..0x10002110、.bss 清至 _estack 0x100029a0。
比对快照 = SRAM0 全量 0x10000000..0x100029a0 + 0x10003000..0x10003100 暂存区。

用法：在 PC12M-2 仓库根目录执行 `python tools/verification/verify_firmware_equivalence_12.py`
"""
from pathlib import Path
import re
import struct
import shutil
import subprocess

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_SP, UC_ARM_REG_R0,
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                               UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[2]
FW = ROOT / "firmware"
# 工具链定位：优先 PATH（CI/Linux 由 arm-none-eabi-gcc-action 注入），
# 找不到再回退本机 Windows 安装路径（本地开发机）。
_WIN_TC = Path(r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin")
NM = shutil.which("arm-none-eabi-nm") or str(_WIN_TC / "arm-none-eabi-nm.exe")
OBJDUMP = shutil.which("arm-none-eabi-objdump") or str(_WIN_TC / "arm-none-eabi-objdump.exe")

SRAM0_END = 0x29A0        # 0x10000000..0x100029a0（12p _estack）
SCRATCH = 0x10003000      # 0x10003000..0x10003100 暂存（crc/modbus 入出缓冲）
STOP = 0x3FF00


def symbols():
    output = subprocess.check_output([NM, "-n", FW / "firmware.elf"], text=True)
    return {m.group(2): int(m.group(1), 16) for line in output.splitlines()
            if (m := re.match(r"([0-9a-fA-F]+)\s+\w\s+(\S+)$", line))}


def sections():
    output = subprocess.check_output([OBJDUMP, "-h", FW / "firmware.elf"], text=True)
    result = {}
    for line in output.splitlines():
        m = re.match(r"\s*\d+\s+(\.\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            result[m.group(1)] = tuple(int(m.group(i), 16) for i in range(2, 5))
    return result


SYMS = symbols()
SECS = sections()
ORIGINAL = (ROOT / "backup" / "pc12m2_orig.bin").read_bytes()
NEW = (FW / "firmware.bin").read_bytes()

# ---- 12p OLD(BIN) 入口（P5 已确认）→ NEW(ELF) 符号名 ----
PAIRS = {
    "state_machine":       (0x4464, "state_machine"),
    "output_stage":        (0xe70c, "output_stage"),
    "run_stop_preset":     (0xf70a, "run_stop_preset"),
    "load_config":         (0x258c, "load_config"),
    "param_sync_live_to_eeprom": (0x3534, "param_sync_live_to_eeprom"),
    "disp_splash_screen":  (0x40b0, "disp_splash_screen"),
    "disp_screen_static":  (0x41b4, "disp_screen_static"),
    "disp_screen_calib":   (0x41ec, "disp_screen_calib"),
    "disp_string":         (0xcec,  "disp_string"),
    "disp_uint4":          (0xe80,  "disp_uint4"),
    "disp_number3":        (0xdf2,  "disp_number3"),
    "disp_clear":          (0x942,  "disp_clear"),
    "crc16":               (0xacd4, "crc16"),
    "modbus_read_reg":     (0xad04, "modbus_read_reg"),
    "modbus_write_multi":  (0xb050, "modbus_write_multi"),
    "closed_loop_integral":(0x10a9c,"closed_loop_integral"),
    "closed_loop_wrapper": (0x110f6,"closed_loop_wrapper"),
    "auth_set_timeout":    (0x108d2,"auth_set_timeout"),
    "auth_challenge":      (0x108dc,"auth_challenge"),
    "auth_verify_loop":    (0x10a38,"auth_verify_loop"),
    "uart3_tx_byte":       (0xab7c, "uart3_tx_byte"),
    "uart3_rx_timeout_monitor": (0xabc0, "uart3_rx_timeout_monitor"),
    "UART3_IRQHandler":    (0xac78, "UART3_IRQHandler"),
    "pin_config":          (0xe308, "pin_config"),
    "gpio2_init":          (0x10888,"gpio2_init"),
    "TIMER0_IRQHandler":   (0x29a,  "TIMER0_IRQHandler"),
    "TIMER1_IRQHandler":   (0xfb0c, "TIMER1_IRQHandler"),
    "TIMER2_IRQHandler":   (0xfae8, "TIMER2_IRQHandler"),
    "WDT_IRQHandler":      (0x1e4,  "WDT_IRQHandler"),
    "out_relay_p021":      (0x107e0,"out_relay_p021"),
    "fio1_pin20_ctrl":     (0x10800,"fio1_pin20_ctrl"),
    "fio1_pin21_ctrl":     (0x10820,"fio1_pin21_ctrl"),
    "fio1_pin23_ctrl":     (0x10840,"fio1_pin23_ctrl"),
    "fio0_pin22_ctrl":     (0xe6c6, "fio0_pin22_ctrl"),
    "fio1_pin22_ctrl":     (0xe6a6, "fio1_pin22_ctrl"),
    "gpio_outputs_set":    (0xe4fa, "gpio_outputs_set"),
    "scan_run_stop":       (0x1976, "scan_run_stop"),
    "debounce_p09":        (0x1a68, "debounce_p09"),
    "debounce_p116":       (0x1a96, "debounce_p116"),
    "debounce_p117":       (0x1aee, "debounce_p117"),
    "debounce_p06":        (0x1b46, "debounce_p06"),
}


def machine(new: bool):
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    for base, size in ((0, 0x40000), (0x10000000, 0x10000), (0x2007C000, 0x4000),
                       (0x2009C000, 0x1000), (0x40000000, 0x1000),
                       (0x40004000, 0x1000), (0x40008000, 0x1000),
                       (0x40090000, 0x1000), (0x4009C000, 0x1000),
                       (0x400FC000, 0x2000), (0xE000E000, 0x2000)):
        uc.mem_map(base, size)
    uc.mem_write(0, NEW if new else ORIGINAL)
    image = (ROOT / "firmware/assets/ram_data_image.bin").read_bytes()
    uc.mem_write(0x10000000, image)
    if new:
        data_size, data_vma, data_lma = SECS[".data"]
        uc.mem_write(data_vma, NEW[data_lma:data_lma + data_size])
    uc.reg_write(UC_ARM_REG_SP, 0x10007000)
    uc.reg_write(UC_ARM_REG_LR, STOP + 1)
    return uc


def run(uc, entry, writes=None, max_insn=2_000_000):
    def stop_at_return(machine, address, size, user):
        if address == STOP:
            machine.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop_at_return)
    if writes is not None:
        uc.hook_add(UC_HOOK_MEM_WRITE,
                    lambda machine, access, address, size, value, user: writes.append((address, size, value)),
                    begin=0x2009C000, end=0x2009CFFF)
    uc.emu_start(entry | 1, 0, count=max_insn)


def snapshot(uc):
    return (bytes(uc.mem_read(0x10000000, SRAM0_END))
            + bytes(uc.mem_read(SCRATCH, 0x100)))


def execute_pair(name, args=(), setup=None, max_insn=2_000_000):
    old, new = PAIRS[name]
    results = []
    regs = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
    for is_new, entry in ((False, old), (True, SYMS[new])):
        uc = machine(is_new)
        if setup:
            setup(uc)
        for reg, value in zip(regs, args):
            uc.reg_write(reg, value & 0xFFFFFFFF)
        run(uc, entry, max_insn=max_insn)
        results.append((uc.reg_read(UC_ARM_REG_R0), snapshot(uc)))
    return results


def verify_vector():
    words = struct.unpack_from("<8I", NEW)
    assert sum(words) & 0xFFFFFFFF == 0, "向量校验和 ≠ 0"
    assert words[0] == 0x100029A0, "复位 SP ≠ 0x100029a0（12p _estack）"
    assert struct.unpack_from("<I", NEW, 0x2FC)[0] == 0xFFFFFFFF, "CRP ≠ 0xFFFFFFFF"
    print("VECTOR: PASS (SP=0x%08x, checksum=0, CRP=0xFFFFFFFF)" % words[0])


# ── GPIO 叶函数直连（写迹 A/B）──────────────────────────────────────────────
def verify_gpio_trace(name, label):
    old, new = PAIRS[name]
    traces = []
    for is_new, entry in ((False, old), (True, SYMS[new])):
        uc = machine(is_new)
        for offset in range(0, 0xA0, 4):
            uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x01010101 ^ offset))
        trace = []
        run(uc, entry, trace)
        traces.append(trace)
    assert traces[0] == traces[1], "%s MMIO trace mismatch: %d/%d" % (
        label, len(traces[0]), len(traces[1]))
    print("%s: PASS writes=%d" % (label, len(traces[0])))


def verify_gpio_pairs():
    for name, label in (("pin_config", "PIN_CONFIG"), ("gpio2_init", "GPIO2_INIT"),
                        ("auth_challenge", "AUTH_CHALLENGE"),
                        ("gpio_outputs_set", "GPIO_OUTPUTS_SET"),
                        ("fio0_pin22_ctrl", "FIO0_P22"), ("fio1_pin22_ctrl", "FIO1_P22")):
        verify_gpio_trace(name, label)


# ── 继电器直连（含 P1.20/P1.21/P1.23，隔离调用；用 until=STOP 防 code-hook 漏停）─
# fio1_pin20/21/23_ctrl 与 out_relay_p021 都是 tiny 叶函数，code-hook 会漏停穿到
# 0x40000 Flash 边界（6p 同款 Unicorn 行为），故统一走 emu_start(until)。
def verify_relay_direct():
    for name in ("out_relay_p021", "fio1_pin20_ctrl", "fio1_pin21_ctrl", "fio1_pin23_ctrl"):
        old, new = PAIRS[name]
        for level in (0, 1):
            traces = []
            for is_new, entry in ((False, old), (True, SYMS[new])):
                uc = machine(is_new)
                trace = []
                cb = lambda machine, access, address, size, value, user, t=trace: t.append((address, value))
                uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=0x2009C000, end=0x2009CFFF)
                uc.reg_write(UC_ARM_REG_R0, level)
                uc.emu_start(entry | 1, STOP, count=500)
                traces.append(trace)
            assert traces[0] == traces[1], "relay A/B mismatch %s(level=%d)" % (name, level)
    print("RELAY_DIRECT: PASS funcs=4 levels=2")


# ── ISR 外设写迹（SRAM0 全量末态）─────────────────────────────────────────────
ISR_RANGES = {
    "TIMER0_IRQHandler": ((0x40004000, 0x40004FFF),),
    "TIMER1_IRQHandler": ((0x40008000, 0x40008FFF), (0x2009C000, 0x2009CFFF)),
    "TIMER2_IRQHandler": ((0x40090000, 0x40090FFF), (0x40008000, 0x40008FFF)),
    "WDT_IRQHandler":    ((0x400FC000, 0x400FDFFF),),
    "UART3_IRQHandler":  ((0x4009C000, 0x4009CFFF), (0x2009C000, 0x2009CFFF)),
}


def verify_isr(name, setup=None):
    old, new = PAIRS[name]
    traces, states = [], []
    for is_new, entry in ((False, old), (True, SYMS[new])):
        uc = machine(is_new)
        if setup:
            setup(uc)
        trace = []
        cb = lambda machine, access, address, size, value, user: trace.append((address, size, value))
        for begin, end in ISR_RANGES[name]:
            uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=begin, end=end)
        run(uc, entry, max_insn=2_000_000)
        traces.append(trace)
        states.append(snapshot(uc))
    assert traces[0] == traces[1], "%s peripheral trace mismatch: %d/%d" % (
        name, len(traces[0]), len(traces[1]))
    assert states[0] == states[1], "%s SRAM mismatch" % name
    print("%s: PASS writes=%d" % (name, len(traces[0])))


def verify_isrs():
    # TIMER0：phase_cnt 非边界（0x50）+ 越过钳位（0xc9）、tick_countdown=1
    def s_t0(uc):
        uc.mem_write(0x10001FD9, b"\x50")
        uc.mem_write(0x10001764, b"\x01")
    verify_isr("TIMER0_IRQHandler", s_t0)
    verify_isr("TIMER2_IRQHandler")
    verify_isr("WDT_IRQHandler")
    # UART3：RX（IIR=4，RBR 有数据）与 THRE（IIR=2，发完/未发完）两路径
    for iir, rbr in ((4, 0xA5), (2, 0xA5)):
        def s_u3(uc, iir=iir, rbr=rbr):
            uc.mem_write(0x10001770, bytes((1, 0, 3)))   # comm_state=1 gap=0 idx=3
            uc.mem_write(0x10001773, b"\x06")            # comm_tx_data(len)=6
            uc.mem_write(0x10001774, b"\x02")            # comm_tx_param=2
            uc.mem_write(0x10002340, b"ABCDEF")          # comm_frame_buf(TX)
            uc.mem_write(0x10002278, b"XYZ\x00" * 4)     # lookup_table(RX buf)
            uc.mem_write(0x4009C008, struct.pack("<I", iir))
            uc.mem_write(0x4009C000, struct.pack("<I", rbr))
        verify_isr("UART3_IRQHandler", s_u3)


# ── TIMER1 触发矩阵（12 段窗口 + 50/60Hz + 方向）──────────────────────────────
def verify_timer1_matrix():
    cases = 0
    scans = list(range(0, 0xf1, 0x14)) + [0xf1, 0x14, 0x15, 0x3c, 0x3d]
    for sc in scans:
        for fh in (0x32, 0x3c, 0x00):
            for tp in (0x00, 0x64, 0xc8):
                for c2 in (0x00, 0x64, 0x65):
                    for mb in (0x00, 0x01):
                        traces, states = [], []
                        for is_new, entry in ((False, PAIRS["TIMER1_IRQHandler"][0]),
                                              (True, SYMS["TIMER1_IRQHandler"])):
                            uc = machine(is_new)
                            uc.mem_write(0x1000204C, bytes((sc,)))    # step_counter
                            uc.mem_write(0x10001FD8, bytes((fh,)))    # freq_hz
                            uc.mem_write(0x1000168C, bytes((tp,)))    # trig_phase
                            uc.mem_write(0x1000164C, bytes((c2,)))    # counter2
                            uc.mem_write(0x1000204D, bytes((mb,)))    # mode_byte
                            uc.mem_write(0x10002050, bytes((sc & 1,)))  # trig_dir 初始
                            for offset in range(0, 0xA0, 4):
                                uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x13570000 ^ offset))
                            trace = []
                            cb = lambda machine, access, address, size, value, user: trace.append((address, size, value))
                            for begin, end in ISR_RANGES["TIMER1_IRQHandler"]:
                                uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=begin, end=end)
                            run(uc, entry, max_insn=2_000_000)
                            traces.append(trace)
                            states.append(snapshot(uc))
                        label = (sc, fh, tp, c2, mb)
                        assert traces[0] == traces[1], "TIMER1 trace mismatch %r" % (label,)
                        assert states[0] == states[1], "TIMER1 SRAM mismatch %r" % (label,)
                        cases += 1
    print("TIMER1_MATRIX: PASS cases=%d" % cases)


# ── CRC 矩阵 ─────────────────────────────────────────────────────────────────
def verify_crc_matrix():
    vectors = (b"", b"\x00", b"\x01\x03\x00\x00\x00\x0a", bytes(range(32)),
               bytes((i * 73 + 19) & 0xFF for i in range(255)))
    for payload in vectors:
        def setup(uc, data=payload):
            uc.mem_write(SCRATCH, data or b"\x00")
        results = execute_pair("crc16", (SCRATCH, len(payload)), setup)
        assert results[0][0] == results[1][0], "CRC mismatch len=%d" % len(payload)
    print("CRC_MATRIX: PASS cases=%d" % len(vectors))


# ── Modbus 读写矩阵 ──────────────────────────────────────────────────────────
def verify_modbus_regs():
    read_cases = 0
    for reg in range(65):
        def setup(uc):
            uc.mem_write(SCRATCH, b"\xA5\x5A\xC3\x3C")
        results = execute_pair("modbus_read_reg", (SCRATCH, reg), setup)
        assert results[0][0] == results[1][0], "read return mismatch reg=%d" % reg
        assert results[0][1] == results[1][1], "read RAM mismatch reg=%d" % reg
        read_cases += 1

    write_cases = 0
    for reg in range(64):
        for value in (0, 1, 0x55, 0x1234, 0xFFFF):
            def setup(uc, v=value):
                uc.mem_write(SCRATCH, struct.pack("<I", v))
            results = execute_pair("modbus_write_multi", (SCRATCH, reg), setup)
            assert results[0][0] == results[1][0], "write return mismatch reg=%d value=%d" % (reg, value)
            assert results[0][1] == results[1][1], "write RAM mismatch reg=%d value=%d" % (reg, value)
            write_cases += 1
    print("MODBUS_REGS: PASS read=%d write=%d" % (read_cases, write_cases))


# ── 闭环 integral + wrapper ──────────────────────────────────────────────────
def verify_closed_loop():
    cases = ((0, 0, 1, 1), (100, 90, 2, 3), (90, 100, 2, 3),
             (0x7FFFFFFF, 1, 7, 11), (1, 0x7FFFFFFF, 7, 11),
             (5000, 4999, 0x100, 0x200))
    for args in cases:
        results = execute_pair("closed_loop_integral", args, max_insn=500_000)
        assert results[0][0] == results[1][0], "closed-loop return mismatch %r" % (args,)
        assert results[0][1] == results[1][1], "closed-loop RAM mismatch %r" % (args,)
    # wrapper：重算计数 0x100020CC=0（触发重算+缓存）与 =7（直接返回缓存 0x10002104）
    for cnt in (0, 7):
        def setup(uc, c=cnt):
            uc.mem_write(0x100020CC, struct.pack("<I", c))
            uc.mem_write(0x10002104, struct.pack("<I", 0xDEAD))
        for args in cases:
            results = execute_pair("closed_loop_wrapper", args, setup, max_insn=500_000)
            assert results[0][0] == results[1][0], "wrapper return mismatch cnt=%d %r" % (cnt, args)
            assert results[0][1] == results[1][1], "wrapper RAM mismatch cnt=%d %r" % (cnt, args)
    print("CLOSED_LOOP: PASS integral=%d wrapper=%d" % (len(cases), 2 * len(cases)))


# ── 输出级矩阵（cfg/gain_sel/input_locked/保护开关/CH 值）─────────────────────
def _seed_output_stage(uc, cfg, gain_sel, locked, th_on, ch3, ch4, ch5):
    uc.mem_write(0x100015CE, b"\x00")                 # scr_set=0（运行）
    uc.mem_write(0x10002051, b"\x09")                 # startup_count=9 → ++=10 进主体
    uc.mem_write(0x10001620, bytes((cfg,)))           # cfg_word
    uc.mem_write(0x1000162C, bytes((gain_sel,)))      # gain_sel
    uc.mem_write(0x10001FE0, struct.pack("<I", locked))  # input_locked
    uc.mem_write(0x1000161C, struct.pack("<I", 0))    # out_param
    uc.mem_write(0x10001658, struct.pack("<I", 0x5A)) # ulim_angle(→softstart 0x5A)
    uc.mem_write(0x10001648, struct.pack("<I", 0x5A)) # llim_angle(→softstop 0x5A)
    uc.mem_write(0x10001630, struct.pack("<I", 0x64)) # gain_a
    uc.mem_write(0x10001634, struct.pack("<I", 0x32)) # gain_b
    uc.mem_write(0x1000163C, struct.pack("<I", 0x14)) # gain_c
    uc.mem_write(0x100015A8, struct.pack("<I", ch3))  # adc_conv_ch3
    uc.mem_write(0x100015B8, struct.pack("<I", 0x28)) # adc_conv_fb
    uc.mem_write(0x100015BC, struct.pack("<I", 0x1E)) # adc_conv_aux1
    uc.mem_write(0x100015B4, struct.pack("<I", 0x1E)) # adc_conv_aux2
    uc.mem_write(0x100015B0, struct.pack("<I", 0))    # DAT_0000f008 watchdog
    uc.mem_write(0x10002054, struct.pack("<I", 0))    # alarm_timer
    uc.mem_write(0x10002058, b"\x00")                 # alarm_flag
    uc.mem_write(0x10002000, struct.pack("<I", 0))    # pid_state
    uc.mem_write(0x10002004, struct.pack("<I", 0))    # pid_active
    uc.mem_write(0x10002074, struct.pack("<I", 0))    # pid_watchdog
    uc.mem_write(0x100015D0, struct.pack("<I", 0x64)) # pid_target_set
    uc.mem_write(0x10001FFC, struct.pack("<I", 0))    # pid_feedback
    uc.mem_write(0x10002030, struct.pack("<I", 0x5DC))# startup_timer
    uc.mem_write(0x10001644, struct.pack("<I", 2))    # startup_div
    uc.mem_write(0x1000201C, b"\x00")                 # ramp_phase
    uc.mem_write(0x1000201D, b"\x00")                 # ramp_state
    uc.mem_write(0x10002040, struct.pack("<I", 0))    # trigger_step
    uc.mem_write(0x10002044, struct.pack("<I", 0))    # trigger_accum
    uc.mem_write(0x10002048, struct.pack("<I", 0))    # trigger_angle
    uc.mem_write(0x10001706, b"\x05")                 # pid_kp2
    uc.mem_write(0x10001707, b"\x03")                 # pid_ki2
    uc.mem_write(0x10001651, b"\x01")                 # eeprom_param_3
    uc.mem_write(0x10001762, b"\x00")                 # menu_flag_5
    # 保护阈值 + CH 输入
    for addr, val in ((0x100016B8, 100), (0x100016C0, 50), (0x100016C8, 80)):
        uc.mem_write(addr, struct.pack("<I", val if th_on else 0))
    for addr, val in ((0x100016BC, 2), (0x100016C4, 2), (0x100016CC, 2)):
        uc.mem_write(addr, bytes((val if th_on else 0,)))
    uc.mem_write(0x10002080, struct.pack("<I", 0))    # ov_count_1
    uc.mem_write(0x10002084, struct.pack("<I", 0))    # ov_count_2
    uc.mem_write(0x10002094, struct.pack("<I", 0))    # ov_count_3
    uc.mem_write(0x10001590, struct.pack("<I", ch5))  # adc_conv_ch5
    uc.mem_write(0x10001594, struct.pack("<I", ch4))  # adc_conv_ch4


def verify_output_stage_matrix():
    cases = 0
    ranges = ((0x2009C000, 0x2009CFFF), (0x40008000, 0x40008FFF),
              (0x40090000, 0x40090FFF), (0x400FC000, 0x400FDFFF))
    for cfg in (0, 1):
        for gain_sel in (0, 1, 2):
            for locked in (0, 4, 5):
                for th_on in (0, 1):
                    ch3 = 100 if cfg else 0
                    ch4 = 120 if th_on else 0
                    ch5 = 130 if th_on else 0
                    traces, states = [], []
                    for is_new, entry in ((False, PAIRS["output_stage"][0]),
                                          (True, SYMS["output_stage"])):
                        uc = machine(is_new)
                        _seed_output_stage(uc, cfg, gain_sel, locked, th_on, ch3, ch4, ch5)
                        for offset in range(0, 0xA0, 4):
                            uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x13570000 ^ offset))
                        trace = []
                        cb = lambda machine, access, address, size, value, user: trace.append((address, size, value))
                        for begin, end in ranges:
                            uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=begin, end=end)
                        run(uc, entry, max_insn=2_000_000)
                        traces.append(trace)
                        states.append(snapshot(uc))
                    label = (cfg, gain_sel, locked, th_on)
                    assert traces[0] == traces[1], "output_stage peripheral mismatch %r" % (label,)
                    assert states[0] == states[1], "output_stage SRAM mismatch %r" % (label,)
                    cases += 1
    print("OUTPUT_STAGE_MATRIX: PASS cases=%d" % cases)


def verify_run_stop_preset():
    for cfg in (0, 1):
        def setup(uc, c=cfg):
            uc.mem_write(0x10001620, bytes((c,)))
            uc.mem_write(0x100017D8, struct.pack("<I", 0x3F))       # protocol_work_3[0]=63
            uc.mem_write(0x100017D8 + 0xFA * 4, struct.pack("<I", 0x23))  # [0xfa]=35
            uc.mem_write(0x10001FDC, struct.pack("<I", 0))          # out_setpoint
            uc.mem_write(0x10001FE0, struct.pack("<I", 0))          # input_locked
        results = execute_pair("run_stop_preset", (), setup)
        # run_stop_preset 为 void 函数（OLD main 0x6B6 调后即 bl 0x238 覆盖 R0，不消费）。
        # OLD 的 R0 是 IAR 残留 cfg_word（cfg==1→1），NEW 是 GCC 乘法常数残留（0x64），
        # 均无契约意义，故只断言 SRAM 末态。
        assert results[0][1] == results[1][1], "run_stop_preset RAM mismatch cfg=%d" % cfg
    print("RUN_STOP_PRESET: PASS cfg=2")


# ── 状态机矩阵（SRAM0 末态，key 经 R0）────────────────────────────────────────
def _seed_state_machine(uc, menu, menu2, menu3):
    uc.mem_write(0x10001724, bytes((menu, menu2, menu3)))   # MENU/MENU2/MENU3
    # case63 item0-4（word）
    uc.mem_write(0x10001690, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    # case63 item5-9（byte，ESTOP/RESET_MODE/item7..9）
    uc.mem_write(0x1000164F, bytes((1, 2, 0, 0, 1)))
    uc.mem_write(0x10001658, struct.pack("<I", 90))        # case63 item0xa 相位
    # case4 item0-9
    uc.mem_write(0x100016B8, struct.pack("<I", 800))
    uc.mem_write(0x100016BC, bytes((0x64,)))
    uc.mem_write(0x100016C0, struct.pack("<I", 0))
    uc.mem_write(0x100016C4, bytes((0x64,)))
    uc.mem_write(0x100016C8, struct.pack("<I", 900))
    uc.mem_write(0x100016CC, bytes((0x64,)))
    uc.mem_write(0x100016D0, struct.pack("<I", 0))
    uc.mem_write(0x100016D4, bytes((0x64,)))
    uc.mem_write(0x100016D5, bytes((1, 0x2a)))             # 缺相/三相平衡
    # prelude 屏蔽（FAULT=0、CFG=0、FAULT_DET_EN=0、周期=0）
    uc.mem_write(0x1000161C, struct.pack("<I", 0))         # FAULT
    uc.mem_write(0x10001728, struct.pack("<I", 0))         # FAULT_CODE
    uc.mem_write(0x10001620, b"\x00")                      # CFG=0
    uc.mem_write(0x100016D5, b"\x00")                      # FAULT_DET_EN=0
    uc.mem_write(0x10001730, struct.pack("<I", 0))         # FAULT_CHK_CYCLE
    uc.mem_write(0x10002098, b"\x00")                      # EINT2_TICK
    uc.mem_write(0x10002099, b"\x00")                      # EINT3_TICK
    uc.mem_write(0x1000209A, b"\x00")                      # ADC_TICK
    uc.mem_write(0x10001734, struct.pack("<I", 0))         # MISS_EINT2
    uc.mem_write(0x10001738, struct.pack("<I", 0))         # MISS_EINT3
    uc.mem_write(0x1000173C, struct.pack("<I", 0))         # MISS_ADC
    # 超时/页面节流
    uc.mem_write(0x10001744, struct.pack("<I", 0))         # TIMEOUT
    uc.mem_write(0x10001740, struct.pack("<I", 0))         # TIMEOUT2
    uc.mem_write(0x10001758, struct.pack("<I", 0))         # TIMEOUT3
    uc.mem_write(0x10001748, struct.pack("<I", 0))         # IDLE
    uc.mem_write(0x10001750, struct.pack("<I", 0))         # LCD_TOUT
    # 运行门控
    uc.mem_write(0x1000175E, b"\x00")                      # RUN
    uc.mem_write(0x1000175F, b"\x00")                      # STOP
    uc.mem_write(0x10001765, b"\x00")                      # RUN_EN
    uc.mem_write(0x10001727, b"\x00")                      # SCAN_RS
    # 首页显示源（case1）
    uc.mem_write(0x100015CC, b"\x01")                      # STATUS=1(停)
    uc.mem_write(0x1000162C, b"\x01")                      # CTRL_MODE=1
    uc.mem_write(0x1000164D, b"\x01")                      # DISP_SEL
    uc.mem_write(0x1000164E, b"\x00")                      # PAIR_MODE
    uc.mem_write(0x100015D8, struct.pack("<I", 90))        # MANUAL
    uc.mem_write(0x100015DC, struct.pack("<I", 90))        # MANUAL_SH
    uc.mem_write(0x100015A8, struct.pack("<I", 100))       # TARGET
    uc.mem_write(0x10001768, struct.pack("<I", 100))       # HSRC
    uc.mem_write(0x10001624, struct.pack("<I", 0))         # PHASE_OFF
    uc.mem_write(0x1000168C, b"\x00")                      # BAL_ANG
    # 运行统计（非边界，避免进位干扰）
    uc.mem_write(0x10001600, struct.pack("<I", 500))       # TICK
    uc.mem_write(0x100015F8, struct.pack("<I", 1))         # HOUR_NOW
    uc.mem_write(0x100015FC, struct.pack("<I", 30))        # MIN_NOW
    uc.mem_write(0x10001604, struct.pack("<I", 2))         # HOUR_TOTAL
    uc.mem_write(0x10001608, struct.pack("<I", 45))        # MIN_TOTAL


def verify_state_machine_matrix():
    cases = []
    for key in (0, 1, 2, 3, 4, 5, 6, 0x16, 0x17, 0x21):
        cases.append((1, 0, 0, key))
    for menu in (2, 3, 4, 5, 7, 0x14, 0x1E, 0x63):
        for menu2 in (0, 1, 3):
            for menu3, key in ((0, 0), (0, 2), (0, 3), (1, 2), (1, 3)):
                cases.append((menu, menu2, menu3, key))
    for menu, menu2, menu3, key in cases:
        states = []
        for is_new, entry in ((False, PAIRS["state_machine"][0]),
                              (True, SYMS["state_machine"])):
            uc = machine(is_new)
            _seed_state_machine(uc, menu, menu2, menu3)
            uc.reg_write(UC_ARM_REG_R0, key)
            run(uc, entry, max_insn=6_000_000)
            states.append(snapshot(uc))
        label = (menu, menu2, menu3, key)
        diffs = [(0x10000000 + i, a, b) for i, (a, b) in
                 enumerate(zip(states[0], states[1])) if a != b]
        assert not diffs, "state_machine SRAM mismatch %r: %r" % (label, diffs[:8])
    print("STATE_MACHINE_MATRIX: PASS cases=%d" % len(cases))


# ── 显示矩阵（hook 跳过 disp 体，捕获调用序列）───────────────────────────────
BIN_DISP = {
    "disp_string": 0x0cec, "disp_uint4": 0x0e80, "disp_number3": 0x0df2,
    "disp_clear": 0x0942, "param_sync_live_to_eeprom": 0x3534,
    "disp_splash_screen": 0x40b0,
}
ARGLESS_DISP = {"disp_clear", "param_sync_live_to_eeprom", "disp_splash_screen"}


def _seed_display_items(uc):
    # case63：item0-4 数值、item5-9 枚举、item0xa 相位
    uc.mem_write(0x10001690, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    uc.mem_write(0x1000164F, bytes((1, 2, 0, 0, 1)))     # ESTOP/RESET_MODE/item7-9
    uc.mem_write(0x10001658, struct.pack("<I", 90))      # 起始相位
    # case4：word 0/2/4/6、byte 1/3/5/7/8/9
    uc.mem_write(0x100016B8, struct.pack("<I", 800))
    uc.mem_write(0x100016BC, bytes((0x64,)))
    uc.mem_write(0x100016C0, struct.pack("<I", 0))
    uc.mem_write(0x100016C4, bytes((0x64,)))
    uc.mem_write(0x100016C8, struct.pack("<I", 900))
    uc.mem_write(0x100016CC, bytes((0x64,)))
    uc.mem_write(0x100016D0, struct.pack("<I", 0))
    uc.mem_write(0x100016D4, bytes((0x64,)))
    uc.mem_write(0x100016D5, bytes((1, 0x2a)))           # 缺相/三相平衡


def verify_display_matrix():
    cases = []
    for menu, n in ((0x63, 0xb), (0x04, 0xa)):
        for menu2 in range(n):
            for menu3 in (0, 1):
                for t3 in (0xfa, 0x1f4):
                    cases.append((menu, menu2, menu3, t3, 0))
    for menu2 in range(0xb):                             # case63 编辑按键
        for key in (2, 3):
            cases.append((0x63, menu2, 1, 0, key))

    new_map = {name: SYMS[name] for name in BIN_DISP}
    for menu, menu2, menu3, t3, key in cases:
        seqs, states = [], []
        for is_new, entry in ((False, PAIRS["state_machine"][0]),
                              (True, SYMS["state_machine"])):
            uc = machine(is_new)
            uc.mem_write(0x10001724, bytes((menu, menu2, menu3)))
            uc.mem_write(0x10001758, struct.pack("<I", t3))
            _seed_display_items(uc)
            seq = []
            for name, addr in (BIN_DISP if not is_new else new_map).items():
                def hook(uc, address, size, user, nm=name):
                    if nm in ARGLESS_DISP:
                        seq.append(nm)
                    else:
                        seq.append((nm,
                                    uc.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
                                    uc.reg_read(UC_ARM_REG_R1) & 0xFF,
                                    uc.reg_read(UC_ARM_REG_R2) & 0xFF,
                                    uc.reg_read(UC_ARM_REG_R3) & 0xFF))
                    uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
                uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr + 1)
            run(uc, entry, max_insn=2_000_000)
            seqs.append(seq)
            states.append(snapshot(uc))
        label = (hex(menu), menu2, menu3, hex(t3), key)
        assert seqs[0] == seqs[1], "display seq mismatch %r: %r vs %r" % (
            label, seqs[0][:14], seqs[1][:14])
        assert states[0] == states[1], "display SRAM mismatch %r" % (label,)
    print("DISPLAY_MATRIX: PASS cases=%d" % len(cases))


def _mask_fio1_p23(trace):
    """掩掉 FIO1 P1.23(LED) 位——Unicorn mem-hook 翻译缓存 bug 的规避（同 6p），
    该位由 RELAY_DIRECT 单独 A/B 覆盖；LCD 像素位(P1.25+)不受影响。"""
    out = []
    for address, size, value in trace:
        if address in (0x2009C038, 0x2009C03C):          # FIO1SET / FIO1CLR
            value &= ~0x800000
            if value == 0:
                continue
        out.append((address, size, value))
    return out


def verify_display_full_exec():
    """真实执行 disp_*（strpool_map + 字符渲染），比对 GPIO 写迹 + SRAM。"""
    for menu, menu2, menu3, t3 in ((0x63, 5, 1, 0xfa), (0x63, 0xa, 1, 0x1f4),
                                   (0x04, 0, 1, 0x1f4), (0x04, 5, 1, 0xfa)):
        traces, states = [], []
        for is_new, entry in ((False, PAIRS["state_machine"][0]),
                              (True, SYMS["state_machine"])):
            uc = machine(is_new)
            uc.mem_write(0x10001724, bytes((menu, menu2, menu3)))
            uc.mem_write(0x10001758, struct.pack("<I", t3))
            _seed_display_items(uc)
            trace = []
            cb = lambda machine, access, address, size, value, user, t=trace: t.append((address, size, value))
            uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=0x2009C000, end=0x2009CFFF)
            run(uc, entry, max_insn=4_000_000)
            traces.append(trace)
            states.append(snapshot(uc))
        label = (hex(menu), menu2, menu3, hex(t3))
        assert _mask_fio1_p23(traces[0]) == _mask_fio1_p23(traces[1]), \
            "disp full-exec GPIO mismatch %r: %d vs %d" % (label, len(traces[0]), len(traces[1]))
        assert states[0] == states[1], "disp full-exec SRAM mismatch %r" % (label,)
    print("DISPLAY_FULL_EXEC: PASS cases=4")


# ── 去抖/扫描（FIO 输入 + 计数种子）──────────────────────────────────────────
def verify_debounce():
    cases = []
    # (name, FIO 输入地址, 输入位, 计数地址, 阈值, 输入电平, 计数初值, 期望返回)
    cases.append(("debounce_p09", 0x2009C014, 0x200, 0x1000157B, 0xf, 0, 0, 0))
    cases.append(("debounce_p09", 0x2009C014, 0x200, 0x1000157B, 0xf, 0x200, 0, 0))
    cases.append(("debounce_p09", 0x2009C014, 0x200, 0x1000157B, 0xf, 0x200, 0xf, 1))
    cases.append(("debounce_p06", 0x2009C014, 0x40,  0x1000157C, 0x32, 0x40, 0x31, 1))
    cases.append(("debounce_p06", 0x2009C014, 0x40,  0x1000157D, 0x32, 0x00, 0x31, 2))
    cases.append(("debounce_p116", 0x2009C034, 0x10000, 0x1000157E, 0xfa, 0x10000, 0xfa, 1))
    cases.append(("debounce_p116", 0x2009C034, 0x10000, 0x1000157F, 0xfa, 0x00000, 0xfa, 2))
    cases.append(("debounce_p117", 0x2009C034, 0x20000, 0x10001580, 0x32, 0x20000, 0x31, 1))
    cases.append(("debounce_p117", 0x2009C034, 0x20000, 0x10001581, 0x32, 0x00000, 0x31, 2))
    for name, fio_in, bit, cnt_addr, thr, level, cnt0, want in cases:
        def setup(uc, f=fio_in, b=bit, c=cnt_addr, l=level, v=cnt0):
            uc.mem_write(f, struct.pack("<I", l))
            uc.mem_write(c, bytes((v,)))
        results = execute_pair(name, (), setup)
        assert results[0][0] == results[1][0], "%s return mismatch (want=%d): %d" % (
            name, want, results[0][0])
        assert results[0][1] == results[1][1], "%s RAM mismatch" % name
    # scan_run_stop：单次触发模式（eeprom_param_1=0）与保持模式（≠0）
    for e1, mode in ((0x00, "single"), (0x01, "hold")):
        for level, cnt, want in ((0x10000000, 0x31, 7), (0x08000000, 0x31, 8)):
            def setup(uc, e=e1, l=level, c=cnt):
                uc.mem_write(0x1000164E, bytes((e,)))       # eeprom_param_1/PAIR_MODE
                uc.mem_write(0x2009C014, struct.pack("<I", l))
                uc.mem_write(0x10001578, bytes((c,)))       # RUN 计数
                uc.mem_write(0x10001579, bytes((c,)))       # STOP 计数
                uc.mem_write(0x1000157A, bytes((0,)))       # 保持锁存
            results = execute_pair("scan_run_stop", (), setup)
            assert results[0][0] == results[1][0], "scan_run_stop %s return mismatch: %d" % (
                mode, results[0][0])
            assert results[0][1] == results[1][1], "scan_run_stop %s RAM mismatch" % mode
    print("DEBOUNCE: PASS cases=%d scan_run_stop=4" % len(cases))


# ── 认证 ─────────────────────────────────────────────────────────────────────
def verify_auth():
    def setup(uc):
        uc.mem_write(0x10001630, struct.pack("<I", 0x64))   # gain_a
        uc.mem_write(0x10001644, struct.pack("<I", 2))      # startup_div
        uc.mem_write(0x10001645, b"\x01")                   # stop_div
        uc.mem_write(0x1000164C, b"\x0a")                   # counter2
        uc.mem_write(0x1000163C, b"\x14")                   # gain_c
        uc.mem_write(0x10001648, struct.pack("<I", 0x5A))   # llim_angle
        uc.mem_write(0x100020C0, b"\xff")                   # auth_pass_flag 初值
        uc.mem_write(0x100020C4, struct.pack("<I", 0))      # 超时窗口
        uc.mem_write(0x100020C8, b"\x00")                   # 重试计数
        for off in range(0, 0x60, 4):
            uc.mem_write(0x2009C000 + off, struct.pack("<I", 0x13570000 ^ off))
    for name in ("auth_set_timeout", "auth_challenge", "auth_verify_loop"):
        # NEW 延时循环每次迭代指令数多于 OLD，auth_verify_loop 5×24bit 挑战
        # 需 ~3.5M 条；2M 会在第 3 次挑战中断，故放宽至 8M（非行为差异）
        results = execute_pair(name, (), setup, max_insn=8_000_000)
        assert results[0][0] == results[1][0], "%s return mismatch" % name
        assert results[0][1] == results[1][1], "%s RAM mismatch" % name
    print("AUTH: PASS funcs=3")


# ── main/参数系统杂项 ─────────────────────────────────────────────────────────
def verify_misc():
    # uart3_tx_byte / uart3_rx_timeout_monitor / load_config / param_sync_live_to_eeprom：
    # 无参函数，R0 与 SRAM 末态均有语义（IAR 返回值/副作用），A/B 逐值对比。
    for name in ("uart3_tx_byte", "uart3_rx_timeout_monitor", "load_config",
                 "param_sync_live_to_eeprom"):
        results = execute_pair(name, (), max_insn=500_000)
        assert results[0][0] == results[1][0], "%s return mismatch" % name
        assert results[0][1] == results[1][1], "%s RAM mismatch" % name
    # disp_screen_static / disp_screen_calib：void 渲染函数，R0 **非 ABI 语义**。
    # OLD R0=0x08 是 char8 内层字体循环 `adds r0,r4,#1` 的编译器寄存器残留（7+1=8）；
    # NEW R0 残留的是 disp_data 返回值——纯编译器代码生成差异，C 源码无法合理复刻。
    # 等价性按行为校验：GPIO 写迹（真实渲染像素位）+ SRAM 末态。
    # 需 8M 指令配额：全屏渲染 4 行 GBK 串，500k 会中途截断产生假差异。
    for name, label in (("disp_screen_static", "DISPLAY_SCREEN_STATIC"),
                        ("disp_screen_calib", "DISPLAY_SCREEN_CALIB")):
        traces, states = [], []
        for is_new, entry in ((False, PAIRS[name][0]), (True, SYMS[name])):
            uc = machine(is_new)
            trace = []
            cb = lambda machine, access, address, size, value, user, t=trace: t.append((address, size, value))
            uc.hook_add(UC_HOOK_MEM_WRITE, cb, begin=0x2009C000, end=0x2009CFFF)
            run(uc, entry, max_insn=8_000_000)
            traces.append(trace)
            states.append(snapshot(uc))
        assert _mask_fio1_p23(traces[0]) == _mask_fio1_p23(traces[1]), \
            "%s GPIO trace mismatch: %d/%d" % (label, len(traces[0]), len(traces[1]))
        assert states[0] == states[1], "%s SRAM mismatch" % (label,)
        print("%s: PASS writes=%d" % (label, len(traces[0])))
    print("MISC: PASS funcs=6")


if __name__ == "__main__":
    verify_vector()
    verify_gpio_pairs()
    verify_relay_direct()
    verify_isrs()
    verify_timer1_matrix()
    verify_crc_matrix()
    verify_modbus_regs()
    verify_closed_loop()
    verify_output_stage_matrix()
    verify_run_stop_preset()
    verify_state_machine_matrix()
    verify_display_matrix()
    verify_display_full_exec()
    verify_debounce()
    verify_auth()
    verify_misc()
    print("\n==== 12p A/B 等价性验证：全部 PASS ====")
