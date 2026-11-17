"""菜单连续编辑/保存、密码取消和空闲超时；持久EEPROM与实际状态机共同执行。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from test_main_software_chain import DISPLAY6,DISPLAY12

def observe(new):
    s=Eeprom();u=setup(new,s);call(u,new,'load_config')
    menu=0x10001724 if IS12 else 0x10001744
    timeout=0x10001744 if IS12 else 0x10001764
    refresh=0x10001758 if IS12 else 0x10001778
    voltage=0x10001634 if IS12 else 0x1000163c
    tables={V.SYMS[n]:n for n in (DISPLAY12 if IS12 else DISPLAY6).values()} if new else (DISPLAY12 if IS12 else DISPLAY6)
    displays=[]
    def display(m,a,n,user):
        if a in tables:
            displays.append((tables[a],m.reg_read(UC_ARM_REG_R0),m.reg_read(UC_ARM_REG_R1),m.reg_read(UC_ARM_REG_R2),m.reg_read(UC_ARM_REG_R3)))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,display)
    for p in (0x2009c014,0x2009c034,0x2009c074):word(u,p,0xffffffff)
    u.mem_write(menu,bytes((3,1,0)));word(u,refresh,250)
    start=readword(u,voltage);events_before=len(s.events);snapshots=[]
    for key in (1,2,2,0,3,4):
        call(u,new,'state_machine',(key,),budget=4000000)
        snapshots.append(bytes(u.mem_read(0x10000000,0x2600)))
    edited=readword(u,voltage)
    assert edited!=start,'未实际编辑电压参数'
    assert u.mem_read(menu,1)[0]==2,'保存未退回参数菜单'
    assert any(e[0]=='commit' for e in s.events[events_before:]),'菜单保存未实际落盘'
    reboot=Eeprom(s.image);v=setup(new,reboot);call(v,new,'load_config')
    assert readword(v,voltage)==edited,'菜单保存后新CPU未加载编辑值'
    # 密码输入屏取消，不输入或强制匹配密码。
    before=bytes(s.image);event_start=len(s.events)
    u.mem_write(menu,bytes((0x62,0,0)))
    call(u,new,'state_machine',(4,),budget=4000000)
    assert u.mem_read(menu,1)[0]==1,'取消密码未回主屏'
    assert bytes(s.image)==before and not any(e[0]=='commit' for e in s.events[event_start:]),'密码取消意外写入介质'
    snapshots.append(bytes(u.mem_read(0x10000000,0x2600)))
    # 编辑态超时，跟踪本帧共享尾和回主屏。
    u.mem_write(menu,bytes((3,1,1)));word(u,timeout,4999);word(u,refresh,250)
    call(u,new,'state_machine',(0,),budget=4000000)
    assert u.mem_read(menu,1)[0]==1,'空闲超时未回主屏'
    snapshots.append(bytes(u.mem_read(0x10000000,0x2600)))
    assert displays,'没有实际执行显示调用'
    return snapshots,displays,s.events,bytes(s.image),parameters(v)

def main():
    a=observe(False);b=observe(True)
    assert a==b,('连续菜单流程差异',[(i,len(x),len(y)) for i,(x,y) in enumerate(zip(a,b)) if x!=y])
    print('MENU_PERSISTENT_WORKFLOW edit/save/reboot + password-cancel + timeout real-state-machine PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
