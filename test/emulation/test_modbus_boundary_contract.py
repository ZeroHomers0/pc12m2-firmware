"""通信单写阈值、读/多写数量与结构、短帧/CRC/功能码全域原BIN执行差分。

寄存器与分发真实执行；外部EEPROM用已验证介质模型，TX接口捕获完整帧。
复位命令在独立专项验证非返回，不在有界调用中以预算截断冒充通过。
"""
import sys,json,hashlib,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from mmio_code_trace import StoreTrace
BASE={};CACHES={False:{},True:{}}

def initialize(new):
    store=Eeprom();u=setup(new,store);call(u,new,'load_config')
    return bytes(u.mem_read(0x10000000,0x2900)),bytes(store.image)

def observe(new,wire,slave=1,state=5):
    saved,image=BASE[new];u=V.machine(new);u.mem_write(0x10000000,saved)
    store=Eeprom(image);store.attach(u,new)
    st,length,address_slave,tx=(0x1770,0x1772,0x16f7,0x2340) if IS12 else (0x1790,0x1792,0x16ff,0x236c)
    u.mem_write(0x10002278 if IS12 else 0x100022a4,wire)
    u.mem_write(0x10000000+st,bytes((state,)));u.mem_write(0x10000000+length,bytes((len(wire)&255,)))
    u.mem_write(0x10000000+address_slave,bytes((slave,)))
    sends=[]
    def send(m,a,n,user):
        length=m.reg_read(UC_ARM_REG_R0)&255
        sends.append(bytes(m.mem_read(0x10000000+tx,length)))
        m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,send,begin=address(new,'uart3_tx_byte'),end=address(new,'uart3_tx_byte'))
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x4009c000,0x4009c100)),cache=CACHES[new])
    call(u,new,'modbus_dispatch',budget=2000000)
    return bytes(u.mem_read(0x10000000,0x2900)),sends,store.events,bytes(store.image),trace.events

def cases():
    values=(0,1,2,3,9,10,11,19,20,21,39,40,41,49,50,51,59,60,61,99,100,101,159,160,161,
            179,180,181,245,246,247,248,249,250,251,254,255,256,999,1000,1001,3999,4000,4001,
            4999,5000,5001,5999,6000,6001,9999,10000,10001,65535)
    for reg,value in itertools.product(range(65),values):
        if reg in (0x25,0x26) and value==0:continue
        yield 'single/%d/%d'%(reg,value),frame((1,6,0x10,reg,value>>8,value&255)),1,5
    for high,reg,count in itertools.product((0,0xf,0x10,0x11,255),(0,1,2,61,62,63,64,255),(0,1,2,3,62,63,64,125,255,256,65535)):
        yield 'read/'+str((high,reg,count)),frame((1,3,high,reg,count>>8,count&255)),1,5
    for reg,count in itertools.product((0,1,2,31,32,36,37,38,61,62,63,64,255),(0,1,2,3,61,62,63)):
        for declared in sorted({0,1,2*count,max(0,2*count-1),min(255,2*count+1),255}):
            yield 'multi/'+str((reg,count,declared)),frame(bytes((1,16,16,reg,0,count,declared))+b'\0\1'*count),1,5
    for func in range(256):
        wire=frame((1,func,0x10,1,0,1))
        yield 'func/'+str(func),wire,1,5
        yield 'badcrc/'+str(func),wire[:-1]+bytes((wire[-1]^255,)),1,5
    good=frame((1,3,16,1,0,1))
    for length in range(1,9):yield 'short/'+str(length),good[:length],1,5
    for state,slave in itertools.product((0,1,2,3,4,5,6,255),(0,1,2,247,255)):
        yield 'gate/'+str((state,slave)),good,slave,state

def main():
    audit='--audit' in sys.argv
    for new in (False,True):BASE[new]=initialize(new)
    failures=[];count=0;responses=0;writes=0
    for label,wire,slave,state in cases():
        count+=1
        try:
            a=observe(False,wire,slave,state);b=observe(True,wire,slave,state)
            responses+=bool(a[1]);writes+=any(e[0]=='commit' for e in a[2])
            if a!=b:
                diff=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:16]
                raise AssertionError(('通信边界差异',diff,'tx',a[1],b[1],'eeprom',a[2]==b[2],'gpio',a[4]==b[4]))
        except Exception as e:
            if not audit:raise
            failures.append({'label':label,'wire':wire.hex(),'slave':slave,'state':state,'error':repr(e)})
            if len(failures)<=12:print('MODBUS_BOUNDARY_FAIL',failures[-1],flush=True)
    if audit:
        out={'project':ROOT.name,'cases':count,'responses':responses,'commit_cases':writes,'failures':failures,
             'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),'firmware_sha256':hashlib.sha256(V.NEW).hexdigest()}
        (ROOT/'tmp/modbus_boundary_audit.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    assert failures or (responses and writes),'未命中真实响应或介质提交'
    print('MODBUS_BOUNDARY_CONTRACT cases=%d responses=%d committed=%d failures=%d'%(count,responses,writes,len(failures)),flush=True)
    return bool(failures)
if __name__=='__main__':raise SystemExit(main())
