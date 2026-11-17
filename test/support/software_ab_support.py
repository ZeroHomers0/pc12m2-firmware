"""双项目软件A/B公共支持。所有地址按各项目原BIN分别登记。

EEPROM模型在字节收发边界提供外部器件响应；寄存器封装、参数加载/同步、
通信分发实际执行。仅跳过已验证的纯空转延时，按其units推进逻辑时间。
不以本模型代替GPIO位收发测试。
"""
import sys
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/verification'))
IS12=(ROOT/'pc12m2_orig.bin').exists()
V=__import__('verify_firmware_equivalence_12' if IS12 else 'verify_firmware_equivalence')
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_PC,UC_ARM_REG_LR

OLD6={'load_config':0x25dc,'param_sync_live_to_eeprom':0x35f2,'i2c_write_reg':0x1e88,'i2c_read_reg':0x1ebc,
      'i2c_start':0x1cae,'i2c_stop':0x1cfe,'i2c_write_byte':0x1d3c,'i2c_read_byte':0x1dfa,'i2c_delay':0x1c82,
      'i2c_delay_short':0x1c6c,'modbus_dispatch':0xb642,'uart3_tx_byte':0xae0c,'output_stage':0xe9ac,
      'UART3_IRQHandler':0xaf08,'uart3_rx_timeout_monitor':0xae50,'TIMER0_IRQHandler':0x29a,
      'adc0_wait_done':0x1fa6,'adc0_scan_channels':0x1fbc,'state_machine':0x458c}
OLD12={'load_config':0x258c,'param_sync_live_to_eeprom':0x3534,'i2c_write_reg':0x1e38,'i2c_read_reg':0x1e6c,
       'i2c_start':0x1c5e,'i2c_stop':0x1cae,'i2c_write_byte':0x1cec,'i2c_read_byte':0x1daa,'i2c_delay':0x1c32,
       'i2c_delay_short':0x1c1c,'modbus_dispatch':0xb3b2,'uart3_tx_byte':0xab7c,'output_stage':0xe70c,
       'UART3_IRQHandler':0xac78,'uart3_rx_timeout_monitor':0xabc0,'TIMER0_IRQHandler':0x29a,
       'adc0_wait_done':0x1f56,'adc0_scan_channels':0x1f6c,'state_machine':0x4464}
OLD=OLD12 if IS12 else OLD6
def address(new,name): return V.SYMS[name] if new else OLD[name]
def word(u,a,v): u.mem_write(a,struct.pack('<I',v&0xffffffff))
def readword(u,a): return struct.unpack('<I',u.mem_read(a,4))[0]
def call(u,new,name,args=(),budget=2000000):
    for r,val in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args): u.reg_write(r,val&0xffffffff)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,address(new,name),max_insn=budget)
    return u.reg_read(UC_ARM_REG_R0)
def crc(data):
    v=0xffff
    for b in data:
        v^=b
        for _ in range(8):v=(v>>1)^0xa001 if v&1 else v>>1
    return v
def frame(data): return bytes(data)+struct.pack('<H',crc(data))
def parameters(u):return bytes(u.mem_read(0x10001620,0x110))

class PowerCut(RuntimeError):
    """测试事件：在两个字节事务之间断电，不能归类为正常返回。"""

class Eeprom:
    """256字节持久介质；写忙5个逻辑units；NACK不提交事务。

    nack_at为每个START后第几个发送字节返回NACK，None为无故障。
    被拒绝后的后续字节依旧能被固件发送，整笔事务不生效。
    """
    def __init__(self,image=None,nack_at=None):
        self.image=bytearray(image if image is not None else b'\xff'*256)
        self.nack_at=nack_at; self.now=0; self.busy_until=0; self.events=[]
        self.segment=[]; self.acknowledged=True; self.cursor=0; self.started=False
        self.cut_after=None;self.commits=0
    def attach(self,u,new):
        names=('i2c_start','i2c_stop','i2c_write_byte','i2c_read_byte','i2c_delay','i2c_delay_short')
        table={address(new,n):n for n in names}
        def hook(m,a,n,unused):
            name=table.get(a)
            if name is None:return
            if name=='i2c_start':
                if len(self.segment)>=2 and self.segment[0]==0xa6:self.cursor=self.segment[1]
                self.segment=[];self.acknowledged=True;self.started=True
                self.events.append(('start',))
                return  # START/STOP GPIO本体仍执行
            if name=='i2c_stop':
                self.events.append(('stop',))
                if len(self.segment)==3 and self.segment[0]==0xa6 and self.acknowledged:
                    if self.cut_after is not None and self.commits>=self.cut_after:raise PowerCut('介质提交边界断电')
                    _,reg,val=self.segment; self.image[reg]=val;self.busy_until=self.now+5
                    self.commits+=1
                    self.events.append(('commit',reg,val))
                self.segment=[];self.started=False
                return
            if name=='i2c_write_byte':
                val=m.reg_read(UC_ARM_REG_R0)&255
                index=len(self.segment)
                nack=self.now<self.busy_until or index==self.nack_at or (index==0 and val not in (0xa6,0xa7))
                self.segment.append(val);self.acknowledged &= not nack
                self.events.append(('tx',val,int(nack)))
                m.reg_write(UC_ARM_REG_R0,int(nack))
            elif name=='i2c_read_byte':
                val=self.image[self.cursor] if self.segment==[0xa7] and self.acknowledged else 255
                self.events.append(('rx',self.cursor,val));m.reg_write(UC_ARM_REG_R0,val)
            elif name=='i2c_delay':
                units=m.reg_read(UC_ARM_REG_R0)
                self.events.append(('wait',units));self.now+=units
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        u.hook_add(UC_HOOK_CODE,hook)
        return self

def setup(new,store):
    u=V.machine(new);store.attach(u,new)
    return u

def dispatch(u,new,wire):
    state,length,slave,txbuf=(0x10001770,0x10001772,0x100016f7,0x10002340) if IS12 else (0x10001790,0x10001792,0x100016ff,0x1000236c)
    u.mem_write(0x10002278 if IS12 else 0x100022a4,wire);u.mem_write(state,b'\x05');u.mem_write(length,bytes((len(wire),)));u.mem_write(slave,b'\x01')
    sent=[]
    def tx(m,a,n,user):
        sent.append(bytes(m.mem_read(txbuf,m.reg_read(UC_ARM_REG_R0)&255)))
        m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    h=u.hook_add(UC_HOOK_CODE,tx,begin=address(new,'uart3_tx_byte'),end=address(new,'uart3_tx_byte'))
    try:call(u,new,'modbus_dispatch')
    finally:u.hook_del(h)
    return sent
