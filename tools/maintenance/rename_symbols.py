# -*- coding: utf-8 -*-
"""rename_symbols.py — 全局变量语义化命名（可读性重构·第一步）

依据 docs/PLAN.md「关键符号速查」+ DATA_SEGMENT §5/§6 的语义映射，把
Ghidra 生成的 DAT_xxxx 符号「合并 + 重命名」为人类可读名。

关键约束：
  * globals.c 是 packed 格式（一行可能塞多个定义，用 `;  /* xxx */` 分隔），
    必须按「定义」粒度解析（finditer），不能按「行」解析（search 会漏掉
    每行第 2 个之后的符号，且整行删除会误删同行其它定义）。
  * 同一 SRAM/外设地址往往对应多个 DAT_ 符号（Ghidra 每引用点一个符号），
    命名时合并为单一语义变量，删除 globals.c 里的重复定义。
  * 仅处理「类型一致」的地址（该地址所有指针符号同类型），否则跳过并记录。
  * 每次运行后须 bash build.sh 零警告 + text/data/bss 与重构前一致（回归）。

用法：cd decompiled && python tools/rename_symbols.py
"""
import re, os, sys, glob
sys.stdout.reconfigure(encoding='utf-8')

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

# ===================== 语义映射表（地址 -> 语义名） =====================
# 来源：docs/PLAN.md 关键符号速查 + DATA_SEGMENT §5/§6
MAP = {
    # 外设基址
    0x2009C000: 'fio_pool',
    0x40008000: 'timer1',
    0x40090000: 'timer2',
    0x4009C000: 'uart3',
    0x4002C000: 'pinsel',
    0x400FC000: 'scb',
    0x400FC0C4: 'pconp',
    0x40034000: 'adc',
    # 输出/配置核心
    0x10001624: 'out_param',
    0x10001628: 'cfg_word',
    0x1000162C: 'out_freq_adj',
    0x10001634: 'gain_sel',
    0x10001638: 'gain_b',
    0x1000163C: 'gain_a',
    0x10001654: 'out_fine',
    0x1000165B: 'out_phase',
    0x1000165C: 'reg61_remote_en',
    0x10001660: 'reg62_start_phase',
    0x10001694: 'phase_calib',
    # 通讯
    0x100016FF: 'slave_addr',
    0x10001700: 'baud_idx',
    0x10001704: 'uart_frame_sel',
    0x10001705: 'comm_detect',
    0x1000178C: 'uart_rx_timeout',
    0x10001790: 'uart_tx_state',
    0x10001791: 'uart_tx_busy',
    0x10001792: 'uart_tx_cnt',
    0x10001793: 'uart_tx_len',
    0x10001794: 'uart_tx_flag',
    0x100017A8: 'reg_cur_idx',
    0x100017AC: 'scratch',
    0x100017B8: 'uart_global_tick',
    0x1000236C: 'uart_tx_buf',
    # 增益/闭环
    0x1000170E: 'act_gain_a',
    0x1000170F: 'act_gain_b',
    0x10001710: 'cfg_pid_sel',
    0x10001722: 'cl_thresh_hi',
    0x10001723: 'cl_thresh_lo',
    0x10001724: 'cl_gain_big',
    0x10001725: 'cl_gain_mid',
    0x10001726: 'cl_gain_small',
    0x1000212C: 'cl_cached_out',
    0x10002130: 'pid_integral',
    # 运行/输出
    0x10001785: 'run_flag',
    0x10001788: 'src_value',
    0x10001FF8: 'freq_hz',
    0x10001FF9: 'phase_cnt',
    0x10001FFC: 'out_setpoint',
    0x10002000: 'soft_start_phase',
    0x1000205C: 'out_div',
    0x10002075: 'mode_byte',
    0x10002076: 'input_state',
    0x100020CC: 'out_scale',
}

GLOBALS_C = 'firmware/globals.c'
GLOBALS_H = 'firmware/inc/globals.h'

# 全局变量命名前缀：Ghidra 反编译已把部分局部变量命名为 pinsel/uart3/out_scale 等
# （按指向的外设/语义），重命名的全局若直接用同名会遮蔽局部变量 → `x = x` 自赋值。
# 加 g_ 前缀（嵌入式 C 惯例）彻底避免撞名。
PREFIX = 'g_'

# 完整定义正则（含尾部注释 `;  /* xxx */`），能匹配 packed 格式里的一行多个定义。
# 组：1=类型 2=星号(指针标志) 3=符号名 4=地址
DEF = re.compile(
    r'(?:volatile\s+)?(uint8_t|uint16_t|uint32_t)\s*(\*?)\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*'
    r'(?:\((?:uint8_t|uint16_t|uint32_t)\s*\*\)\s*)?(0x[0-9a-fA-F]+);\s*/\*[^*]*\*/'
)

# ---- 1) 解析 globals.c 全部定义（按定义粒度，非按行） ----
text_c = open(GLOBALS_C, encoding='latin-1').read()
defs = []
for m in DEF.finditer(text_c):
    typ, star, name, addr = m.groups()
    defs.append((name, int(addr, 16), typ, bool(star), m.start(), m.end()))

from collections import defaultdict
addr_syms = defaultdict(list)
for name, addr, typ, isptr, s, e in defs:
    if isptr:  # 仅指针符号参与地址映射（标量的"值"可能恰好等于外设地址，须排除）
        addr_syms[addr].append((name, typ))

# ---- 2) 分类映射地址 ----
plan = {}     # addr -> semantic_name（可执行：类型一致）
skip = {}     # addr -> reason
for addr, name in sorted(MAP.items()):
    syms = addr_syms.get(addr, [])
    if not syms:
        skip[addr] = 'no-symbol'
        continue
    types = set(t for _, t in syms)
    if len(types) > 1:
        skip[addr] = 'mixed-type:' + '/'.join(sorted(types))
        continue
    plan[addr] = PREFIX + name

print('== classification ==')
print('  rename(类型一致): %d' % len(plan))
print('  skip: %d' % len(skip))
for a, r in sorted(skip.items()):
    print('    skip 0x%08X  %s' % (a, r))

# ---- 3) 构建 symbol -> semantic_name（按符号名长度降序，避免前缀误伤） ----
symbol_map = {}
for addr, name in plan.items():
    for sym, typ in addr_syms[addr]:
        symbol_map[sym] = name
ordered = sorted(symbol_map.items(), key=lambda kv: -len(kv[0]))

print()
print('== symbol -> name (%d symbols merged into %d vars) ==' % (len(ordered), len(plan)))

# ---- 4) src 文件：文本替换 symbol -> name ----
src_files = sorted(glob.glob('firmware/src/*.c')) + [
    f for f in glob.glob('firmware/*.c') if os.path.normpath(f) != os.path.normpath(GLOBALS_C)]
for f in src_files:
    t = open(f, encoding='latin-1').read()
    o = t
    for sym, name in ordered:
        t = t.replace(sym, name)
    if t != o:
        open(f, 'w', encoding='latin-1').write(t)
        print('  src  %s' % f)

# ---- 5) globals.h：extern 声明替换 + 按语义名去重 ----
lines_h = open(GLOBALS_H, encoding='latin-1').read().split('\n')
seen_h = set()
out_h = []
for ln in lines_h:
    for sym, name in ordered:
        ln = ln.replace(sym, name)
    if ln in seen_h:
        continue
    seen_h.add(ln)
    out_h.append(ln)
open(GLOBALS_H, 'w', encoding='latin-1').write('\n'.join(out_h))
print('  hdr  %s' % GLOBALS_H)

# ---- 6) globals.c：按定义粒度重写，合并重复（每地址保留首个，改语义名，删其余） ----
out = []
prev = 0
seen = set()
removed = 0
for name, addr, typ, isptr, s, e in defs:
    out.append(text_c[prev:s])
    if isptr and addr in plan:
        if addr in seen:
            removed += 1          # 删除重复定义
        else:
            seen.add(addr)
            seg = text_c[s:e]
            seg = seg.replace(name, plan[addr], 1)
            out.append(seg)
    else:
        out.append(text_c[s:e])   # 非计划地址 / 标量：原样保留
    prev = e
out.append(text_c[prev:])
open(GLOBALS_C, 'w', encoding='latin-1').write(''.join(out))
print('  def  %s (删除重复定义 %d 处)' % (GLOBALS_C, removed))

print()
print('DONE. Next: cd firmware && bash build.sh')
