"""公共运行器在正常返回、预算耗尽及Unicorn异常后释放本次钩子。"""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *

def main():
    for new in (False,True):
        u=V.machine(new);active=set();original_add=u.hook_add;original_del=u.hook_del
        if not any(lo<=0x40000000<=hi for lo,hi,_ in u.mem_regions()):u.mem_map(0x40000000,0x1000)
        def add(*args,**kwargs):
            h=original_add(*args,**kwargs);active.add(h);return h
        def delete(h):original_del(h);active.remove(h)
        u.hook_add=add;u.hook_del=delete
        for _ in range(50):
            u.reg_write(UC_ARM_REG_LR,0x3ff01)
            V.run(u,V.SYMS['wd_feed'] if new else 0x238,max_insn=100)
            assert not active,'正常返回后钩子泄漏'
        u.mem_write(0x3f000,b'\xfe\xe7')
        try:V.run(u,0x3f000,writes=[],max_insn=10)
        except AssertionError:pass
        else:raise AssertionError('死循环未被拒绝')
        assert not active,'预算失败后钩子泄漏'
        try:V.run(u,0x60000000,writes=[],max_insn=10)
        except Exception as e:
            from unicorn import UcError
            assert isinstance(e,UcError),repr(e)
        else:raise AssertionError('非法地址未触发异常')
        assert not active,'Unicorn异常后钩子泄漏'
    print('RUNNER_HOOK_LIFETIME normal_100_calls+timeout_2+exception_2 no_leaked_callbacks PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
