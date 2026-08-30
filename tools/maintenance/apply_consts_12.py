# -*- coding: utf-8 -*-
# 替换 12_closed_loop.c 的 0x1771(增益量程上限)->RANGE_MAX，并加 include。
import io, sys
sys.stdout.reconfigure(encoding='utf-8')

repl = {'0x1771': 'RANGE_MAX'}
f = 'firmware/src/12_closed_loop.c'
t = io.open(f, encoding='latin-1').read()
o = t
for k, v in repl.items():
    t = t.replace(k, v)
if 'inc/consts.h' not in t:
    t = t.replace('#include "inc/globals.h"', '#include "inc/globals.h"\n#include "inc/consts.h"', 1)
if t != o:
    io.open(f, 'w', encoding='latin-1').write(t)
    print('12: 替换完成 + include, 变动 %d 字符' % (len(o) - len(t)))
else:
    print('12: 无变化')
