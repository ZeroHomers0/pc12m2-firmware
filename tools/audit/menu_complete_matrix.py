"""全屏/项/键/视图/刷新矩阵：实际状态机、显示调用、SRAM与持久介质。

密码空转使用独立十副本契约支持的精确末态摘要；不mask任何计数器。
运行前必须执行 test_password_delay_all_copies.py。任何异常/未返回均计失败。
"""
import sys,json,hashlib,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from menu_audit_support import *
from mmio_code_trace import StoreTrace

MENU=0x10001724 if IS12 else 0x10001744
REFRESH=0x10001758 if IS12 else 0x10001778
SCREENS=(0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0x14,0x1e,0x5a,0x62,0x63)
KEYS=(0,1,2,3,4,5,6,0xb,0xe,0x16,0x17,0x21)
CACHES={False:{},True:{}}
BASE={}

def base(new):
    store=Eeprom();u=setup(new,store);call(u,new,'load_config')
    return bytes(u.mem_read(0x10000000,0x2600)),bytes(store.image)

def observe(new,screen,item,key,view,refresh):
    state,image=BASE[new];u=V.machine(new);u.mem_write(0x10000000,state)
    store=Eeprom(image);calls,delays=install(u,new,store)
    for p in (0x2009c014,0x2009c034,0x2009c074):word(u,p,0xffffffff&~0x200)
    # 参数/种子先写，菜单三字节最后写并回读，防止旧探针的seed覆盖屏号。
    word(u,REFRESH,refresh);u.mem_write(0x100015f2,b'\xff'*6)
    u.mem_write(MENU,bytes((screen,item,view)))
    assert bytes(u.mem_read(MENU,3))==bytes((screen,item,view))
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x400fc000,0x400fc200)),cache=CACHES[new])
    call(u,new,'state_machine',(key,),budget=3000000)
    return bytes(u.mem_read(0x10000000,0x2600)),calls,delays,trace.events,store.events,bytes(store.image)

def main():
    for spec in SPECS:prove_delay(False,spec)
    prove_delay(True,NEW_DELAY)
    for new in (False,True):BASE[new]=base(new)
    failures=[];count=0;nonempty=0;delay_cases=0
    screens=tuple(int(x,0) for x in sys.argv[1:]) or SCREENS
    for screen in screens:
        for item,key,view,refresh in itertools.product(range(17),KEYS,(0,1),(250,500)):
            count+=1;label=(screen,item,key,view,refresh)
            try:
                a=observe(False,*label);b=observe(True,*label)
                if a!=b:
                    diffs=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:12]
                    first=next(((x,y) for x,y in itertools.zip_longest(a[1],b[1]) if x!=y),None)
                    raise AssertionError(('菜单差异',diffs,'display',first,'delay',a[2],b[2],'gpio',a[3]==b[3],'eeprom',a[4]==b[4]))
                nonempty+=bool(a[1]);delay_cases+=bool(a[2])
            except Exception as e:
                failures.append({'input':label,'error':repr(e)})
                if len(failures)<=20:print('MENU_MATRIX_FAIL',failures[-1],flush=True)
        print('MENU_SCREEN_DONE',hex(screen),'cases',count,'failures',len(failures),flush=True)
    result={'project':ROOT.name,'cases':count,'nonempty_display_cases':nonempty,'password_delay_cases':delay_cases,
            'failures':failures,'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(),
            'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (ROOT/'tmp/menu_complete_matrix.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('MENU_COMPLETE_MATRIX',count,'nonempty',nonempty,'delay',delay_cases,'failures',len(failures),flush=True)
    return bool(failures)
if __name__=='__main__':raise SystemExit(main())
