"""从执行中的STR指令采集MMIO写迹，不安装FIO的MEM_WRITE hook。

只观察实际执行的单寄存器STR/STRB/STRH；遇到其它MMIO store类型应显式扩展，
不能把未解码指令静默当作没有写。该观察器不提供硬件SET/CLR/W1C语义。
"""
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_MEM,ARM_OP_REG
from unicorn import UC_HOOK_CODE
from unicorn import arm_const as A

class StoreTrace:
    def __init__(self,u,ranges=((0x2009c000,0x2009c100),),cache=None,on_store=None):
        self.events=[];self.ranges=ranges;self.cache={} if cache is None else cache;self.cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);self.cs.detail=True
        self.on_store=on_store
        self.handle=u.hook_add(UC_HOOK_CODE,self.code)
    def reg(self,u,ins,r):
        name=ins.reg_name(r)
        if name=='pc':return (ins.address+4)&~3
        return u.reg_read(getattr(A,'UC_ARM_REG_'+name.upper()))
    def code(self,u,a,n,user):
        if a not in self.cache:
            ins=next(self.cs.disasm(bytes(u.mem_read(a,n)),a),None)
            self.cache[a]=ins if ins and ins.mnemonic.startswith(('str','stm')) else None
        ins=self.cache[a]
        if ins is None:return
        if ins.mnemonic.startswith('stm'):
            base=self.reg(u,ins,ins.operands[0].reg)
            span=4*(len(ins.operands)-1)
            assert not any(base-span < hi and base+span >= lo for lo,hi in self.ranges),('unsupported observed STM store',hex(a),ins.mnemonic,ins.op_str)
            return
        if len(ins.operands)<2 or ins.operands[1].type!=ARM_OP_MEM:
            for operand in ins.operands:
                if operand.type==ARM_OP_MEM:
                    addr=(self.reg(u,ins,operand.mem.base)+operand.mem.disp)&0xffffffff
                    assert not any(lo<=addr<hi for lo,hi in self.ranges),('unsupported observed store',hex(a),ins.mnemonic,ins.op_str)
            return
        op=ins.operands[1];m=op.mem
        base=self.reg(u,ins,m.base) if m.base else 0
        index=self.reg(u,ins,m.index) if m.index else 0
        if op.shift.value:index <<= op.shift.value
        addr=(base+index+m.disp)&0xffffffff
        if not any(lo<=addr<hi for lo,hi in self.ranges):return
        assert ins.operands[0].type==ARM_OP_REG and ins.mnemonic.startswith('str'),('unsupported MMIO store',ins.mnemonic,ins.op_str)
        size=1 if ins.mnemonic.startswith('strb') else 2 if ins.mnemonic.startswith('strh') else 4
        event=(addr,size,self.reg(u,ins,ins.operands[0].reg)&((1<<(size*8))-1))
        if self.on_store is not None and self.on_store(u,a,event) is False:return
        self.events.append(event)
