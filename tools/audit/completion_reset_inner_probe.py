"""两屏故障复位两副本：每段第一外层的2000次内层真实执行，其余纯空转加速。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
import test_state_lifecycle_contract as L
from test_state_lifecycle_contract import *

def reset(new,screen,mask,enabled=True,real_first_inner=False):
    u,store,displays,trace=fixture(new,screen,2,0,0 if enabled else 1,0)
    word(u,FAULT,mask);u.mem_write(0x1000157f,b'\x32')
    word(u,0x2009c034,0xffffffff&~0x20000)
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    decoded={};feeds=[0];inner=[0];stopped=[False];selfhits={}
    wd=V.SYMS['wd_feed'] if new else 0x238
    def code(m,a,n,user):
        if a==wd:feeds[0]+=1
        if a not in decoded:
            i=next(cs.disasm(bytes(m.mem_read(a,n)),a),None);action=None
            if i and i.mnemonic.startswith('cmp') and len(i.operands)==2 and i.operands[1].type==ARM_OP_IMM and i.operands[1].imm==2000:
                action=('inner',getattr(ARM,'UC_ARM_REG_'+i.reg_name(i.operands[0].reg).upper()))
            elif i and i.mnemonic=='b' and i.operands[0].type==ARM_OP_IMM and i.operands[0].imm==a:action=('self',0)
            decoded[a]=action
        action=decoded[a]
        if action and action[0]=='inner':
            inner[0]+=1
            if not (real_first_inner and readword(m,OUT)==0):
                word(m,INNER,2000);m.reg_write(action[1],2000)
        elif action and action[0]=='self':
            selfhits[a]=selfhits.get(a,0)+1
            if selfhits[a]==5:stopped[0]=True;m.emu_stop()
    u.hook_add(UC_HOOK_CODE,code)
    u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(address(new,'state_machine')|1,0x3ff00,count=1000000)
    if enabled:
        assert stopped[0] and readword(u,FAULT)==0,'未实际清故障并进入自跳'
        # 原机while在循环前多比较一次，GCC do/while每次递增后比较；语义轮数均2000。
        expected_comparisons=(9998 if new else 10000) if real_first_inner else 6000
        assert (readword(u,OUT),readword(u,INNER),feeds[0],inner[0])==(3000,2000,6000,expected_comparisons),('复位延时/喂狗不符',feeds,inner)
        wd_writes=[(size,value) for a,size,value in trace.events if a==0x40000008]
        assert len(wd_writes)==12000 and [v for size,v in wd_writes]==[0xaa,0x55]*6000,'未实际产生6000份AA/55喂狗写序'
        assert u.mem_read(CFG,1)==b'\0'
    else:
        assert not stopped[0] and u.reg_read(UC_ARM_REG_PC)==0x3ff00,'禁用复位不应自跳'
        assert readword(u,FAULT)==mask and feeds[0]==0,'辅助模式意外清故障'
    inner_contract=[5998,4000] if real_first_inner else inner
    return snapshot(u),displays,trace.events,store.events,bytes(store.image),feeds,inner_contract,'EXPECTED_NONRETURN' if stopped[0] else 'NORMAL_RETURN'

def main():
    L.seed()
    for screen in (1,0x1e):
        a=reset(False,screen,1,True,real_first_inner=True)
        b=reset(True,screen,1,True,real_first_inner=True)
        L.compare(('复位首轮真实内层',screen),a,b)
    print('RESET_REAL_FIRST_INNER 2 screens / 4 original loops actual_2000_inner + 6000_feeds_each PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
