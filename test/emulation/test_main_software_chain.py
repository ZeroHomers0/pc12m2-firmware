"""实际main初始化与64拍连续主循环：ADC/扫描/状态机/输出/通信调度。

模型仅提供外部就绪位、ADC样本、GPIO输入和有效认证响应；字节级EEPROM使用
持久介质。long_delay/EEPROM等待与显示叶函数跳过（分别有独立契约），短Delay实际执行，业务函数不跳过。
正常节拍按tick_ready读取请求提供，不把指令数解释为真实2ms。
"""
import sys,json,collections
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *

DISPLAY6={0xd3c:'disp_string',0xed0:'disp_uint4',0xe42:'disp_number3',0x13e8:'disp_uint2',
          0x11bc:'disp_signed_angle',0x143c:'disp_fixed_1dec',0x1092:'disp_number',0xf8c:'disp_uint5',0x12b0:'disp_offset'}
DISPLAY12={0xcec:'disp_string',0xe80:'disp_uint4',0xdf2:'disp_number3',0x1398:'disp_uint2',
           0x116c:'disp_signed_angle',0x13ec:'disp_fixed_1dec',0x1042:'disp_number',0xf3c:'disp_uint5',0x1260:'disp_offset'}
WATCH=('SystemInit','pin_config','gpio1_init','long_delay','gpio0_input_init','read_input_p02','timer0_init',
       'gpio_inputs_dir_init','i2c_gpio_init','adc_init','load_config','gpio2_init','timer1_init','timer2_init',
       'eint1_init','eint2_init','eint3_init','uart3_init','disp_splash_screen','auth_retry','auth_verify_loop',
       'wdt_init','adc0_scan_channels','input_scan_state','state_machine','output_stage','wd_feed',
       'uart3_rx_timeout_monitor','modbus_dispatch')

def old_map():
    if IS12:
        import test_init_trace_12 as I
        d={sym:a for a,sym,_,_ in I.TARGETS}
        d.update({n:p[0] for n,p in V.PAIRS.items()})
        d.update(OLD12)
        d.update({'wdt_init':0x200,'wd_feed':0x238,'auth_verify_loop':0x10a38,'auth_challenge':0x108dc,'input_scan_state':0x15ae,'Delay':0x758,'adc0_start':0x1f30})
        return d
    inv=json.loads((ROOT/'tools/audit/full_review_2026-11-16/function_inventory.json').read_text(encoding='utf-8'))
    return {m['name']:int(a,16) for a,m in inv.items()}

def observe(new,scenario='encoder'):
    fixture=Eeprom()
    if scenario=='terminal':
        temp=setup(new,fixture);call(temp,new,'load_config')
        for reg,val in ((0xb,0),(0xc,0)):
            wire=frame((1,6,0x10,reg,0,val))
            assert dispatch(temp,new,wire)==[wire],'端子场景配置未实际通过通信落盘'
    limit=200 if scenario=='terminal' else 64
    u=V.machine(new);store=Eeprom(fixture.image);store.attach(u,new)
    word(u,0x2009c014,0xffffffff&~4)
    word(u,0x2009c034,0xffffffff)
    word(u,0x2009c074,0xffffffff)
    for a in (0x40000000,0x40004000,0x4002c000,0x40034000):
        try:u.mem_map(a,0x1000)
        except Exception:pass
    olds=old_map();lookup=V.SYMS if new else olds
    table={lookup[n]:n for n in WATCH if n in lookup}
    displays={V.SYMS[n]:n for n in (DISPLAY12 if IS12 else DISPLAY6).values()} if new else (DISPLAY12 if IS12 else DISPLAY6)
    skip={lookup[n] for n in ('long_delay',) if n in lookup}
    auth=lookup['auth_challenge'];adc_start=lookup['adc0_start']
    frames=[0];conversions=[0];sample=[0];samples=[];calls=[];snapshots=[];stopped=[False]
    recent=collections.deque(maxlen=12);delay_args=[]
    terminal_events=[];scan_return=[]
    def code(m,a,n,unused):
        recent.append(hex(a))
        if scan_return and a==scan_return[-1]:
            scan_return.pop();terminal_events.append(m.reg_read(UC_ARM_REG_R0)&255)
        if a==lookup.get('scan_run_stop'):scan_return.append(m.reg_read(UC_ARM_REG_LR)&~1)
        if a==lookup.get('Delay') and len(delay_args)<15:delay_args.append((hex(m.reg_read(UC_ARM_REG_R0)),hex(m.reg_read(UC_ARM_REG_LR))))
        name=table.get(a)
        if name:
            calls.append(name)
            if name=='wd_feed':
                assert 'output_stage' in calls,'main陷入认证锁机，不能冒充业务主循环'
                frames[0]+=1;snapshots.append(bytes(m.mem_read(0x10000000,0x2600)))
                word(m,0x2009c034,0xffffffff&~(0x40000 if scenario=='encoder' and 5<=frames[0]<36 else 0))
                if scenario=='terminal':
                    pins=0xffffffff&~4
                    if 40<=frames[0]<100:pins&=~0x10000000
                    if 120<=frames[0]<180:pins&=~0x08000000
                    word(m,0x2009c014,pins)
        if a==adc_start:
            channel=readword(m,0x40034000)&63
            sample[0]=(frames[0]*73+channel*193+17)&0xfff
            samples.append((channel,sample[0]));conversions[0]+=1
        if a==auth:
            # 原机成功认证场景；新版保留永久放行。此替身不用于认证模块自身验收。
            if IS12:m.mem_write(0x100020c0,b'\0')
            else:m.mem_write(0x100020e8,b'\0');word(m,0x1000172c,1)
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        if a in displays or a in skip:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,code)
    from unicorn import UC_HOOK_MEM_READ
    def read(m,access,a,n,val,unused):
        if a==0x10000006:
            if frames[0]>=limit:stopped[0]=True;m.emu_stop();return
            m.mem_write(a,b'\x01')
        elif a==0x40034004:word(m,a,0x80000000|(sample[0]<<4))
        elif a==0x400fc1a0:word(m,a,readword(m,a)|0x40)
        elif a==0x400fc088:word(m,a,readword(m,a)|0x7000000)
        elif a==0x400fc0a8:word(m,a,readword(m,a)|0x700)
        elif a==0x4009c014:word(m,a,0x20)
    for a in (0x10000006,0x40034004,0x400fc1a0,0x400fc088,0x400fc0a8,0x4009c014):
        u.hook_add(UC_HOOK_MEM_READ,read,begin=a,end=a+3)
    try:u.emu_start((V.SYMS['main'] if new else 0x5cc)|1,0,count=5000000)
    except Exception:
        print('MAIN_EXCEPTION',new,calls[-12:],list(recent));raise
    if not stopped[0]:print('MAIN_INCOMPLETE',new,calls[-12:],list(recent),delay_args,hex(u.reg_read(UC_ARM_REG_R0)),hex(u.reg_read(UC_ARM_REG_LR)))
    assert stopped[0] and frames[0]==limit,('main未完成指定拍数',new,frames[0],hex(u.reg_read(UC_ARM_REG_PC)))
    for name in ('adc0_scan_channels','input_scan_state','state_machine','output_stage','uart3_rx_timeout_monitor','modbus_dispatch'):
        expected=limit*2 if name=='adc0_scan_channels' else limit
        assert calls.count(name)==expected,('业务调度次数错误',new,name,calls.count(name),expected)
    assert conversions[0]==limit*12,'ADC转换未按每帧12次实际执行'
    if scenario=='terminal':
        events=set(terminal_events)
        assert {7,8}<=events,('端子真实去抖未产生启停事件',events)
        cfg=0x1620 if IS12 else 0x1628
        assert any(s[cfg]==1 for s in snapshots[80:120]) and snapshots[-1][cfg]==0,'启停事件未实际改变运行配置'
    return calls,snapshots,bytes(u.mem_read(0x10000000,0x2600)),samples,store.events,bytes(store.image),terminal_events

def main():
    for scenario in ('encoder','terminal'):
        a=observe(False,scenario);b=observe(True,scenario)
        if a!=b:
            for i,(x,y) in enumerate(zip(a[0],b[0])):
                if x!=y:print('MAIN_CALL_DIFF',i,x,y);break
            for tick,(x,y) in enumerate(zip(a[1],b[1])):
                if x!=y:
                    print('MAIN_STATE_DIFF tick=',tick,[(hex(0x10000000+i),x[i],y[i]) for i in range(len(x)) if x[i]!=y[i]][:12]);break
        assert a==b,('main链差异',scenario,[(i,len(x),len(y)) for i,(x,y) in enumerate(zip(a,b)) if x!=y])
        print('MAIN_SOFTWARE_CHAIN',scenario,'frames=',len(a[1]),'conversions=',len(a[3]),'PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
