"""基本参数异常项号导航上界：全字节项号直接对原BIN执行。

覆盖正常0..15与异常16..255；异常种子不表示正常UI可达。
"""
import sys,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import menu_complete_matrix as M
from test_menu_parameter_gap_contract import compare

def main():
    original=dict(M.BASE)
    try:
        for new in (False,True):M.BASE[new]=M.base(new)
        for item,key,view in itertools.product(range(256),(0,1,2,3,0x16,0x21),(0,1)):
            compare(('basic-navigation-byte',item,key,view),M.observe(False,3,item,key,view,250),M.observe(True,3,item,key,view,250))
        print('BASIC_NAVIGATION_BOUNDS_CONTRACT 3072 PASS',flush=True)
    finally:M.BASE.clear();M.BASE.update(original)
    return 0
if __name__=='__main__':raise SystemExit(main())
