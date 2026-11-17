"""启停/急停/模式切换/故障清除连续契约：两侧真实执行状态机与输入去抖。

只替代显示叶函数与EEPROM外部字节响应；复位仅加速2000次纯空转内层，
两段3000次外层与6000次喂狗真实执行；确切自跳归为预期非返回。
"""
import sys,itertools,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from menu_audit_support import *
from mmio_code_trace import StoreTrace
from capstone.arm import ARM_OP_IMM
import unicorn.arm_const as ARM

MENU=0x10001724 if IS12 else 0x10001744
CFG=0x10001620 if IS12 else 0x10001628
FAULT=0x1000161c if IS12 else 0x10001624
METHOD=0x1000164d if IS12 else 0x10001655
START=METHOD+1;ESTOP=METHOD+2;AUX=METHOD+3
LOSS_EN=0x100016d5 if IS12 else 0x100016dd
BASE={};CACHE={False:{},True:{}}

def seed():
    for new in (False,True):
        store=Eeprom();u=setup(new,store);call(u,new,'load_config')
        BASE[new]=(bytes(u.mem_read(0x10000000,0x2900)),bytes(store.image))

def fixture(new,screen,method,start,aux,estop):
    state,image=BASE[new];u=V.machine(new);u.mem_write(0x10000000,state)
    store=Eeprom(image);displays,delays=install(u,new,store)
    for a,v in ((METHOD,method),(START,start),(AUX,aux),(ESTOP,estop),(LOSS_EN,0),(CFG,0)):
        u.mem_write(a,bytes((v,)))
    u.mem_write(MENU,bytes((screen,0,0)));word(u,FAULT,0)
    word(u,0x2009c014,0xffffffff&~0x200);word(u,0x2009c034,0xffffffff)
    word(u,0x2009c074,0xffffffff)
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x40000000,0x40000010),(0x400fc000,0x400fc200)),cache=CACHE[new])
    return u,store,displays,trace

def snapshot(u):return bytes(u.mem_read(0x10000000,0x2900))

def lifecycle(new,spec):
    screen,method,start,aux,estop=spec
    u,store,displays,trace=fixture(new,*spec);states=[];running=[]
    # 真实持续GPIO输入跨过50拍去抖边界；面板键与端子事件分开消费。
    phases=((2,0,0,0),(52,5,1,0),(2,0,0,0),(52,6,2,0),
            (52,5,1,1),(2,0,0,0),(52,5,1,0),(52,6,2,0))
    for phase,(length,key,terminal,emergency) in enumerate(phases):
        for tick in range(length):
            pins=0xffffffff&~0x200
            if terminal==1:pins&=~0x10000000
            if terminal==2:pins&=~0x08000000
            if emergency:pins&=~0x40
            word(u,0x2009c014,pins)
            call(u,new,'state_machine',(key if tick==0 else 0,),budget=300000)
            states.append(snapshot(u));running.append(u.mem_read(CFG,1)[0])
    # 单次/保持与方法组合的门控以原BIN为准，不写独立手工期望模型。
    if method==2 and start==0:
        assert 1 in running[:54],('未实际启动',spec)
        assert running[107]==0,('未实际停止',spec)
    return states,displays,trace.events,store.events,bytes(store.image),running

def reset(new,screen,mask,enabled=True):
    u,store,displays,trace=fixture(new,screen,2,0,0 if enabled else 1,0)
    word(u,FAULT,mask);u.mem_write(0x1000157f,b'\x32')
    word(u,0x2009c034,0xffffffff&~0x20000)
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    decoded={};feeds=[0];inner=[0];stopped=[False];selfhits={}
    wd=V.SYMS['wd_feed'] if new else 0x238
    def code(m,a,n,user):
        if a==wd:feeds[0]+=1
        if a not in decoded:
            i=next(cs.disasm(bytes(m.mem_read(a,n)),a),None);action=None
            if i and i.mnemonic.startswith('cmp') and len(i.operands)==2 and i.operands[1].type==ARM_OP_IMM and i.operands[1].imm==2000:
                action=('inner',getattr(ARM,'UC_ARM_REG_'+i.reg_name(i.operands[0].reg).upper()))
            elif i and i.mnemonic=='b' and i.operands[0].type==ARM_OP_IMM and i.operands[0].imm==a:action=('self',0)
            decoded[a]=action
        action=decoded[a]
        if action and action[0]=='inner':
            word(m,INNER,2000);m.reg_write(action[1],2000);inner[0]+=1
        elif action and action[0]=='self':
            selfhits[a]=selfhits.get(a,0)+1
            if selfhits[a]==5:stopped[0]=True;m.emu_stop()
    u.hook_add(UC_HOOK_CODE,code)
    u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(address(new,'state_machine')|1,0x3ff00,count=1000000)
    if enabled:
        assert stopped[0] and readword(u,FAULT)==0,'未实际清故障并进入自跳'
        assert (readword(u,OUT),readword(u,INNER),feeds[0],inner[0])==(3000,2000,6000,6000),('复位延时/喂狗不符',feeds,inner)
        wd_writes=[(size,value) for a,size,value in trace.events if a==0x40000008]
        assert len(wd_writes)==12000 and [v for size,v in wd_writes]==[0xaa,0x55]*6000,'未实际产生6000份AA/55喂狗写序'
        assert u.mem_read(CFG,1)==b'\0'
    else:
        assert not stopped[0] and u.reg_read(UC_ARM_REG_PC)==0x3ff00,'禁用复位不应自跳'
        assert readword(u,FAULT)==mask and feeds[0]==0,'辅助模式意外清故障'
    return snapshot(u),displays,trace.events,store.events,bytes(store.image),feeds,inner,'EXPECTED_NONRETURN' if stopped[0] else 'NORMAL_RETURN'

def compare(label,a,b):
    if a!=b:
        if isinstance(a[0],bytes):x,y=a[0],b[0]
        else:
            k=next((k for k,(x,y) in enumerate(zip(a[0],b[0])) if x!=y),0);x,y=a[0][k],b[0][k]
        diffs=[(hex(0x10000000+i),p,q) for i,(p,q) in enumerate(zip(x,y)) if p!=q][:12]
        raise AssertionError((label,'state',diffs,'different_parts',[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]))

def main():
    seed();count=0;frames=0
    specs=list(itertools.product((1,0x1e),(0,1,2),(0,1),(0,1,2),(0,1,2)))
    if '--audit' not in sys.argv:
        specs=[(screen,method,start,(method+start)%3,(method+2*start)%3)
               for screen,method,start in itertools.product((1,0x1e),(0,1,2),(0,1))]
    for spec in specs:
        a=lifecycle(False,spec);b=lifecycle(True,spec);compare(spec,a,b)
        count+=1;frames+=len(a[0])
        if count%12==0:print('STATE_LIFECYCLE_PROGRESS',count,flush=True)
    print('STATE_LIFECYCLE chains=%d persistent_frames=%d PASS'%(count,frames),flush=True)
    resets=0
    for screen,mask in itertools.product((1,0x1e),[1<<n for n in range(16)]+[0xffff,0xffffffff]):
        for enabled in (False,True):
            compare(('reset',screen,mask,enabled),reset(False,screen,mask,enabled),reset(True,screen,mask,enabled));resets+=1
    print('FAULT_RESET_CONTRACT cases=%d actual_6000_watchdog_feeds_each_enabled_reset PASS'%resets,flush=True)
    out={'project':ROOT.name,'chains':count,'persistent_frames':frames,'reset_cases':resets,'status':'PASS',
         'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),'firmware_sha256':hashlib.sha256(V.NEW).hexdigest()}
    (ROOT/'tmp/state_lifecycle_contract.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
