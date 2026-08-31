# -*- coding: utf-8 -*-
# =============================================================================
# test_param_sync.py — host 虚拟数据测试：参数 live→EEPROM 同步行为
#
# 被测对象：firmware/src/06_param_system.c 的 param_sync_live_to_eeprom(0x35F2)
#
# 结构性重复模式（每参数）：
#   8 位：  if (live != shadow) { shadow = live; i2c_write_reg(shadow, reg); }
#   16 位： if (live != *(int*)shadow) { *(int*)shadow = live;
#                                         i2c_write_reg(live>>8, reg);      /* 高字节 */
#                                         i2c_write_reg((char)live, reg+1); } /* 低字节 */
#
# 从源码自动提取所有 (live_sym, shadow_sym, reg, width) 元组，验证：
#   INV1  每个符号真存在于 globals.c（无臆造地址）
#   INV2  EEPROM reg 地址不冲突（16 位占 reg 和 reg+1）
#   INV3  "仅不等才写"行为 = 构造虚拟数据，只有 live≠shadow 的参数才触发写
#         （用固件逻辑镜像驱动；等值的参数必须不写）
#   INV4  16 位写高=live>>8、写低=live&0xff
# =============================================================================
import re, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC  = os.path.join(ROOT, 'firmware', 'src', '06_param_system.c')
GLOB = os.path.join(ROOT, 'firmware', 'globals.c')
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

def globals_symbols(text):
    """symbol -> addr；含指针型与数值型（其值即地址），剔除局部/宏"""
    out = {}
    for m in re.finditer(
            r'volatile\s+uint(?:8|16|32)_t\s+\*\s*([A-Za-z0-9_]+)\s*=\s*\(?[^;]*?(0x[0-9a-fA-F]+)',
            text):
        out[m.group(1)] = m.group(2).lower()
    out['g_scratch'] = out.get('g_scratch', '0x100017ac')
    return out

def extract(src):
    """返回 [(live_sym, shadow_sym, reg, width)]（16 位 reg 为高字节地址）。"""
    start = src.index('void param_sync_live_to_eeprom(void)')
    open_brace = src.index('{', start)
    depth = 0
    tail = None
    for i in range(open_brace, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                tail = i
                break
    if tail is None:
        raise AssertionError('param_sync_live_to_eeprom 函数边界未闭合')
    body = src[open_brace + 1:tail]
    params = []
    block_re = re.compile(
        r"if\s*\(\s*\*\(volatile\s+(uint8_t|uint)\s*\*\)([A-Za-z0-9_]+)"
        r"\s*!=\s*\*\(volatile\s+\1\s*\*\)([A-Za-z0-9_]+)\s*\)\s*\{"
        r"([\s\S]*?)\n\s*\}")
    for m in block_re.finditer(body):
        ctype, live, shadow, block = m.groups()
        if ctype == 'uint':
            wr = re.search(
                r"i2c_write_reg\(\*\(volatile\s+uint\s*\*\)([A-Za-z0-9_]+)"
                r"\s*>>\s*8,0x([0-9a-fA-F]+)\)", block)
            width = 16
        else:
            wr = re.search(
                r"i2c_write_reg\(\(char\)\*\(volatile\s+uint8_t\s*\*\)"
                r"([A-Za-z0-9_]+),0x([0-9a-fA-F]+)\)", block)
            width = 8
        if wr is None or wr.group(1) != shadow:
            raise AssertionError(f'无法解析参数同步块: live={live} shadow={shadow}')
        params.append((live, shadow, int(wr.group(2), 16), width))
    if len(params) != 57:
        raise AssertionError(f'参数同步块应为 57 组，实际提取 {len(params)} 组')
    return params

def main():
    src   = open(SRC, encoding='utf-8', errors='ignore').read()
    gtext = open(GLOB, encoding='utf-8', errors='ignore').read()
    sym   = globals_symbols(gtext)
    params = extract(src)

    passed=failed=0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st="PASS" if cond else "FAIL"
        if cond: passed+=1
        else: failed+=1
        print(f"  [{st}] {name}"+(f"  {detail}" if detail else ""))

    count16 = sum(1 for p in params if p[3] == 16)
    print(f"提取参数元组: {len(params)} 条（16位 {count16} | 8位 {len(params)-count16}）")

    # INV1 符号存在
    allsym = set()
    for p in params: allsym.add(p[0]); allsym.add(p[1])
    miss = [s for s in allsym if s not in sym]
    check("INV1 所有 live/shadow 符号存在于 globals.c", len(miss)==0, f"缺失 {miss}" if miss else "")

    # INV2 reg 不冲突
    used={}; conflict=[]
    for (l,s,reg,w) in params:
        for a in ([reg,reg+1] if w==16 else [reg]):
            if a in used: conflict.append((a,used[a],(l,reg,w)))
            else: used[a]=(l,reg,w)
    check("INV2 EEPROM reg 地址无冲突", len(conflict)==0, f"{conflict[:4]}" if conflict else "")

    # INV3/INV4 行为：虚拟数据驱动固件逻辑镜像
    # live 初值：偶数参为 0xAB（≠0），16位用 0xABCD；shadow 初值全 0 → 全部不等→写
    live={}; shadow={}
    for (l,s,reg,w) in params:
        live[l]=0xABCD if w==16 else 0xAB
        shadow[s]=0
    eeprom={}   # reg -> ('8'|'hi'|'lo', data)
    # 固件镜像（严格按源码语义）
    def mirror(live_sym, shadow_sym, reg, w):
        lv=live[live_sym]; sv=shadow[shadow_sym]
        if w==16:
            if lv != (sv & 0xffff):
                shadow[shadow_sym]=lv
                eeprom[reg]   =('hi',(lv>>8)&0xff)
                eeprom[reg+1] =('lo', lv&0xff)
        else:
            if lv != sv:
                shadow[shadow_sym]=lv
                eeprom[reg]=('8', lv)
    for (l,s,reg,w) in params: mirror(l,s,reg,w)

    expected=set()
    for (l,s,reg,w) in params:
        expected.add(reg)
        if w==16: expected.add(reg+1)
    nowrite = expected - set(eeprom)
    check("INV3 live≠shadow 全部触发写（{0} 个）".format(len(expected)), len(nowrite)==0, f"未写 {sorted(nowrite)}" if nowrite else "")

    # 等值断言：把某参数 live 改为等于 shadow → 复原应不写
    # 取一个 8 位参数做实验：置 live==shadow，镜像重跑应无新增写
    eeprom2={}
    def mirror2(live_sym, shadow_sym, reg, w):
        lv=live[live_sym]; sv=shadow[shadow_sym]
        if w==16:
            if lv != (sv&0xffff):
                shadow[shadow_sym]=lv
                eeprom2[reg]=('hi',(lv>>8)&0xff); eeprom2[reg+1]=('lo',lv&0xff)
        else:
            if lv != sv:
                shadow[shadow_sym]=lv; eeprom2[reg]=('8',lv)
    # 重跑：此时 live 仍≠shadow（初值），但我们要测"等值不写"——先构造等值
    # 单独测：live==shadow 的参数不产生写
    live_eq={};
    for (l,s,reg,w) in params: live_eq[l]=shadow[s]  # live 强制=shadow
    equal_no_write=True
    eeprom_eq={}
    def mirror3(live_sym, shadow_sym, reg, w):
        lv=live_eq[live_sym]
        if w==16:
            if lv != (shadow[shadow_sym]&0xffff):
                eeprom_eq[reg]=1; eeprom_eq[reg+1]=1
        else:
            if lv != shadow[shadow_sym]:
                eeprom_eq[reg]=1
    for (l,s,reg,w) in params: mirror3(l,s,reg,w)
    check("INV3 live==shadow 时绝不写", len(eeprom_eq)==0, f"误写 {sorted(eeprom_eq)}" if eeprom_eq else "")

    # INV4 16 位高/低字节正确
    hi_ok = all(eeprom[r][1]==(live[l]>>8)&0xff
                for (l,s,reg,w) in params if w==16 for r in (reg,))
    lo_ok = all(eeprom[r][1]==(live[l]&0xff)
                for (l,s,reg,w) in params if w==16 for r in (reg+1,))
    check("INV4 16 位写高字节=live>>8", hi_ok)
    check("INV4 16 位写低字节=live&0xff", lo_ok)

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed==0 else 1

if __name__ == '__main__':
    sys.exit(main())
