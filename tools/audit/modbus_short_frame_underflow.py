"""实测残留功能码的1字节帧：长度减2下溢，CRC字节计数实际读取255字节。

原反汇编uxtb使计数逐拍截成8位，不会读65535字节。记录完整返回和异常响应，
不把早期未证实的越界推测作为结论，也不推断硬件后果。
"""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/emulation'),str(ROOT/'test/support')]
import test_modbus_boundary_contract as B
from software_ab_support import *
from unicorn import UC_HOOK_MEM_READ

def observe(new):
    store=Eeprom();u=setup(new,store);call(u,new,'load_config')
    state,length,slave,rx=(0x1770,0x1772,0x16f7,0x2278) if IS12 else (0x1790,0x1792,0x16ff,0x22a4)
    # 上一帧残留在RX；本次只收到站址一个字节后成帧。
    u.mem_write(0x10000000+rx,frame((1,3,16,1,0,1)))
    for a,v in ((state,5),(length,1),(slave,1)):u.mem_write(0x10000000+a,bytes((v,)))
    entries=[];reads=[];active=[False];ret=[None];sent=[]
    crc_entry=V.SYMS['crc16'] if new else (0xacd4 if IS12 else 0xaf64)
    def code(m,a,n,user):
        if active[0] and a==ret[0]:active[0]=False
        if a==crc_entry:
            args=(m.reg_read(UC_ARM_REG_R0),m.reg_read(UC_ARM_REG_R1));entries.append(args)
            if args[0]==0x10000000+rx:active[0]=True;ret[0]=m.reg_read(UC_ARM_REG_LR)&~1
        if a==address(new,'uart3_tx_byte'):
            tx=0x10002340 if IS12 else 0x1000236c
            sent.append(bytes(m.mem_read(tx,m.reg_read(UC_ARM_REG_R0)&255)))
            m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    def read(m,access,a,n,value,user):
        if active[0]:reads.append((a,n))
    u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_READ,read,begin=0x10000000+rx,end=0x10000000+rx+0x1000)
    u.reg_write(UC_ARM_REG_LR,0x3ff01)
    u.emu_start(address(new,'modbus_dispatch')|1,0x3ff00,count=300000)
    assert u.reg_read(UC_ARM_REG_PC)==0x3ff00,'短帧分发未正常返回'
    assert entries and entries[0][0]==0x10000000+rx and entries[0][1]&255==255,entries
    assert reads==[(0x10000000+rx+i,1) for i in range(255)],(len(reads),reads[:3],reads[-3:])
    assert sent==[frame((1,0x83,4))],sent
    return {'variant':'new' if new else 'original','status':'NORMAL_RETURN_CRC_ERROR_RESPONSE',
            'crc_arguments':entries,'input_data_reads':len(reads),'response':[s.hex() for s in sent],
            'sram_sha256':__import__('hashlib').sha256(bytes(u.mem_read(0x10000000,0x2900))).hexdigest()}

def main():
    rows=[observe(False),observe(True)]
    assert rows[0]['sram_sha256']==rows[1]['sram_sha256']
    (ROOT/'tmp/modbus_short_frame_underflow.json').write_text(json.dumps({'project':ROOT.name,'cases':rows},ensure_ascii=False,indent=2),encoding='utf-8')
    print('SHORT_FRAME_DIAGNOSTIC original/new actual_CRC_reads=255 normal_return+CRC_error_response+full_SRAM PASS; no_65535byte_read')
    return 0
if __name__=='__main__':raise SystemExit(main())
