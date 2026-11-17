"""运行统计进位、周期落盘门控、缺相信号消失/恢复：逐帧保留SRAM及介质。"""
from test_state_lifecycle_contract import *

def runtime(new,values):
    u,store,displays,trace=fixture(new,0,2,0,1,0)
    tick,minute,total,hour,persist,pending,retry=values
    u.mem_write(CFG,b'\1')
    for a,v in ((0x10001600,tick),(0x100015fc,minute),(0x10001608,total),
                (0x100015f8,hour),(0x10001604,hour)):
        word(u,a,v)
    if not IS12:
        for a,v in ((0x10001614,persist),(0x10001730,pending),(0x10001734,retry)):word(u,a,v)
    states=[]
    for _ in range(3):
        # P0.9实际去抖达到16拍，触发累计时间/影子落盘，而非直接调用存储替身。
        u.mem_write(0x1000157b,b'\x0f');word(u,0x2009c014,0xffffffff)
        call(u,new,'state_machine',(0,),budget=500000)
        states.append(snapshot(u))
    assert store.commits>0,'统计变化未实际触发EEPROM写入'
    return states,displays,trace.events,store.events,bytes(store.image)

def recovery(new,enabled,flags,counter,interval):
    u,store,displays,trace=fixture(new,1,2,0,1,0)
    u.mem_write(LOSS_EN,bytes((enabled,)))
    cycle=0x10001730 if IS12 else 0x10001750
    counts=cycle+4
    signal=0x10002098 if IS12 else 0x100020c0
    word(u,cycle,interval)
    for i in range(3):word(u,counts+4*i,counter)
    u.mem_write(signal,bytes((bool(flags&1),bool(flags&2),bool(flags&4))))
    states=[]
    call(u,new,'state_machine',(5,),budget=500000);states.append(snapshot(u))
    # 下一拍保留故障，真实状态机封锁；随后信号恢复，在实际检测窗口清缺相位。
    call(u,new,'state_machine',(5,),budget=500000);states.append(snapshot(u))
    word(u,cycle,100);u.mem_write(signal,b'\1\1\1')
    call(u,new,'state_machine',(5,),budget=500000);states.append(snapshot(u))
    assert readword(u,FAULT)&7==0,'同步恢复未清除缺相位'
    call(u,new,'state_machine',(5,),budget=500000);states.append(snapshot(u))
    assert u.mem_read(CFG,1)==b'\1','缺相清除后面板再启动未实际执行'
    return states,displays,trace.events,store.events,bytes(store.image)

def mode_switch(new,screen):
    u,store,displays,trace=fixture(new,screen,2,0,1,0);states=[]
    for method,start,aux,estop in ((2,0,1,0),(0,0,1,1),(1,1,2,2),(2,1,1,0),(0,1,2,1),(2,0,2,2)):
        for a,v in ((METHOD,method),(START,start),(AUX,aux),(ESTOP,estop)):u.mem_write(a,bytes((v,)))
        for tick in range(104):
            pins=0xffffffff&~0x200
            pins&=~(0x10000000 if tick<52 else 0x08000000)
            word(u,0x2009c014,pins)
            call(u,new,'state_machine',((5 if tick==0 else 6 if tick==52 else 0),),budget=300000)
            states.append(snapshot(u))
    return states,displays,trace.events,store.events,bytes(store.image)

def main():
    seed();n=0
    for tick,minute,total,hour in itertools.product((29999,30000,30001,0xffffffff),(58,59,60,0xffffffff),(59,60),(0,0xffffffff)):
        values=(tick,minute,total,hour,0,0,0)
        compare(('runtime',values),runtime(False,values),runtime(True,values));n+=1
    if not IS12:
        for persist,pending,retry in itertools.product((119,120,299,300,319,320),(0,1,2),(0,1)):
            values=(30000,59,59,7,persist,pending,retry)
            compare(('sync',values),runtime(False,values),runtime(True,values));n+=1
    print('STATE_RUNTIME_PERSIST cases=%d three_persistent_frames_each PASS'%n,flush=True)
    m=0
    for spec in itertools.product((0,1),range(8),(0,4,5,255,0xffffffff),(99,100,101,0xffffffff)):
        compare(('phase-recovery',spec),recovery(False,*spec),recovery(True,*spec));m+=1
    print('STATE_PHASE_RECOVERY cases=%d four_persistent_frames_each PASS'%m,flush=True)
    for screen in (1,0x1e):compare(('mode-switch',screen),mode_switch(False,screen),mode_switch(True,screen))
    print('STATE_MODE_SWITCH chains=2 persistent_frames=1248 PASS',flush=True)
    (ROOT/'tmp/state_runtime_recovery.json').write_text(json.dumps({'project':ROOT.name,'runtime_cases':n,'phase_recovery_cases':m,'mode_switch_frames':1248,'status':'PASS'},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
