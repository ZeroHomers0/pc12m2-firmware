"""固定种子持续状态事件回放：真实ADC/启停/闭环/通信IRQ/菜单保存/故障/重启。

每个事件后比较完整业务SRAM、MMIO写迹、显示调用、EEPROM及实际发送字节。
仅外部样本、GPIO、LSR就绪、I2C器件响应和明示冷启动边界由模型提供。
tick是指定函数调度的逻辑单位，不宣称真实2ms或自动异常入栈。
"""
import sys,json,random,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from menu_audit_support import install
from test_main_software_chain import old_map
from mmio_code_trace import StoreTrace
from scenario_reducer import minimize
from unicorn import UC_HOOK_MEM_READ

MENU=0x10001724 if IS12 else 0x10001744
CFG=0x10001620 if IS12 else 0x10001628
FAULT=0x1000161c if IS12 else 0x10001624
STATE=0x10001770 if IS12 else 0x10001790
RX=0x10002278 if IS12 else 0x100022a4
ORIG=old_map();ORIG.update(OLD);ORIG.update({'EINT3_IRQHandler':0xf78c if IS12 else 0xfa2c,'TIMER0_IRQHandler':0x29a,
                          'uart3_init':0xa994 if IS12 else 0xac24,'input_scan_state':0x15ae if IS12 else 0x15fe})
CACHE={False:{},True:{}}
RANGES=((0x2009c000,0x2009c100),(0x40004000,0x40004100),(0x40008000,0x40008100),
        (0x40090000,0x40090100),(0x4009c000,0x4009c100),(0x400fc000,0x400fc200))

class Difference(AssertionError):
    def __init__(self,record):super().__init__(record);self.record=record

class Session:
    def __init__(self,new,mutant=False):
        self.new=new;self.mutant=mutant;self.ticks=0;self.pid_calls=0;self.limit_entries=0;self.limit_exits=0
        self.running_ticks=0;self.fault_ticks=0;self.reboots=0;self.wires=[];self.samples=0;self.phase_last=0
        self.boot(None)
    def boot(self,image):
        self.store=Eeprom(image);self.u=setup(self.new,self.store);u=self.u
        for page in (0x40000000,0x40004000,0x4002c000,0x40034000):
            if not any(lo<=page<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(page,0x1000)
        self.displays,self.delays=install(u,self.new,self.store,attach_store=False)
        self.trace=StoreTrace(u,RANGES,cache=CACHE[self.new])
        self.call('load_config');self.call('uart3_init',21)
        self.adc_sample=0;self.external={'level':'low','fault':False,'noise':0};self.poll=0
        start=V.SYMS['adc0_start'] if self.new else ORIG['adc0_start']
        pid=V.SYMS['closed_loop_wrapper'] if self.new else (0x110f6 if IS12 else 0x10f0a)
        def code(m,a,n,user):
            if a==pid:self.pid_calls+=1
            if a==start:
                ch=readword(m,0x40034000)&63
                mode=m.mem_read(0x1000162c if IS12 else 0x10001634,1)[0]
                high=self.external['level']=='high'
                # 两路反馈故意不同：高阶段触发当前控制通道的限流/限压。
                field=(4000 if mode==0 else 100) if high else 70
                voltage=(100 if mode==0 else 4000) if high else 70
                vals={4:123,2:317,1:619,32:2000,8:field,16:voltage}
                self.adc_sample=max(0,min(4095,vals[ch]+self.external['noise']));self.samples+=1
        u.hook_add(UC_HOOK_CODE,code)
        def read(m,access,a,n,value,user):
            if a==0x40034004:word(m,a,0x80000000|(self.adc_sample<<4))
            elif a==0x4009c014:
                word(m,a,0x20 if self.poll>=2 else 0);self.poll+=1
        for addr in (0x40034004,0x4009c014):u.hook_add(UC_HOOK_MEM_READ,read,begin=addr,end=addr+3)
        # 冷启动的同步初值；随后仅TIMER0/EINT3实际ISR维护频率/保持计数。
        u.mem_write(0x10001fd8 if IS12 else 0x10001ff8,b'\x32')
        word(u,0x2009c014,0xffffffff&~0x200);word(u,0x2009c034,0xffffffff);word(u,0x2009c074,0xffffffff)
        if self.mutant:u.mem_write(V.SYMS['output_stage'] if self.new else ORIG['output_stage'],b'\x70\x47')
        self.trace.events.clear();self.store.events.clear();self.displays.clear();self.delays.clear()
    def call(self,name,*args,budget=1000000):
        u=self.u
        for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(r,v)
        u.reg_write(UC_ARM_REG_LR,0x3ff01)
        V.run(u,V.SYMS[name] if self.new else ORIG[name],max_insn=budget)
        return u.reg_read(UC_ARM_REG_R0)
    def wire(self,data,reset=False):
        u=self.u;assert u.mem_read(STATE,1)==b'\0','上帧未回到接收空闲'
        for byte in data:
            word(u,0x4009c000,byte);word(u,0x4009c008,4);self.call('UART3_IRQHandler')
        for _ in range(11):self.call('uart3_rx_timeout_monitor')
        assert u.mem_read(STATE,1)==b'\x05','真实帧间隔未形成完整帧'
        cut=len(self.trace.events);self.poll=0
        if reset:
            u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_LR,0x3ff01)
            u.emu_start((V.SYMS['modbus_dispatch'] if self.new else ORIG['modbus_dispatch'])|1,0x3ff00,count=100000)
            pc=u.reg_read(UC_ARM_REG_PC)
            assert bytes(u.mem_read(pc,2))==b'\xfe\xe7','复位命令未到原机确切自跳'
        else:self.call('modbus_dispatch')
        drains=0
        while u.mem_read(STATE,1)==b'\x06':
            word(u,0x4009c008,2);self.call('UART3_IRQHandler');drains+=1
            assert drains<255,'TX ISR未收尾'
        sent=bytes(v&255 for a,n,v in self.trace.events[cut:] if a==0x4009c000)
        if data[0]==1:
            assert sent and crc(sent[:-2])==int.from_bytes(sent[-2:],'little'),'应答缺失或CRC错误'
            if data[1]==6:assert sent==data,'合法写未返回原帧'
        else:assert not sent,'非本站请求错误响应'
        assert u.mem_read(STATE,1)==b'\0','通信结束未恢复空闲'
        self.wires.append(sent)
    def tick(self,event):
        self.external=event;u=self.u
        word(u,0x2009c034,0xfffeffff if event.get('fault') else 0xffffffff)
        for _ in range(10):self.call('TIMER0_IRQHandler')
        self.call('EINT3_IRQHandler')
        self.call('adc0_scan_channels');self.call('input_scan_state');self.call('adc0_scan_channels')
        self.call('state_machine',event.get('key',0));self.call('output_stage');self.call('wd_feed');self.call('uart3_rx_timeout_monitor')
        self.ticks+=1;self.running_ticks+=u.mem_read(CFG,1)[0]!=0;self.fault_ticks+=readword(u,FAULT)!=0
        phase=u.mem_read(0x1000204f if IS12 else 0x10002077,1)[0]
        if phase!=self.phase_last:
            self.limit_entries+=phase==1;self.limit_exits+=phase==0;self.phase_last=phase
    def event(self,e):
        self.trace.events.clear();self.store.events.clear();self.displays.clear();self.delays.clear();self.wires=[]
        if e['op']=='tick':self.tick(e)
        elif e['op']=='write':self.wire(frame((1,6,0x10,e['reg'],e['value']>>8,e['value']&255)))
        elif e['op']=='wire':self.wire(bytes.fromhex(e['hex']))
        elif e['op']=='key':self.call('state_machine',e['key'])
        elif e['op']=='reset':
            self.wire(frame((1,6,0x10,0x25,0,0)),reset=True)
            assert readword(self.u,FAULT)==0,'复位命令未清故障'
        elif e['op']=='reboot':
            image=bytes(self.store.image);self.boot(image);self.reboots+=1
        elif e['op']=='checkpoint':
            if 'menu' in e:assert self.u.mem_read(MENU,1)[0]==e['menu'],'未实际到达目标菜单'
            if 'voltage_range' in e:
                value=e['voltage_range'];addr=0x10001634 if IS12 else 0x1000163c
                assert readword(self.u,addr)==value,'菜单编辑/重启未保留实际参数'
                assert bytes(self.store.image[0x18:0x1a])==value.to_bytes(2,'big'),'菜单保存没有写入实际高低字节'
        else:raise AssertionError(('未知事件',e))
        return (bytes(self.u.mem_read(0x10000000,0x2900)),tuple(self.trace.events),tuple(self.displays),tuple(self.delays),
                tuple(self.store.events),bytes(self.store.image),tuple(self.wires))

def events_for(seed,channel):
    rng=random.Random(seed);events=[]
    # 全部通过真实通信配置，不把运行中参数直接写进RAM。
    for reg,value in ((1,channel),(2,600),(3,1200),(5,300),(6,250),(7,1),(8,1),(0xb,2),(0xc,0),(0xd,0),(0xf,0),(0x11,0),(0x15,0)):
        events.append({'op':'write','reg':reg,'value':value})
    events.append({'op':'key','key':5})
    for level,count in (('low',100),('high',350),('low',600)):
        for i in range(count):
            events.append({'op':'tick','level':level,'fault':False,'noise':rng.choice((-3,0,3))})
            if i in (45,145):
                reg=6 if channel==0 else 5;events.append({'op':'write','reg':reg,'value':rng.choice((100,200,250))})
        events.append({'op':'wire','hex':frame((2,3,0x10,2,0,1)).hex()})
    # 真实密码键进入菜单，编辑电压量程并保存，再退出；无屏号/RAM强跳。
    for key in (6,1,2,2,2,3,3,3,1,1,3,1,2,2,3,4,4):events.append({'op':'key','key':key})
    events.append({'op':'checkpoint','menu':1,'voltage_range':601})
    # 非法功能码后接合法读，持续同一RX/TX工作区。
    events.append({'op':'wire','hex':'0199'})
    events.append({'op':'wire','hex':frame((1,3,0x10,2,0,1)).hex()})
    events.append({'op':'key','key':5})
    for i in range(270):events.append({'op':'tick','level':'low','fault':True,'noise':rng.choice((-3,0,3))})
    events.extend(({'op':'reset'},{'op':'reboot'},{'op':'checkpoint','voltage_range':601},{'op':'key','key':5}))
    for i in range(160):events.append({'op':'tick','level':'low','fault':False,'noise':rng.choice((-3,0,3))})
    voltage_after=601
    # 固定seed的有效事件交错：运行中换控制通道/限值/增益、反复启停与读帧。
    for i in range(300):
        choice=rng.randrange(10)
        if choice<7:events.append({'op':'tick','level':rng.choice(('low','high')),'fault':False,'noise':rng.randrange(-10,11)})
        elif choice==7:
            reg=rng.choice((1,2,3,5,6,0x17));value=rng.choice({1:(0,1),2:(600,700,800),3:(1200,1400,1600),5:(100,200,300),6:(100,200,250),0x17:(1,2,3,4)}[reg])
            if reg==2:voltage_after=value
            events.append({'op':'write','reg':reg,'value':value})
        elif choice==8:events.append({'op':'key','key':rng.choice((5,6))})
        else:events.append({'op':'wire','hex':frame((1,3,0x10,rng.choice((1,2,3,0x17,0x28)),0,1)).hex()})
    events.append({'op':'key','key':6});events.append({'op':'reboot'});events.append({'op':'checkpoint','voltage_range':voltage_after})
    return events

def replay(events,channel,goals=True,mutant=False):
    old=Session(False);new=Session(True,mutant=mutant)
    for index,event in enumerate(events):
        a=old.event(event);b=new.event(event)
        if a!=b:
            record={'event_index':index,'event':event,'parts':[i for i,(x,y) in enumerate(zip(a,b)) if x!=y],
                    'SRAM_diff':[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:20]}
            record['signature']=[event['op'],record['parts'],record['SRAM_diff'][0][0] if record['SRAM_diff'] else None]
            raise Difference(record)
    if goals:
        assert old.pid_calls==new.pid_calls and old.pid_calls>0,'没有真实进入PID'
        assert old.limit_entries and old.limit_exits,'未实际进入并退出限流/限压'
        assert old.running_ticks and old.fault_ticks and old.reboots==2,'未实际覆盖运行/故障/重启'
        assert old.samples==old.ticks*12,'ADC真实转换次数不符'
    return {'events':len(events),'ticks':old.ticks,'PID_calls':old.pid_calls,'limit_entries':old.limit_entries,
            'limit_exits':old.limit_exits,'fault_ticks':old.fault_ticks,'reboots':old.reboots,'ADC_conversions':old.samples}

def reduce_failure(events,channel,seed,failure,destination,mutant=False):
    evidence={'seed':seed,'channel':channel,'failure':failure.record,'events':events,
              'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),
              'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(),
              'injected_output_noop':mutant,'reduction_status':'running'}
    # 先留完整反例，即使缩减被中断也不能丢失首次失败证据。
    destination.write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8')
    def reproduces(candidate):
        try:replay(candidate,channel,goals=False,mutant=mutant)
        except Difference as again:return again.record['signature']==failure.record['signature']
        except Exception:return False
        return False
    short=minimize(events[:failure.record['event_index']+1],reproduces)
    evidence.update(minimized=short,reduction_status='complete')
    destination.write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8')
    assert reproduces(short),'冻结反例不能重现原差异签名'
    return short

def main():
    audit='--audit' in sys.argv;results=[]
    for seed in ((1765,20261118,0x5a1700) if audit else (1765,)):
        for channel in (0,1):
            events=events_for(seed,channel)
            try:result=replay(events,channel)
            except Difference as failure:
                reduce_failure(events,channel,seed,failure,ROOT/'tmp/cross_module_sequence_failure.json')
                raise
            results.append({'seed':seed,'channel':channel,**result});print('CROSS_MODULE_SEQUENCE',results[-1],'PASS',flush=True)
    # 仿真中只对新固件注入输出空跑，缩减器必须找到非空、可重复的真实差异。
    small=events_for(1765,0)[:25]
    try:replay(small,0,goals=False,mutant=True)
    except Difference as failure:
        reduced=reduce_failure(small,0,1765,failure,ROOT/'tmp/cross_module_sequence_negative_reduction.json',mutant=True)
    else:raise AssertionError('输出空跑未被检出')
    assert 0<len(reduced)<len(small),'反例缩减器无效'
    output={'project':ROOT.name,'status':'PASS','results':results,'negative_control_reduced_events':reduced,
            'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),'firmware_sha256':hashlib.sha256(V.NEW).hexdigest()}
    (ROOT/'tmp/cross_module_event_sequences.json').write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
    print('CROSS_MODULE_EVENT_SEQUENCES PASS negative_output_noop_detected reduced_events=%d'%len(reduced),flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
