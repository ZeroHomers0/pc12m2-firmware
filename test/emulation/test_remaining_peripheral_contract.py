"""剩余ADC参考输入、UART配置/超时和信息屏条件的执行级契约。

ADC仅提供GDR输入，真实执行start/wait/scan；UART用指令写迹避免FIO内存hook。
信息屏只截显示叶，比较调用参数，不据此声称LCD整链实际执行。
"""
import sys,itertools
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'support'),str(Path(__file__).resolve().parent)]
from software_ab_support import *
from test_main_software_chain import old_map,DISPLAY6,DISPLAY12
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def mapped(u,a):
    if not any(lo<=a<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(a,0x1000)

def adc(new,mode,cfg,control,sample):
    u=V.machine(new);mapped(u,0x40034000)
    shift=8 if IS12 else 0
    for a,v in ((0x165a,mode),(0x1628,cfg),(0x1634,control)):u.mem_write(0x10000000+a-shift,bytes((v,)))
    for a,v in ((0x1638,975),(0x163c,1234),(0x1640,1111),(0x1698,3911),(0x16a0,3929),(0x16a8,3943),(0x16b0,3953),(0x16b8,3967)):
        word(u,0x10000000+a-shift,v)
    # 原样本/平均数组提前填充同一参考电平，避免十点平均稀释目标边界。
    # 两项目数组相对布局不同，由本机原指令literal登记的指针头取得。
    import re
    if IS12:
        # 12M扫描数组与6M相差0x2C，索引字段相同。
        ref,avg=0x1000223c,0x10002250
    else:ref,avg=0x10002268,0x1000227c
    for a,n in ((ref,5),(avg,10)):
        for i in range(n):word(u,a+i*4,sample)
    seen=[];current=[0]
    start=V.SYMS['adc0_start'] if new else (0x1f30 if IS12 else 0x1f80)
    def code(m,a,n,user):
        if a==start:
            ch=readword(m,0x40034000)&63
            current[0]=sample if ch==0x20 else (ch*97+123)&4095
            seen.append((ch,current[0]))
    def read(m,access,a,n,value,user):word(m,a,0x80000000|(current[0]<<4))
    u.hook_add(UC_HOOK_CODE,code,begin=start,end=start)
    u.hook_add(UC_HOOK_MEM_READ,read,begin=0x40034004,end=0x40034007)
    results=[]
    for i in range(6):
        call(u,new,'adc0_scan_channels')
        results.append(bytes(u.mem_read(0x10000000,0x2600)))
    assert len(seen)==36 and {ch for ch,s in seen}=={1,2,4,8,16,32}
    return results,seen,bytes(u.mem_read(0x40034000,0x40))

def uart(new,index,parity,timeout=False):
    u=V.machine(new);mapped(u,0x4002c000)
    delta=0x20 if IS12 else 0
    word(u,0x10001700-(8 if IS12 else 0),index)
    u.mem_write(0x10001704-(8 if IS12 else 0),bytes((parity,)))
    word(u,0x4009c00c,0x5a);word(u,0x4009c004,0x80)
    if timeout:
        detection,gap,tick,txstate,busy=timeout
        u.mem_write(0x10001705-(8 if IS12 else 0),bytes((detection,)))
        word(u,0x1000178c-delta,gap);word(u,0x100017b8-delta,tick)
        u.mem_write(0x10001790-delta,bytes((txstate,busy)))
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x40000000,0x40100000),(0xe000e000,0xe000f000)))
    name='uart3_rx_timeout_monitor' if timeout else 'uart3_init'
    u.reg_write(UC_ARM_REG_R0,21);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS[name] if new else old_map()[name],max_insn=300000)
    return bytes(u.mem_read(0x10000000,0x2600)),trace.events,bytes(u.mem_read(0x4009c000,0x40))

def screen(new,name,values):
    u=V.machine(new);shift=8 if IS12 else 0
    for a,n,v in values:
        if a==0x1745:a-=0x20 if IS12 else 0
        else:a-=shift
        u.mem_write(0x10000000+a,v.to_bytes(n,'little'))
    table=dict(DISPLAY12 if IS12 else DISPLAY6);table.pop(0x40b0 if IS12 else 0x427c,None)
    if new:table={V.SYMS[n]:n for n in table.values()}
    trace=[]
    def hook(m,a,n,user):
        if a in table:
            trace.append((table[a],*(m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3))))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    old=(0x40b0 if IS12 else 0x427c) if name=='disp_splash_screen' else (0x41ec if IS12 else 0x44c2)
    V.run(u,V.SYMS[name] if new else old,max_insn=100000)
    assert trace,'信息屏必须真实调用显示函数'
    return trace,bytes(u.mem_read(0x10000000,0x2600))

def main():
    count=0
    for spec in itertools.product((0,1,2,255),(0,1),(0,1,2),(0,1,9,10,39,40,799,800,807,811,812,815,816,831,3960,4000,4095)):
        a=adc(False,*spec);b=adc(True,*spec);assert a==b,('ADC',spec);count+=1
    for spec in itertools.product(range(8),(0,1,2,3,4,255)):
        assert uart(False,*spec)==uart(True,*spec),('UART',spec);count+=1
    for spec in itertools.product((0,1),(0,29999,30000,30001),(0,299,300,301),(0,1,5),(0,9,10,11,255)):
        assert uart(False,0,0,spec)==uart(True,0,0,spec),('UART-timeout',spec);count+=1
    for source,cfg,control,fault in itertools.product((0,1,2,3,255),(0,1,2),(0,1,2,3),(0,1)):
        values=((0x1655,1,source),(0x1628,1,cfg),(0x1634,1,control),(0x1624,4,fault))
        assert screen(False,'disp_splash_screen',values)==screen(True,'disp_splash_screen',values),('splash',values);count+=1
    for cursor in (0,1,2,3,4,255):
        values=((0x1745,1,cursor),)
        assert screen(False,'disp_screen_calib',values)==screen(True,'disp_screen_calib',values),('calibration',cursor);count+=1
    print('REMAINING_PERIPHERAL_CONTRACT',count,'PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())

