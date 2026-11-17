"""PID 符号边界：直接执行各自原 BIN；比较返回值和完整业务 SRAM。

6M: 0x108F2/0x10954、0x1090C/20/2C/40；
12M: 0x10ADE/0x10B40、0x10AF8/0C/18/2C 均为有符号分支。
同时覆盖正负输入、差值回绕和不同死区增益，不能只比较被钳位的输出。
"""
import sys
import struct
import itertools
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
IS12 = (ROOT / 'pc12m2_orig.bin').exists()
V = __import__('verify_firmware_equivalence_12' if IS12 else 'verify_firmware_equivalence')
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3

VALUES = (0, 10, 11, 120, 121, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff)
OLD = 0x10a9c if IS12 else 0x108b0
BRANCHES = (0x10ade, 0x10b40) if IS12 else (0x108f2, 0x10954)
WORK = 0x100020d0 if IS12 else 0x100020f8
ABS = WORK + 0x20
GAIN = WORK + 0x24
THRESH = 0x1000171a if IS12 else 0x10001722

def observe(new, sp, fb, mode, history):
    u = V.machine(new)
    def word(addr, val): u.mem_write(addr, struct.pack('<I', val & 0xffffffff))
    u.mem_write(0x1000162c if IS12 else 0x10001634, bytes((mode,)))
    word(0x10001630 if IS12 else 0x1000163c, 150)
    word(0x10001634 if IS12 else 0x10001638, 550)
    word(0x1000200c if IS12 else 0x10002024, 0)
    # 上/下界与三个增益都不同；历史误差和累加器跨符号边界。
    u.mem_write(THRESH, bytes((120, 10, 50, 30, 7)))
    word(WORK + 0x14, history)
    word(WORK + 0x0c, history ^ 0x12345678)
    word(WORK + 0x28, 600000)
    word(WORK + 0x30, 8)
    hits = set()
    if not new:
        def hook(m, a, n, unused):
            if a in BRANCHES: hits.add(a)
        u.hook_add(UC_HOOK_CODE, hook)
    for r, val in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), (sp, fb, 5, 3)):
        u.reg_write(r, val)
    V.run(u, V.SYMS['closed_loop_integral'] if new else OLD, max_insn=10000)
    return bytes(u.mem_read(0x10000000, 0x2200)), u.reg_read(UC_ARM_REG_R0), hits

def main():
    failures = 0
    covered = set()
    count = 0
    for sp, fb, mode, history in itertools.product(VALUES, VALUES, (0, 1, 2), (0, 0x80000000)):
        a, ar, hits = observe(False, sp, fb, mode, history)
        b, br, _ = observe(True, sp, fb, mode, history)
        covered |= hits
        count += 1
        if a != b or ar != br:
            failures += 1
            if failures <= 5:
                idx = next((i for i in range(len(a)) if a[i] != b[i]), None)
                print('PID_DIFF', hex(sp), hex(fb), mode, hex(history),
                      'first=', hex(0x10000000 + idx) if idx is not None else 'return', ar, br)
    assert covered == set(BRANCHES), '未执行到原机符号分支'
    print('PID_SIGNED_BOUNDARIES cases=%d failures=%d original_branches=%s' %
          (count, failures, [hex(a) for a in sorted(covered)]))
    return int(bool(failures))

if __name__ == '__main__':
    raise SystemExit(main())
