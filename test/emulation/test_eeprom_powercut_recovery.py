"""在持久字节事务边界断电，再以全新CPU加载；保留原版非原子写入行为。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *

def observe(new,cut):
    s=Eeprom();u=setup(new,s);call(u,new,'load_config')
    s.events=[];s.commits=0;s.cut_after=cut
    interrupted=False
    try:
        for reg,v in ((2,0x1234),(3,0x456),(7,77)):
            response=dispatch(u,new,frame((1,6,0x10,reg,v>>8,v&255)))
            assert response,'未被断电的写请求没有应答'
    except PowerCut:interrupted=True
    assert interrupted==(cut is not None),'断电事件未实际触发或意外触发'
    if cut is not None:assert s.commits==cut,'未在指定字节事务边界断电'
    # 只转移介质，新CPU、RAM与通信状态不继承。
    reboot=Eeprom(s.image);v=setup(new,reboot);call(v,new,'load_config')
    expected=(s.image[0x18]<<8)|s.image[0x19]
    addr=0x10001634 if IS12 else 0x1000163c
    assert readword(v,addr)==expected,'重启加载未反映实际介质内容'
    return bytes(s.image),s.events,parameters(v),reboot.events,interrupted

def main():
    for cut in (0,1,2,3,4,None):
        a=observe(False,cut);b=observe(True,cut)
        assert a==b,('断电重载与原BIN不同',cut)
    print('EEPROM_POWERCUT_RECOVERY 5 byte-boundary cuts + normal-complete PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
