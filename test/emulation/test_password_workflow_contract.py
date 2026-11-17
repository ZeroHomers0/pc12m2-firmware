"""实际逐键输入→逐位比较→成功/逐位失败→延时反馈/EEPROM清密码/非返回。"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'support'))
from menu_audit_support import *
from mmio_code_trace import StoreTrace
from capstone.arm import ARM_OP_IMM

def observe(new,screen,key,pwd,wrong):
    store=Eeprom();u=setup(new,store);call(u,new,'load_config')
    menu=0x10001724 if IS12 else 0x10001744
    for p in (0x2009c014,0x2009c034,0x2009c074):word(u,p,0xffffffff&~0x200)
    u.mem_write(pwd,bytes((2,3,5,6,7,8)));u.mem_write(0x100015f2,b'\0'*6)
    u.mem_write(menu,bytes((screen,0,0)))
    calls,delays=install(u,new,store,attach_store=False)
    trace=StoreTrace(u,((0x2009c000,0x2009c100),))
    states=[]
    for pos,digit in enumerate((2,3,5,6,7,8)):
        call(u,new,'state_machine',(9 if pos==wrong else digit,),budget=3000000)
        assert u.mem_read(menu+1,1)[0]==pos+1,'未实际逐键写入密码缓冲'
        states.append(bytes(u.mem_read(0x10000000,0x2900)))
    cut=len(store.events);nonreturn=[False];hits={};cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    def loop(m,a,n,user):
        ins=next(cs.disasm(bytes(m.mem_read(a,n)),a),None)
        if ins and ins.mnemonic=='b' and ins.operands[0].type==ARM_OP_IMM and ins.operands[0].imm==a:
            hits[a]=hits.get(a,0)+1
            if hits[a]==5:nonreturn[0]=True;m.emu_stop()
    hook=u.hook_add(UC_HOOK_CODE,loop)
    u.reg_write(UC_ARM_REG_R0,key);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(address(new,'state_machine')|1,0x3ff00,count=3000000)
    u.hook_del(hook)
    expected_nonreturn=screen==6 and wrong is None
    assert nonreturn[0]==expected_nonreturn,'密码路径非返回分类不符'
    if not expected_nonreturn:assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'有界密码路径未正常返回'
    commits=[(e[1],e[2]) for e in store.events[cut:] if e[0]=='commit']
    if expected_nonreturn:
        assert len(delays)==3,'密码正确未经过三段反馈延时'
        assert commits==([(5,0),(6,0)] if key==1 else [(5,0),(6,0),(7,0),(8,0)]),commits
    else:
        assert len(delays)==(0 if wrong is None else 1)
        expected_menu=(2 if screen==0xa else 0x63) if wrong is None else (2 if screen==6 else 1)
        assert u.mem_read(menu,1)[0]==expected_menu,'密码处理后屏号错误'
        assert not commits,'非复位密码路径意外改写介质'
    states.append(bytes(u.mem_read(0x10000000,0x2900)))
    return states,calls,delays,trace.events,store.events,bytes(store.image),'EXPECTED_NONRETURN' if nonreturn[0] else 'NORMAL_RETURN'

def main():
    count=0
    for screen,key,pwd in ((0xa,1,0x100015e0),(0x62,1,0x100015ec),(6,1,0x100015e6),(6,14,0x100015ec)):
        for wrong in (None,0,1,2,3,4,5):
            assert observe(False,screen,key,pwd,wrong)==observe(True,screen,key,pwd,wrong),(screen,key,wrong)
            count+=1
    print('PASSWORD_WORKFLOW_CONTRACT cases=%d real_six_key_entry+each_mismatch_position+success+EEPROM_order+expected_nonreturn PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
