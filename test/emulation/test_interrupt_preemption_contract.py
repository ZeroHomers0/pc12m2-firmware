"""关键共享量写入前/后抢占：保存CPU上下文，实际执行ISR，再恢复被打断函数。

有限语义调度：TIMER1扫描自增被TIMER2复位打断；输出节拍自增被EINT3打断。
不声称穷举所有指令交错。抢占期间保持SRAM与外设副作用，恢复的只有CPU上下文。
"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from mmio_code_trace import StoreTrace
from test_output_persistent_replay import prepare
from unicorn import UC_HOOK_CODE

CACHES={False:{},True:{}}
def execute(new,target,stage,channel):
    if target=='output_stage':u,_=prepare(new,channel)
    else:
        u=V.machine(new)
        for a,v in ((0x1000204c if IS12 else 0x10002074,18 if IS12 else 38),
                    (0x1000204d if IS12 else 0x10002075,1),
                    (0x10001fd8 if IS12 else 0x10001ff8,50)):
            u.mem_write(a,bytes((v,)))
    point=(0x10002051 if IS12 else 0x10002078) if target=='output_stage' else (0x1000204c if IS12 else 0x10002074)
    tentry=V.SYMS[target] if new else ((0xe70c if IS12 else 0xe9ac) if target=='output_stage' else (0xfb0c if IS12 else 0xff6c))
    irq='EINT3_IRQHandler' if target=='output_stage' else 'TIMER2_IRQHandler'
    ientry=V.SYMS[irq] if new else ((0xf78c if IS12 else 0xfa2c) if target=='output_stage' else (0xfae8 if IS12 else 0xff48))
    armed=[True];after=[False];paused=[False]
    def store(m,pc,event):
        if armed[0] and event[0]==point:
            armed[0]=False
            if stage=='before':paused[0]=True;m.emu_stop();return False
            after[0]=True
    def next_instruction(m,a,n,user):
        if after[0]:after[0]=False;paused[0]=True;m.emu_stop()
    # 下一指令钩子先于store观察器，after只在目标STR执行之后生效。
    u.hook_add(UC_HOOK_CODE,next_instruction)
    trace=StoreTrace(u,((0x10000000,0x10002600),(0x2009c000,0x2009c100),
                       (0x40008000,0x40008100),(0x40090000,0x40090100),(0x400fc000,0x400fc200)),cache=CACHES[new],on_store=store)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(tentry|1,0x3ff00,count=300000)
    assert paused[0] and u.reg_read(UC_ARM_REG_PC)!=0x3ff00,'未命中抢占窗口'
    context=u.context_save();pc=u.reg_read(UC_ARM_REG_PC)
    cut=len(trace.events)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(ientry|1,0x3ff00,count=300000)
    assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'抢占ISR未返回'
    assert len(trace.events)>cut,'抢占ISR没有实际副作用'
    u.context_restore(context)
    u.emu_start(pc|1,0x3ff00,count=300000)
    assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'恢复后被打断函数未返回'
    final=bytes(u.mem_read(0x10000000,0x2600))
    final+=bytes(u.mem_read(0x2009c000,0x100))+bytes(u.mem_read(0x40008000,0x100))+bytes(u.mem_read(0x40090000,0x100))
    return trace.events,final

def main():
    count=0
    for target in ('TIMER1_IRQHandler','output_stage'):
        for stage in ('before','after'):
            for channel in (0,1):
                a=execute(False,target,stage,channel);b=execute(True,target,stage,channel)
                assert a==b,('抢占差异',target,stage,channel,len(a[0]),len(b[0]),a[0][:12],b[0][:12])
                count+=1
    print('INTERRUPT_PREEMPTION cases=%d shared-store before/after real-ISR context-save/restore PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
