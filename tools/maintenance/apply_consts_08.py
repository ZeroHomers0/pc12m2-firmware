# -*- coding: utf-8 -*-
# 替换 08_uart3_modbus.c 波特率分频因子为 consts.h 宏，并加 include。
import io, sys
sys.stdout.reconfigure(encoding='utf-8')

repl = {
    '0x3bb': 'BAUD_FAC_0', '0x3BB': 'BAUD_FAC_0',
    '0x3b6': 'BAUD_FAC_3', '0x3B6': 'BAUD_FAC_3',
    '0x3b1': 'BAUD_FAC_4', '0x3B1': 'BAUD_FAC_4',
    '0x3aa': 'BAUD_FAC_5', '0x3AA': 'BAUD_FAC_5',
    '0x39d': 'BAUD_FAC_6', '0x39D': 'BAUD_FAC_6',
    '0x393': 'BAUD_FAC_7', '0x393': 'BAUD_FAC_7',
}
f = 'firmware/src/08_uart3_modbus.c'
t = io.open(f, encoding='latin-1').read()
o = t
for k, v in repl.items():
    t = t.replace(k, v)
# 加 include（若未包含 consts.h）
if 'inc/consts.h' not in t:
    t = t.replace('#include "inc/globals.h"', '#include "inc/globals.h"\n#include "inc/consts.h"', 1)
if t != o:
    io.open(f, 'w', encoding='latin-1').write(t)
    print('08: 替换完成 + include 添加, 变动 %d 字符' % (len(o) - len(t)))
else:
    print('08: 无变化')
