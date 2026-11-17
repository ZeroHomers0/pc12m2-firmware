"""原BIN差分的执行完成门禁：循环预算耗尽不能作为PASS。"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
V = __import__('verify_firmware_equivalence_12' if ROOT.name == 'PC12M-2' else 'verify_firmware_equivalence')

def expect_incomplete(uc, entry, budget):
    try:
        V.run(uc, entry, max_insn=budget)
    except AssertionError as error:
        assert 'execution incomplete' in str(error), error
    else:
        raise AssertionError('未返回的执行被错误接受')

def main():
    u = V.machine(False)
    u.mem_write(0x3FE00, b'\xfe\xe7')
    expect_incomplete(u, 0x3FE00, 32)
    u = V.machine(False)
    u.mem_write(0x3FE00, b'\x70\x47')
    V.run(u, 0x3FE00, max_insn=32)
    for new in (False, True):
        entry = V.SYMS['uart3_tx_byte'] if new else (0xAB7C if ROOT.name == 'PC12M-2' else 0xAE0C)
        u = V.machine(new)
        u.mem_write(0x4009C014, b'\0'*4)
        expect_incomplete(u, entry, 2000)
        u = V.machine(new)
        u.mem_write(0x4009C014, b'\x20\0\0\0')
        V.run(u, entry, max_insn=10000)
    print('EXECUTION_COMPLETION: PASS loop/return + original/ELF UART blocked/ready')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
