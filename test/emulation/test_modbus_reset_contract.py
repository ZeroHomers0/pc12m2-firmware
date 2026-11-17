"""两条复位命令：真实EEPROM事务、先应答再清密码、确切自跳非返回。"""
import sys
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parent),str(Path(__file__).resolve().parents[1]/'support')]
from software_ab_support import *
import test_modbus_boundary_contract as B

def observe(new,reg):
    saved,image=B.BASE[new];u=V.machine(new);u.mem_write(0x10000000,saved)
    store=Eeprom(image);store.attach(u,new)
    wire=frame((1,6,16,reg,0,0));st,length,slave,rx,tx=(0x1770,0x1772,0x16f7,0x2278,0x2340) if IS12 else (0x1790,0x1792,0x16ff,0x22a4,0x236c)
    for a,data in ((st,b'\5'),(length,b'\10'),(slave,b'\1'),(rx,wire)):u.mem_write(0x10000000+a,data)
    events=[];stopped=[False];hits={}
    def code(m,a,n,user):
        if a==address(new,'uart3_tx_byte'):
            events.append(('tx',bytes(m.mem_read(0x10000000+tx,m.reg_read(UC_ARM_REG_R0)&255))))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
        elif a==address(new,'i2c_write_reg'):events.append(('i2c',m.reg_read(UC_ARM_REG_R0),m.reg_read(UC_ARM_REG_R1)))
        elif n==2 and bytes(m.mem_read(a,2))==b'\xfe\xe7':
            hits[a]=hits.get(a,0)+1
            if hits[a]==5:stopped[0]=True;m.emu_stop()
    u.hook_add(UC_HOOK_CODE,code);u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(address(new,'modbus_dispatch')|1,0x3ff00,count=300000)
    assert stopped[0] and u.reg_read(UC_ARM_REG_PC)!=0x3ff00,'未进入原机要求的确切自跳等待'
    expected=[('tx',wire)]
    if reg==0x26:expected.extend((('i2c',0,5),('i2c',0,6)))
    assert events==expected,('复位顺序不符',events,expected)
    commits=[e for e in store.events if e[0]=='commit']
    assert commits==([] if reg==0x25 else [('commit',5,0),('commit',6,0)]),commits
    return bytes(u.mem_read(0x10000000,0x2900)),events,store.events,bytes(store.image)

def main():
    for new in (False,True):B.BASE[new]=B.initialize(new)
    for reg in (0x25,0x26):assert observe(False,reg)==observe(True,reg),hex(reg)
    print('MODBUS_RESET_CONTRACT cases=2 response_before_real_EEPROM_commit+self_loop EXPECTED_NONRETURN PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
