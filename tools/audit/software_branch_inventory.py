"""对实际回归执行采集原 BIN 条件分支两出口；未命中保持未覆盖。

不改业务状态、不停止执行。分支候选来自原始逐函数反汇编，故可能包含被误反汇编
的数据；未执行候选只登记，不能直接判为可达业务分支。沿用被测套件自身的替身，
不将候选覆盖率解释为全程序等价证明。
"""
import sys
import re
import json
import runpy
import hashlib
import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IS12 = (ROOT / 'pc12m2_orig.bin').exists()
sys.path[:0] = [str(ROOT / 'tools/verification'), str(ROOT / 'test/support')]
V = __import__('verify_firmware_equivalence_12' if IS12 else 'verify_firmware_equivalence')
from unicorn import UC_HOOK_CODE

CONDITIONS = {'beq','bne','bcs','bcc','bhs','blo','bmi','bpl','bvs','bvc','bhi','bls','bge','blt','bgt','ble','cbz','cbnz'}
branches = {}
for p in sorted((ROOT / 'evidence/reverse/disassembly/functions').glob('*.txt')):
    for line in p.read_text(encoding='utf-8', errors='replace').splitlines():
        m = re.match(r'^([0-9a-fA-F]{8})\s+(\w+)(?:\.w|\.n)?\s+(.+)', line)
        if not m or m[2].lower() not in CONDITIONS: continue
        targets = re.findall(r'0x([0-9a-fA-F]+)',m[3])
        if not targets: continue
        a = int(m[1],16)
        branches[a] = {'function_file':p.name,'instruction':line.strip(),'target':int(targets[-1],16),'outcomes':{'taken':0,'fallthrough':0,'other':0}}

machines = 0
def attach(u):
    global machines
    machines += 1
    pending = []
    def code(m,a,n,user):
        if pending:
            pc,fall = pending.pop()
            b=branches[pc]
            key='taken' if a==b['target'] else ('fallthrough' if a==fall else 'other')
            b['outcomes'][key]+=1
        if a in branches: pending.append((a,a+n))
    u.hook_add(UC_HOOK_CODE,code)
    return u

machine = V.machine
def wrapped(new):
    u=machine(new)
    return u if new else attach(u)
V.machine=wrapped
if not IS12:
    import unicorn_harness as H
    original=H.load_original
    def load_original(*args,**kw): return attach(original(*args,**kw))
    H.load_original=load_original
args=sys.argv[1:]
full_ab='--full-ab' in args
args=[a for a in args if a!='--full-ab']
sys.argv=[str(ROOT/'test/run_tests.py'),*args]
rc=1
try:
    try:
        runpy.run_path(str(ROOT/'test/run_tests.py'),run_name='__main__')
    except SystemExit as ex:
        rc=ex.code or 0
    if rc==0 and full_ab:
        # 用原模块命名空间执行其实际__main__，保留附加观察器；不另造一份矩阵。
        tree=ast.parse(Path(V.__file__).read_text(encoding='utf-8'))
        blocks=[node for node in tree.body if isinstance(node,ast.If) and '__name__' in ast.unparse(node.test)]
        assert len(blocks)==1,'官方入口结构变化，需人工确认'
        exec(compile(ast.Module(body=blocks[0].body,type_ignores=[]),V.__file__,'exec'),V.__dict__)
except BaseException:
    rc=1
    raise
finally:
    rows={hex(a):b for a,b in sorted(branches.items())}
    both=sum(bool(b['outcomes']['taken']) and bool(b['outcomes']['fallthrough']) for b in branches.values())
    one=sum(bool(b['outcomes']['taken']) != bool(b['outcomes']['fallthrough']) for b in branches.values())
    none=sum(not b['outcomes']['taken'] and not b['outcomes']['fallthrough'] for b in branches.values())
    out={'project':ROOT.name,'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(),'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(),
         'test_exit_code':rc,'included_full_ab':full_ab,'machines':machines,'summary':{'candidate_branches':len(branches),'both':both,'one':one,'unhit':none},'branches':rows}
    (ROOT/'tmp/software_branch_inventory.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BRANCH_INVENTORY',out['summary'],'test_exit_code=',rc)
sys.exit(rc)
