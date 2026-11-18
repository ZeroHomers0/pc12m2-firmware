"""通信高地址拒绝、PID档位选择与写增益后的活动系数：各自原BIN实际执行。"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
import test_modbus_boundary_contract as B
from software_ab_support import *

PROFILE=0x10001708 if IS12 else 0x10001710
ACTIVE=0x10001706 if IS12 else 0x1000170e
def main():
    for new in (False,True):B.BASE[new]=B.initialize(new)
    count=0;commits=0
    for function,high,reg in itertools.product((6,16),(0,15,17,255),(0,1,2,21,22,23,24,25,26,37,38,63,64,255)):
        data=(1,function,high,reg,0,1) if function==6 else (1,function,high,reg,0,1,2,0,1)
        wire=frame(data);a=B.observe(False,wire);b=B.observe(True,wire)
        assert a==b,('地址高字节门控差异',function,high,reg)
        assert len(a[1])==1 and not any(e[0]=='commit' for e in a[2]),'非法高地址意外改写介质或未应答'
        count+=1
    # 从不同PID档位开始，再写参数；不能只在默认档2下测试全部增益寄存器。
    original=dict(B.BASE)
    try:
        for profile in (0,1,2,3,4,255):
            for new in (False,True):
                state,image=original[new];state=bytearray(state)
                state[PROFILE-0x10000000]=profile
                state[ACTIVE-0x10000000:ACTIVE-0x10000000+2]=b'\x51\x63'
                B.BASE[new]=(bytes(state),image)
            for reg,val in itertools.product(range(0x15,0x1a),(0,1,4,5,99,100,101,255,256)):
                wire=frame((1,6,16,reg,val>>8,val&255));a=B.observe(False,wire);b=B.observe(True,wire)
                assert a==b,('写增益后活动档位差异',profile,reg,val)
                commits+=any(e[0]=='commit' for e in a[2]);count+=1
    finally:B.BASE.clear();B.BASE.update(original)
    assert commits,'没有实际写入任何增益参数'
    print('MODBUS_ADDRESS_PROFILE_CONTRACT cases=%d gain_commit_cases=%d PASS'%(count,commits),flush=True)
    (ROOT/'tmp/modbus_address_profile_contract.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'gain_commit_cases':commits,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
