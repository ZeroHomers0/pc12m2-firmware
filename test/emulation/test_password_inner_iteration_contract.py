"""密码十份内联延时第一轮真实跑满1000次，后续仅加速内层；补真实循环双出口。"""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from menu_audit_support import *

def observe(new,spec):
    u=V.machine(new);map_watchdog(u);word(u,OUT,0);word(u,INNER,77)
    feeds=[0];real_compares=[0]
    def hook(m,a,n,user):
        if a==spec[1]:
            if readword(m,OUT)==0:real_compares[0]+=1
            else:word(m,INNER,1000);m.reg_write(UC_ARM_REG_R3 if new else UC_ARM_REG_R0,1000)
        if a==(V.SYMS['wd_feed'] if new else 0x238):feeds[0]+=1
    u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(spec[0]|1,spec[2],count=2000000)
    assert u.reg_read(UC_ARM_REG_PC)==spec[2],'延时没有完整退出'
    assert real_compares[0]>=1000,'第一轮内层被提前跳过'
    result=(readword(u,OUT),readword(u,INNER),feeds[0],readword(u,0x40000008))
    assert result==(10000,1000,10000,0x55),result
    return result

def main():
    expected=observe(True,NEW_DELAY)
    for spec in SPECS:assert observe(False,spec)==expected
    print('PASSWORD_INNER_ITERATION_CONTRACT original_inline=10 first_inner_1000_real+outer_10000+WDT_10000 PASS',flush=True)
    (ROOT/'tmp/password_inner_iteration_contract.json').write_text(json.dumps({'project':ROOT.name,'original_copies':10,'first_inner_real':1000,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
