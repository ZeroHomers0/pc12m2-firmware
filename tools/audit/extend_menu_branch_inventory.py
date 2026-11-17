"""将现有全菜单矩阵接入当前完整回归分支台账，不把历史其它镜像的覆盖直接合并。"""
import sys,json,hashlib,copy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import menu_complete_matrix as M

def main():
    pid_only='--pid-only' in sys.argv
    baseline=ROOT/('tmp/menu_branch_inventory.json' if pid_only else 'tmp/software_branch_inventory.json')
    out=json.loads(baseline.read_text(encoding='utf-8'))
    assert out['test_exit_code']==0 and out['included_full_ab']
    assert out['firmware_sha256']==hashlib.sha256(V.NEW).hexdigest()
    assert out['original_sha256']==hashlib.sha256(V.ORIGINAL).hexdigest()
    rows={int(a,16):b for a,b in out['branches'].items()};before=copy.deepcopy(out['summary'])
    original_machine=V.machine;machines=[0]
    def wrapped(new):
        u=original_machine(new)
        if new:return u
        machines[0]+=1;pending=[]
        def code(m,a,n,user):
            if pending:
                pc,fall=pending.pop();b=rows[pc]
                key='taken' if a==b['target'] else 'fallthrough' if a==fall else 'other'
                b['outcomes'][key]+=1
            if a in rows:pending.append((a,a+n))
        u.hook_add(UC_HOOK_CODE,code)
        return u
    rc=1
    try:
        V.machine=wrapped;sys.argv=[str(ROOT/'tools/audit/menu_complete_matrix.py')]
        if pid_only:
            import test_pid_range_table_contract as P
            rc=P.main()
        else:rc=M.main()
    finally:
        V.machine=original_machine
        out['summary']={'candidate_branches':len(rows),
                        'both':sum(bool(b['outcomes']['taken']) and bool(b['outcomes']['fallthrough']) for b in rows.values()),
                        'one':sum(bool(b['outcomes']['taken']) != bool(b['outcomes']['fallthrough']) for b in rows.values()),
                        'unhit':sum(not b['outcomes']['taken'] and not b['outcomes']['fallthrough'] for b in rows.values())}
        out['baseline_summary']=before
        out['pid_range_exit_code' if pid_only else 'menu_matrix_exit_code']=rc
        out['baseline_inventory_sha256']=hashlib.sha256(baseline.read_bytes()).hexdigest()
        out['menu_matrix_source_sha256']=hashlib.sha256(Path(M.__file__).read_bytes()).hexdigest()
        out['added_original_machines']=machines[0]
        output=ROOT/('tmp/pid_branch_inventory.json' if pid_only else 'tmp/menu_branch_inventory.json')
        output.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
        print('PID_BRANCH_EXTENSION' if pid_only else 'MENU_BRANCH_EXTENSION',before,'=>',out['summary'],'matrix_exit',rc,flush=True)
    return rc
if __name__=='__main__':raise SystemExit(main())
