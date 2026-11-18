"""认证器按实际24个时钟提供响应；覆盖0/1、匹配/高字节错/低字节错。

认证main永久放行另测，此处不强写认证结果；所有位延时真实执行。
"""
import sys
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'support'),str(Path(__file__).resolve().parent)]
from software_ab_support import *
from test_main_software_chain import old_map
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def observe(new,kind):
    u=V.machine(new);shift=8 if IS12 else 0
    values=((0x1638,4,1234),(0x164c,1,17),(0x164d,1,29),(0x1654,1,43),(0x1644,4,0x1234),(0x1650,4,153))
    for a,n,v in values:u.mem_write(0x10000000+a-shift,v.to_bytes(n,'little'))
    a=1234+17+29+43+0x31;b=0x34+43+153+0xc
    hi=((a^0xc2)+(a|0x1b)+(a&0xb2))&255
    lo=((b^0x3f)+(b|0xa9)+(b&0xbc))&255
    response={'zero':0,'one':65535,'match':hi*256+lo,'wrong-low':hi*256+(lo^1),'wrong-high':(hi^1)*256+lo}[kind]
    stores=[0];reads=[]
    def store(m,pc,event):
        if event[0]==0x2009c05c:stores[0]+=1
    trace=StoreTrace(u,((0x2009c000,0x2009c100),),on_store=store)
    def read(m,access,a,n,v,user):
        # 初始复位CLR一次，每bit数据CLR/时钟CLR各一次，故低电平读取落在3+2*bit。
        bit=(stores[0]-3)//2
        assert 0<=bit<24,('读不在有效时钟',stores[0])
        value=((0xa5>>(7-bit))&1) if bit<8 else ((response>>(23-bit))&1)
        word(m,a,value*4);reads.append((bit,value))
    u.hook_add(UC_HOOK_MEM_READ,read,begin=0x2009c054,end=0x2009c057)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS['auth_challenge'] if new else old_map()['auth_challenge'],max_insn=2000000)
    status=u.mem_read(0x100020c0 if IS12 else 0x100020e8,1)[0]
    assert status==int(kind!='match'),('认证应答未真正匹配/拒绝',new,kind,status,hex(response))
    return bytes(u.mem_read(0x10000000,0x2600)),trace.events,reads

def main():
    for kind in ('zero','one','match','wrong-low','wrong-high'):
        a=observe(False,kind);b=observe(True,kind)
        assert a==b,('认证真实设备差异',kind,'parts',[i for i,(x,y) in enumerate(zip(a,b)) if x!=y], 'reads',a[2],b[2])
    print('AUTH_RESPONSE_DEVICE_CONTRACT 5 PASS actual_bit_delays GPIO_reads_and_writes',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
