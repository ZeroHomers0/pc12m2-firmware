"""最终构建一致性、定制、反测与新增回归的独立退出码门禁。"""
import sys,subprocess,json,hashlib,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
    is12=(ROOT/'pc12m2_orig.bin').exists();suffix='_12' if is12 else ''
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    expected=digest(ROOT/'firmware/firmware.bin');rows=[]
    commands=[('build_repeat',[r'D:\software\Git\bin\bash.exe','firmware/build.sh']),
        ('menu_custom',[sys.executable,'tools/verification/verify_menu_text'+suffix+'.py']),
        ('backlight_custom',[sys.executable,'tools/verification/verify_backlight_timeout'+suffix+'.py']),
        ('startup_adaptation',[sys.executable,'tools/verification/verify_startup_adaptation_contract.py']),
        ('bounds_auth_before_fix',[sys.executable,'tools/audit/completion_bounds_negative.py']),
        ('five_negative_controls',[sys.executable,'tools/audit/branch_gap_negative_controls.py']),
        ('updated_regression',[sys.executable,'test/run_tests.py','test_main_modes_contract','test_completion_leaf_contract'])]
    for name,args in commands:
        result=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        log=ROOT/('tmp/completion_gate_'+name+'.log');log.write_bytes(result.stdout)
        rows.append({'name':name,'command':args,'exit_code':result.returncode,'log':str(log.relative_to(ROOT)),
                     'log_sha256':digest(log),'firmware_sha256':digest(ROOT/'firmware/firmware.bin')})
        assert result.returncode==0,(name,result.returncode,result.stdout[-2000:].decode('utf-8',errors='replace'))
        assert digest(ROOT/'firmware/firmware.bin')==expected,'重复构建改变了被验镜像'
        print('FINAL_GATE',name,'PASS',flush=True)
    (ROOT/'tmp/completion_final_gates.json').write_text(json.dumps({'project':ROOT.name,'status':'PASS','firmware_sha256':expected,'gates':rows},ensure_ascii=False,indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
