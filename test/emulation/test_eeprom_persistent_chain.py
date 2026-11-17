"""实际寄存器封装→参数持久化→通信写参→新CPU重新加载；NACK/忙单列。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *

def scenario(new,initial):
    store=Eeprom(initial);u=setup(new,store)
    call(u,new,'load_config')
    states=[parameters(u)];responses=[]
    for reg,value in ((2,1234),(3,2345),(7,37),(9,17),(0xa,90)):
        wire=frame((1,6,0x10,reg,value>>8,value&255))
        sent=dispatch(u,new,wire)
        assert sent==[wire], ('合法写参未原样应答',reg,sent)
        responses.append(sent);states.append(parameters(u))
    before=len(store.events)
    call(u,new,'param_sync_live_to_eeprom')
    assert not any(e[0]=='commit' for e in store.events[before:]),'无变化重复同步仍写EEPROM'
    # 新建CPU/RAM，只带持久介质；不把旧RAM复制过去。
    reloaded=Eeprom(store.image);v=setup(new,reloaded)
    call(v,new,'load_config')
    assert not any(e[0]=='commit' for e in reloaded.events),'有效银行重启后仍恢复默认'
    for addr,value,n in ((0x10001634 if IS12 else 0x1000163c,1234,4),
                         (0x10001630 if IS12 else 0x10001638,2345,4),
                         (0x10001644 if IS12 else 0x1000164c,37,1),
                         (0x10001648 if IS12 else 0x10001650,17,4),
                         (0x1000164c if IS12 else 0x10001654,90,1)):
        assert int.from_bytes(v.mem_read(addr,n),'little')==value,('重启未恢复写值',hex(addr),value)
    assert len(store.events)>100 and any(e[0]=='commit' for e in store.events),'未真正落盘'
    return states,responses,bytes(store.image),store.events,parameters(v),reloaded.events

def fault(new,nack_at,busy):
    store=Eeprom(bytes(range(256)),nack_at=nack_at);store.busy_until=5 if busy else 0
    u=setup(new,store)
    call(u,new,'i2c_write_reg',(0x5a,0x27))
    call(u,new,'i2c_read_reg',(0x10003000,0x27))
    # ACK槽记录与继续发送的事务是原厂行为，不把NACK包装成成功。
    return store.events,bytes(store.image),bytes(u.mem_read(0x1000158c,1)),bytes(u.mem_read(0x10003000,1))

def main():
    for tag,image in (('blank',b'\xff'*256),('zero',b'\0'*256)):
        a=scenario(False,image);b=scenario(True,image)
        assert a==b,('持久化链差异',tag)
        print('EEPROM_CHAIN',tag,'events=',len(a[3]),'reboot_reads=',len(a[5]),'PASS')
    for nack,busy in ((None,False),(0,False),(1,False),(2,False),(None,True)):
        a=fault(False,nack,busy);b=fault(True,nack,busy)
        assert a==b,('I2C错误路径差异',nack,busy,a,b)
        rejected=nack is not None or busy
        assert (a[1][0x27]==0x27)==rejected,('NACK/忙错误提交了介质',nack,busy)
        assert any(e[0]=='tx' and e[2] for e in a[0])==rejected
        print('EEPROM_FAULT nack=',nack,'busy=',busy,'PASS')
    return 0

if __name__=='__main__':raise SystemExit(main())
