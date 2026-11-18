"""真实main启动门控、未就绪节拍、联锁循环与认证永久放行的限定定制。

认证叶响应替身只用于main边界；认证叶自身由设备模型独立实际执行。
"""
import sys
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'support'),str(Path(__file__).resolve().parent)]
from software_ab_support import *
from test_main_software_chain import old_map,DISPLAY6,DISPLAY12
from unicorn import UC_HOOK_MEM_READ

def observe(new,interlock,auth_failure,enabled,retry):
    u=V.machine(new);store=Eeprom();store.attach(u,new);lookup=V.SYMS if new else old_map()
    if not new:
        lookup.update({n:a for a,n in (DISPLAY12 if IS12 else DISPLAY6).items()})
        lookup['disp_clear']=0x942 if IS12 else 0x992
    for a in (0x40000000,0x40004000,0x4002c000,0x40034000):
        if not any(lo<=a<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(a,0x1000)
    pins=0xffffffff&~4 if not enabled else 0xffffffff
    if interlock:pins&=~12
    word(u,0x2009c014,pins);word(u,0x2009c034,0xffffffff);word(u,0x2009c074,0xffffffff)
    display={lookup[n]:n for n in (DISPLAY12 if IS12 else DISPLAY6).values()}
    display[lookup['disp_clear']]='disp_clear'
    events=[];feeds=[0];outputs=[0];tickreads=[0];stopped=[False];load_return=[]
    def code(m,a,n,user):
        if load_return and a==load_return[-1]:
            load_return.pop()
            if not IS12:
                word(m,0x10001734,retry)
                # 认证被跳过的入口使用已授权的初始认证通过标志；失败场景另行保留锁机。
                word(m,0x1000172c,int(not auth_failure))
        if a==lookup['load_config']:load_return.append(m.reg_read(UC_ARM_REG_LR)&~1)
        if a in display:
            events.append(display[a]);m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        if a==lookup['long_delay']:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        if a==lookup['auth_challenge']:
            m.mem_write(0x100020c0 if IS12 else 0x100020e8,bytes((auth_failure,)))
            if not IS12:word(m,0x1000172c,int(not auth_failure))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        if a==lookup['output_stage']:outputs[0]+=1
        if a==lookup['wd_feed']:
            feeds[0]+=1
            if feeds[0]>=2:stopped[0]=True;m.emu_stop()
    u.hook_add(UC_HOOK_CODE,code)
    def read(m,access,a,n,v,user):
        if a==0x10000006:
            tickreads[0]+=1;m.mem_write(a,b'\x01' if tickreads[0]%3==0 else b'\0')
        elif a==0x400fc1a0:word(m,a,readword(m,a)|0x40)
        elif a==0x400fc088:word(m,a,readword(m,a)|0x7000000)
        elif a==0x400fc0a8:word(m,a,readword(m,a)|0x700)
        elif a==0x4009c014:word(m,a,0x20)
        elif a==0x40034004:word(m,a,0x80001230)
    for a in (0x10000006,0x400fc1a0,0x400fc088,0x400fc0a8,0x4009c014,0x40034004):u.hook_add(UC_HOOK_MEM_READ,read,begin=a,end=a+3)
    u.emu_start((lookup['main'] if new else 0x5cc)|1,0,count=5000000)
    assert stopped[0] and feeds[0]==2,'main未到达预期运行/联锁/认证锁机观察点'
    expected_lock=(not new and auth_failure)
    if expected_lock:assert outputs[0]==0 and tickreads[0]==0,'原认证失败应停在原锁机循环'
    elif interlock:assert outputs[0]==0 and tickreads[0]>=6,'认证放行扩散到硬件安全联锁'
    else:assert outputs[0]==2 and tickreads[0]>=6,('主业务或未就绪等待未实际执行',new,interlock,auth_failure,enabled,retry,outputs[0],tickreads[0],events)
    return outputs[0],tickreads[0],events

def main():
    count=0
    for interlock in (False,True):
        for enabled in (0,1):
            for retry in (0,3):
                a=observe(False,interlock,0,enabled,retry);b=observe(True,interlock,0,enabled,retry)
                assert a==b,('main安全/正常路径',interlock,enabled,retry,a,b);count+=1
        a=observe(False,interlock,1,0,0);b=observe(True,interlock,1,0,0)
        assert a[0]==0 and b[0]==(0 if interlock else 2),'授权放行范围错误';count+=1
    print('MAIN_MODES_CONTRACT PASS_EQ=8 PASS_CUSTOM=2 bounded_observation',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())

