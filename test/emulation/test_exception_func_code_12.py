# -*- coding: utf-8 -*-
"""PC12M-2 异常应答功能码按 8 位加法回绕（原 BIN 0xB42C / 0xB4C2 `adds r0,#0x80`）。

覆盖两条异常路径：
  * 功能码不属于 0x03/0x06/0x10 → 异常码 0x01，功能码 = (func + 0x80) & 0xFF；
  * 功能码合法但 CRC 错误     → 异常码 0x04，功能码 = (func + 0x80) & 0xFF。
逐帧比较原 BIN 与新 ELF 的真实应答字节。
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
from verify_firmware_equivalence_12 import machine, run, SYMS, STOP

OLD_TX = 0xAB7C
NEW_TX = SYMS['uart3_tx_byte']
OLD_ENTRY = 0xB3B2
NEW_ENTRY = SYMS['modbus_dispatch']

STATE = 0x10001770
TX_LEN = 0x10001772
SLAVE = 0x100016F7
RX = 0x10002278
TX = 0x10002340


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def wire(payload, bad_crc=False):
    body = bytes((1,)) + bytes(payload)
    value = crc16(body) ^ (0xFFFF if bad_crc else 0)
    return body + bytes((value & 0xFF, value >> 8))


def send(is_new, data):
    uc = machine(is_new)
    uc.mem_write(SLAVE, b'\x01')
    uc.mem_write(STATE, b'\x05')
    uc.mem_write(TX_LEN, bytes((len(data) & 0xFF,)))
    uc.mem_write(RX, data)
    sent = []
    hop = NEW_TX if is_new else OLD_TX

    def tx_hook(m, address, size, user):
        count = m.reg_read(UC_ARM_REG_R0) & 0xFF
        sent.append(bytes(m.mem_read(TX, count or 1))[:count])
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, tx_hook, begin=hop, end=hop + 1)
    uc.reg_write(UC_ARM_REG_LR, STOP + 1)
    run(uc, NEW_ENTRY if is_new else OLD_ENTRY, max_insn=2_000_000)
    return b''.join(sent)


def main():
    checked = 0
    for func in (0x00, 0x04, 0x05, 0x2B, 0x7F, 0x80, 0x83, 0x86, 0x90, 0xFF):
        data = wire((func, 0x10, 0x01, 0x00, 0x01))
        old = send(False, data)
        new = send(True, data)
        assert old == new, f'非法功能码 0x{func:02X}: OLD={old.hex()} NEW={new.hex()}'
        low = (func + 0x80) & 0xFF
        assert len(old) >= 3 and old[1] == low and old[2] == 0x01, \
            f'非法功能码 0x{func:02X} 应答不符: {old.hex()}'
        checked += 1

    # 功能码过滤在 CRC 校验之前，故 CRC 路径只用合法的 0x03/0x06/0x10。
    for func in (0x03, 0x06, 0x10):
        data = wire((func, 0x10, 0x01, 0x00, 0x01), bad_crc=True)
        old = send(False, data)
        new = send(True, data)
        assert old == new, f'CRC 异常 0x{func:02X}: OLD={old.hex()} NEW={new.hex()}'
        low = (func + 0x80) & 0xFF
        assert len(old) >= 3 and old[1] == low and old[2] == 0x04, \
            f'CRC 异常 0x{func:02X} 应答不符: {old.hex()}'
        checked += 1

    print(f'12p 异常应答功能码 func+0x80：{checked}/{checked} 帧与原 BIN 一致')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
