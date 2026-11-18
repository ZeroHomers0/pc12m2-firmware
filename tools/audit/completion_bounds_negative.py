"""冻结的修复前真实ELF/BIN反测，必须复现原BIN的标题调用差异。"""
import sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import menu_complete_matrix as M
import test_auth_response_device_contract as A

def main():
    saved=(V.NEW,V.SYMS,V.SECS,V.FW);before=ROOT/'tmp/completion_before_bounds_fix'
    try:
        V.FW=before;V.NEW=(before/'firmware.bin').read_bytes();V.SYMS=V.symbols();V.SECS=V.sections()
        for cache in M.CACHES.values():cache.clear()
        for new in (False,True):M.BASE[new]=M.base(new)
        old=M.observe(False,3,17,2,0,250);bad=M.observe(True,3,17,2,0,250)
        assert old[0]==bad[0] and old[1]!=bad[1],'修复前未复现仅标题调用差异'
        result={'status':'DETECTED','before_sha256':hashlib.sha256(V.NEW).hexdigest(),'input':[3,17,2,0,250],
                'old_display':old[1],'before_fix_display':bad[1],'normal_return_both':True,'SRAM_equal':True}
        auth_old=A.observe(False,'zero');auth_bad=A.observe(True,'zero')
        assert auth_old[:2]==auth_bad[:2] and auth_old[2]!=auth_bad[2]
        assert len(auth_old[2])==24 and len(auth_bad[2])==16,'修复前认证读取差异未复现'
        result['auth_before_read_counts']=[len(auth_old[2]),len(auth_bad[2])]
    finally:V.NEW,V.SYMS,V.SECS,V.FW=saved;M.BASE.clear()
    for cache in M.CACHES.values():cache.clear()
    for new in (False,True):M.BASE[new]=M.base(new)
    assert M.observe(False,3,17,2,0,250)==M.observe(True,3,17,2,0,250),'当前修复后仍有差异'
    assert A.observe(False,'zero')==A.observe(True,'zero'),'当前认证读取仍与原机不同'
    result['restored_current_pass']=True
    (ROOT/'tmp/completion_bounds_negative.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BOUNDS_BEFORE_FIX_DETECTED CURRENT_PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
