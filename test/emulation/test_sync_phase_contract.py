"""EINT3全部相位字节、保持超时与触发计算；失锁→封锁→同步恢复连续执行。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
CACHES={False:{},True:{}}
ADDR={
 'phase':0x10001fd9 if IS12 else 0x10001ff9,'debounce':0x10001fda if IS12 else 0x10001ffa,
 'freq':0x10001fd8 if IS12 else 0x10001ff8,'hold':0x1000209c if IS12 else 0x100020c4,
 'run':0x10001fe0 if IS12 else 0x10002000,'setpoint':0x10001fdc if IS12 else 0x10001ffc,
 'mode':0x1000204d if IS12 else 0x10002075,'cfg':0x10001620 if IS12 else 0x10001628,
 'fault':0x1000161c if IS12 else 0x10001624,'adjust':0x10001624 if IS12 else 0x1000162c}

def byte(u,k,v):u.mem_write(ADDR[k],bytes((v,)))
def prepare(new,phase,freq,hold,run,mode,setpoint,debounce=9,outphase=0):
    u=V.machine(new)
    for k,v in (('phase',phase),('freq',freq),('mode',mode),('debounce',debounce),('cfg',1)):byte(u,k,v)
    for k,v in (('hold',hold),('run',run),('setpoint',setpoint),('fault',0),('adjust',90)):word(u,ADDR[k],v)
    if not IS12:u.mem_write(0x1000165b,bytes((outphase,)))
    return u
def fire(u,new):
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS['EINT3_IRQHandler'] if new else (0xf78c if IS12 else 0xfa2c),max_insn=30000)
def snapshot(u):return bytes(u.mem_read(0x10000000,0x2600))+bytes(u.mem_read(0x2009c000,0x100))+bytes(u.mem_read(0x40090000,0x40))
def case(new,args):
    u=prepare(new,*args)
    t=StoreTrace(u,((0x10000000,0x10002600),(0x2009c000,0x2009c100),(0x40090000,0x40090100),(0x400fc000,0x400fc200)),cache=CACHES[new])
    fire(u,new)
    return t.events,snapshot(u)

def sequence(new,freq):
    phase=100 if freq==50 else 83
    u=prepare(new,phase,freq,0,5,1,1000000)
    fire(u,new)
    assert u.mem_read(ADDR['freq'],1)[0]==freq,'未进入同步窗口'
    snapshots=[]
    for tick in range(60):
        byte(u,'phase',0);fire(u,new);snapshots.append(snapshot(u))
    assert readword(u,ADDR['fault'])&0x2000,'同步丢失未置故障'
    assert not u.mem_read(ADDR['cfg'],1)[0] and readword(u,ADDR['run'])==0,'失锁未停机'
    for tick in range(10):
        byte(u,'phase',phase);fire(u,new);snapshots.append(snapshot(u))
    assert u.mem_read(ADDR['freq'],1)[0]==freq,'同步恢复未重识别频率'
    assert readword(u,ADDR['fault'])&0x2000,'恢复同步意外清除故障锁存'
    return snapshots

def main():
    count=0
    cases=[]
    for phase,freq,hold in itertools.product(range(256),(0,50,60),(3,4)):
        cases.append((phase,freq,hold,5,1,1000000,9,0))
    for freq,run,mode,sp,deb,outphase in itertools.product((0,49,50,60,61),(0,1,2,7,8,0xffffffff),(0,1,2,255),
                        (0,1,1000000,0x7fffffff,0x80000000,0xffffffff),(0,255),(0,1) if not IS12 else (0,)):
        cases.append((0,freq,0,run,mode,sp,deb,outphase))
    for args in cases:
        a=case(False,args);b=case(True,args)
        assert a==b,('同步/触发参数差异',args,a[0][:10],b[0][:10])
        count+=1
    for freq in (50,60):assert sequence(False,freq)==sequence(True,freq),('失锁恢复连续链差异',freq)
    print('SYNC_PHASE_CONTRACT cases=%d phase-byte+state/frequency/scale bounds + 2x70-event loss/recovery PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
