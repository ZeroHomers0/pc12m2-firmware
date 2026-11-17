"""逐字节IRQ接收→帧间隔→分发→实际发送与THRE中断收尾，非法后接合法。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def scenario(new):
    store=Eeprom();u=setup(new,store);call(u,new,'load_config')
    state=0x10001770 if IS12 else 0x10001790
    slave=0x100016f7 if IS12 else 0x100016ff
    u.mem_write(state,b'\x00\x00\x00');u.mem_write(slave,b'\x01')
    poll=[0]
    def lsr(m,access,a,n,v,user):
        word(m,0x4009c014,0 if poll[0]<3 else 0x20);poll[0]+=1
    u.hook_add(UC_HOOK_MEM_READ,lsr,begin=0x4009c014,end=0x4009c017)
    trace=StoreTrace(u,((0x4009c000,0x4009c100),(0x2009c000,0x2009c100)))
    valid=frame((1,6,0x10,2,0x04,0xd2))
    crc_bad=valid[:-1]+bytes((valid[-1]^0x80,))
    sequences=(b'\x01\x99',crc_bad,valid,frame((2,3,0x10,2,0,1)),frame((1,3,0x10,2,0,1)))
    outputs=[]
    for index,wire in enumerate(sequences):
        for b in wire:
            word(u,0x4009c000,b);word(u,0x4009c008,4)
            call(u,new,'UART3_IRQHandler')
            if index==2:call(u,new,'uart3_rx_timeout_monitor')  # 小间隙未成帧
        assert u.mem_read(state,1)[0]==1,'接收帧未进入接收态'
        for _ in range(11):call(u,new,'uart3_rx_timeout_monitor')
        assert u.mem_read(state,1)[0]==5,'帧间隙后未进入待处理态'
        before=len(trace.events);poll[0]=0
        call(u,new,'modbus_dispatch')
        drains=0
        while u.mem_read(state,1)[0]==6:
            word(u,0x4009c008,2);call(u,new,'UART3_IRQHandler')
            drains+=1;assert drains<255,'发送收尾无法回到空闲'
        sent=bytes(v&255 for a,n,v in trace.events[before:] if a==0x4009c000)
        if index==3:assert not sent,'非本站帧错误发送'
        else:
            assert sent and crc(sent[:-2])==int.from_bytes(sent[-2:],'little'),('响应CRC错误',index,sent)
        if index==0:assert sent[:3]==b'\x01\x19\x01','异常功能码未按加法回绕'
        if index==1:assert sent[:3]==b'\x01\x86\x04','CRC错误未发送04'
        if index==2:assert sent==valid,'合法写在非法帧后未恢复'
        assert u.mem_read(state,1)[0]==0,'链路未恢复空闲，下一帧可能受污染'
        outputs.append((sent,parameters(u),bytes(store.image),list(store.events)))
    assert poll[0]>=4,'发送就绪模型未实际经历忙循环'
    return outputs

def main():
    a=scenario(False);b=scenario(True)
    assert a==b,'逐字节UART连续链与原BIN不同'
    print('UART_WIRE_CHAIN 5 frames invalid/CRC/write/wrong-slave/read real-TX+THRE PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
