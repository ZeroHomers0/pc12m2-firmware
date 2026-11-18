"""最后候选：异常PID项号、扫描中GPIO边沿、IAR解压零重复token与背光定制。"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
from test_main_software_chain import old_map
from mmio_code_trace import StoreTrace
import menu_complete_matrix as M

def edge(new):
    u=V.machine(new);word(u,0x10001588,24);u.mem_write(0x10001570,b'\x5a')
    for a in (0x2009c014,0x2009c034,0x2009c074):word(u,a,0xffffffff)
    word(u,0x2009c034,0xfff7ffff);changes=[]
    def store(m,pc,event):
        if event[0]==0x10001588:
            assert event[2]==25 and not changes,'GPIO边沿不是落在第25拍计数提交点'
            changes.append(event);word(m,0x2009c034,0xffffffff)
    trace=StoreTrace(u,((0x10001588,0x1000158c),),on_store=store)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    V.run(u,V.SYMS['input_scan_state'] if new else (0x15ae if IS12 else 0x15fe))
    assert len(changes)==1,'未实际发生GPIO输入边沿'
    return bytes(u.mem_read(0x10000000,0x2600)),u.reg_read(UC_ARM_REG_R0),trace.events

def main():
    for new in (False,True):
        state,image=M.base(new);state=bytearray(state);state[0x1708 if IS12 else 0x1710]=4
        M.BASE[new]=(bytes(state),image)
    n=0
    for item,key in itertools.product(range(256),(2,3)):
        a=M.observe(False,7,item,key,0,250);b=M.observe(True,7,item,key,0,250)
        assert a==b,('PID异常导航',item,key);n+=1
    assert edge(False)==edge(True),'扫描过程中输入恢复高电平的路径不同'
    # 已授权启动适配：GCC不包含IAR解压器。只验证原通用token分支本身；
    # 新旧固定镜像完整启动数据/BSS相等另由reset_initialization契约验收。
    u=V.machine(False);u.mem_write(0x10005000,b'\x02\0\x5a')
    for r,v in ((UC_ARM_REG_R0,0x10005000),(UC_ARM_REG_R1,0x10006000),(UC_ARM_REG_R2,1),(UC_ARM_REG_LR,0x3ff01)):u.reg_write(r,v)
    V.run(u,0x100,max_insn=100)
    assert u.mem_read(0x10006000,1)==b'\x5a','IAR零重复token未按原语义完成'
    B=__import__('verify_backlight_timeout_12' if IS12 else 'verify_backlight_timeout')
    B.main()
    print('FINAL_CANDIDATES PID_AB=%d GPIO_edge_AB=1 IAR_runtime_only=1 BACKLIGHT_CUSTOM=28 PASS'%n,flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
