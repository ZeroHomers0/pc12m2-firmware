"""脉冲扫描ISR全字节域及相邻窗口：实际GPIO/MMIO写序+SRAM终态。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
CACHES={False:{},True:{}}

def observe(new,sc,fh,mb,c2,direction,tp):
    u=V.machine(new)
    for a,v in ((0x1000204c if IS12 else 0x10002074,sc),
                (0x1000204d if IS12 else 0x10002075,mb),
                (0x10001fd8 if IS12 else 0x10001ff8,fh)):
        u.mem_write(a,bytes((v,)))
    if IS12:
        for a,v in ((0x1000164c,c2),(0x10002050,direction),(0x1000168c,tp)):u.mem_write(a,bytes((v,)))
    for a in range(0x2009c000,0x2009c0a0,4):word(u,a,0x13570000^a)
    word(u,0x40008018,0x24681357)
    t=StoreTrace(u,((0x2009c000,0x2009c100),(0x40008000,0x40008100)),cache=CACHES[new])
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS['TIMER1_IRQHandler'] if new else (0xfb0c if IS12 else 0xff6c),max_insn=10000)
    assert t.events,'未实际写MMIO'
    return t.events,bytes(u.mem_read(0x10000000,0x2600)),bytes(u.mem_read(0x40008000,0x40))

def main():
    count=0
    profiles=list(itertools.product((50,60,0),(0,1),(99,100,101),(0,1))) if IS12 else [(f,m,0,0) for f,m in itertools.product((50,60,0),(0,1,255))]
    for sc in range(256):
        for i,(fh,mb,c2,d) in enumerate(profiles):
            tp=(0,100,200,255)[(i+sc)%4]
            args=sc,fh,mb,c2,d,tp
            a=observe(False,*args);b=observe(True,*args)
            assert a==b,('脉冲扫描差异',args,a[0],b[0])
            count+=1
    print('TIMER1_BYTE_DOMAIN cases=%d full-scan-byte+frequency/mode/counter-bounds+STR-trace PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
