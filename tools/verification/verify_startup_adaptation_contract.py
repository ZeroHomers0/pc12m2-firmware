"""原IAR空钩子/semihost退出与GCC复位链终止适配；不视为业务返回等价。"""
import sys,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM

def main():
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    u=V.machine(False);before=bytes(u.mem_read(0x10000000,0x2600))
    instruction=next(cs.disasm(V.ORIGINAL[0x5ca:0x5cc],0x5ca))
    assert (instruction.mnemonic,instruction.op_str)==('bx','lr'),'空钩子原指令变化'
    u.reg_write(UC_ARM_REG_LR,0x3ff01);V.run(u,0x5ca,max_insn=10)
    assert bytes(u.mem_read(0x10000000,0x2600))==before
    print('STARTUP_IDLE_HOOK original_bx_lr NORMAL_RETURN no_SRAM_effect; GCC_optimized_away ADAPTATION')
    start=0x111cc if IS12 else 0x11028;trap=start+4;reached=[]
    def stop(m,a,n,user):
        if a==trap:reached.append(1);m.emu_stop()
    u=V.machine(False);u.hook_add(UC_HOOK_CODE,stop)
    u.emu_start(start|1,0x3ff00,count=10)
    i=next(cs.disasm(V.ORIGINAL[trap:trap+2],trap))
    assert reached and i.mnemonic=='bkpt' and i.operands[0].imm==0xab
    assert u.reg_read(UC_ARM_REG_R0)==0x18 and u.reg_read(UC_ARM_REG_R1)==0x20026
    print('IAR_EXIT EXPECTED_SEMIHOST_TRAP SYS_EXIT ADAPTATION; not_PASS_EQ_return')
    reset=struct.unpack_from('<I',V.NEW,4)[0]&~1
    instructions=list(cs.disasm(V.NEW[reset:reset+100],reset))
    maincall=next(i for i in instructions if i.mnemonic=='bl' and i.operands[0].type==ARM_OP_IMM and i.operands[0].imm==V.SYMS['main'])
    fallback=next(i for i in instructions if i.address==maincall.address+maincall.size)
    assert fallback.mnemonic=='b' and fallback.operands[0].imm==fallback.address
    u=V.machine(True);u.emu_start(fallback.address|1,0x3ff00,count=10)
    assert u.reg_read(UC_ARM_REG_PC)==fallback.address
    print('GCC_MAIN_RETURN_FALLBACK EXPECTED_NONRETURN_SELF_LOOP ADAPTATION')
    return 0
if __name__=='__main__':raise SystemExit(main())
