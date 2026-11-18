"""菜单字段阈值、PID档4编辑与各屏空闲超时：真实状态机/调用序列/SRAM/介质比较。"""
import sys,itertools,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
import menu_complete_matrix as M
from software_ab_support import *

BYTE=(0,1,2,3,4,5,127,128,199,200,249,250,254,255)
WORD=(0,1,9,10,99,100,179,180,181,199,200,201,999,1000,1001,5999,6000,6001,65535,0xffffffff)
PROFILE=0x1708 if IS12 else 0x1710
def fields():
    # 六相/十二相独立地址，与各自原BIN字面量及菜单实参对照。
    basic=((0,0x1634,1),(1,0x163c,4),(2,0x1638,4),(3,0x1640,4),(4,0x1648,4),(5,0x1644,4),
           (6,0x164c,1),(7,0x164d,1),(8,0x1650,4),(9,0x1654,1),(10,0x1655,1),(11,0x1656,1),
           (12,0x1657,1),(13,0x1659,1),(14,0x165a,1),(15,0x1660,4))
    protect=((0,0x16c0,4),(1,0x16c4,1),(2,0x16c8,4),(3,0x16cc,1),(4,0x16d0,4),
             (5,0x16d4,1),(6,0x16d8,4),(7,0x16dc,1),(8,0x16dd,1),(9,0x16de,1))
    for screen,items in ((3,basic),(4,protect)):
        for item,addr,size in items:yield screen,item,addr-(8 if IS12 else 0),size
    for item,addr,size in ((0,0x16ff,1),(1,0x1700,4),(2,0x1704,1),(3,0x1705,1)):
        yield 5,item,addr-(8 if IS12 else 0),size
    for item,addr in ((0,0x1710),(1,0x1717),(2,0x1718),(4,0x1722),(5,0x1723),(6,0x1724),(7,0x1725),(8,0x1726)):
        yield 7,item,addr-(8 if IS12 else 0),1

def compare(label,a,b):
    if a!=b:
        diffs=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:16]
        raise AssertionError((label,diffs,'different_parts',[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]))

def main():
    original={new:M.base(new) for new in (False,True)};count=0
    try:
        for screen,item,addr,size in fields():
            values=BYTE if size==1 else WORD
            if screen==5 and item==1:values=(0,1,2,3,4,5,6,7,8,255)
            for value in values:
                for new in (False,True):
                    state,image=original[new];state=bytearray(state)
                    if screen==7:state[PROFILE]=4
                    state[addr:addr+size]=value.to_bytes(size,'little')
                    M.BASE[new]=(bytes(state),image)
                for key,view,refresh in itertools.product((0,2,3,4,0x16,0x21),(0,1),(250,500)):
                    label=(screen,item,value,key,view,refresh)
                    compare(label,M.observe(False,screen,item,key,view,refresh),M.observe(True,screen,item,key,view,refresh));count+=1
            print('MENU_FIELD_DONE',hex(screen),item,'cases',count,flush=True)
        timeout_cases=0
        idle=0x1744 if IS12 else 0x1764
        for timeout in (0,4998,4999,5000,14999,15000,49999,50000,0xffffffff):
            for new in (False,True):
                state,image=original[new];state=bytearray(state)
                state[idle:idle+4]=timeout.to_bytes(4,'little');M.BASE[new]=(bytes(state),image)
            for screen,item,key,view in itertools.product(M.SCREENS,(0,5,15),(0,4,0x17),(0,1)):
                compare(('idle',timeout,screen,item,key,view),M.observe(False,screen,item,key,view,500),M.observe(True,screen,item,key,view,500))
                timeout_cases+=1
    finally:M.BASE.clear();M.BASE.update(original)
    print('MENU_PARAMETER_GAP_CONTRACT parameter_cases=%d timeout_cases=%d PASS'%(count,timeout_cases),flush=True)
    (ROOT/'tmp/menu_parameter_gap_contract.json').write_text(json.dumps({'project':ROOT.name,'parameter_cases':count,'timeout_cases':timeout_cases,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
