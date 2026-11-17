"""外设等待模型：先忙后就绪与永不就绪，实际执行ADC/UART轮询本体。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def run(new,target,delay,sample):
    u=V.machine(new)
    adc=target=='adc0_wait_done';port=0x40034004 if adc else 0x4009c014
    if adc:u.mem_map(0x40034000,0x1000)
    txbuf=0x10002340 if IS12 else 0x1000236c
    u.mem_write(txbuf,b'\xa5\x5a\xc3')
    reads=[0]
    def hook(m,access,a,n,v,user):
        ready=delay is not None and reads[0]>=delay
        val=(0x80000000|(sample<<4)) if adc else 0x20
        word(m,port,val if ready else 0)
        reads[0]+=1
    u.hook_add(UC_HOOK_MEM_READ,hook,begin=port,end=port+3)
    writes=StoreTrace(u,((0x4009c000,0x4009c100),(0x2009c000,0x2009c100)))
    complete=True
    try:r=call(u,new,target,() if adc else (3,),budget=3000)
    except AssertionError as e:
        assert 'execution incomplete' in str(e),e
        complete=False;r=None
    if delay is None:
        assert not complete and reads[0]>20,'永不就绪应保持等待，不能算等价返回'
        return 'WAIT_PERIPHERAL'
    assert complete and reads[0]>delay,'未实际经过忙循环'
    if adc:assert r==sample
    else:assert any(a==0x4009c000 and v==0xa5 for a,n,v in writes.events),'未实际发出首字节'
    return ('PASS_EQ',r if adc else None,reads[0],writes.events)

def main():
    count=0
    for target in ('adc0_wait_done','uart3_tx_byte'):
        for delay in (0,1,2,7,31,None):
            for value in ((0,1,0xabc,0xfff) if target.startswith('adc') else (0,)):
                a=run(False,target,delay,value);b=run(True,target,delay,value)
                assert a==b,('外设轮询差异',target,delay,value,a,b)
                count+=1
    print('PERIPHERAL_POLLING cases=%d delayed-ready+WAIT_PERIPHERAL PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
