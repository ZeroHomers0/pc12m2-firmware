"""通信完整业务SRAM与CRC副本状态契约；保留工作槽，不以响应相同豁免。"""
import sys
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parent),str(Path(__file__).resolve().parents[1]/'support')]
import test_modbus_boundary_contract as B

def main():
    for new in (False,True):B.BASE[new]=B.initialize(new)
    cases=[('unsupported-register',B.frame((1,6,16,0,0,19))),
           ('invalid-value',B.frame((1,6,16,1,0,3))),
           ('valid-write',B.frame((1,6,16,1,0,1))),
           ('read',B.frame((1,3,16,1,0,1))),
           ('multi',B.frame((1,16,16,1,0,1,2,0,1)))]
    good=B.frame((1,3,16,1,0,1))
    cases.append(('bad-crc',good[:-1]+bytes((good[-1]^255,))))
    for label,wire in cases:
        a=B.observe(False,wire);b=B.observe(True,wire)
        diff=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y]
        assert a==b,(label,diff[:20],a[1],b[1])
    print('MODBUS_SRAM_CONTRACT cases=6 full_SRAM_0x2900+response+persistent_medium PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
