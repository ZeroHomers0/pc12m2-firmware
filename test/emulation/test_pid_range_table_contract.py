"""PID量程分段表全部阈值、反馈量程选择及包装计数回绕：原BIN执行级比较。"""
import sys,itertools,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *

BOUNDS=sorted({0,1,0x7fffffff,0x80000000,0xffffffff,*[n+d for n in (220,550,1000,1500,2000,2500,3000,4000,5000,6000) for d in (-1,0,1)]})
WORK=0x100020d0 if IS12 else 0x100020f8
SOURCE=0x10002004 if IS12 else 0x10002024
if IS12:assert int.from_bytes(V.ORIGINAL[0x10ee4:0x10ee8],'little')==SOURCE
entry=0x10a9c if IS12 else 0x108b0
evidence=next((ROOT/'evidence/reverse/disassembly/functions').glob('%08x*'%entry))
REQUIRED={int(m[1],16):int(m[2],16) for line in evidence.read_text(encoding='utf-8').splitlines()
          if (m:=re.match(r'^([0-9a-fA-F]{8})\s+bcs(?:\.w|\.n)?\s+0x([0-9a-fA-F]+)',line))}
OUTCOMES={a:set() for a in REQUIRED}
def observe(new,mode,source,axis,value,reverse=False,wrapper=False,counter=0):
    u=V.machine(new)
    u.mem_write(0x1000162c if IS12 else 0x10001634,bytes((mode,)))
    ranges=(0x10001634,0x10001630) if IS12 else (0x1000163c,0x10001638)
    word(u,ranges[0],123);word(u,ranges[1],2345);word(u,ranges[axis],value)
    word(u,SOURCE,source)
    u.mem_write(0x1000171a if IS12 else 0x10001722,bytes((120,10,50,30,7)))
    word(u,WORK+0x28,600000);word(u,WORK+0x30,77)
    word(u,WORK-4,counter);word(u,WORK+0x34,654321)
    integral=V.SYMS['closed_loop_integral'] if new else (0x10a9c if IS12 else 0x108b0)
    hits=[];u.hook_add(UC_HOOK_CODE,lambda m,a,n,user:hits.append(a),begin=integral,end=integral)
    if not new:
        pending=[]
        def branch(m,a,n,user):
            if pending:
                pc,fall=pending.pop()
                assert a in (REQUIRED[pc],fall),('PID分支后继异常',hex(pc),hex(a))
                OUTCOMES[pc].add(a==REQUIRED[pc])
            if a in REQUIRED:pending.append((a,a+n))
        u.hook_add(UC_HOOK_CODE,branch)
    args=(300,526,5,3) if reverse else (526,300,5,3)
    for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(r,v)
    target=(V.SYMS['closed_loop_wrapper'] if new else (0x110f6 if IS12 else 0x10f0a)) if wrapper else integral
    V.run(u,target,max_insn=10000)
    if wrapper:
        assert bool(hits)==(counter!=0xffffffff),'包装计数回绕未实际命中缓存/重算两路径'
        if counter==0xffffffff:assert u.reg_read(UC_ARM_REG_R0)==654321,'回绕路径未返回原缓存'
    return bytes(u.mem_read(0x10000000,0x2600)),u.reg_read(UC_ARM_REG_R0),len(hits)

def main():
    for values in OUTCOMES.values():values.clear()
    count=0
    for spec in itertools.product((0,1,2),(0,1,2),(0,1),BOUNDS,(False,True)):
        assert observe(False,*spec)==observe(True,*spec),('PID量程表差异',spec)
        count+=1
    for mode,counter in itertools.product((0,1,2),(0,1,0xfffffffe,0xffffffff)):
        args=(mode,1,0,2500,False,True,counter)
        assert observe(False,*args)==observe(True,*args),('PID缓存回绕差异',args)
        count+=1
    missing=[hex(a) for a,values in OUTCOMES.items() if values!={False,True}]
    assert not missing,('PID量程比较未覆盖双出口',missing)
    print('PID_RANGE_TABLE_CONTRACT cases=%d original_BCS_both_outcomes=%d/%d all_range_thresholds+source_selection+wrapper_wrap PASS'%(count,len(REQUIRED),len(REQUIRED)),flush=True)
    (ROOT/'tmp/pid_range_table_contract.json').write_text(json.dumps({'project':ROOT.name,'cases':count,'status':'PASS','BCS_branches_both':len(REQUIRED),'source_selector':hex(SOURCE)},indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
