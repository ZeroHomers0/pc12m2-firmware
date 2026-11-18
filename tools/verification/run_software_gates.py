"""跨平台代码/模拟门禁：实际加载镜像核对、完整A/B、定制及负向检出。

不发布固件、不操作Git、不烧写。每个子进程分别记录退出码和原始日志。
"""
import sys,subprocess,argparse,hashlib,json,shutil,platform,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
GOLDEN={'PC6M-10':'dd629eac0243a13cecc826b1fc2b4b54066f7647d8c1d09a62d5caf7b4f13f65',
        'PC12M-2':'2bc608683992ad6db2d3cb75129bf57ea37d6d681d7c2c0a132e064ebd271bd1'}

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',action='store_true');p.add_argument('--extended',action='store_true')
    p.add_argument('--output',default='tmp/software_gates');a=p.parse_args();out=ROOT/a.output;out.mkdir(parents=True,exist_ok=True)
    is12=(ROOT/'pc12m2_orig.bin').exists();project='PC12M-2' if is12 else 'PC6M-10';suffix='_12' if is12 else ''
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    golden=ROOT/('pc12m2_orig.bin' if is12 else 'LPC1765.bin')
    assert golden.stat().st_size==262144 and sha(golden)==GOLDEN[project],'金标准BIN变化'
    if a.build:
        bash=shutil.which('bash')
        if bash is None:
            git=shutil.which('git');candidate=Path(git).parent.parent/'bin/bash.exe' if git else None
            if candidate and candidate.exists():bash=str(candidate)
        assert bash,'找不到Git Bash/bash'
        result=subprocess.run([bash,'firmware/build.sh'],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/'build.log').write_bytes(result.stdout);assert result.returncode==0,'构建失败，见build.log'
    paths=[ROOT/'firmware'/n for n in ('firmware.bin','firmware.elf','firmware.map')]
    assert all(p.exists() for p in paths),'须先构建同批ELF/BIN/map'
    baseline={p.relative_to(ROOT).as_posix():sha(p) for p in paths};baseline[golden.name]=sha(golden)
    inputs={}
    for p in (ROOT/'firmware').rglob('*'):
        if p.is_file() and p.suffix.lower() in ('.c','.h','.s','.ld','.sh','.bin','.txt'):inputs[p.relative_to(ROOT).as_posix()]=sha(p)
    commands=[('regression','test/run_tests.py',[]),
              ('original_AB','tools/verification/verify_firmware_equivalence'+suffix+'.py',[]),
              ('menu_custom','tools/verification/verify_menu_text'+suffix+'.py',[]),
              ('backlight_custom','tools/verification/verify_backlight_timeout'+suffix+'.py',[]),
              ('startup_adaptation','tools/verification/verify_startup_adaptation_contract.py',[]),
              ('negative_controls','tools/audit/branch_gap_negative_controls.py',[])]
    if a.extended:commands.append(('extended_sequences','test/emulation/test_cross_module_event_sequences.py',['--audit']))
    rows=[];success=False
    try:
        for name,entry,args in commands:
            for f,h in {**baseline,**inputs}.items():assert sha(ROOT/f)==h,('门禁期间输入发生变化',f)
            record=out/(name+'_loaded_images.json');log=out/(name+'.log')
            cmd=[sys.executable,'tools/verification/run_attested_entry.py','--output',str(record.relative_to(ROOT)),entry,*args]
            start=time.monotonic()
            with log.open('wb') as stream:
                result=subprocess.run(cmd,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
            row={'name':name,'entry':entry,'args':args,'exit_code':result.returncode,'seconds':round(time.monotonic()-start,3),
                 'log_sha256':sha(log)};rows.append(row)
            assert result.returncode==0,('门禁失败',name,str(log))
            loaded=json.loads(record.read_text(encoding='utf-8'));row['loads']=loaded['loads']
            assert loaded['exit_code']==0 and loaded['original_sha256']==baseline[golden.name]
            assert loaded['firmware_sha256']==baseline['firmware/firmware.bin']
            if name not in ('menu_custom',):assert loaded['loads']['original_BIN'] and (loaded['loads']['current_BIN'] or loaded['loads']['current_ELF_sections']),'未实际双机执行'
            for f,h in {**baseline,**inputs}.items():assert sha(ROOT/f)==h,('执行后源码/镜像发生变化',f)
            print('SOFTWARE_GATE',name,'PASS',row['loads'],flush=True)
        success=True
    finally:
        import unicorn,capstone
        manifest={'project':project,'status':'PASS' if success else 'FAIL','python':platform.python_version(),
                  'unicorn':unicorn.__version__,'capstone':capstone.__version__,'images':baseline,'firmware_inputs':inputs,
                  'steps':rows,'extended':a.extended,'hardware_actions':False,'publishes_release':False}
        (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
