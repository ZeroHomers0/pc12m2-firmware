# -*- coding: utf-8 -*-
"""I2C/UART叶函数的MMIO写地址、宽度、值和顺序A/B。"""
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
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_R0
from verify_firmware_equivalence import machine, run, SYMS

OLD = {
    "i2c_gpio_init": 0x1C40, "i2c_start": 0x1CAE, "i2c_stop": 0x1CFE,
    "i2c_write_byte": 0x1D3C, "i2c_read_byte": 0x1DFA,
    "uart3_init": 0xAC24, "uart3_tx_byte": 0xAE0C,
    "uart3_rx_timeout_monitor": 0xAE50,
}
RANGES = ((0x2009C000, 0x2009CFFF), (0x4002C000, 0x4002CFFF),
          (0x4009C000, 0x4009CFFF), (0x400FC000, 0x400FDFFF),
          (0xE000E000, 0xE000FFFF))


def execute(is_new, name, arg=0, setup=None):
    uc = machine(is_new)
    for base in (0x4002C000,):
        try:
            uc.mem_map(base, 0x1000)
        except Exception:
            pass
    for offset in range(0, 0xA0, 4):
        uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x24680000 ^ offset))
    for offset in range(0, 0x40, 4):
        uc.mem_write(0x4009C000 + offset, struct.pack("<I", 0x20 if offset == 0x14 else 0))
    if setup:
        setup(uc)
    trace = []
    callback = lambda machine_, access, address, size, value, user: trace.append((address, size, value))
    # Unicorn在Thumb IT条件写上存在“内存写hook改变后续寄存器”的插桩问题；
    # uart3_init的frame=1..3恰使用IT块，故该函数比较最终MMIO/RAM，不挂写hook。
    if name != "uart3_init":
        for begin, end in RANGES:
            uc.hook_add(UC_HOOK_MEM_WRITE, callback, begin=begin, end=end)
    uc.reg_write(UC_ARM_REG_R0, arg)
    uc.reg_write(UC_ARM_REG_LR, 0x3FF01)
    run(uc, SYMS[name] if is_new else OLD[name], max_insn=2_000_000)
    final_mmio = (bytes(uc.mem_read(0x2009C000, 0xA0)) +
                  bytes(uc.mem_read(0x4002C000, 0x40)) +
                  bytes(uc.mem_read(0x4009C000, 0x40)))
    return trace, bytes(uc.mem_read(0x10001500, 0x400)), final_mmio


def main():
    cases = [
        ("i2c_gpio_init", 0, None), ("i2c_start", 0, None), ("i2c_stop", 0, None),
        ("i2c_write_byte", 0x00, None), ("i2c_write_byte", 0x55, None),
        ("i2c_write_byte", 0xA6, None), ("i2c_write_byte", 0xFF, None),
        ("i2c_read_byte", 0, None),
        ("uart3_tx_byte", 0x00, None), ("uart3_tx_byte", 0x55, None),
        ("uart3_tx_byte", 0xFF, None),
        ("uart3_rx_timeout_monitor", 0, lambda uc: uc.mem_write(0x10001790, bytes((1, 5, 7)))),
        ("uart3_rx_timeout_monitor", 0, lambda uc: uc.mem_write(0x10001790, bytes((1, 10, 7)))),
    ]
    # uart3_init读取全局波特率/帧格式；覆盖完整选择域。
    for baud in range(8):
        for frame in range(4):
            def setup(uc, b=baud, f=frame):
                uc.mem_write(0x10001700, struct.pack("<I", b))
                uc.mem_write(0x10001704, bytes((f,)))
            cases.append(("uart3_init", 0, setup))

    failed = 0
    for index, (name, arg, setup) in enumerate(cases):
        old = execute(False, name, arg, setup)
        new = execute(True, name, arg, setup)
        if old != new:
            failed += 1
            print(f"  [FAIL] #{index} {name}(0x{arg:X}) 原写={old[0][:8]} 新写={new[0][:8]}")
    print(f"  [{'PASS' if not failed else 'FAIL'}] 外设叶函数写序列 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
