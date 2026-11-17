"""非零通信工作槽/CRC副本哨兵与跨帧SRAM回放；每帧按ABI从函数入口执行。

跨帧保留SRAM和EEPROM介质，CPU寄存器按新调用初始化；每帧器件初态已就绪。
不替代真实UART中断连续链测试，不屏蔽任何工作槽或CRC副本字节。
"""
import sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/emulation'),str(ROOT/'test/support')]
import test_modbus_boundary_contract as B

def main():
    slots=(0x17d0,0x17d4,0x1784,0x1790,0x1788,0x1780,0x1778,0x177c,0x1794) if B.IS12 else (0x17f0,0x17f4,0x17a4,0x17b0,0x17a8,0x17a0,0x1798,0x179c,0x17b4)
    copy=0x2728 if B.IS12 else 0x2754
    requests=[(1,6,16,0,0,19),(1,6,16,1,0,3),(1,3,0,1,0,1),(1,3,16,0,0,1),
              (1,3,16,1,0,0),(1,3,16,1,0,64),(1,3,16,1,0,1),(1,16,0,1,0,1,2,0,1),
              (1,16,16,0,0,1,2,0,1),(1,16,16,1,0,0,0),(1,16,16,1,0,63,0),
              (1,16,16,1,0,1,1,0,1),(1,16,16,1,0,1,2,0,1),(1,0x99,16,1,0,1),(2,3,16,1,0,1)]
    count=0
    for pattern in (0x55,0xaa):
        initial={}
        for new in (False,True):
            saved,image=B.initialize(new);saved=bytearray(saved)
            for slot in slots:saved[slot:slot+4]=bytes((pattern,))*4
            saved[copy:copy+256]=bytes((pattern,))*256
            initial[new]=(bytes(saved),image)
        for carry in (False,True):
            B.BASE.update(initial)
            for index,request in enumerate(requests):
                if not carry:B.BASE.update(initial)
                wire=B.frame(request);a=B.observe(False,wire);b=B.observe(True,wire)
                diff=[(hex(0x10000000+i),x,y) for i,(x,y) in enumerate(zip(a[0],b[0])) if x!=y]
                assert a==b,(pattern,carry,index,diff[:16],a[1],b[1])
                if carry:
                    B.BASE[False]=(a[0],a[3]);B.BASE[True]=(b[0],b[3])
                count+=1
    out={'project':ROOT.name,'cases':count,'result':'PASS_EQ','patterns':[0x55,0xaa],
         'slots':[hex(0x10000000+s) for s in slots],'crc_copy':[hex(0x10000000+copy),256],
         'firmware_sha256':hashlib.sha256(B.V.NEW).hexdigest(),'original_sha256':hashlib.sha256(B.V.ORIGINAL).hexdigest()}
    (ROOT/'tmp/modbus_work_slot_replay.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print('MODBUS_WORK_SLOT_REPLAY cases=%d nonzero_sentinels+CRC_copy_256bytes+cross_frame_SRAM PASS'%count)
    return 0
if __name__=='__main__':raise SystemExit(main())
