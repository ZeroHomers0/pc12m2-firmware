"""帧态5期间，实际UART RX ISR不得覆盖已完成接收帧；全字节连续输入。"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'support'))
from software_ab_support import *

def observe(new):
    u=V.machine(new);state=0x10001770 if IS12 else 0x10001790
    buf=0x10002278 if IS12 else 0x100022a4;image=bytes((i*37+19)&255 for i in range(256))
    u.mem_write(state,b'\x05');u.mem_write(buf,image);word(u,0x4009c008,4)
    result=[]
    for byte in range(256):
        word(u,0x4009c000,byte);call(u,new,'UART3_IRQHandler')
        assert u.mem_read(state,1)==b'\x05' and bytes(u.mem_read(buf,256))==image,'完整帧被RX ISR覆盖'
        result.append(bytes(u.mem_read(0x10000000,0x2600)))
    return result

def main():
    assert observe(False)==observe(True),'完整帧冻结时UART ISR副作用不同'
    print('FRAME_READY_STABILITY_CONTRACT 256 persistent_RX_ISR_inputs full_frame_unchanged PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
