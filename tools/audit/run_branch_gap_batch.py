"""以第五批固定台账为基线采集本批新增执行出口；异常/预算截断不可转为通过。"""
import sys,json,hashlib,importlib,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--baseline',default='evidence/reverse/reports/state_lifecycle_audit_2026-11-18/pid_branch_inventory.json')
    parser.add_argument('--output',default='tmp/branch_gap_batch_6_inventory.json')
    parser.add_argument('tests',nargs='*');args=parser.parse_args()
    baseline=ROOT/args.baseline
    out=json.loads(baseline.read_text(encoding='utf-8'))
    assert out['firmware_sha256']==hashlib.sha256(V.NEW).hexdigest(),'镜像变化，需重新冻结基线'
    assert out['original_sha256']==hashlib.sha256(V.ORIGINAL).hexdigest()
    before=dict(out['summary']);rows={int(a,16):b for a,b in out['branches'].items()}
    original=V.machine
    def wrapped(new):
        u=original(new)
        if not new:
            pending=[]
            def code(m,a,n,user):
                if pending:
                    pc,fall=pending.pop();b=rows[pc]
                    key='taken' if a==b['target'] else 'fallthrough' if a==fall else 'other'
                    assert key!='other',('分支后继异常',hex(pc),hex(a))
                    b['outcomes'][key]+=1
                if a in rows:pending.append((a,a+n))
            u.hook_add(UC_HOOK_CODE,code)
        return u
    tests=args.tests or ['test_modbus_address_profile_contract','test_parameter_bank_profile_contract',
                         'test_menu_parameter_gap_contract','test_password_inner_iteration_contract']
    assert out.get('batch_exit_code',0)==0,'增量基线未通过'
    rc=1;completed=list(out.get('completed_tests',[]));saved_argv=sys.argv[:]
    def save(path,status):
        out['summary']={'candidate_branches':len(rows),
                        'both':sum(bool(b['outcomes']['taken']) and bool(b['outcomes']['fallthrough']) for b in rows.values()),
                        'one':sum(bool(b['outcomes']['taken']) != bool(b['outcomes']['fallthrough']) for b in rows.values()),
                        'unhit':sum(not b['outcomes']['taken'] and not b['outcomes']['fallthrough'] for b in rows.values())}
        out['batch_exit_code']=status;out['completed_tests']=completed;out['baseline_summary']=before
        out['baseline_inventory_sha256']=hashlib.sha256(baseline.read_bytes()).hexdigest()
        (ROOT/path).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    try:
        V.machine=wrapped
        for name in tests:
            module=importlib.import_module(name)
            # 被测入口的参数域独立，禁止把采集器--baseline等传给菜单屏号解析器。
            sys.argv=[module.__file__]
            assert module.main()==0,name
            if name not in completed:completed.append(name)
            # 只在整个模块成功后落盘；后续模块失败不能污染这个已完成检查点。
            save(args.output+'.checkpoint.json',0)
        rc=0
    finally:
        sys.argv=saved_argv
        V.machine=original
        save(args.output,rc)
        print('BRANCH_GAP_BATCH',before,'=>',out['summary'],'exit',rc,flush=True)
    return rc
if __name__=='__main__':raise SystemExit(main())
