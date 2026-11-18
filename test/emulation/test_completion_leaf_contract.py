"""可控PLL等待、字符查表耗尽、读寄存器档位与认证应答的真实叶契约。"""
import sys,itertools
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'support'),str(Path(__file__).resolve().parent)]
from software_ab_support import *
from test_main_software_chain import old_map
from mmio_code_trace import StoreTrace
from unicorn import UC_HOOK_MEM_READ

def execute(new,name,args=(),values=(),skip=(),poll=False):
    u=V.machine(new);lookup=V.SYMS if new else old_map();events=[];reads={}
    if not new:lookup['EINT3_IRQHandler']=0xf78c if IS12 else 0xfa2c
    if not new and IS12:lookup.update({'lcd_ctrl_line':0x766,'disp_render_char16':0xbea,'disp_cmd':0x8fa,'disp_data':0x8a4,'chk_p02_p03':0x1b9e,'i2c_delay':0x1c32})
    for a in (0x40000000,0x40004000,0x4002c000):
        if not any(lo<=a<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(a,0x1000)
    for a,n,v in values:u.mem_write(a,v.to_bytes(n,'little'))
    skips={lookup[n]:n for n in skip}
    def code(m,a,n,user):
        if a in skips:
            events.append((skips[a],*(m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1))))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,code)
    if poll:
        def read(m,access,a,n,val,user):
            reads[a]=reads.get(a,0)+1
            mask={0x400fc1a0:0x40,0x400fc088:0x7000000,0x400fc0a8:0x700}[a]
            word(m,a,mask if reads[a]>=4 else 0)
        for a in (0x400fc1a0,0x400fc088,0x400fc0a8):u.hook_add(UC_HOOK_MEM_READ,read,begin=a,end=a+3)
    def store(m,a,event):
        if poll and event[0] in (0x400fc080,0x400fc0a0):
            reads[event[0]+8]=0
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x400fc000,0x400fc200)),on_store=store)
    for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(r,v)
    u.reg_write(UC_ARM_REG_LR,0x3ff01);V.run(u,lookup[name],max_insn=2000000)
    # VOID函数不比较未定义返回寄存器，业务SRAM与外设写迹仍完整比较。
    return bytes(u.mem_read(0x10000000,0x2600)),events,trace.events,reads

def main():
    count=0
    a=execute(False,'SystemInit',poll=True);b=execute(True,'SystemInit',poll=True)
    assert a[:3]==b[:3],('PLL等待',a[3],b[3]);assert all(v>=4 for v in a[3].values()) and all(v>=4 for v in b[3].values());count+=1
    for val in (0,1,0xffffffff,0x80000000):
        assert execute(False,'lcd_ctrl_line',(val,))==execute(True,'lcd_ctrl_line',(val,));count+=1
    # 未定义汉字必须遍历原字库并返回，不能把早期命中字形当查表穷尽证据。
    for hi,lo in ((0xff,0xff),(0xa1,0xa1),(0,0)):
        args=(hi,lo,0,0)
        assert execute(False,'disp_render_char16',args,skip=('Delay','disp_cmd','disp_data'))==execute(True,'disp_render_char16',args,skip=('Delay','disp_cmd','disp_data'));count+=1
    for profile,reg in itertools.product((0,1,2,3,4,5,255),range(64)):
        values=((0x10001708 if IS12 else 0x10001710,1,profile),(0x10003000,4,0x12345678))
        a=execute(False,'modbus_read_reg',(0x10003000,reg),values)
        b=execute(True,'modbus_read_reg',(0x10003000,reg),values)
        assert a==b,('read-profile',profile,reg);count+=1
    for level in (0,4,8,12):
        assert execute(False,'chk_p02_p03',values=((0x2009c014,4,level),))==execute(True,'chk_p02_p03',values=((0x2009c014,4,level),));count+=1
    for phase,fault in itertools.product((2,7),(0,1,0x8000,0xffffffff)):
        values=((0x10001fe0 if IS12 else 0x10002000,4,phase),
                (0x1000161c if IS12 else 0x10001624,4,fault),
                (0x10001fd8 if IS12 else 0x10001ff8,1,50))
        assert execute(False,'EINT3_IRQHandler',values=values)==execute(True,'EINT3_IRQHandler',values=values),('EINT3故障门控',phase,fault);count+=1
    if IS12:
        for units in (0,1,2):
            assert execute(False,'i2c_delay',(units,))==execute(True,'i2c_delay',(units,));count+=1
    print('COMPLETION_LEAF_CONTRACT',count,'PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())

