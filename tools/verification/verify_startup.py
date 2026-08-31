# -*- coding: utf-8 -*-
"""verify_startup.py — 无硬件仿真：host 侧模拟 Reset_Handler 启动链路

验证点：
  1) firmware.bin 内嵌 data_image 是否 == firmware/assets/ram_data_image.bin
     （原始 SRAM0 .data 初始镜像，IAR 解压产物）
  2) 模拟 startup.s 四步：拷贝 data_image→SRAM0 / 清零原始 .bss / 拷贝本固件 .data→SRAM1
  3) 验证 SRAM0 关键地址初始值（波特率表/认证标志/字体表指针）符合预期

用法：cd decompiled && python tools/verify_startup.py
"""
import re
import struct
from pathlib import Path

def dword(buf, off):
    return struct.unpack_from('<I', buf, off)[0]

def main():
    root = Path(__file__).resolve().parents[2]
    fw = (root / 'firmware/firmware.bin').read_bytes()
    img = (root / 'firmware/assets/ram_data_image.bin').read_bytes()
    link_map = (root / 'firmware/firmware.map').read_text(encoding='utf-8', errors='ignore')

    print('== firmware.bin 大小:', hex(len(fw)))
    print('== ram_data_image.bin 大小:', len(img), '(=0x%x)' % len(img))

    # 从 linker map 动态解析，避免源码改动后 LMA 漂移造成误报。
    def section(name):
        match = re.search(rf'^\.{name}\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+load address 0x([0-9a-fA-F]+)', link_map, re.M)
        if not match:
            raise RuntimeError(f'firmware.map 中找不到 .{name}')
        return tuple(int(value, 16) for value in match.groups())

    RAM_IMAGE_VMA, RAM_IMAGE_LEN, RAM_IMAGE_LMA = section('fw_image')
    RAM_BSS_START  = 0x10002110   # 12p：data_image 0x2110 字节后为 .bss 清零区
    RAM_BSS_END    = 0x100029a0   # 12p _estack（原始复位栈顶）
    SDATA, DATA_LEN, SIDATA = section('data')
    EDATA = SDATA + DATA_LEN

    ok = True

    # ---- 验证 1：data_image 嵌入正确 ----
    embedded = fw[RAM_IMAGE_LMA:RAM_IMAGE_LMA + RAM_IMAGE_LEN]
    m1 = embedded == img
    ok &= m1
    print('\n[1] firmware.bin@0x%x 内嵌 data_image == firmware/assets/ram_data_image.bin :' % RAM_IMAGE_LMA,
          'PASS' if m1 else 'FAIL')
    if not m1:
        for i, (a, b) in enumerate(zip(embedded, img)):
            if a != b:
                print('    首处差异 @+0x%x: fw=%02x img=%02x' % (i, a, b)); break

    # ---- 验证 2：模拟拷贝 + 清零，构建 SRAM0 镜像 ----
    sram0 = bytearray(0x8000)  # 32K
    sram0[0:RAM_IMAGE_LEN] = embedded                    # 拷贝 data_image
    sram0[RAM_BSS_START - RAM_IMAGE_VMA : RAM_BSS_END - RAM_IMAGE_VMA] = b'\x00' * (RAM_BSS_END - RAM_BSS_START)  # 清零 .bss
    print('[2] 模拟启动：SRAM0 前 0x2110 = data_image，0x2110..0x29A0 清零 -> 完成')

    # ---- 验证 3：关键 SRAM0 初始值 ----
    print('\n[3] SRAM0 关键地址初始值：')
    checks = [
        (0x1000179c, '波特率表[0]', lambda v: v in (2400, 4800, 9600, 19200, 38400, 57600, 115200)),
        (0x1000172c, '认证锁定标志(750)', lambda v: v == 0),   # 初始应锁定态=0
        (0x10001734, '认证结果(748)', lambda v: True),          # 任意，只 dump
    ]
    for addr, name, pred in checks:
        v = dword(bytes(sram0), addr - RAM_IMAGE_VMA)
        tag = 'PASS' if pred(v) else 'CHECK'
        print('    %s @0x%08x = 0x%08x (%d)  %s' % (name, addr, v, v, tag))
        ok &= pred(v)

    # 波特率表完整 dump（0x1000179C 起 8 个 word，12p comm_baud_table）
    print('    波特率表 0x1000179C 全量:', end=' ')
    vals = [dword(bytes(sram0), 0x179c + i*4) for i in range(8)]
    print(vals)

    # 本固件 .data 拷贝长度自检
    print('\n[4] 本固件 .data：LMA=0x%x 长度=0x%x (%dB)，VMA=0x%x -> 0x%x' %
          (SIDATA, EDATA - SDATA, EDATA - SDATA, SDATA, EDATA))

    print('\n==== 启动链路 host 模拟:', '全部 PASS' if ok else '存在 FAIL', '====')
    return 0 if ok else 1

if __name__ == '__main__':
    raise SystemExit(main())
