"""镜像实装检查须拒绝已加载但被换成错误内容的仿真器。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'tools/verification')]
from software_ab_support import *
from run_attested_entry import attest_memory

def main():
    for new in (False,True):
        u=V.machine(new);expected=V.NEW if new else V.ORIGINAL
        attest_memory(u,expected)
        u.mem_write(0x400,bytes((expected[0x400]^1,)))
        try:attest_memory(u,expected)
        except AssertionError as e:assert 'LOADED_IMAGE_MISMATCH' in str(e)
        else:raise AssertionError('错误镜像被接受')
        u.mem_write(0x400,expected[0x400:0x401]);attest_memory(u,expected)
    print('LOADED_IMAGE_ATTESTATION_CONTRACT original/current corrupt/recover PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
