"""两银行魔数逐字节有效性、PID档位与手动值同步后重启恢复，持久介质参与比较。"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from mmio_code_trace import StoreTrace

PROFILE=0x10001708 if IS12 else 0x10001710
ACTIVE=0x10001706 if IS12 else 0x1000170e
CACHE={False:{},True:{}}
def observe(new,a,b,profile):
    image=bytearray((n*73+19)&255 for n in range(256))
    image[5:7]=bytes((0x55 if a&1 else 0,0x55 if a&2 else 0))
    image[7:9]=bytes((0x66 if b&1 else 0,0x66 if b&2 else 0))
    image[0x5a]=profile;image[0x5b:0x63]=bytes((11,21,12,22,13,23,14,24))
    store=Eeprom(image);u=setup(new,store)
    u.mem_write(ACTIVE,b'\x51\x63')
    trace=StoreTrace(u,((0x2009c000,0x2009c100),),cache=CACHE[new])
    call(u,new,'load_config',budget=1000000)
    state=bytes(u.mem_read(0x10000000,0x2900));events=list(store.events)
    if a:
        assert u.mem_read(PROFILE,1)[0]==profile,'有效银行未实际加载PID档位'
        if 1<=profile<=4:assert bytes(u.mem_read(ACTIVE,2))==bytes((10+profile,20+profile)),'活动增益没有使用选中档位'
        else:assert bytes(u.mem_read(ACTIVE,2))==b'\x51\x63','非法档位意外覆盖活动增益'
    # 手动值与影子故意不同，先同步再无变化复调，最后新建CPU只带介质重启。
    word(u,0x100015d8,0x1234);word(u,0x100015dc,0x5678)
    cut=len(store.events);call(u,new,'param_sync_live_to_eeprom')
    commits=[e for e in store.events[cut:] if e[0]=='commit']
    assert ('commit',0x1d,0x12) in commits and ('commit',0x1e,0x34) in commits,'未实际同步手动值高低字节'
    call(u,new,'param_sync_live_to_eeprom')
    reloaded=Eeprom(store.image);v=setup(new,reloaded);call(v,new,'load_config',budget=1000000)
    assert readword(v,0x100015d8)==0x1234,'重启未恢复手动值'
    return state,events,trace.events,bytes(u.mem_read(0x10000000,0x2900)),store.events,bytes(store.image),bytes(v.mem_read(0x10000000,0x2900)),reloaded.events

def main():
    count=0
    for spec in itertools.product(range(4),range(4),(0,1,2,3,4,255)):
        a=observe(False,*spec);b=observe(True,*spec)
        assert a==b,('银行/档位加载或同步恢复差异',spec,[i for i,(x,y) in enumerate(zip(a,b)) if x!=y])
        count+=1
    print('PARAMETER_BANK_PROFILE_CONTRACT cases=%d OR_magic_each_byte+four_PID_profiles+manual_sync_reboot PASS'%count,flush=True)
    (ROOT/'tmp/parameter_bank_profile_contract.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
