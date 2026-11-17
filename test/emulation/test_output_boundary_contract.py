"""输出/保护边界：原BIN实际执行、PID真实执行、逐次MMIO写序与完整业务SRAM。

不跳业务函数，不安装FIO写hook。参数按各项目实际地址独立登记；不把此有限
组合解释为所有状态穷举。审计模式保存失败输入与原条件分支出口增量。
"""
import sys,json,hashlib,itertools,random
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from test_output_persistent_replay import prepare
from mmio_code_trace import StoreTrace

ADDR={
 'cfg':(0x1620 if IS12 else 0x1628,1),'mode':(0x162c if IS12 else 0x1634,1),
 'run':(0x1fe0 if IS12 else 0x2000,4),'tick':(0x2051 if IS12 else 0x2078,1),'guard':(0x15ce,4),
 'adc_ref':(0x15a8,4),'voltage':(0x1590,4),'current':(0x1594,4),
 'vfb':(0x15b8,4),'ifb':(0x15bc,4),'vraw':(0x15b0,4),'iraw':(0x15ac,4),'aux':(0x15b4,4),
 'v_range':(0x1634 if IS12 else 0x163c,4),'i_range':(0x1630 if IS12 else 0x1638,4),
 'i_limit':(0x163c if IS12 else 0x1644,4),'v_limit':(0x1640 if IS12 else 0x1648,4),
 'start_div':(0x1644 if IS12 else 0x164c,1),'stop_div':(0x1645 if IS12 else 0x164d,1),
 'start_angle':(0x1658 if IS12 else 0x1660,4),'limit_angle':(0x1648 if IS12 else 0x1650,4),
 'feedback_mode':(0x1651 if IS12 else 0x1659,1),'protect_mode':(0x1762 if IS12 else 0x1782,1),
 'stop_req':(0x175d if IS12 else 0x177d,1),'fault':(0x161c if IS12 else 0x1624,4),
 'over_limit':(0x16b8 if IS12 else 0x16c0,4),'over_time':(0x16bc if IS12 else 0x16c4,1),
 'under_limit':(0x16c0 if IS12 else 0x16c8,4),'under_time':(0x16c4 if IS12 else 0x16cc,1),
 'imb_limit':(0x16c8 if IS12 else 0x16d0,4),'imb_time':(0x16cc if IS12 else 0x16d4,1),
 'over_count':(0x2080 if IS12 else 0x20a8,4),'under_count':(0x2084 if IS12 else 0x20ac,4),
 'imb_count':(0x2094 if IS12 else 0x20bc,4),'alarm_count':(0x2054 if IS12 else 0x207c,4),
 'state_a':(0x2000 if IS12 else 0x2020,4),'state_b':(0x1ff8 if IS12 else 0x2018,4),
 'state_c':(0x1ffc if IS12 else 0x201c,4),'limited':(0x2030 if IS12 else 0x2058,4),
 'target':(0x15d0,4),'phase_mode':(0x204f if IS12 else 0x2077,1),
 'alarm':(0x2058 if IS12 else 0x2080,1),'watchdog':(0x2074 if IS12 else 0x209c,4),
 'ramp':(0x2044 if IS12 else 0x206c,4),'ramp_target':(0x2048 if IS12 else 0x2070,4),
 'ramp_phase':(0x201c if IS12 else 0x203c,1),'ramp_state':(0x201d if IS12 else 0x203d,1),
}
CACHES={False:{},True:{}}
BRANCHES={};EXECUTED={}

def patch(u,values):
    for k,v in values.items():
        a,n=ADDR[k];u.mem_write(0x10000000+a,(v&((1<<(8*n))-1)).to_bytes(n,'little'))

def observe(new,channel,values):
    u,_=prepare(new,channel);patch(u,values)
    trace=StoreTrace(u,((0x2009c000,0x2009c100),(0x40008000,0x40008100),
                       (0x40090000,0x40090100),(0x400fc000,0x400fc200)),cache=CACHES[new])
    calls=[];pid=V.SYMS['closed_loop_wrapper'] if new else (0x110f6 if IS12 else 0x10f0a)
    pending=[]
    def code(m,a,n,user):
        if a==pid:calls.append(tuple(m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)))
        if not new and BRANCHES:
            if pending:
                previous,fall=pending.pop();b=BRANCHES[previous]
                key='taken' if a==b['target'] else ('fallthrough' if a==fall else 'other')
                EXECUTED[previous][key]+=1
            if a in BRANCHES:pending.append((a,a+n))
    u.hook_add(UC_HOOK_CODE,code)
    call(u,new,'output_stage',budget=300000)
    state=bytes(u.mem_read(0x10000000,0x2600))
    state+=b''.join(bytes(u.mem_read(a,n)) for a,n in ((0x2009c000,0x100),(0x40008000,0x100),(0x40090000,0x100),(0x400fc000,0x200)))
    return state,trace.events,calls

def scenarios():
    for channel in (0,1):
        yield channel,'baseline',{}
        for key,(a,n) in ADDR.items():
            values=(0,1,2,3,8,9,10,49,50,51,99,100,101,179,180,181,254,255) if n==1 else (0,1,2,4,5,9,10,49,50,51,99,100,101,499,500,501,1499,1500,1501,5999,6000,6001,0x7fffffff,0x80000000,0xffffffff)
            for v in values:yield channel,key+'='+hex(v),{key:v}
        # 保护阶梯的比较值与计时前一拍/等值/后一拍，独立关掉其它保护。
        for family in ('over','under','imb'):
            for duration in (0,1,2,255):
                times=(50,20,10,5,2) if family=='imb' else (50,)
                samples=(99,100,101,149,150,151,199,200,201,249,250,251,299,300,301,349,350,351) if family=='imb' else (99,100,101)
                counts=sorted({0,1,0xffffffff,*[max(0,duration*t+d) for t in times for d in (-1,0,1)]})
                for sample,count in itertools.product(samples,counts):
                    p={'mode':3,'adc_ref':100,'over_limit':0,'under_limit':0,'imb_limit':0,
                       family+'_limit':100,family+'_time':duration,family+'_count':count,
                       'current' if family=='imb' else 'voltage':sample}
                    yield channel,'%s/t%d/s%d/c%d'%(family,duration,sample,count),p
        # 零分母及启动/停止状态组合，不能仅靠正常运行路径。
        for mode,run,cfg,div in itertools.product((0,1,2),(0,4,5),(0,1),(0,1,255)):
            yield channel,'ramp/%d/%d/%d/%d'%(mode,run,cfg,div),{'mode':mode,'run':run,'cfg':cfg,'start_div':div,'stop_div':div}
        yield channel,'angle-zero',{'start_angle':180}
        # 同时满足前置门，不能用逐参数扰动代替真实闭环动作分支。
        for ranges,phase,fb,limited,counter in itertools.product(((4000,5000),(5000,4000),(4000,4000)),
                (0,1,2),(0,1,100,1000,3000),(0,1,2,1000,6000),(0,9,10)):
            p={'mode':channel,'run':5,'current':100,'voltage':100,'target':1000,
               'i_limit':50,'v_limit':50,'i_range':ranges[0],'v_range':ranges[1],
               'phase_mode':phase,'vfb':fb,'ifb':fb,'limited':limited,'state_c':counter,'state_b':0}
            yield channel,'action/'+str((ranges,phase,fb,limited,counter)),p
        for target,limited,count in itertools.product((0,100,5999,6000,6001),(0,1,100,5999,6000,6001),(49,50,0xffffffff)):
            yield channel,'target-ramp/'+str((target,limited,count)),{'mode':channel,'current':10,'voltage':10,
                'target':target,'limited':limited,'state_b':count,'state_c':0,'i_limit':500,'v_limit':500,
                'i_range':4000,'v_range':5000}
        for raw,alarm,count in itertools.product((0,4,5),(0,1,2),(0,99,100,0xffffffff)):
            yield channel,'feedback-watchdog/'+str((raw,alarm,count)),{'vraw':raw,'iraw':raw,
                'alarm':alarm,'alarm_count':0,'watchdog':count,'feedback_mode':1}
        for run,stop,ramp,rstate,rphase in itertools.product((4,5),(1,2,255),(0,1,10000), (0,1,2),(0,1,2)):
            yield channel,'softstop/'+str((run,stop,ramp,rstate,rphase)),{'cfg':0,'run':run,'stop_div':stop,
                'ramp':ramp,'ramp_state':rstate,'ramp_phase':rphase,'stop_req':0}
        # 恒压模式先取模10000，再钳到10..1000；必须跨过10000才会取模为0。
        for reference,run in itertools.product((10,999,1000,1001,9999,10000,10001,0xffffffff),(0,4,5)):
            yield channel,'mode2-modulo/'+str((reference,run)),{'mode':2,'adc_ref':reference,'run':run}
        rng=random.Random(1765+channel)
        for index in range(2500):
            values={'cfg':rng.choice((0,1,1,1)),'mode':rng.randrange(3),'run':rng.choice((0,4,5,5)),
                'start_angle':rng.choice((0,90,179,180,181)),'limit_angle':rng.choice((0,90,179,180)),
                'start_div':rng.choice((0,1,2,255)),'stop_div':rng.choice((0,1,2,255)),
                'adc_ref':rng.choice((9,10,100,1000)), 'protect_mode':rng.randrange(2),
                'phase_mode':rng.randrange(3),'ramp_phase':rng.randrange(3),'ramp_state':rng.randrange(3),
                'alarm':rng.randrange(2),'alarm_count':rng.choice((0,1499,1500)),
                'feedback_mode':rng.randrange(2),'vraw':rng.choice((0,4,5)),'iraw':rng.choice((0,4,5)),
                'watchdog':rng.choice((0,99,100,101)),'stop_req':rng.randrange(2),
                'state_b':rng.choice((0,49,50,51)),'state_c':rng.choice((0,9,10,11)),
                'ramp':rng.choice((0,1,100,10000,100000,0xffffffff))}
            for key in ('i_range','v_range','i_limit','v_limit','voltage','current','vfb','ifb','aux','target','limited'):
                values[key]=rng.choice((0,1,2,99,100,101,1000,5999,6000,6001))
            yield channel,'coupled-seed1765/'+str(index),values

def main():
    audit='--audit' in sys.argv
    if audit:
        inventory=json.loads((ROOT/'evidence/reverse/reports/software_revalidation_2026-11-17/branch_inventory.json').read_text(encoding='utf-8'))
        for a,b in inventory['branches'].items():
            if b['function_file'].startswith('0000e70c' if IS12 else '0000e9ac'):
                BRANCHES[int(a,16)]=b;EXECUTED[int(a,16)]={'taken':0,'fallthrough':0,'other':0}
    failures=[];total=0;pid_cases=0
    for channel,label,values in scenarios():
        total+=1
        try:
            a=observe(False,channel,values);b=observe(True,channel,values)
            if a!=b:
                diffs=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0][:0x2600],b[0][:0x2600])) if x!=y][:12]
                raise AssertionError(('执行结果差异',diffs,'MMIO',a[1]==b[1],'PID',a[2],b[2]))
            pid_cases+=bool(a[2])
        except Exception as e:
            if not audit:raise
            failures.append({'channel':channel,'label':label,'values':values,'error':repr(e)})
            if len(failures)<=12:print('OUTPUT_BOUNDARY_FAIL',failures[-1],flush=True)
    assert pid_cases>0,'矩阵没有实际执行PID'
    if audit:
        output={'project':ROOT.name,'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(),'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),
                'test_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'cases':total,'pid_cases':pid_cases,'failures':failures,'branches':{hex(a):{'instruction':BRANCHES[a]['instruction'],'baseline':BRANCHES[a]['outcomes'],'added':v} for a,v in EXECUTED.items()}}
        (ROOT/'tmp/output_boundary_audit.json').write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
    print('OUTPUT_BOUNDARY_CONTRACT cases=%d actual_pid_cases=%d failures=%d'%(total,pid_cases,len(failures)),flush=True)
    return bool(failures)
if __name__=='__main__':raise SystemExit(main())
