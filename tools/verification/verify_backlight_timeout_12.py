"""执行原 BIN/新 ELF 状态机，验证背光超时的有意定制差异。"""
import struct
import verify_firmware_equivalence_12 as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR

def frame(new, ticks, key):
    uc = V.machine(new)
    uc.mem_write(0x10001750, struct.pack("<I", ticks))
    uc.mem_write(0x10001724, bytes((0xfe,0,0)))  # 未分配页面：仍完整执行公共入口
    calls=[]
    lcd=V.SYMS["lcd_ctrl_line"] if new else 0x766
    def hook(m, pc, size, user):
        if pc==lcd: calls.append(m.reg_read(UC_ARM_REG_R0))
    uc.hook_add(UC_HOOK_CODE,hook)
    uc.reg_write(UC_ARM_REG_R0,key)
    V.run(uc,V.SYMS["state_machine"] if new else 0x4464)
    assert uc.reg_read(UC_ARM_REG_PC)==0x3ff00
    return struct.unpack("<I",uc.mem_read(0x10001750,4))[0],calls

def main():
    for new, limit in ((False,5000),(True,10000)):
        for ticks in (0,4999,5000,5001,9999,10000,10001):
            expected=(0,[0]) if ticks+1>limit else (ticks+1,[])
            assert frame(new,ticks,0)==expected,(new,ticks)
            assert frame(new,ticks,2)==(0,[1]),(new,ticks,"key")
    print("BACKLIGHT_TIMEOUT: PASS old=5000/new=10000 ticks; boundary and key reset 28 cases")

if __name__=="__main__":main()
