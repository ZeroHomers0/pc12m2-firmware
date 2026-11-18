"""末批真实端子保持/越界模式的运行中停止，以及外部故障输入置位。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from test_state_lifecycle_contract import *
from test_main_software_chain import old_map

def observe(new,screen,method,start,stop):
    u,store,display,trace=fixture(new,screen,method,start,1,1)
    delta=0x20 if IS12 else 0
    for a,v in ((0x177e,0),(0x177f,0),(0x1785,1)):u.mem_write(0x10000000+a-delta,bytes((v,)))
    u.mem_write(CFG,b'\x01' if stop else b'\0')
    u.mem_write(0x10001578,b'\x31\x31')
    word(u,0x2009c014,0xffffffff&~0x200&(~0x08000000 if stop else ~0x10000000))
    event=[];pending=[];scan=V.SYMS['scan_run_stop'] if new else old_map()['scan_run_stop']
    def code(m,a,n,user):
        if pending and a==pending[-1]:pending.pop();event.append(m.reg_read(UC_ARM_REG_R0))
        if a==scan:pending.append(m.reg_read(UC_ARM_REG_LR)&~1)
    u.hook_add(UC_HOOK_CODE,code)
    call(u,new,'state_machine',(0,),budget=500000)
    assert event and event[0]==(8 if stop else 7),('未实际产生端子事件',screen,method,start,stop,event)
    return snapshot(u),display,trace.events,store.events,bytes(store.image),event

def main():
    seed();count=0
    for spec in itertools.product((1,0x1e),(0,1,2),(1,2,255),(False,True)):
        assert observe(False,*spec)==observe(True,*spec),('保持端子模式',spec);count+=1
    for screen in (1,0x1e):
        values=[]
        for new in (False,True):
            u,store,display,trace=fixture(new,screen,2,0,1,1)
            u.mem_write(0x1000157d,b'\xfa');word(u,0x2009c034,0xfffeffff)
            call(u,new,'state_machine',(0,),budget=500000)
            assert readword(u,FAULT)&0x4000,'真实外故障未置位'
            values.append((snapshot(u),display,trace.events,store.events,bytes(store.image)))
        assert values[0]==values[1];count+=1
    print('COMPLETION_TERMINAL_PROBE',count,'actual_events_7_8_and_fault_0x4000 PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
