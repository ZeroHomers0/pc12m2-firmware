"""归档本批原始日志、覆盖缺口与实际源码/镜像哈希；未通过的门禁拒绝归档为完成。"""
import sys,json,hashlib,subprocess,re,shutil,platform
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'evidence/reverse/reports/state_lifecycle_audit_2026-11-18'

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    mappings={'lifecycle_final_branch_regression.log':'final_regression_and_ab.log',
              'software_branch_inventory.json':'branch_inventory.json',
              'branch_cfg_classification.json':'branch_cfg_classification.json',
              'test_intervention_inventory.json':'intervention_inventory.json',
              'state_runtime_recovery.json':'runtime_recovery.json',
              'peripheral_semantic_contract.json':'peripheral_semantics.json',
              'lifecycle_negative_controls.json':'negative_controls.json',
              'lifecycle_negative_controls.log':'negative_controls.log',
              'lifecycle_menu_custom.log':'menu_custom.log',
              'lifecycle_backlight_custom.log':'backlight_custom.log',
              'menu_branch_inventory.json':'menu_branch_inventory.json',
              'menu_branch_extension.log':'menu_branch_extension.log',
              'menu_complete_matrix.json':'menu_complete_matrix.json',
              'menu_branch_cfg_classification.json':'menu_branch_cfg_classification.json',
              'pid_branch_inventory.json':'pid_branch_inventory.json',
              'pid_branch_extension.log':'pid_branch_extension.log',
              'pid_branch_cfg_classification.json':'pid_branch_cfg_classification.json',
              'pid_range_table_contract.json':'pid_range_table_contract.json',
              'pid_initial_selector_exploration.log':'pid_initial_selector_exploration.log',
              'state_lifecycle_before_hook_cleanup.log':'interrupted_lifecycle_before_cleanup.log',
              'branch_regression_before_hook_cleanup.log':'interrupted_regression_before_cleanup.log',
              'lifecycle_profile.log':'runner_leak_profile.log'}
    inventory=json.loads((ROOT/'tmp/software_branch_inventory.json').read_text(encoding='utf-8'))
    assert inventory['test_exit_code']==0 and inventory['included_full_ab'],'完整回归/A/B未完成'
    log=(ROOT/'tmp/lifecycle_final_branch_regression.log').read_text(encoding='utf-8',errors='replace')
    match=re.search(r'模块通过 (\d+)/(\d+)',log);assert match and match[1]==match[2],'回归模块未全部通过'
    assert 'Traceback (most recent call last)' not in log,'最终日志有未解释异常'
    assert inventory['firmware_sha256']==hashlib.sha256((ROOT/'firmware/firmware.bin').read_bytes()).hexdigest()
    for src,dst in mappings.items():
        p=ROOT/'tmp'/src
        if p.exists():shutil.copyfile(p,OUT/dst)
    accepted=json.loads((OUT/'full_lifecycle_108.json').read_text(encoding='utf-8'))
    assert accepted['chains']==108 and accepted['firmware_sha256']==inventory['firmware_sha256']
    merged_path=ROOT/'tmp/menu_branch_inventory.json'
    merged=json.loads(merged_path.read_text(encoding='utf-8')) if merged_path.exists() else inventory
    if merged_path.exists():
        assert merged['menu_matrix_exit_code']==0 and merged['baseline_inventory_sha256']==hashlib.sha256((ROOT/'tmp/software_branch_inventory.json').read_bytes()).hexdigest(),'扩展覆盖来源未通过或基线变化'
    pid_path=ROOT/'tmp/pid_branch_inventory.json'
    if pid_path.exists():
        pid=json.loads(pid_path.read_text(encoding='utf-8'))
        assert pid['pid_range_exit_code']==0 and pid['baseline_inventory_sha256']==hashlib.sha256(merged_path.read_bytes()).hexdigest(),'PID覆盖扩展来源未通过或基线变化'
        merged=pid
    pid_result=json.loads((ROOT/'tmp/pid_range_table_contract.json').read_text(encoding='utf-8'))
    assert pid_result['status']=='PASS' and pid_result['BCS_branches_both']==40
    pending=[]
    for addr,b in merged['branches'].items():
        if not (b['outcomes']['taken'] and b['outcomes']['fallthrough']):pending.append({'address':addr,**b,'status':'DYNAMIC_OUTCOME_PENDING_NOT_PROVEN_UNREACHABLE'})
    (OUT/'pending_branch_outcomes.json').write_text(json.dumps(pending,ensure_ascii=False,indent=2),encoding='utf-8')
    def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT).decode('utf-8',errors='replace')
    (OUT/'working_tree.patch').write_text(git('diff','--no-ext-diff'),encoding='utf-8')
    files={}
    for folder in ('firmware','test','tools/audit','tools/verification'):
        for p in sorted((ROOT/folder).rglob('*')):
            if p.is_file() and (p.suffix.lower() in ('.c','.h','.py','.s','.ld','.sh','.bin','.elf') or p.name=='compiler_flags.txt'):
                files[p.relative_to(ROOT).as_posix()]=hashlib.sha256(p.read_bytes()).hexdigest()
    for p in sorted(OUT.iterdir()):
        if p.is_file() and p.name!='manifest.json':files[p.relative_to(ROOT).as_posix()]=hashlib.sha256(p.read_bytes()).hexdigest()
    for name in ('AGENTS.md','DOCUMENTATION_INDEX.md','docs/analysis/STATE_LIFECYCLE_AUDIT_2026-11-18.md'):
        p=ROOT/name;files[name]=hashlib.sha256(p.read_bytes()).hexdigest()
    import unicorn,capstone
    manifest={'project':ROOT.name,'date':'2026-11-18','baseline_commit':git('rev-parse','HEAD').strip(),
              'git_status':git('status','--short'),'module_results':match[0],
              'python':platform.python_version(),'unicorn':unicorn.__version__,'capstone':capstone.__version__,
              'business_source_changed':False,'rebuilt_this_batch':False,
              'firmware_sha256':inventory['firmware_sha256'],'original_sha256':inventory['original_sha256'],
              'branch_summary':inventory['summary'],'combined_branch_summary':merged['summary'],
              'additional_PID_cases_after_full_suite':pid_result['cases'],'all_S0_S7_closed':False,
              'remaining':['全程序可达分支缺失出口逐项核销','逐项测试干预契约核销','FIOCLR只写寄存器读行为/完整异常模型','更广中断优先级分组、临界点交错与全部参数组合'],
              'files':files}
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(ROOT.name,match[0],inventory['summary'],'pending_outcomes',len(pending),'ARCHIVED')
    return 0
if __name__=='__main__':raise SystemExit(main())
