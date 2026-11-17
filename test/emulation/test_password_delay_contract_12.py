"""十份原厂内联密码延时：仅加速内层，外层/喂狗真实执行。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from unicorn.arm_const import UC_ARM_REG_R3
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB

BODIES=(0x5220,0x536c,0x8988,0x8aa4,0x8aee,0x8b38,0x8bba,0x8c70,0x8cba,0x8d04)

def observe(new,body=None):
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
    if new:
        start=V.SYMS['sm6_delay_loop'];end=0x3ff00
        ins=list(cs.disasm(V.NEW[start:start+0x60],start))
    else:
        start=body
        ins=list(cs.disasm(V.ORIGINAL[start:start+0x60],start))
        upper=next(i.address for i in ins if i.mnemonic=='movw' and '#0x2710' in i.op_str)
        branch=next(i for i in ins if i.address>upper and i.mnemonic in ('blo','bcc'))
        end=branch.address+branch.size
    compares=[i.address for i in ins if i.mnemonic.startswith('cmp') and '#0x3e8' in i.op_str]
    assert len(compares)==1,('延时内层布局变化',new,body,compares)
    u=V.machine(new);word(u,0x10001614,0);word(u,0x10001618,77)
    feeds=[0];feed=V.SYMS['wd_feed'] if new else 0x238
    def code(m,a,n,user):
        if a==compares[0]:
            word(m,0x10001618,1000);m.reg_write(UC_ARM_REG_R3 if new else UC_ARM_REG_R0,1000)
        if a==feed:feeds[0]+=1
    u.hook_add(UC_HOOK_CODE,code);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(start|1,end,count=2000000)
    assert u.reg_read(UC_ARM_REG_PC)==end,'延时未正常退出'
    result=bytes(u.mem_read(0x10001614,8)),feeds[0],readword(u,0x40000000)
    assert result[0]==__import__('struct').pack('<II',10000,1000) and feeds[0]==10000,result
    return result

def main():
    expected=observe(True)
    for body in BODIES:assert observe(False,body)==expected,hex(body)
    print('PASSWORD_DELAY_12 inline_copies=10 outer=10000 inner=1000 real_watchdog_calls=10000 PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
