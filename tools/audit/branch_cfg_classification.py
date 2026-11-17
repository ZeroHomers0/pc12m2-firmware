"""给动态分支清单补原始证据CFG分类，不把数据误反汇编直接当遗漏。

OUTSIDE_RESOLVED_CFG不等于证明不可达；含未解析间接跳转的函数保留待确认。
本工具不依据缺少动态命中擅自删除源码或分支。
"""
import json,re,sys
from pathlib import Path
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM
ROOT=Path(__file__).resolve().parents[2]
is12=(ROOT/'pc12m2_orig.bin').exists()
binary=(ROOT/('pc12m2_orig.bin' if is12 else 'LPC1765.bin')).read_bytes()
input_path=ROOT/sys.argv[1] if len(sys.argv)>1 else ROOT/'tmp/software_branch_inventory.json'
inventory=json.loads(input_path.read_text(encoding='utf-8'))
cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
cache={};metadata={}
def cfg(file):
    if file in cache:return cache[file]
    text=(ROOT/'evidence/reverse/disassembly/functions'/file).read_text(encoding='utf-8',errors='replace')
    entrymatch=re.search(r'entry=0x([0-9a-fA-F]+)',text)
    if not entrymatch:return set(),['missing-entry']
    entry=int(entrymatch[1],16);known=set();targets={};current=None
    for line in text.splitlines():
        m=re.match(r'^([0-9a-fA-F]{8})\s+',line)
        if m:current=int(m[1],16);known.add(current)
        m=re.search(r'; jump -> 0x([0-9a-fA-F]+)',line)
        if m and current is not None:targets.setdefault(current,set()).add(int(m[1],16)&~1)
    work=[entry];reached=set();unresolved=[]
    while work:
        a=work.pop()
        if a in reached:continue
        if a not in known:
            unresolved.append(hex(a)+':missing-evidence');continue
        reached.add(a)
        i=next(cs.disasm(binary[a:a+4],a),None)
        if i is None:unresolved.append(hex(a)+':invalid-decode');continue
        name=i.mnemonic.split('.')[0];fall=a+i.size
        imm=[o.imm&~1 for o in i.operands if o.type==ARM_OP_IMM]
        if name in ('bl','blx'):work.append(fall)
        elif name=='b':work.extend(imm)
        elif name.startswith('b') and name not in ('bic','bics','bfi','bfc','bx','bkpt'):
            work.extend(imm);work.append(fall)
        elif name in ('cbz','cbnz'):work.extend(imm);work.append(fall)
        elif name=='bx':
            if i.op_str!='lr':
                if targets.get(a):work.extend(targets[a])
                else:unresolved.append(hex(a)+':indirect-bx')
        elif name in ('bkpt','udf'):pass
        elif ('pop' in name or name.startswith('ldm')) and 'pc' in i.op_str:pass
        elif name in ('tbb','tbh') or (name.startswith('ldr') and i.op_str.startswith('pc,')):
            if targets.get(a):work.extend(targets[a])
            else:unresolved.append(hex(a)+':indirect-table')
        else:work.append(fall)
    cache[file]=(reached,unresolved)
    return cache[file]

counts={};pending=[]
for addr,b in inventory['branches'].items():
    hits=b['outcomes'];both=bool(hits['taken']) and bool(hits['fallthrough'])
    reached,unresolved=cfg(b['function_file'])
    if both:status='BOTH_EXECUTED'
    elif int(addr,16) in reached:status='RESOLVED_CFG_MISSING_OUTCOME'
    elif unresolved:status='UNRESOLVED_CFG_MISSING_OUTCOME'
    else:status='OUTSIDE_RESOLVED_CFG'
    counts[status]=counts.get(status,0)+1
    if status.endswith('MISSING_OUTCOME'):pending.append({'address':addr,**b,'status':status,'cfg_unresolved':unresolved})
out={'project':ROOT.name,'summary':counts,'pending':pending,'cfg_functions':{f:{'instructions':len(r),'unresolved':u} for f,(r,u) in cache.items()}}
output_path=ROOT/('tmp/pid_branch_cfg_classification.json' if input_path.stem.startswith('pid_') else 'tmp/menu_branch_cfg_classification.json') if len(sys.argv)>1 else ROOT/'tmp/branch_cfg_classification.json'
output_path.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(ROOT.name,'BRANCH_CFG_CLASSIFICATION',counts)
