"""冻结最终代码/模拟验收；核查当前镜像、增量链、门禁和分类，保留失败原日志。"""
import sys,json,re,hashlib,subprocess,shutil,platform
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'evidence/reverse/reports/final_software_review_2026-11-18'

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def data(name):return json.loads((ROOT/'tmp'/name).read_text(encoding='utf-8'))
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)

def main():
    OUT.mkdir(exist_ok=True,parents=True)
    (OUT/'.gitattributes').write_bytes(b'* -text whitespace=cr-at-eol,-blank-at-eof,-blank-at-eol\n')
    is12=(ROOT/'pc12m2_orig.bin').exists();fw=ROOT/'firmware/firmware.bin'
    inventory=data('completion_closed_inventory.json');gates=data('completion_final_gates.json')
    assert digest(fw)==inventory['firmware_sha256']==gates['firmware_sha256'] and gates['status']=='PASS'
    assert digest(ROOT/('pc12m2_orig.bin' if is12 else 'LPC1765.bin'))==inventory['original_sha256']
    chain=('software_branch_inventory.json','completion_replay_inventory.json','completion_closed_inventory.json')
    for prior,current in zip(chain,chain[1:]):
        j=data(current);assert j['batch_exit_code']==0 and j['baseline_inventory_sha256']==digest(ROOT/'tmp'/prior)
    assert data(chain[0])['test_exit_code']==0 and data(chain[0])['included_full_ab']
    branches=data('completion_branch_acceptance.json');functions=data('completion_function_contracts.json')
    interventions=data('completion_intervention_contracts.json');negative=data('completion_bounds_negative.json')
    assert branches['pending_unexplained']==functions['unclassified']==interventions['unclassified']==0
    assert negative['status']=='DETECTED' and negative['restored_current_pass'] and negative['auth_before_read_counts']==[24,16]
    assert data('branch_gap_negative_controls.json')['restored_baseline_pass']
    # 当前模块集合逐项核对：完整回归 + 最后两个新增模块及更新叶函数，重叠模块以后者为准。
    sources={}
    for p in (ROOT/'test').rglob('test_*.py'):
        if 'def main(' not in p.read_text(encoding='utf-8-sig',errors='replace'):continue
        key='test_'+str(p.relative_to(ROOT/'test')).replace('\\','_').replace('/','_').removesuffix('.py')
        sources[key]=p.relative_to(ROOT).as_posix()
    accepted={};runs=[]
    for logname in ('completion_fresh_regression_ab.log','completion_updated_regression.log'):
        text=(ROOT/'tmp'/logname).read_text(encoding='utf-8',errors='replace')
        assert 'Traceback (most recent call last)' not in text and ': FAIL' not in text
        match=re.search(r'模块通过 (\d+)/(\d+)',text);assert match and match[1]==match[2]
        names=re.findall(r'^\[run_tests\] (\S+): OK$',text,re.M)
        assert len(names)==int(match[1]),('模块摘要与逐项结果不一致',logname)
        for name in names:accepted[name]={'source':sources[name],'last_success_log':logname}
        runs.append({'log':logname,'modules':int(match[1])})
    assert set(accepted)==set(sources),('存在未验证的当前测试模块',set(sources)-set(accepted),set(accepted)-set(sources))
    if is12:
        ab=(ROOT/'tmp/completion_official_unmasked_ab.log').read_text(encoding='utf-8',errors='replace')
        assert 'DISPLAY_FULL_EXEC: PASS cases=4' in ab and 'DISPLAY_SCREEN_STATIC: PASS' in ab and 'Traceback' not in ab
    # 原日志和前置失败保留原字节；不把旧镜像台账并入本次动态统计。
    for p in (ROOT/'tmp').glob('completion_*'):
        if p.is_file() and p.suffix in ('.json','.log','.txt'):shutil.copyfile(p,OUT/p.name)
    before=OUT/'before_fix';before.mkdir(exist_ok=True)
    for p in (ROOT/'tmp/completion_before_bounds_fix').iterdir():
        if p.is_file():shutil.copyfile(p,before/p.name)
    (OUT/'working_tree.patch').write_bytes(git('diff','--binary','--no-ext-diff'))
    files={}
    for directory in ('firmware','test','tools/audit','tools/verification'):
        for p in (ROOT/directory).rglob('*'):
            if p.is_file() and p.suffix.lower() in ('.c','.h','.s','.ld','.sh','.py','.elf','.bin','.hex','.map'):
                files[p.relative_to(ROOT).as_posix()]=digest(p)
    for p in OUT.rglob('*'):
        if p.is_file() and p.name!='manifest.json':files[p.relative_to(ROOT).as_posix()]=digest(p)
    for name in ('AGENTS.md','DOCUMENTATION_INDEX.md','docs/analysis/FINAL_SOFTWARE_REVIEW_2026-11-18.md',
                 'docs/project/FIRMWARE_EQUIVALENCE_REMEDIATION_PLAN_2026-11-17.md'):
        p=ROOT/name
        if p.exists():files[name]=digest(p)
    import unicorn,capstone
    manifest={'project':ROOT.name,'date':'2026-11-18','HEAD':git('rev-parse','HEAD').decode().strip(),
        'branch':git('branch','--show-current').decode().strip(),'working_tree_status':git('status','--short').decode('utf-8',errors='replace'),
        'original_sha256':inventory['original_sha256'],'firmware_sha256':digest(fw),'firmware_bytes':fw.stat().st_size,
        'firmware_elf_sha256':digest(ROOT/'firmware/firmware.elf'),'python':platform.python_version(),
        'unicorn':unicorn.__version__,'capstone':capstone.__version__,
        'regression_composition':runs,'current_modules_passed':len(accepted),'module_results':accepted,
        'raw_branch_summary':inventory['summary'],'branch_classification':branches['classification'],
        'unexplained_candidates':0,'unclassified_functions':0,'function_contracts':functions['counts'],
        'test_interventions':interventions['counts'],'all_declared_software_review_items_disposed':True,
        'scope':'本报告声明的源码/模拟输入与有限外设调度契约；不是任意RAM/全部中断交错/实机量化等价证明',
        'hardware_actions_this_task':False,'committed_or_pushed_this_task':False,'files':files}
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    for name,sha in files.items():assert digest(ROOT/name)==sha,('冻结期间文件发生变化',name)
    print('FINAL_REVIEW_ARCHIVED',ROOT.name,'modules',len(accepted),'pending=0','files',len(files),flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
