"""真实ISR在SET/CLR、IR W1C、NVIC使能/挂起模型下的有限顺序比较。

先以小段真实STR指令验证模型，随后执行原BIN与新ELF；不以模型输出代替原BIN。
PRIGROUP=0，八位优先级仅高5位有效；未模拟异常栈帧与自动抢占。
"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from peripheral_semantics import PeripheralModel,get,put
from test_main_software_chain import old_map

def bus(u,model,addr,value,size=4):
    # str/strh/strb r1,[r0]; bx lr：副作用由实际指令触发。
    opcode={4:b'\x01\x60',2:b'\x01\x80',1:b'\x01\x70'}[size]
    u.mem_write(0x3f000,opcode+b'\x70\x47')
    u.reg_write(UC_ARM_REG_R0,addr);u.reg_write(UC_ARM_REG_R1,value)
    u.reg_write(UC_ARM_REG_LR,0x3ff01);V.run(u,0x3f000,max_insn=100)
    model.flush()

def prove_model():
    u=V.machine(False);p=PeripheralModel(u)
    bus(u,p,0x2009c000,0xffffffff)
    bus(u,p,0x2009c018,0x12340000);bus(u,p,0x2009c018,5)
    assert get(u,0x2009c018)==0x12340005
    bus(u,p,0x2009c01c,1);assert get(u,0x2009c018)==0x12340004
    bus(u,p,0x2009c010,4);bus(u,p,0x2009c01c,4)
    assert get(u,0x2009c018)==0x12340004 and get(u,0x2009c014)==0x12340000
    bus(u,p,0x2009c010,0);bus(u,p,0x2009c019,0x80,1)
    assert get(u,0x2009c018)==0x12348004
    p.raise_timer(0x40008000,0xa5);bus(u,p,0x40008000,5)
    assert get(u,0x40008000)==0xa0
    put(u,0x40008008,123);put(u,0x40008010,7);bus(u,p,0x40008004,2)
    assert get(u,0x40008008)==get(u,0x40008010)==0
    bus(u,p,0xe000e100,2);bus(u,p,0xe000e100,8)
    assert get(u,0xe000e100)==10
    bus(u,p,0xe000e180,2);assert get(u,0xe000e100)==8
    p.pend(3);assert p.next_irq()==3 and p.next_irq(primask=1) is None
    bus(u,p,0xe000e100,2);p.pend(1)
    u.mem_write(0xe000e401,b'\x40');u.mem_write(0xe000e403,b'\x20')
    assert p.next_irq()==3 and p.next_irq(basepri=0x20) is None
    assert p.next_irq(active_priority=0x20) is None
    p.accept(3);assert p.next_irq()==1
    bus(u,p,0xe000e280,2);assert p.next_irq() is None

IRQS={1:'TIMER0_IRQHandler',2:'TIMER1_IRQHandler',3:'TIMER2_IRQHandler',19:'EINT1_IRQHandler',20:'EINT2_IRQHandler',21:'EINT3_IRQHandler'}
ORIG=({1:0x29a,2:0xfb0c,3:0xfae8,19:0xf748,20:0xf76a,21:0xf78c} if IS12 else
      {1:0x29a,2:0xff6c,3:0xff48,19:0xf9e8,20:0xfa0a,21:0xfa2c})

def observe(new,order,masked):
    u=V.machine(new)
    if not any(lo<=0x4002c000<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(0x4002c000,0x1000)
    for port in range(5):put(u,0x2009c000+port*32,0xffffffff)
    p=PeripheralModel(u);calls=[];states=[]
    for name in ('timer0_init','timer1_init','timer2_init','eint1_init','eint2_init','eint3_init'):
        u.reg_write(UC_ARM_REG_LR,0x3ff01)
        V.run(u,V.SYMS[name] if new else old_map()[name],max_insn=100000);p.flush()
    assert p.enabled[0]&sum(1<<x for x in IRQS)==sum(1<<x for x in IRQS),'初始化未实际累加NVIC使能'
    for irq in IRQS:u.mem_write(0xe000e400+irq,bytes((0x20+8*order.index(irq),)))
    for irq in IRQS:p.pend(irq)
    assert p.next_irq(primask=1) is None
    while (irq:=p.next_irq(basepri=masked)) is not None:
        p.accept(irq)
        if irq in (1,2,3):p.raise_timer({1:0x40004000,2:0x40008000,3:0x40090000}[irq],0xa5)
        else:p.raise_eint(1<<(irq-18))
        u.reg_write(UC_ARM_REG_LR,0x3ff01)
        V.run(u,V.SYMS[IRQS[irq]] if new else ORIG[irq],max_insn=300000);p.flush()
        calls.append(irq);states.append(bytes(u.mem_read(0x10000000,0x2600)))
    if masked:assert len(calls)<6 and any(p.irq_pending),'BASEPRI未实际阻止低优先级中断'
    else:assert calls==list(order),'实际调度顺序不符合优先级'
    return calls,states,p.events,p.latch,p.enabled,p.irq_pending,p.ir,p.ext

def main():
    prove_model();count=0
    orders=list(itertools.permutations(IRQS))[:24]+[tuple(reversed(IRQS)),tuple(IRQS)]
    for order,mask in itertools.product(orders,(0,0x38)):
        a=observe(False,order,mask);b=observe(True,order,mask)
        assert a==b,('寄存器语义模型下ISR差异',order,mask,[i for i,(x,y) in enumerate(zip(a,b)) if x!=y])
        count+=1
    print('PERIPHERAL_SEMANTIC_CONTRACT model_STR_probes+NVIC_priority_mask+real_ISR_schedules=%d PASS'%count,flush=True)
    (ROOT/'tmp/peripheral_semantic_contract.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'status':'PASS','FIOCLR_read_assumption':0,'NVIC_grouping':0},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
