"""逐份原inline延时验证外层与喂狗，再允许菜单大矩阵使用摘要。"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'support'))
from menu_audit_support import *

def main():
    expected=prove_delay(True,NEW_DELAY)
    for spec in SPECS:assert prove_delay(False,spec)==expected,spec
    print('PASSWORD_DELAY_ALL_COPIES inline=10 outer=10000 inner=1000 real_wd_feed=10000 PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
