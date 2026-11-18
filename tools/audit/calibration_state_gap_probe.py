"""按剩余原条件分支定向补校准结果、微调、故障显示及主屏手动给定路径。"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import menu_complete_matrix as M
from test_menu_parameter_gap_contract import compare
from mmio_code_trace import StoreTrace
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB

ORIGINAL={}
FREQ_OLD=0xa856 if IS12 else 0xab48
first_call=next(Cs(CS_ARCH_ARM,CS_MODE_THUMB).disasm(V.ORIGINAL[FREQ_OLD+4:FREQ_OLD+8],FREQ_OLD+4))
assert first_call.mnemonic=='bl' and int(first_call.op_str[1:],16)==(0x1a68 if IS12 else 0x1ab8),'频率微调入口必须实际调用本机P0.9去抖'
def seed(values):
    for new in (False,True):
        state,image=ORIGINAL[new];state=bytearray(state)
        for addr,size,value in values:state[addr:addr+size]=(value&((1<<(size*8))-1)).to_bytes(size,'little')
        M.BASE[new]=(bytes(state),image)

def run(screen,item,key,view,refresh,label):
    compare(label,M.observe(False,screen,item,key,view,refresh),M.observe(True,screen,item,key,view,refresh))

def frequency(new,value,key,level,equal):
    state,image=ORIGINAL[new];u=V.machine(new);u.mem_write(0x10000000,state)
    store=Eeprom(image);calls,delays=M.install(u,new,store)
    live=0x10001624 if IS12 else 0x1000162c
    word(u,live,value);word(u,live+4,value if equal else value^0x1234)
    u.mem_write(0x1000157b,b'\x0f');word(u,0x2009c014,0x200 if level else 0)
    trace=StoreTrace(u,((0x2009c000,0x2009c100),),cache=M.CACHES[new])
    u.reg_write(UC_ARM_REG_R0,key);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS['freq_adjust_sync'] if new else FREQ_OLD,max_insn=300000)
    return bytes(u.mem_read(0x10000000,0x2600)),calls,trace.events,store.events,bytes(store.image)

def main():
    for new in (False,True):ORIGINAL[new]=M.base(new)
    count=0
    try:
        for item in range(11):
            if item<5:addr=0x1698+item*8;size=4;values=(0,1,3499,3500,3501,4499,4500,4501,65535,0xffffffff)
            elif item==10:addr=0x1660;size=4;values=(0,1,179,180,181,0xffffffff)
            else:addr={5:0x1657,6:0x1658,7:0x1659,8:0x165a,9:0x165b}[item];size=1;values=(0,1,2,3,255)
            addr-=8 if IS12 else 0
            for value in values:
                seed(((addr,size,value),))
                for key,view,refresh in itertools.product((0,2,3,4,0x16,0x21),(0,1),(250,500)):
                    run(0x63,item,key,view,refresh,('calibration',item,value,key,view,refresh));count+=1
        refresh=0x1748 if IS12 else 0x1768
        cfg=0x1620 if IS12 else 0x1628
        for screen,values,addr,size in ((8,(0,1,68,69,70,687,688,689,0xffffffff),0x1624 if IS12 else 0x162c,4),
                                        (0x5a,(0,1,2,198,199,200,255),0x168c if IS12 else 0x1694,1)):
            for value,on,key in itertools.product(values,(0,1),(0,2,3,4,5,6,0x16,0x21)):
                seed(((addr,size,value),(cfg,1,on),(refresh,4,349)))
                run(screen,0,key,1,250,('tuning',screen,value,on,key));count+=1
        fault=0x161c if IS12 else 0x1624
        for mask,item,key in itertools.product([0,*[1<<n for n in range(16)],0xffff,0xffffffff],(0,1,3),(0,4)):
            seed(((fault,4,mask),));run(0x14,item,key,0,500,('fault-display',mask,item,key));count+=1
        method=0x164d if IS12 else 0x1655;mode=0x162c if IS12 else 0x1634
        target=0x1754 if IS12 else 0x1774
        for screen,value,control,key in itertools.product((1,0x1e),(0,1,9,10,11,999,1000,1001,0xffffffff),(0,1,2,3),(0,2,3,0x16,0x21)):
            seed(((method,1,2),(mode,1,control),(0x15d8,4,value),(refresh,4,349),(target,4,789),(0x15d4,4,876)))
            run(screen,0,key,0,250,('manual-main',screen,value,control,key));count+=1
        for spec in itertools.product((0,1,68,69,70,687,688,689,0xffffffff),(0,2,3,5,6,0x16,0x21,255),(0,1),(0,1)):
            compare(('frequency',spec),frequency(False,*spec),frequency(True,*spec));count+=1
    finally:M.BASE.clear();M.BASE.update(ORIGINAL)
    print('CALIBRATION_STATE_GAP_PROBE actual_AB_cases=%d calibration+tuning+fault_display+manual_main+frequency_sync PASS'%count,flush=True)
    (ROOT/'tmp/calibration_state_gap_probe.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
