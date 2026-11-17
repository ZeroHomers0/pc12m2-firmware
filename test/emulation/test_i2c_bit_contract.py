"""GPIO位收发实际执行：256个字节、ACK/NACK、SDA逐位输入、精确写序。

只跳过5次空转短延时；不跳过I2C收发本体。外部SDA按每次PIN读提供，
不模拟电气上拉和真实微秒；START/STOP及实际字节协议由持久化链另验。
"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def observe(new,name,value,ack,pattern):
    u=V.machine(new)
    for a in (0x2009c000,0x2009c014,0x2009c018,0x2009c01c):word(u,a,pattern)
    trace=StoreTrace(u)
    bits=[ack] if name=='i2c_write_byte' else [(value>>shift)&1 for shift in range(7,-1,-1)]
    cursor=[0]
    def read(m,access,a,n,v,user):
        assert cursor[0]<len(bits),'SDA读取次数超出协议'
        val=(pattern&~0x400)|(bits[cursor[0]]<<10);word(m,0x2009c014,val);cursor[0]+=1
    u.hook_add(UC_HOOK_MEM_READ,read,begin=0x2009c014,end=0x2009c017)
    def short(m,a,n,user):m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,short,begin=address(new,'i2c_delay_short'),end=address(new,'i2c_delay_short'))
    result=call(u,new,name,(value,) if name=='i2c_write_byte' else (),budget=10000)
    assert cursor[0]==len(bits),'SDA协议未完整执行'
    assert result==(ack if name=='i2c_write_byte' else value),('ACK/数据返回错误',new,name,value,ack,result)
    assert len(trace.events)>15,'I2C GPIO写迹为空或未完整执行'
    return result,trace.events

def main():
    count=0
    for value in range(256):
        for ack in (0,1):
            a=observe(False,'i2c_write_byte',value,ack,0x12340000)
            b=observe(True,'i2c_write_byte',value,ack,0x12340000)
            assert a==b,('I2C发送写序不一致',value,ack,a,b)
            count+=1
        a=observe(False,'i2c_read_byte',value,0,0x87650000)
        b=observe(True,'i2c_read_byte',value,0,0x87650000)
        assert a==b,('I2C接收写序不一致',value,a,b)
        count+=1
    print('I2C_BIT_CONTRACT cases=%d ACK/NACK+all256values+GPIO_STR_trace PASS'%count)
    return 0

if __name__=='__main__':raise SystemExit(main())
