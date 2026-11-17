"""逐行登记活动验证中的停止、PC/返回值改写、外设钩子与掩码机制。

输出只是可审计清单，不自动把干预批准为正确；具体作用和替代验证必须读合同报告。
不修改测试、源码或原始证据。
"""
import ast,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
rows=[]
for directory in ('test','tools/verification'):
    for p in sorted((ROOT/directory).rglob('*.py')):
        if '__pycache__' in p.parts:continue
        src=p.read_text(encoding='utf-8',errors='replace')
        tree=ast.parse(src)
        for node in ast.walk(tree):
            kind=None
            if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute):
                method=node.func.attr
                if method in ('emu_start','emu_stop','hook_add','context_save','context_restore'):kind=method
                elif method=='reg_write' and node.args:
                    reg=ast.unparse(node.args[0])
                    if 'REG_PC' in reg:kind='PC_OVERRIDE'
                    elif 'REG_R0' in reg:kind='RETURN_OR_INPUT_R0'
            elif isinstance(node,ast.FunctionDef) and ('mask' in node.name or 'skip' in node.name):kind='MASK_OR_SKIP_HELPER'
            if kind:
                rows.append({'source':str(p.relative_to(ROOT)).replace('\\','/'),'line':node.lineno,'mechanism':kind,
                             'expression':ast.unparse(node)[:500] if not isinstance(node,ast.FunctionDef) else node.name})
from collections import Counter
out={'project':ROOT.name,'summary':dict(Counter(r['mechanism'] for r in rows)),
     'status':'MECHANISM_INVENTORY_NOT_AUTOMATIC_APPROVAL','entries':rows}
(ROOT/'tmp/test_intervention_inventory.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(ROOT.name,'INTERVENTION_INVENTORY',len(rows),out['summary'])
