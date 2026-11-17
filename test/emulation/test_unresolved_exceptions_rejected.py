"""负向门禁：两个版本即使抛同一种仿真异常，也不能标成等价通过。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from unicorn import UcError,UC_ERR_READ_UNMAPPED

def main():
    is12=(ROOT/'pc12m2_orig.bin').exists()
    m=__import__('test_extra_coverage_12' if is12 else 'test_modbus_dispatch_execution')
    name='call' if is12 else 'run_frame'
    original=getattr(m,name)
    def invalid(*args,**kw):raise UcError(UC_ERR_READ_UNMAPPED)
    setattr(m,name,invalid)
    try:
        for new in (False,True):
            try:
                if is12:m._dispatch_run(new,b'\x01\x03')
                else:m.run_outcome(m.load_firmware if new else m.load_original,0,0,b'\x01\x03')
            except AssertionError as e:
                assert '禁止' in str(e),e
            else:raise AssertionError('未解释UcError仍被接受为可比较结果')
    finally:setattr(m,name,original)
    print('UNRESOLVED_EXCEPTIONS_REJECTED original/ELF same-UcError both rejected PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
