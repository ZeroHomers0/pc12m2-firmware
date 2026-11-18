"""第六批证据冻结：验证增量覆盖来源链、最终回归/原BIN A/B及原始字节哈希。"""
import sys,json,re,hashlib,subprocess,shutil,platform
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'evidence/reverse/reports/branch_gap_audit_2026-11-18'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'.gitattributes').write_bytes(b'* -text whitespace=cr-at-eol,-blank-at-eof,-blank-at-eol\n')
    if '--record-baseline-docs' in sys.argv:
        (OUT/'preexisting_document_changes.patch').write_bytes(git('diff','--no-ext-diff','--','AGENTS.md','DOCUMENTATION_INDEX.md'))
        print('已保留本批之前存在的文档差异');return 0
    old_path=ROOT/'evidence/reverse/reports/state_lifecycle_audit_2026-11-18/pid_branch_inventory.json'
    stages=[ROOT/'tmp'/name for name in ('branch_gap_batch_6_inventory.json','branch_gap_batch_6_full_inventory.json','branch_gap_batch_6_final_inventory.json')]
    before=json.loads(old_path.read_text(encoding='utf-8'));previous=old_path
    for p in stages:
        current=json.loads(p.read_text(encoding='utf-8'))
        assert current['batch_exit_code']==0 and current['baseline_inventory_sha256']==digest(previous),'增量证据链未通过或被替换'
        assert current['firmware_sha256']==before['firmware_sha256'] and current['original_sha256']==before['original_sha256']
        previous=p
    assert digest(ROOT/'firmware/firmware.bin')==current['firmware_sha256'],'当前镜像不是本批执行版本'
    golden=ROOT/('pc12m2_orig.bin' if (ROOT/'pc12m2_orig.bin').exists() else 'LPC1765.bin')
    assert digest(golden)==current['original_sha256'],'原BIN被改变'
    log=(ROOT/'tmp/branch_gap_batch_6_regression.log').read_text(encoding='utf-8',errors='replace')
    match=re.search(r'模块通过 (\d+)/(\d+)',log);assert match and match[1]==match[2]
    assert 'Traceback (most recent call last)' not in log,'最终回归包含未解释异常'
    ab=(ROOT/'tmp/branch_gap_batch_6_ab.log').read_text(encoding='utf-8',errors='replace')
    assert 'Traceback (most recent call last)' not in ab and 'DISPLAY_FULL_EXEC: PASS' in ab and 'XO_FONT: PASS' in ab
    closed=[];pending=[]
    for addr,b in current['branches'].items():
        assert b['outcomes']['other']==0,('异常分支后继',addr)
        complete=all(b['outcomes'][k] for k in ('taken','fallthrough'))
        prior=before['branches'][addr]['outcomes']
        if complete and not all(prior[k] for k in ('taken','fallthrough')):closed.append({'address':addr,'instruction':b['instruction'],'function_file':b['function_file'],'before':prior,'after':b['outcomes']})
        if not complete:pending.append({'address':addr,**b,'status':'DYNAMIC_OUTCOME_PENDING_NOT_PROVEN_UNREACHABLE'})
    (OUT/'newly_closed_branches.json').write_text(json.dumps(closed,ensure_ascii=False,indent=2),encoding='utf-8')
    (OUT/'pending_branch_outcomes.json').write_text(json.dumps(pending,ensure_ascii=False,indent=2),encoding='utf-8')
    names=('branch_gap_batch_6_inventory.json','branch_gap_batch_6_full_inventory.json','branch_gap_batch_6_final_inventory.json',
           'branch_gap_batch_6_expanded.log','branch_gap_batch_6_gpio.log','branch_gap_batch_6_calibration.log',
           'branch_gap_batch_6_regression.log','branch_gap_batch_6_ab.log','branch_gap_negative_controls.log','branch_gap_negative_controls.json',
           'modbus_address_profile_contract.json','parameter_bank_profile_contract.json','menu_parameter_gap_contract.json',
           'password_inner_iteration_contract.json','gpio_readback_probe_contract.json','calibration_state_gap_probe.json',
           'gpio_probe_initial_no_clear_case.log','calibration_initial_wrong_entry.log','test_intervention_inventory.json','menu_branch_cfg_classification.json')
    for name in names:
        p=ROOT/'tmp'/name
        if p.exists():shutil.copyfile(p,OUT/name)
    for name in ('modbus_address_profile_contract','parameter_bank_profile_contract','menu_parameter_gap_contract','password_inner_iteration_contract','gpio_readback_probe_contract','calibration_state_gap_probe'):
        result=json.loads((OUT/(name+'.json')).read_text(encoding='utf-8'));assert result['status']=='PASS'
    negative=json.loads((OUT/'branch_gap_negative_controls.json').read_text(encoding='utf-8'))
    assert len(negative['injections_detected'])==5 and negative['restored_baseline_pass']
    files={}
    for directory in ('firmware','test','tools/audit','tools/verification'):
        for p in sorted((ROOT/directory).rglob('*')):
            if p.is_file() and (p.suffix.lower() in ('.c','.h','.py','.s','.ld','.sh','.bin','.elf') or p.name=='compiler_flags.txt'):files[p.relative_to(ROOT).as_posix()]=digest(p)
    for name in ('AGENTS.md','DOCUMENTATION_INDEX.md','docs/analysis/BRANCH_GAP_AUDIT_2026-11-18.md'):
        files[name]=digest(ROOT/name)
    for p in OUT.iterdir():
        if p.is_file() and p.name!='manifest.json':files[p.relative_to(ROOT).as_posix()]=digest(p)
    import unicorn,capstone
    manifest={'project':ROOT.name,'date':'2026-11-18','HEAD':git('rev-parse','HEAD').decode().strip(),
              'working_tree_status':git('status','--short').decode('utf-8',errors='replace'),
              'original_sha256':before['original_sha256'],'firmware_sha256':current['firmware_sha256'],
              'regression':match[0],'official_AB':'PASS','business_source_changed':False,'rebuilt_this_batch':False,
              'baseline_branch_summary':before['summary'],'current_branch_summary':current['summary'],
              'newly_both':len(closed),'pending':len(pending),'negative_controls_detected':5,
              'python':platform.python_version(),'unicorn':unicorn.__version__,'capstone':capstone.__version__,
              'all_S0_S7_closed':False,'hardware_actions_this_batch':False,'files':files}
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(ROOT.name,match[0],current['summary'],'newly_both',len(closed),'pending',len(pending),'ARCHIVED')
    return 0
if __name__=='__main__':raise SystemExit(main())
