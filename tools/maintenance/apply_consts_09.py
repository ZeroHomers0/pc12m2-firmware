# -*- coding: utf-8 -*-
# 替换 09_output_stage.c 中语义唯一常量(角度系数/周期基量/软起初始)
# 0x32 因语境多样(50Hz档/÷50/频率倍频/803)不替换，避免误伤。
import sys, io
sys.stdout.reconfigure(encoding='utf-8')

repl = {
    '0x18bd': 'ANGLE_SCALE', '0x18BD': 'ANGLE_SCALE',
    '0x2c88': 'TRIG_PERIOD', '0x2C88': 'TRIG_PERIOD',
    '0x1771': 'SOFT_START_INIT',
}
f = 'firmware/src/09_output_stage.c'
t = io.open(f, encoding='latin-1').read()
o = t
for k, v in repl.items():
    t = t.replace(k, v)
if t != o:
    io.open(f, 'w', encoding='latin-1').write(t)
    print('09: 替换完成, 变动 %d 字符' % (len(o) - len(t)))
else:
    print('09: 无变化')
