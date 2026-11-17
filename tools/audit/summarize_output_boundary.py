"""输出级分支出口增量清单；未命中保留缺口，禁止自动判不可达。"""
import json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=ROOT/'tmp/output_boundary_audit.json'
j=json.loads(p.read_text(encoding='utf-8'))
baseline_path=ROOT/'evidence/reverse/reports/software_revalidation_2026-11-17/branch_inventory.json'
baseline_inventory=json.loads(baseline_path.read_text(encoding='utf-8'))
assert baseline_inventory['firmware_sha256']==j['firmware_sha256'],'不能把其它固件版本的基线覆盖并入本轮'
assert baseline_inventory['original_sha256']==j['original_sha256'],'原BIN基线不一致'
assert j['firmware_sha256']==hashlib.sha256((ROOT/'firmware/firmware.bin').read_bytes()).hexdigest(),'覆盖证据不属于当前镜像'
assert not j['failures'],'存在差异或观测失败，不能生成通过摘要'
baseline=after=0;missing=[];closed=[]
for addr,b in j['branches'].items():
    assert b['baseline']==baseline_inventory['branches'][addr]['outcomes'],('基线出口统计被替换',addr)
    before=all(b['baseline'][k] for k in ('taken','fallthrough'))
    merged={k:b['baseline'][k]+b['added'][k] for k in ('taken','fallthrough','other')}
    complete=all(merged[k] for k in ('taken','fallthrough'))
    assert merged['other']==0,('分支后继异常，需先解释',addr,merged)
    baseline+=before;after+=complete
    if complete and not before:closed.append(addr)
    if not complete:missing.append({'address':addr,'instruction':b['instruction'],'combined_outcomes':merged})
result={'project':j['project'],'firmware_sha256':j['firmware_sha256'],'audit_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
        'baseline_inventory_sha256':hashlib.sha256(baseline_path.read_bytes()).hexdigest(),
        'cases':j['cases'],'actual_pid_cases':j['pid_cases'],'conditional_branches':len(j['branches']),
        'baseline_both':baseline,'combined_both':after,'newly_both':closed,'still_missing':missing,
        'scope':'仅输出级已列出的条件分支；不代表全程序或全部参数组合等价'}
(ROOT/'tmp/output_boundary_summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(j['project'],'OUTPUT_BRANCH_OUTCOMES',len(j['branches']),'baseline_both',baseline,'combined_both',after,'remaining',len(missing))
