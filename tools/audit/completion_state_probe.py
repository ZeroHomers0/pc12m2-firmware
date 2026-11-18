"""最终剩余状态条件：缓存一致/不一致、运行/故障、档位、计时和输入饱和。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import menu_complete_matrix as M
from test_menu_parameter_gap_contract import compare
from test_main_software_chain import old_map

def main():
    original={new:M.base(new) for new in (False,True)};count=0
    def run(screen,item,key,view,values,refresh=500):
        nonlocal count
        for new in (False,True):
            state,image=original[new];state=bytearray(state)
            state[0x1748 if IS12 else 0x1768:0x174c if IS12 else 0x176c]=(349).to_bytes(4,'little')
            for a,n,v in values:state[a:a+n]=v.to_bytes(n,'little')
            M.BASE[new]=(bytes(state),image)
        label=(screen,item,key,view,values,refresh)
        compare(label,M.observe(False,screen,item,key,view,refresh),M.observe(True,screen,item,key,view,refresh));count+=1
    shift=8 if IS12 else 0;menu_shift=0x20 if IS12 else 0
    def addr(a):return a-(shift if 0x1620<=a<0x1730 else menu_shift if a>=0x1744 else 0)
    try:
        # 状态屏缓存不再每次初始化；特定缓存与真实业务参数相同/不同均必须执行。
        for screen,cfg,mode,latch,status,key in itertools.product((1,0x1e,8,0x5a),(0,1,2),(0,1,2),(0,1,2,3),(0,1),(0,1,4,0x17)):
            vals=((addr(0x1628),1,cfg),(addr(0x1634),1,mode),(0x15cc,1,latch),(0x15cd,1,latch),(addr(0x1624),4,status))
            run(screen,0,key,0,vals)
        for profile,item,view in itertools.product((0,1,2,3,4,5,255),range(9),(0,1)):
            run(7,item,0,view,((addr(0x1710),1,profile),),refresh=250)
        for screen,item,view,frame,detection in itertools.product((5,),(0,1,2,3,4),(0,1),(0,1,2,3,255),(0,1,2)):
            run(screen,item,0,view,((addr(0x1704),1,frame),(addr(0x1705),1,detection)))
        for screen,item,view in itertools.product((3,4),range(12,19),(0,1)):
            for key in (0,1,2,3,4,0x16):run(screen,item,key,view,())
        # 菜单2确认进入各页时独立绘制整页，不能以页内值刷新代替。
        for item,profile,frame,control in itertools.product(range(9),(1,2,3,4),(0,1,2),(0,1,2)):
            run(2,item,1,0,((addr(0x1710),1,profile),(addr(0x1704),1,frame),(addr(0x1705),1,1),(addr(0x1634),1,control)))
        for screen,counter in itertools.product((0xa,0x62,0xb,0xc),(0,1,2,3,255,256)):
            run(screen,0,0,0,((addr(0x1760),4,counter),(addr(0x1764),4,4999),(addr(0x1768),4,499)))
        for screen,start,method,aux,cache in itertools.product((1,0x1e),(0,1,2,255),(0,1,2),(0,1,2),(0,1,2)):
            vals=((addr(0x1656),1,start),(addr(0x1655),1,method),(addr(0x1658),1,aux),(addr(0x177c),1,cache),(0x15cd,1,2))
            for key in (7,8):run(screen,0,key,0,vals)
        for screen in (1,0x1e):
            for value in (0,1,59,60,61,3599,3600,0xffffffff):
                run(screen,0,0,0,((0x1608,4,value),(0x15d8,4,51),(0x15dc,4,50),(addr(0x1628),1,1),(0x1600,4,30000)))
        run(0x14,0,0,1,((addr(0x1624),4,1),(addr(0x1764),4,4999)))
        # 全部状态屏的空闲阈值边界（含1E与待机入口）。
        for screen in M.SCREENS:
            for idle in (4999,5000,5001,14999,15000,49999,50000):
                run(screen,0,0,0,((addr(0x1764),4,idle),))
    finally:M.BASE.clear();M.BASE.update(original)
    # 真实低电平去抖的辅助/急停输入及保持端子，逐拍比较；不向业务事件槽强写结果。
    import test_state_lifecycle_contract as L
    L.seed()
    for screen,aux,estop,mode,cached,level,start in itertools.product((1,0x1e),(0,1,2),(0,1,2),(0,1,2),(1,2),(0,1),(0,1,2,255)):
        if aux==0 and level==0:continue  # 此组合为故障复位非返回，独立复位契约处理。
        results=[]
        for new in (False,True):
            u,store,display,trace=L.fixture(new,screen,mode,start,aux,estop)
            u.mem_write(0x10000000+addr(0x1634),bytes((mode,)));u.mem_write(0x100015cd,bytes((cached,)))
            u.mem_write(0x10001578,b'\x31\x31');u.mem_write(0x1000157c,b'\xf9\xf9')
            u.mem_write(0x10001580,b'\x32\x32');u.mem_write(0x1000157e,b'\x32\x32')
            word(u,0x2009c034,0xffffffff if level else 0xfffdffff)
            word(u,0x2009c014,0xffffffff&~0x200&(~0x40 if not level else 0xffffffff)&(~0x10000000 if start else ~0x08000000))
            states=[]
            for tick in range(3):
                call(u,new,'state_machine',(0,),budget=300000)
                states.append(bytes(u.mem_read(0x10000000,0x2900)))
            results.append((states,display,trace.events,store.events,bytes(store.image)))
        assert results[0]==results[1],('aux-terminal',screen,aux,estop,mode,cached,level,start);count+=1
    results=[]
    for new in (False,True):
        u,store,display,trace=L.fixture(new,1,2,0,1,1)
        word(u,0x100015d8,51);word(u,0x100015dc,50);u.mem_write(0x1000157b,b'\x0f')
        word(u,0x2009c014,0xffffffff)
        call(u,new,'state_machine',(0,));results.append((bytes(u.mem_read(0x10000000,0x2900)),display,trace.events,store.events,bytes(store.image)))
    assert results[0]==results[1],'手动给定与影子不同必须真实持久化';count+=1
    # 去抖：六输入全部组合，计数25锁存/250饱和及慢键计数边界。
    lookup=old_map()
    pins=((0x2009c034,0x40000),(0x2009c034,0x80000),(0x2009c014,0x40000000),(0x2009c014,0x20000000),(0x2009c074,0x2000000),(0x2009c074,0x4000000))
    for bits,counter,encoder in itertools.product(range(64),(24,249,250),(0,10,11,30,31,254,255)):
        outputs=[]
        for new in (False,True):
            u=V.machine(new);word(u,0x10001588,counter);u.mem_write(0x10001571,bytes((encoder,)))
            levels={a:0xffffffff for a,b in pins}
            for i,(a,b) in enumerate(pins):
                if bits&(1<<i):levels[a]&=~b
            for a,v in levels.items():word(u,a,v)
            u.reg_write(UC_ARM_REG_LR,0x3ff01);V.run(u,V.SYMS['input_scan_state'] if new else lookup['input_scan_state'])
            outputs.append((u.reg_read(UC_ARM_REG_R0),bytes(u.mem_read(0x10000000,0x2600))))
        assert outputs[0]==outputs[1],('input',bits,counter,encoder);count+=1
    for phase,countdown in itertools.product((0,199,200,201,254,255),(0,1,2,255)):
        results=[]
        for new in (False,True):
            u=V.machine(new)
            if not any(lo<=0x40004000<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(0x40004000,0x1000)
            u.mem_write(0x10001fd9 if IS12 else 0x10001ff9,bytes((phase,)))
            u.mem_write(0x10001764 if IS12 else 0x10001784,bytes((countdown,)))
            call(u,new,'TIMER0_IRQHandler');results.append(bytes(u.mem_read(0x10000000,0x2600)))
        assert results[0]==results[1],('T0',phase,countdown);count+=1
    print('COMPLETION_STATE_PROBE',count,'PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())

