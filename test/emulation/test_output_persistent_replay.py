"""输出级持久 SRAM 回放：不同反馈、进入/退出阈值、停机/故障再恢复。

只在序列开始初始化双机；每拍只写外部输入，不重置 PID 历史与内部计数。
不跳过输出或 PID 函数体，不挂 FIO 写 hook；比较每拍完整业务 SRAM 和外设末态。
"""
import sys
import struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
IS12 = (ROOT / 'pc12m2_orig.bin').exists()
V = __import__('verify_firmware_equivalence_12' if IS12 else 'verify_firmware_equivalence')
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR

def word(u, addr, value): u.mem_write(addr, struct.pack('<I', value))

def prepare(new, channel):
    u = V.machine(new)
    if IS12:
        V._seed_output_stage(u, 1, channel, 5, 0, 100, 120, 130)
        for addr, value in ((0x10001634, 5000), (0x10001630, 4000), (0x10001640, 526),
                            (0x1000163c, 500), (0x100015b8, 3000), (0x100015bc, 1)):
            word(u, addr, value)
    else:
        words = {0x100015a8:100,0x100015ce:0,0x10001660:90,0x10001650:0,
                 0x10002000:5,0x10002018:49,0x1000201c:1,0x10002020:2,
                 0x10001590:3000,0x10001594:100,0x100015b8:3000,0x100015bc:1,
                 0x100015d0:1000,0x10001648:526,0x10001638:4000,0x1000163c:5000,
                 0x10001644:1,0x10002058:6000,0x10001624:0,0x1000207c:0,
                 0x100016c0:0,0x100016c4:0,0x100020bc:0,0x100016d0:0}
        for addr, value in words.items(): word(u,addr,value)
        for addr, value in {0x10001628:1,0x10001634:channel,0x10001782:0,0x1000203d:0,
                            0x1000177d:0,0x1000203c:0,0x10002075:2,0x10002077:1,
                            0x10002080:1,0x10001659:1,0x100015ac:0,0x100016d4:0,
                            0x10002078:9}.items(): u.mem_write(addr,bytes((value,)))
    calls=[]
    pid=V.SYMS['closed_loop_wrapper'] if new else (0x110f6 if IS12 else 0x10f0a)
    u.hook_add(UC_HOOK_CODE, lambda m,a,n,user:calls.append(a), begin=pid,end=pid)
    return u,calls

def main():
    for channel in (0,1):
        old, old_calls=prepare(False,channel)
        new, new_calls=prepare(True,channel)
        for tick in range(240):
            for u in (old,new):
                # 120拍后关输出，160拍恢复；中途一段故障输入，再清除外部故障。
                u.mem_write(0x10001620 if IS12 else 0x10001628,bytes((0 if 120 <= tick < 160 else 1,)))
                word(u,0x1000161c if IS12 else 0x10001624,0x10 if 180 <= tick < 200 else 0)
                word(u,0x10001594,(9,10,100,499,500,501)[(tick//20)%6])
                word(u,0x10001590,(0,100,3000,2999,3001,130)[(tick//20)%6])
                u.reg_write(UC_ARM_REG_LR,0x3ff01)
                V.run(u,V.SYMS['output_stage'] if u is new else (0xe70c if IS12 else 0xe9ac),max_insn=300000)
            for addr,length in ((0x10000000,0x2200),(0x2009c000,0x100),(0x40008000,0x100),
                                (0x40090000,0x100),(0x400fc000,0x200)):
                a=bytes(old.mem_read(addr,length)); b=bytes(new.mem_read(addr,length))
                if a!=b:
                    off=next(i for i in range(length) if a[i]!=b[i])
                    raise AssertionError(('persistent replay mismatch',channel,tick,hex(addr+off),a[off],b[off]))
        assert old_calls and len(old_calls)==len(new_calls), ('未进入PID或调用数不同',channel,len(old_calls),len(new_calls))
        print('OUTPUT_PERSISTENT_REPLAY channel=%d ticks=240 actual_pid_calls=%d PASS' % (channel,len(old_calls)))
    return 0

if __name__=='__main__': raise SystemExit(main())
