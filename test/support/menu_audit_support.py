"""菜单分层差分支持：显示叶调用观测、EEPROM介质及已独立验证的密码延时摘要。"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'emulation'))
from software_ab_support import *
from test_main_software_chain import DISPLAY6,DISPLAY12
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R3

OUT=0x10001614 if IS12 else 0x1000161c
INNER=OUT+4
OLD_UPPERS=(0x5256,0x539a,0x89b6,0x8ad2,0x8b1c,0x8b66,0x8be8,0x8c9e,0x8ce8,0x8d32) if IS12 else (0x549e,0x55e0,0x8c88,0x8d40,0x8d8a,0x8dd4,0x8e56,0x8f0c,0x8fda,0x9026)

def delay_specs():
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
    specs=[]
    for upper in OLD_UPPERS:
        ins=list(cs.disasm(V.ORIGINAL[upper:upper+12],upper))
        assert ins[0].mnemonic=='movw' and '#0x2710' in ins[0].op_str
        branch=next(i for i in ins if i.mnemonic in ('bcc','blo'))
        body=int(branch.op_str.removeprefix('#'),16)
        body_ins=list(cs.disasm(V.ORIGINAL[body:upper],body))
        comparisons=[i for i in body_ins if i.mnemonic.startswith('cmp')]
        assert len(comparisons)==1
        specs.append((body,comparisons[0].address,branch.address+branch.size))
    start=V.SYMS['sm6_delay_loop' if IS12 else 'delay_for_password_feedback']
    ins=list(cs.disasm(V.NEW[start:start+0x70],start))
    compare=next(i.address for i in ins if i.mnemonic.startswith('cmp') and '#0x3e8' in i.op_str)
    return specs,(start,compare,0x3ff00)

SPECS,NEW_DELAY=delay_specs()

def map_watchdog(u):
    if not any(lo<=0x40000008<=hi for lo,hi,perms in u.mem_regions()):u.mem_map(0x40000000,0x1000)

def prove_delay(new,spec):
    u=V.machine(new);map_watchdog(u);word(u,OUT,0);word(u,INNER,77);feeds=[0]
    def hook(m,a,n,user):
        if a==spec[1]:word(m,INNER,1000);m.reg_write(UC_ARM_REG_R3 if new else UC_ARM_REG_R0,1000)
        if a==(V.SYMS['wd_feed'] if new else 0x238):feeds[0]+=1
    u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(spec[0]|1,spec[2],count=2000000)
    assert u.reg_read(UC_ARM_REG_PC)==spec[2],'密码延时未完整退出'
    result=(readword(u,OUT),readword(u,INNER),feeds[0],readword(u,0x40000008))
    assert result==(10000,1000,10000,0x55),result
    return result

def install(u,new,store,attach_store=True):
    map_watchdog(u)
    calls=[];delays=[]
    table=dict(DISPLAY12 if IS12 else DISPLAY6)
    table.update({0x942 if IS12 else 0x992:'disp_clear',0x40b0 if IS12 else 0x427c:'disp_splash_screen',
                  0xd44 if IS12 else 0xd94:'disp_digit',0xaf4 if IS12 else 0xb44:'disp_render_char8'})
    if new:table={V.SYMS[name]:name for name in table.values()}
    compares={s[1] for s in SPECS}
    def code(m,a,n,user):
        if a in table:
            name=table[a];argc=0 if name in ('disp_clear','disp_splash_screen') else 1 if name=='disp_digit' else 4
            args=tuple(m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3))[:argc]
            calls.append((name,*args));m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        elif new and a==NEW_DELAY[0]:
            delays.append((10000,1000,10000));word(m,OUT,10000);word(m,INNER,1000);m.mem_write(0x40000008,b'\x55')
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        elif not new and a in compares:
            delays.append((10000,1000,10000));word(m,OUT,9999);word(m,INNER,1000);m.reg_write(UC_ARM_REG_R0,1000)
            # 最后一轮仍实际执行，保留原inline循环的寄存器与最终喂狗副作用。
    u.hook_add(UC_HOOK_CODE,code)
    if attach_store:store.attach(u,new)
    return calls,delays
