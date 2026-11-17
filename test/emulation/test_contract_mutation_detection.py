"""在独立仿真机内注入指令缺陷，证明I2C位写序与TIMER1门禁能检出。

仅改仿真内存，不改源码、磁盘固件或原BIN；注入前后都执行真实机器码。
"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM
import test_i2c_bit_contract as I
import test_timer1_byte_domain as T

def mutation(symbol,imm,newimm):
    start=V.SYMS[symbol];end=min(a for a in V.SYMS.values() if a>start)
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    # 函数内可能夹字面量池，线性反汇编会在池内停止。逐半字候选解码，TIMER1再以实际执行集合过滤。
    decoded=[next(cs.disasm(V.NEW[a:a+4],a),None) for a in range(start,end,2)]
    candidates=[ins for ins in decoded if ins is not None and ins.mnemonic.split('.')[0] in ('mov','movs')
                and len(ins.operands)==2 and ins.operands[1].type==ARM_OP_IMM and ins.operands[1].imm==imm]
    if symbol=='TIMER1_IRQHandler':candidates=[ins for ins in candidates if ins.address in T.CACHES[True]]
    assert len(candidates)==1,('注入点结构变化',symbol,[(hex(i.address),i.op_str) for i in candidates])
    ins=candidates[0]
    # GCC可能选16位MOVS或32位MOV.W；只接受解码后寄存器/标志语义不变的单字节立即数变更。
    for pos in range(ins.size):
        for value in range(256):
            data=bytearray(ins.bytes);data[pos]=value
            decoded=next(cs.disasm(bytes(data),ins.address),None)
            if decoded and decoded.size==ins.size and decoded.mnemonic==ins.mnemonic and len(decoded.operands)==2 \
                    and decoded.operands[0].reg==ins.operands[0].reg and decoded.operands[1].type==ARM_OP_IMM \
                    and decoded.operands[1].imm==newimm:
                return ins.address,bytes(data)
    raise AssertionError(('无法构造经解码验证的立即数突变',symbol,hex(ins.address)))

def main():
    original=V.machine
    for symbol,imm,value,probe in (
        ('i2c_write_byte',8,7,lambda new:I.observe(new,'i2c_write_byte',0xa5,0,0x12340000)),
        ('TIMER1_IRQHandler',54,55,lambda new:T.observe(new,0,50,0,99,0,0))):
        old=probe(False);good=probe(True)
        assert old==good,'突变前基线本身不一致'
        pc,data=mutation(symbol,imm,value)
        def mutated(new):
            u=original(new)
            if new:u.mem_write(pc,data)
            return u
        V.machine=mutated
        try:
            # 本注入必须正常完成并产生可观察差异；预算/模型断言异常不能冒充检出。
            changed=probe(True)
            detected=(changed!=old)
            assert detected,('实际缺陷未被检出',symbol,hex(pc))
        finally:V.machine=original
        assert probe(True)==good,'恢复模型后基线发生变化'
        print('MUTATION_DETECTED',symbol,hex(pc),'imm',imm,'->',value,'PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
