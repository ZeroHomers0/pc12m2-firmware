# -*- coding: utf-8 -*-
"""多个中断按不同先后顺序连续执行的原 BIN / 新 ELF A/B。"""
import os
import sys
import struct

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_LR
from verify_firmware_equivalence import machine, run, SYMS

OLD = {"T0": 0x29A, "T1": 0xFF6C, "T2": 0xFF48,
       "E1": 0xF9E8, "E2": 0xFA0A, "E3": 0xFA2C, "RX": 0xAED0}
NEW = {"T0": SYMS["TIMER0_IRQHandler"], "T1": SYMS["TIMER1_IRQHandler"],
       "T2": SYMS["TIMER2_IRQHandler"], "E1": SYMS["EINT1_IRQHandler"],
       "E2": SYMS["EINT2_IRQHandler"], "E3": SYMS["EINT3_IRQHandler"],
       "RX": SYMS["func_0x0000aed0"]}
RANGES = ((0x2009C000, 0x2009CFFF), (0x40008000, 0x40008FFF),
          (0x40004000, 0x40004FFF),
          (0x40090000, 0x40090FFF), (0x400FC000, 0x400FDFFF),
          (0xE000E000, 0xE000FFFF))


def execute(is_new, sequence):
    uc = machine(is_new)
    try:
        uc.mem_map(0x40004000, 0x1000)
    except Exception:
        pass
    table = NEW if is_new else OLD
    uc.mem_write(0x10002074, bytes((39, 1)))
    uc.mem_write(0x10001FF8, b"\x32")
    uc.mem_write(0x10001790, bytes((0, 7, 0)))
    uc.mem_write(0x4009C000, b"\xA5")
    for offset in range(0, 0xA0, 4):
        uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x13570000 ^ offset))
    trace = []
    rx_count = 0
    callback = lambda machine_, access, address, size, value, user: trace.append((address, size, value))
    for begin, end in RANGES:
        uc.hook_add(UC_HOOK_MEM_WRITE, callback, begin=begin, end=end)
    for name in sequence:
        if name == "RX":
            uc.mem_write(0x4009C000, bytes([(0xA5 + rx_count) & 0xFF]))
            rx_count += 1
        uc.reg_write(UC_ARM_REG_LR, 0x3FF01)
        run(uc, table[name], max_insn=2_000_000)
    return bytes(uc.mem_read(0x10000000, 0x2200)), trace


def main():
    # strict_mmio=False 的人工顺序只比较SRAM：Unicorn把W1C、SET/CLR和定时器
    # 寄存器当普通RAM，前一ISR留下的“寄存器值”不代表真实外设下一时刻的读值。
    cases = [
        ("过零链正序", ("E1", "T2", "T1", "E2", "E3"), True),
        ("过零链逆序", ("E3", "E2", "T1", "T2", "E1"), False),
        ("节拍夹在触发中断之间", ("E1", "T0", "T2", "T1", "T0", "E3"), True),
        ("UART与定时器交错", ("RX", "T0", "T1", "RX", "T2", "T0"), False),
        ("重复完整序列", ("T0", "E1", "E2", "E3", "T2", "T1") * 4, True),
    ]
    failed = 0
    for name, sequence, strict_mmio in cases:
        old = execute(False, sequence)
        new = execute(True, sequence)
        ram_ok = old[0] == new[0]
        mmio_ok = old[1] == new[1]
        ok = ram_ok and (mmio_ok or not strict_mmio)
        scope = "RAM+MMIO" if strict_mmio else "RAM（MMIO模型受限）"
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  {scope} handlers={len(sequence)} writes={len(old[1])}")
        if not ok:
            if old[0] != new[0]:
                diff = next(i for i, (a, b) in enumerate(zip(old[0], new[0])) if a != b)
                print(f"    RAM首差异：0x{0x10000000 + diff:08X} 原=0x{old[0][diff]:02X} 新=0x{new[0][diff]:02X}")
            if old[1] != new[1]:
                pos = next((i for i, (a, b) in enumerate(zip(old[1], new[1])) if a != b),
                           min(len(old[1]), len(new[1])))
                print(f"    MMIO首差异：index={pos}\n      原={old[1][max(0,pos-2):pos+3]}\n      新={new[1][max(0,pos-2):pos+3]}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
