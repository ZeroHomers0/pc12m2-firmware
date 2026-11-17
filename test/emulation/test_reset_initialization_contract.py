"""从复位向量实际执行原IAR启动链及GCC替代链，停在main入口。

起始SRAM填非零，禁止预置已解包镜像掩盖启动缺陷。比较实际数据镜像和
栈以下BSS；顶部128字节允许各自启动运行库使用栈，但记录为启动ABI边界。
GCC自己的SRAM1 .data按ELF加载段验证，不要求与原IAR的指针池布局相同。
"""
import sys,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *
from unicorn.arm_const import UC_ARM_REG_SP
from unicorn import UC_HOOK_MEM_READ

def boot(new,pattern):
    u=V.machine(new)
    u.mem_write(0x10000000,bytes((pattern,))*0x10000)
    u.mem_write(0x2007c000,bytes((pattern,))*0x4000)
    sp,reset=struct.unpack_from('<II',V.NEW if new else V.ORIGINAL)
    u.reg_write(UC_ARM_REG_SP,sp)
    # 原复位入口在数据解包前会启用时钟；此样本提供就绪状态，不跳过SystemInit。
    def clock_ready(m,access,a,n,val,user):
        for addr,bits in ((0x400fc1a0,0x40),(0x400fc088,0x07000000),(0x400fc0a8,0x700)):
            if a==addr:word(m,addr,readword(m,addr)|bits)
    u.hook_add(UC_HOOK_MEM_READ,clock_ready,begin=0x400fc000,end=0x400fc1ff)
    main=V.SYMS['main'] if new else 0x5cc
    hits=set()
    def code(m,a,n,user):hits.add(a)
    u.hook_add(UC_HOOK_CODE,code)
    u.emu_start(reset,main,count=2000000)
    assert u.reg_read(UC_ARM_REG_PC)==main,('启动未到达main',new,hex(u.reg_read(UC_ARM_REG_PC)))
    image=(ROOT/'firmware/assets/ram_data_image.bin').read_bytes()
    data=bytes(u.mem_read(0x10000000,len(image)))
    assert data==image,('实际启动数据与提取镜像不同',new,next(i for i in range(len(image)) if data[i]!=image[i]))
    bss=bytearray(u.mem_read(0x10000000+len(image),sp-128-0x10000000-len(image)))
    if not new:
        # 原运行库临时把SP切到context对齐地址+0x60，PUSH {r5,lr}留下8字节。
        # 6M 0x10FE4..0x10FEA；12M 0x11186..0x1118E。仅豁免这份已核实栈残值。
        context=0x1000273c if IS12 else 0x10002768
        scratch=(context&~7)+0x58
        expected=struct.pack('<II',context,0x18d)
        assert bytes(u.mem_read(scratch,8))==expected,'IAR临时栈残值不符合原指令'
        off=scratch-(0x10000000+len(image));bss[off:off+8]=b'\0'*8
    assert not any(bss),('启动BSS有未解释非零值',new,[(hex(0x10000000+len(image)+i),v) for i,v in enumerate(bss) if v][:20])
    if new and '.data' in V.SECS:
        size,vma,lma=V.SECS['.data']
        assert bytes(u.mem_read(vma,size))==V.NEW[lma:lma+size],'GCC指针数据段未正确初始化'
    return data,bytes(bss),hits

def main():
    for pattern in (0x55,0xaa):
        a,az,hits=boot(False,pattern);b,bz,_=boot(True,pattern)
        assert (a,az)==(b,bz),'启动数据/BSS与原版不同'
        if IS12:
            assert any(0x188<=a<0x1a2 for a in hits),'未经过原IAR散装载入口'
            assert 0x11178 in hits and 0x111c4 in hits,'未实际经过IAR上下文初始化'
        print('RESET_INITIALIZATION pattern=%#x data=%d BSS=%d original_instructions=%d PASS'%(pattern,len(a),len(az),len(hits)))
    return 0
if __name__=='__main__':raise SystemExit(main())
