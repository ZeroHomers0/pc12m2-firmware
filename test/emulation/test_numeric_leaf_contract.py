"""直接执行数字格式化；仅替换字符8叶，比较全部绘制参数与正常返回。"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'support'))
from software_ab_support import *

ENTRIES={'disp_digit':0xd94,'disp_number3':0xe42,'disp_uint4':0xed0,'disp_uint5':0xf8c,
         'disp_number':0x1092,'disp_signed_angle':0x11bc,'disp_offset':0x12b0,
         'disp_uint2':0x13e8,'disp_fixed_1dec':0x143c,'disp_decimal1':0x14f6}
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)

def session(new):
    u=V.machine(new);trace=[]
    leaf=V.SYMS['disp_render_char8'] if new else (0xaf4 if IS12 else 0xb44)
    def hook(m,a,n,user):
        trace.append(tuple(m.reg_read(r) for r in REGS))
        m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,hook,begin=leaf,end=leaf)
    return u,trace

def main():
    machines=[session(False),session(True)];count=0
    values=sorted(set([0,1,2,8,9,10,11,98,99,100,101,109,110,199,200,227,228,229,
                      255,256,318,319,327,328,329,337,338,427,428,429,
                      998,999,1000,1001,9998,9999,10000,10001,99999,100000,
                      100001,0x7fffffff,0x80000000,0xffffff9c,0xffffffff]))
    for name,old in ENTRIES.items():
        for value in values:
            for row,col,inv in ((0,0,0),(3,7,1)):
                outputs=[]
                for new,(u,trace) in enumerate(machines):
                    trace.clear()
                    for reg,val in zip(REGS,(value,row,col,inv)):u.reg_write(reg,val)
                    u.reg_write(UC_ARM_REG_LR,0x3ff01)
                    V.run(u,V.SYMS[name] if new else old-(0x50 if IS12 else 0),max_insn=100000)
                    outputs.append(tuple(trace))
                assert outputs[0]==outputs[1],(name,value,row,col,inv,outputs)
                count+=1
    print('NUMERIC_LEAF_CONTRACT',count,'PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
