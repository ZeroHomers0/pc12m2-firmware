"""有限MMIO模型：GPIO SET/CLR、定时器IR W1C/TCR复位、NVIC使能/挂起。

依据NXP UM10360第6/9/21章。只支持本测试声明的边沿EINT输入和PRIGROUP=0。
在STR实际执行后、下一条指令前应用副作用；不挂FIO MEM_WRITE钩子。
没有实现总线周期、自动定时器、异常栈帧、尾链或电气行为。
FIOCLR是WO；本模型读回0只是显式探针假设，不证明真实AHB读行为。
"""
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_CODE
import struct

def get(u,a):return int.from_bytes(u.mem_read(a,4),'little')
def put(u,a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))

class PeripheralModel:
    timers=(0x40004000,0x40008000,0x40090000,0x40094000)
    def __init__(self,u):
        self.u=u;self.pending=[];self.events=[];self.latch=[get(u,0x2009c014+32*p) for p in range(5)]
        self.inputs=self.latch.copy();self.enabled=[0,0];self.irq_pending=[0,0]
        for a in (*self.timers,0xe000e000):
            if not any(lo<=a<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(a,0x1000)
        self.ir={a:get(u,a) for a in self.timers};self.ext=get(u,0x400fc140)
        # 顺序必须先安装flush、再安装写迹观察器。
        u.hook_add(UC_HOOK_CODE,lambda m,a,n,user:self.flush())
        self.trace=StoreTrace(u,((0x2009c000,0x2009c0a0),(0x40004000,0x40008080),
                                (0x40090000,0x40094080),(0x400fc140,0x400fc144),(0xe000e100,0xe000e284)),on_store=self.store)
    def store(self,u,pc,event):self.pending.append(event)
    def gpio(self,p):
        b=0x2009c000+p*32;direction=get(self.u,b);mask=get(self.u,b+16)
        pins=((self.latch[p]&direction)|(self.inputs[p]&~direction))&~mask
        put(self.u,b+20,pins);put(self.u,b+24,self.latch[p]);put(self.u,b+28,0)
    def input(self,p,value):self.inputs[p]=value;self.gpio(p)
    def raise_timer(self,base,bits=1):self.ir[base]|=bits;put(self.u,base,self.ir[base])
    def raise_eint(self,bits):self.ext|=bits;put(self.u,0x400fc140,self.ext)
    def mirrors(self,bank):
        for off,values in ((0x100,self.enabled),(0x180,self.enabled),(0x200,self.irq_pending),(0x280,self.irq_pending)):
            put(self.u,0xe000e000+off+4*bank,values[bank])
    def pend(self,irq):bank,bit=divmod(irq,32);self.irq_pending[bank]|=1<<bit;self.mirrors(bank)
    def next_irq(self,primask=0,basepri=0,active_priority=256):
        if primask:return None
        choices=[]
        for irq in range(64):
            bank,bit=divmod(irq,32)
            priority=self.u.mem_read(0xe000e400+irq,1)[0]&0xf8
            if self.enabled[bank]&self.irq_pending[bank]&(1<<bit) and priority<active_priority and (basepri==0 or priority<basepri):choices.append((priority,irq))
        return min(choices)[1] if choices else None
    def accept(self,irq):bank,bit=divmod(irq,32);self.irq_pending[bank]&=~(1<<bit);self.mirrors(bank)
    def flush(self):
        for addr,size,value in self.pending:
            self.events.append((addr,size,value))
            if 0x2009c000<=addr<0x2009c0a0:
                p,off=divmod(addr-0x2009c000,32);aligned=off&~3;shift=(off&3)*8
                bits=((1<<(8*size))-1)<<shift;value<<=shift;b=0x2009c000+p*32
                unmasked=bits&~get(self.u,b+16)&get(self.u,b)
                if aligned in (20,24,28):
                    if aligned==20:self.latch[p]=(self.latch[p]&~unmasked)|(value&unmasked)
                    elif aligned==24:self.latch[p]|=value&unmasked
                    else:self.latch[p]&=~(value&unmasked)
                self.gpio(p)
            elif addr in self.ir:self.ir[addr]&=~value;put(self.u,addr,self.ir[addr])
            elif addr-4 in self.ir and value&2:
                put(self.u,addr+4,0);put(self.u,addr+12,0) # TC与PC
            elif addr==0x400fc140:self.ext&=~value;put(self.u,addr,self.ext)
            elif 0xe000e100<=addr<=0xe000e284:
                assert size==4 and addr%4==0,'NVIC模型只允许字访问'
                off=addr-0xe000e000;group=off&~0x7f;bank=(off&0x7f)//4
                assert bank<2 and group in (0x100,0x180,0x200,0x280),'未支持NVIC寄存器'
                values=self.enabled if group<0x200 else self.irq_pending
                if group in (0x100,0x200):values[bank]|=value
                else:values[bank]&=~value
                self.mirrors(bank)
        self.pending=[]
