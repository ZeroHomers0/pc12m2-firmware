"""真实SystemInit的振荡器/PLL延迟就绪及永久未就绪契约。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from test_main_software_chain import old_map
from unicorn import UC_HOOK_MEM_READ

PORTS={0x400fc1a0:0x40,0x400fc088:0x07000000,0x400fc0a8:0x700}

def observe(new,blocked,delay):
    u=V.machine(new);reads={a:0 for a in PORTS}
    def read(m,access,a,n,value,user):
        if a not in PORTS:return
        ready=a!=blocked or (delay is not None and reads[a]>=delay)
        value=readword(m,a)&~PORTS[a]
        word(m,a,value|(PORTS[a] if ready else 0));reads[a]+=1
    u.hook_add(UC_HOOK_MEM_READ,read,begin=0x400fc000,end=0x400fc1ff)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    complete=True
    try:V.run(u,V.SYMS['SystemInit'] if new else old_map()['SystemInit'],max_insn=10000)
    except AssertionError as e:
        assert 'execution incomplete' in str(e),e
        complete=False
    assert reads[blocked]>0,('未进入目标轮询',hex(blocked))
    if delay is None:
        assert not complete and reads[blocked]>50,'永久未就绪应保持等待'
        return 'WAIT_PERIPHERAL'
    assert complete and reads[blocked]>delay,'未实际执行延迟就绪'
    return 'PASS_EQ',bytes(u.mem_read(0x400fc000,0x200))

def main():
    count=0
    for port in PORTS:
        for delay in (0,1,7,31,None):
            a=observe(False,port,delay);b=observe(True,port,delay)
            assert a==b,('时钟轮询差异',hex(port),delay)
            count+=1
    print('CLOCK_POLLING cases=%d delayed-ready+WAIT_PERIPHERAL PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
