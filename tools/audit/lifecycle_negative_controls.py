"""用HEAD旧运行器与仿真内喂狗缺失证明新增门禁的检出能力，不改原BIN/磁盘固件。"""
import sys,ast,subprocess,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
import test_runner_hook_lifetime as H
import test_state_lifecycle_contract as T

def main():
    path='tools/verification/'+('verify_firmware_equivalence_12.py' if T.IS12 else 'verify_firmware_equivalence.py')
    source=subprocess.check_output(['git','show','HEAD:'+path],cwd=ROOT).decode('utf-8')
    node=next(n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef) and n.name=='run')
    namespace=dict(T.V.__dict__);exec(compile(ast.Module(body=[node],type_ignores=[]),'<HEAD-runner>', 'exec'),namespace)
    actual=T.V.run
    try:
        T.V.run=namespace['run']
        try:H.main()
        except AssertionError as e:assert '钩子泄漏' in str(e),repr(e)
        else:raise AssertionError('旧运行器未被门禁检出')
    finally:T.V.run=actual
    H.main();T.seed();real=T.V.machine
    def missing_feed(new):
        u=real(new)
        if new:u.mem_write(T.V.SYMS['wd_feed'],b'\x70\x47')
        return u
    try:
        T.V.machine=missing_feed
        try:T.reset(True,1,16)
        except AssertionError as e:assert 'AA/55' in str(e),repr(e)
        else:raise AssertionError('缺少喂狗写序未被检出')
    finally:T.V.machine=real
    T.compare('mutation-restored',T.reset(False,1,16),T.reset(True,1,16))
    out={'project':ROOT.name,'HEAD_runner_leak_detected':True,'missing_watchdog_feed_detected_at_expected_self_loop':True,'restored_baseline_pass':True}
    (ROOT/'tmp/lifecycle_negative_controls.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print('LIFECYCLE_NEGATIVE_CONTROLS old_runner_leak+missing_WDT_write DETECTED; restored_baseline PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
