"""真实输出保护→状态机封锁→输入恢复/再启动尝试，逐拍保持故障与控制历史。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from test_output_persistent_replay import prepare
from test_output_boundary_contract import patch,ADDR
from mmio_code_trace import StoreTrace

def observe(new,channel,family=None,mask=0):
    u,_=prepare(new,channel);store=Eeprom();store.attach(u,new)
    menu=0x10001724 if IS12 else 0x10001744
    u.mem_write(menu,b'\0\0\0')
    for port in (0x2009c014,0x2009c034,0x2009c074):word(u,port,0xffffffff&~0x200)
    patch(u,{'mode':3,'over_limit':0,'under_limit':0,'imb_limit':0,'fault':0})
    t=StoreTrace(u,((0x2009c000,0x2009c100),(0x40008000,0x40008100),(0x40090000,0x40090100),(0x400fc000,0x400fc200)))
    expected=mask
    if family:
        expected={'over':0x10,'under':0x20,'imb':8,'severe':0x200}[family]
        f='imb' if family=='severe' else family
        patch(u,{f+'_limit':100,f+'_time':0,f+'_count':0,'tick':9,
                 'current' if f=='imb' else 'voltage':251 if family=='severe' else (99 if family=='under' else 101)})
        call(u,new,'output_stage',budget=300000)
        assert readword(u,0x10000000+ADDR['fault'][0])&expected,'未实际触发目标保护'
    else:patch(u,{'fault':mask})
    states=[]
    for step,key in enumerate((0,0,5,7,0)):
        if step==1:patch(u,{'over_limit':0,'under_limit':0,'imb_limit':0,'voltage':100,'current':100})
        cut=len(t.events)
        call(u,new,'state_machine',(key,),budget=3000000)
        fault=readword(u,0x10000000+ADDR['fault'][0])
        assert fault&expected==expected,('故障锁存被清除',step,hex(fault),hex(expected))
        assert u.mem_read(0x10000000+ADDR['cfg'][0],1)[0]==0,'有故障仍允许输出配置开启'
        assert readword(u,0x10000000+ADDR['run'][0])==0,'有故障未清运行状态'
        # 逐引脚必须出现安全态写入；不能只比较旧新都没执行的末态。
        writes=t.events[cut:]
        for addr,bits in ((0x2009c018,0xf8180),(0x2009c058,0x3e0),(0x2009c01c,0x400000),(0x2009c03c,0x400000)):
            seen=0
            for a,n,value in writes:
                if a==addr:seen|=value
            assert seen&bits==bits,('故障封锁缺少安全引脚写入',step,hex(addr),hex(seen),hex(bits))
        patch(u,{'tick':9})
        call(u,new,'output_stage',budget=300000)
        states.append(bytes(u.mem_read(0x10000000,0x2600)))
    return states,t.events,store.events,bytes(store.image)

def main():
    count=0
    bits=[1<<n for n in range(16)]
    for channel in (0,1):
        for family in ('over','under','imb','severe'):
            assert observe(False,channel,family)==observe(True,channel,family),('保护封锁链差异',channel,family)
            count+=1
        for mask in [*bits,*[a|b for a,b in itertools.combinations(bits,2)],0xffff,0xffffffff]:
            assert observe(False,channel,mask=mask)==observe(True,channel,mask=mask),('故障组合封锁差异',channel,hex(mask))
            count+=1
    print('PROTECTION_SHUTDOWN_CHAIN scenarios=%d five_persistent_frames_each actual_safety_writes+fault_latch+restart_rejected PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
