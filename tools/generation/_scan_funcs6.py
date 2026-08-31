# -*- coding: utf-8 -*-
# _scan_funcs6.py — 扫描 6p 13 个模块文件的函数地址注释分布
import re, glob, sys
sys.stdout.reconfigure(encoding='utf-8')
SRC = [f for f in sorted(glob.glob(r'D:\code\LPC1765FBD100\decompiled\firmware\src\*.c'))
       if not f.endswith(('strpool.c', 'crc16_table.c'))]
for fp in SRC:
    txt = open(fp, encoding='utf-8', errors='replace').read()
    adds = re.findall(r'/\*\s*0x([0-9A-Fa-f]{4,8})\s*(?:——|-)?', txt)
    name = fp.replace('\\', '/').split('/')[-1]
    print('%-26s %3d 函数: %s' % (name, len(adds), ' '.join(a.lower() for a in adds)))
