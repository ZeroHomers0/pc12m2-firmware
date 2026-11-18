"""三种明确的FIOCLR读回探针下比较真实GPIO读/写序列，不推断硅片WO读行为。"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from mmio_code_trace import StoreTrace
from capstone.arm import ARM_OP_MEM
import test_peripheral_semantic_contract as P

class ReadTrace(StoreTrace):
    def code(self,u,a,n,user):
        if a not in self.cache:
            ins=next(self.cs.disasm(bytes(u.mem_read(a,n)),a),None)
            self.cache[a]=ins if ins and ins.mnemonic.startswith(('ldr','ldm')) else None
        ins=self.cache[a]
        if ins is None:return
        if ins.mnemonic.startswith('ldm'):
            base=self.reg(u,ins,ins.operands[0].reg)
            assert not 0x2009bf80<=base<0x2009c120,('未支持GPIO LDM',hex(a))
            return
        op=next((op for op in ins.operands if op.type==ARM_OP_MEM),None)
        if op is None:return
        m=op.mem;base=self.reg(u,ins,m.base) if m.base else 0
        index=self.reg(u,ins,m.index) if m.index else 0
        if op.shift.value:index<<=op.shift.value
        addr=(base+index+m.disp)&0xffffffff
        if not 0x2009c000<=addr<0x2009c0a0:return
        assert not ins.mnemonic.startswith('ldrd'),'未支持GPIO LDRD'
        size=1 if ins.mnemonic.startswith(('ldrb','ldrsb')) else 2 if ins.mnemonic.startswith(('ldrh','ldrsh')) else 4
        self.events.append((addr,size,int.from_bytes(u.mem_read(addr,size),'little')))

def observe(new,order,masked,readback):
    original=P.PeripheralModel;traces=[]
    class Probe(original):
        def gpio(self,p):
            super().gpio(p);P.put(self.u,0x2009c01c+32*p,readback)
        def __init__(self,u):
            super().__init__(u)
            for p in range(5):self.gpio(p)
            observer=ReadTrace(u);traces.append(observer.events)
    try:
        P.PeripheralModel=Probe
        result=P.observe(new,order,masked)
    finally:P.PeripheralModel=original
    assert len(traces)==1 and traces[0],'没有观察到实际GPIO读取'
    return result,traces[0]

def leaf(new,name,value,readback):
    u=V.machine(new)
    for port in range(5):
        P.put(u,0x2009c000+port*32,0xffffffff)
        P.put(u,0x2009c014+port*32,0xa5a55a5a)
    class Probe(P.PeripheralModel):
        def gpio(self,p):super().gpio(p);P.put(self.u,0x2009c01c+32*p,readback)
    model=Probe(u)
    for port in range(5):model.gpio(port)
    trace=ReadTrace(u);u.reg_write(UC_ARM_REG_R0,value);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS[name] if new else P.old_map()[name],max_insn=1000);model.flush()
    if value==0:assert any((a-0x2009c000)%32==28 for a,n,v in trace.events),'GPIO关断叶函数没有实际读取FIOCLR'
    return trace.events,model.events,model.latch

def main():
    orders=list(itertools.permutations(P.IRQS))[:24]+[tuple(reversed(P.IRQS)),tuple(P.IRQS)]
    count=reads=leaves=0
    for order,masked,readback in itertools.product(orders,(0,0x38),(0,0xffffffff,0x5aa55aa5)):
        a=observe(False,order,masked,readback);b=observe(True,order,masked,readback)
        assert a==b,('GPIO读回探针差异',order,masked,hex(readback),'read_equal',a[1]==b[1])
        count+=1;reads+=len(a[1])
    for name,value,readback in itertools.product(('fio0_pin22_ctrl','fio1_pin22_ctrl','fio1_pin23_ctrl'),(0,1),(0,0xffffffff,0x5aa55aa5)):
        assert leaf(False,name,value,readback)==leaf(True,name,value,readback),('GPIO叶函数读回差异',name,value,hex(readback))
        leaves+=1
    print('GPIO_READBACK_PROBE_CONTRACT schedules=%d original_GPIO_reads=%d real_clear_leaf_cases=%d three_explicit_WO_read_patterns PASS'%(count,reads,leaves),flush=True)
    (ROOT/'tmp/gpio_readback_probe_contract.json').write_text(json.dumps({'project':ROOT.name,'schedules':count,'original_GPIO_reads':reads,'leaf_cases':leaves,'readback_probes':[0,0xffffffff,0x5aa55aa5],'hardware_WO_read_behavior_proven':False,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
