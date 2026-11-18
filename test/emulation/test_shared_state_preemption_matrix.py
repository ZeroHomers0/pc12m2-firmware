"""共享业务量读/写前后插入真实ISR；合法单层抢占，不模拟自动异常入栈。

中断期间保留SRAM/MMIO，只恢复CPU上下文。显示叶和EEPROM外部字节模型
沿用独立契约，输出、通信、状态机及ISR函数体真实执行。
"""
import sys,json,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from mmio_code_trace import StoreTrace
from test_output_persistent_replay import prepare
import test_state_lifecycle_contract as L
from capstone.arm import ARM_OP_MEM
from unicorn import arm_const as A

SHIFT=8 if IS12 else 0
FREQ=0x10001fd8 if IS12 else 0x10001ff8
IRQS={'EINT3_IRQHandler':(21,0xf78c if IS12 else 0xfa2c),
      'TIMER0_IRQHandler':(1,0x29a),'UART3_IRQHandler':(8,0xac78 if IS12 else 0xaf08)}
RANGES=((0x2009c000,0x2009c100),(0x40004000,0x40004100),(0x40008000,0x40008100),
        (0x40090000,0x40090100),(0x4009c000,0x4009c100),(0x400fc000,0x400fc200))
CACHE={False:{},True:{}}

def scenarios():
    for label,target,slot,irq in (
        ('start-config','state_start',0x10001628-SHIFT,'EINT3_IRQHandler'),
        ('external-fault','state_fault',0x10001624-SHIFT,'EINT3_IRQHandler'),
        ('fault-recovery','state_recover',0x10001624-SHIFT,'EINT3_IRQHandler'),
        ('output-config','output',0x10001628-SHIFT,'EINT3_IRQHandler'),
        ('output-fault','output_fault',0x10001624-SHIFT,'EINT3_IRQHandler'),
        ('output-tick','output',0x10002051 if IS12 else 0x10002078,'EINT3_IRQHandler'),
        ('output-phase','output',0x10001fe0 if IS12 else 0x10002000,'EINT3_IRQHandler'),
        ('write-phase','modbus_phase',0x1000165b-SHIFT,'EINT3_IRQHandler'),
        ('write-run','modbus_run',0x10001765 if IS12 else 0x10001785,'EINT3_IRQHandler'),
        ('transmit-state','modbus_read',0x10001770 if IS12 else 0x10001790,'UART3_IRQHandler'),
        ('transmit-index','modbus_read',0x10001774 if IS12 else 0x10001794,'UART3_IRQHandler'),
        ('transmit-length','modbus_read',0x10001773 if IS12 else 0x10001793,'UART3_IRQHandler')):
        for access in ('read','write'):
            # 启停/写参/发送槽以真实写入为主要边界；输出门控以读取为主要边界。
            if (target.startswith('output') and (access=='read' or label in ('output-tick','output-fault'))) or (not target.startswith('output') and access=='write'):
                yield label,target,slot,irq,access

def fixture(new,target,channel):
    if target.startswith('output'):
        u,_=prepare(new,channel);store=Eeprom(L.BASE[new][1]);store.attach(u,new)
        displays=[];entry=V.SYMS['output_stage'] if new else OLD['output_stage'];key=0
        if target=='output_fault':
            word(u,0x100016c0-SHIFT,1);u.mem_write(0x100016c4-SHIFT,b'\0')
    else:
        u,store,displays,_=L.fixture(new,1,2,0,1,1)
        if target.startswith('state'):
            entry=V.SYMS['state_machine'] if new else OLD['state_machine'];key=5 if target=='state_start' else 0
            if target=='state_fault':
                u.mem_write(0x1000157d,b'\xfa');word(u,0x2009c034,0xfffeffff)
            elif target=='state_recover':
                word(u,L.FAULT,1);word(u,0x10001730 if IS12 else 0x10001750,100)
                u.mem_write(0x10002098 if IS12 else 0x100020c0,b'\x01\x01\x01')
        else:
            entry=V.SYMS['modbus_dispatch'] if new else OLD['modbus_dispatch'];key=0
            reg,val={'modbus_phase':(0x3c,1),'modbus_run':(0x27,1),'modbus_read':(2,1)}[target]
            func=3 if target=='modbus_read' else 6;wire=frame((1,func,0x10,reg,val>>8,val&255))
            state,length,buf=(0x10001770,0x10001772,0x10002278) if IS12 else (0x10001790,0x10001792,0x100022a4)
            u.mem_write(state,b'\x05');u.mem_write(length,bytes((len(wire),)));u.mem_write(buf,wire)
            u.mem_write(0x100016f7 if IS12 else 0x100016ff,b'\x01')
    for base in (0x40004000,0x4002c000):
        if not any(lo<=base<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(base,0x1000)
    # 运行中的同步状态作为初始条件；ISR自身决定是否进入保持超时/关断。
    word(u,0x10001fe0 if IS12 else 0x10002000,5)
    u.mem_write(FREQ,b'\x32');u.mem_write(FREQ+1,b'\0');u.mem_write(FREQ+2,b'\x09')
    word(u,0x1000209c if IS12 else 0x100020c4,4)
    word(u,0x4009c014,0x20);word(u,0x4009c008,2)
    return u,store,displays,entry,key

class AccessPoint(StoreTrace):
    def __init__(self,u,slot,kind,callback):
        self.slot=slot;self.kind=kind;self.callback=callback
        super().__init__(u,cache={})
    def code(self,u,a,n,user):
        if a not in self.cache:self.cache[a]=next(self.cs.disasm(bytes(u.mem_read(a,n)),a),None)
        ins=self.cache[a]
        if ins is None or not ins.mnemonic.startswith('ldr' if self.kind=='read' else 'str'):return
        op=next((o for o in ins.operands if o.type==ARM_OP_MEM),None)
        if op is None:return
        m=op.mem;base=self.reg(u,ins,m.base) if m.base else 0;index=self.reg(u,ins,m.index) if m.index else 0
        if op.shift.value:index<<=op.shift.value
        if (base+index+m.disp)&0xffffffff==self.slot:self.callback(u,a,ins)

def observe(new,spec,stage,channel,blocked=False,inject=True,mutant=False):
    label,target,slot,irq,access=spec
    # 先在独立机器发现语义指令及其后继；正式机器用until地址拆分翻译块。
    # CODE回调内emu_stop在Thumb IT块里可能延迟到函数返回，不能用它作为停点证据。
    discovery,_,_,dentry,dkey=fixture(new,target,channel)
    found=[];next_after=[False]
    def next_pc(m,a,n,user):
        if next_after[0]:next_after[0]=False;found.append(a)
    def locate(m,pc,ins):
        if found or next_after[0]:return
        if stage=='before':found.append(pc)
        else:next_after[0]=True
    discovery.hook_add(UC_HOOK_CODE,next_pc)
    AccessPoint(discovery,slot,access,locate)
    discovery.reg_write(UC_ARM_REG_R0,dkey);discovery.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(discovery,dentry,max_insn=1500000)
    assert len(found)==1,('共享量目标未执行',new,spec,stage,found)
    u,store,display,entry,key=fixture(new,target,channel)
    irqnum,oldentry=IRQS[irq]
    # 声明优先级与使能；主线程只插入一个已挂起、未屏蔽的中断。
    mode='primask' if blocked is True else blocked
    word(u,0xe000e100,0 if mode=='disabled' else 1<<irqnum);word(u,0xe000e200,1<<irqnum);u.mem_write(0xe000e400+irqnum,b'\x20')
    # UC_MODE_THUMB不自动提供Cortex-M异常屏蔽；这是调度器的显式CPU状态输入。
    # 不把对未实现PRIMASK/BASEPRI伪寄存器的写入冒充硬件生效。
    primask=int(mode=='primask');basepri=0x20 if mode=='basepri' else 0
    enabled=bool(readword(u,0xe000e100)&readword(u,0xe000e200)&(1<<irqnum))
    allowed=enabled and not primask and (not basepri or 0x20<basepri)
    assert bool(allowed)==(not bool(blocked)),'场景未满足声明的NVIC屏蔽前提'
    trace=StoreTrace(u,RANGES,cache=CACHE[new])
    u.reg_write(UC_ARM_REG_R0,key);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(entry|1,0x3ff00 if blocked else found[0],count=1500000)
    if blocked:
        assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'PRIMASK屏蔽无效'
    else:
        assert u.reg_read(UC_ARM_REG_PC)==found[0],('未在共享量边界暂停',new,spec,stage,found,hex(u.reg_read(UC_ARM_REG_PC)))
        context=u.context_save();pc=u.reg_read(UC_ARM_REG_PC);cut=len(trace.events)
        if inject:
            if mutant:u.mem_write(V.SYMS[irq] if new else oldentry,b'\x70\x47')
            u.reg_write(UC_ARM_REG_LR,0x3ff01)
            V.run(u,V.SYMS[irq] if new else oldentry,max_insn=300000)
            assert len(trace.events)>cut,'插入ISR没有实际MMIO副作用'
        u.context_restore(context)
        u.emu_start(pc|1,0x3ff00,count=1500000)
        assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'恢复后主函数未正常返回'
    # 中断可清故障/改变状态，这些原版竞态结果不强行改成手工安全模型。
    return bytes(u.mem_read(0x10000000,0x2900)),trace.events,display,store.events,bytes(store.image)

def main():
    L.seed();count=0
    for spec,stage,channel in itertools.product(tuple(scenarios()),('before','after'),(0,1)):
        # 拆分/恢复本身必须与不中断执行相同；特别覆盖IT条件执行块。
        for new in (False,True):
            assert observe(new,spec,stage,channel,inject=False)==observe(new,spec,stage,channel,blocked=True),('空抢占恢复改变业务',new,spec,stage,channel)
        a=observe(False,spec,stage,channel);b=observe(True,spec,stage,channel)
        if a!=b:
            result={'input':[spec,stage,channel],'parts':[i for i,(x,y) in enumerate(zip(a,b)) if x!=y],
                    'SRAM_diff':[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:20]}
            (ROOT/'tmp/shared_state_preemption_failure.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
            raise AssertionError(result)
        count+=1
    for spec,mode in itertools.product(tuple(scenarios())[::3],('primask','basepri','disabled')):
        assert observe(False,spec,'before',0,mode)==observe(True,spec,'before',0,mode);count+=1
    try:observe(True,tuple(scenarios())[0],'before',0,mutant=True)
    except AssertionError as e:assert '没有实际MMIO副作用' in str(e),repr(e)
    else:raise AssertionError('ISR空跑未被门禁拒绝')
    print('SHARED_STATE_PREEMPTION_MATRIX',count,'read/write boundaries + no-IRQ_resume + three_NVIC_blocks + empty_ISR_rejected PASS',flush=True)
    (ROOT/'tmp/shared_state_preemption_result.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'status':'PASS','automatic_exception_stacking':False},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
