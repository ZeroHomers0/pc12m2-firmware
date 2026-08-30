# -*- coding: utf-8 -*-
# gen_globals.py — 从反编译 .c 提取 DAT_/PTR_/_DAT_/uRam 符号，生成 globals.h/.c
# 2026-08-21 目标B 阶段2
# 策略（已确认）：每个 flash 字面量池符号（DAT_0000xxxx）定义为 uint32_t 全局变量，
#   初值 = LPC1765.bin 该地址的 4 字节。则 *DAT_x = 解引用变量 = 访问其值指向的
#   SRAM/外设/表，精确复现反编译语义。
# 直接 MMIO 符号（地址>=0x40000000：DAT_40000000/_DAT_4000400c/uRame000e100）不生成
#   变量——应替换为 reg.h 宏（阶段4），仅列入报告。
# 输出：firmware/inc/globals.h + firmware/globals.c + docs/_globals_report.txt
import re, struct, sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')

ROOT = str(Path(__file__).resolve().parents[2])
BIN = open(ROOT + r"\LPC1765.bin", "rb").read()
FLASH_LEN = len(BIN)
OUT = []

MODULES = [
    "evidence/reverse/decompiled/01_startup.c", "evidence/reverse/decompiled/02_lcd_display.c",
    "evidence/reverse/decompiled/03_input_debounce.c", "evidence/reverse/decompiled/04_i2c.c",
    "evidence/reverse/decompiled/05_adc.c", "evidence/reverse/decompiled/06_param_system.c",
    "evidence/reverse/decompiled/07_state_machine.c", "evidence/reverse/decompiled/08_uart3_modbus.c",
    "evidence/reverse/decompiled/09_output_stage.c", "evidence/reverse/decompiled/10_relay_led.c",
    "evidence/reverse/decompiled/11_auth.c", "evidence/reverse/decompiled/12_closed_loop.c",
    "evidence/reverse/decompiled/13_gpio_init.c",
]

# ── 1. 扫描符号全集 ──────────────────────────────────────────
sym = {}
for mod in MODULES:
    src = open(ROOT + "\\" + mod, "rb").read().decode("utf-8", errors="replace")
    for m in re.finditer(r'\b(?:PTR_)?_?DAT_[0-9a-fA-F]{4,8}\b', src):
        s = m.group(0)
        sym.setdefault(s, 0)
        sym[s] += 1
    # PTR_<label>_<hex> 型（Ghidra 人类命名指针表标签，如 PTR_disp_scan_0001002c）。
    # 初值 = flash 该地址存放的指针（SRAM/外设地址）。仅 09 大量使用。
    for m in re.finditer(r'\bPTR_[A-Za-z][A-Za-z0-9_]*_[0-9a-fA-F]{4,8}\b', src):
        s = m.group(0)
        sym.setdefault(s, 0)
        sym[s] += 1
    for m in re.finditer(r'\buRam[a-fA-F0-9]{8}\b', src):
        s = m.group(0)
        sym.setdefault(s, 0)
        sym[s] += 1

# ── 1.5 拼接全部源码，供访问模式启发式 ──────────────────────
ALL_SRC = ""
for mod in MODULES:
    ALL_SRC += open(ROOT + "\\" + mod, "rb").read().decode("utf-8", errors="replace") + "\n"

def has_offset_arith(name):
    """DAT_x 参与字节偏移算术（DAT_x + off / DAT_x - off）→ 必须值型！
       若误声明为指针，DAT_x+off 变成元素偏移x4 → 静默错误。
       注意 lookbehind (?<!*)：排除 `*DAT_x + ...` 这类**解引用值**的
       算术（如 *DAT_a - *DAT_b），它们不是偏移算术；只有裸值 DAT_x + off
       才是。修复：12_closed_loop 等 61 符号曾误判。"""
    pat = r"(?<!\*)\b%s\s*[+-]" % re.escape(name)
    return re.search(pat, ALL_SRC) is not None

def has_deref_or_index(name):
    """*DAT_x 解引用 或 DAT_x[i] 下标 → 指针型"""
    pat = r"\*%s\b|%s\[" % (re.escape(name), re.escape(name))
    return re.search(pat, ALL_SRC) is not None

def has_cast_deref(name):
    """*(type *)(DAT_x + off) 强转解引用（CRC 表等）→ DAT_x 是地址值，
       字节偏移由强转控制，必须 value（若指针型，DAT_x+off 会×4 静默错）。"""
    pat = r"\*\([^)]*\*\)\s*\(?\s*%s\s*[+-]" % re.escape(name)
    return re.search(pat, ALL_SRC) is not None

def has_char_literal_access(name):
    """*DAT_x 与 char 字面量接触（'\\0'、'x'、== ' 等）→ byte 访问证据"""
    pat = r"\*%s\s*(?:[=!<>]=?)\s*'" % re.escape(name)
    return re.search(pat, ALL_SRC) is not None

def has_byte_ptr_assign(name):
    """DAT_x 被赋给 byte* 局部（Ghidra 标注 byte* 的计数/状态变量）→ ptr_byte。
       证据：`byte *pbVar2; ... pbVar2 = DAT_x;`。05_adc 轮转计数等。"""
    for m in re.finditer(r"(\w+)\s*=\s*%s\b" % re.escape(name), ALL_SRC):
        var = m.group(1)
        if re.search(r"\bbyte\s*\*\s*%s\b" % re.escape(var), ALL_SRC):
            return True
    return False

def sym_type(name, cls):
    """符号类型决策（优先级）：
       0) 手判白名单：value_forced→value；ptr_byte_forced→ptr_byte
       1) 直接解引用/下标（*DAT_x / DAT_x[i]）→ 指针型（DAT_x+off 的 ×4 是真实元素偏移）
       2) 强转解引用（*(byte*)(DAT_x+off)）→ 值型（字节偏移由强转控制）
       3) 裸值偏移算术 DAT_x+off 且初值为地址 → 指针型
       4) 其余 → 值型。修复：has_offset 原优先导致 DAT_01efc 等 61+49 符号误判。"""
    if cls == "value_forced":
        return "value"
    if cls == "ptr_byte_forced":
        return "ptr_byte"
    # PTR_<label>_<hex>（非 DAT）：Ghidra 命名指针表标签 → 初值即指针地址。
    # 09 中一律作 undefined*/byte 基址 + 字节偏移访问（`*PTR_x`、`*(type*)(PTR_x+off)`、
    #   `puVar=PTR_x`、`*PTR_x=='x'`），word 访问全部经显式 cast → 统一 ptr_byte。
    if re.match(r'PTR_[A-Za-z][A-Za-z0-9_]*_[0-9a-fA-F]{4,8}$', name) \
            and not name.startswith("PTR_DAT_"):
        return "ptr_byte"
    if has_deref_or_index(name):
        return "ptr_byte" if (has_char_literal_access(name) or has_byte_ptr_assign(name)) else "ptr_word"
    if has_cast_deref(name):
        return "value"
    if has_offset_arith(name):
        if cls in ("sram", "peri", "flash"):
            return "ptr_word"
        return "value"
    return "value"

# ── 2. 地址解析 + 分类 ───────────────────────────────────────
def sym_addr(name):
    if name.startswith("uRam"):
        return int(name[4:], 16)
    # DAT_ / _DAT_ / PTR_DAT_ 尾随 hex
    return int(re.search(r'[0-9a-fA-F]+$', name).group(0), 16)

def dword(addr):
    if addr + 4 > FLASH_LEN:
        return None
    return struct.unpack("<I", BIN[addr:addr+4])[0]

SRAM_BASE, SRAM_END = 0x10000000, 0x10008000
PERI = (0x20000000, 0x400FC000)  # FIO 池 / 外设区判断

def classify(val):
    if val is None:
        return "uninit"
    if SRAM_BASE <= val < SRAM_END:
        return "sram"
    if (0x20000000 <= val < 0x20100000) or (0x40000000 <= val < 0x40100000) or (0xE0000000 <= val < 0xE0100000):
        return "peri"
    if 0x100 < val < FLASH_LEN:
        return "flash"
    return "const"

# 值覆盖白名单（启发式无法区分"flash 地址初值"与"纯数值常量"时手判）。
# DAT_0000149c：初值 0x186A0=100000 是 disp_number 的十万位位权除数（02_lcd 除/
#   比较语义），不是 flash 地址 → 必须 value；has_offset_arith 曾误判 ptr_word。
FORCE_VALUE = {"DAT_0000149c"}
# 字节指针覆盖白名单：被赋给 `undefined *` 后按字节偏移解引用（puVar+off = +off 字节）。
# PTR_DAT_0000e988：FIO 池基址 0x2009C000，09 pin_config 全部按字节偏移访问
#   （puVar1+0x1c=FIO0CLR、+0x18=FIO0SET、+0x40=FIO2DIR、+0x58=FIO2SET）；
#   启发式只看 `*(uint *)PTR_DAT_xxx` 判 value，实为 undefined*/byte* 基址 → 必须 ptr_byte。
# 其余 PTR_DAT_0000fbd0/fbd4/1000c/10030/10034/10454/10640 同理（09 TIMER1 ISR/
#   EINT/TIMER2 基址，全部 `*(type *)(base + off)` 字节偏移 + 赋给 undefined*）。
# DAT_00000748/750/a4（01 主控状态）：0x10000748=认证重试计数（<3/自增）、
#   0x10000750=锁机标志（0/1）、0x100007A4=input_code（input_scan_state 返回
#   undefined1，1 字节）→ 反编译上下文确证字节变量，必须 ptr_byte（否则 *DAT_x=val
#   写 4 字节会污染相邻 SRAM 变量）。0x1000078C 宽度未定，暂不强制。
FORCE_PTR_BYTE = {"PTR_DAT_0000e988", "PTR_DAT_0000fbd0", "PTR_DAT_0000fbd4",
                  "PTR_DAT_0001000c", "PTR_DAT_00010030", "PTR_DAT_00010034",
                  "PTR_DAT_00010454", "PTR_DAT_00010640",
                  "DAT_00000748", "DAT_00000750", "DAT_000007a4"}

mmio, normal = [], {}
for name in sorted(sym):
    addr = sym_addr(name)
    if addr >= 0x40000000:            # 直接 MMIO 符号（非字面量池）
        mmio.append((name, addr))
        continue
    val = dword(addr)
    if name in FORCE_VALUE:
        cls = "value_forced"
    elif name in FORCE_PTR_BYTE:
        cls = "ptr_byte_forced"
    else:
        cls = classify(val)
    normal[name] = (addr, val, cls)

# ── 3. 写 globals.h / globals.c ──────────────────────────────
H = ["/* 自动生成：tools/generation/generate_globals.py（目标B 阶段2）。勿手改。",
     " * 每个符号初值 = LPC1765.bin flash 字面量池内容。类型默认 uint32_t；",
     " * 访问宽度（byte/word）与符号语义不一致由阶段4在 src 修正。 */",
     "#ifndef GLOBALS_H", "#define GLOBALS_H", "#include <stdint.h>", ""]
C = ["/* 自动生成：tools/generation/generate_globals.py（目标B 阶段2）。勿手改。 */",
     '#include "inc/globals.h"', ""]

_TY = {}
def sym_ty(name, cls):
    key = (name, cls)
    if key not in _TY:
        _TY[key] = sym_type(name, cls)
    return _TY[key]

for name in sorted(normal):
    addr, val, cls = normal[name]
    if val is None:
        continue
    ty = sym_ty(name, cls)
    if ty == "ptr_byte":
        H.append("extern volatile uint8_t *%s;" % name)
        C.append("volatile uint8_t *%s = (uint8_t *)0x%08X;  /* %s byte */" % (name, val, cls))
    elif ty == "ptr_word":
        H.append("extern volatile uint32_t *%s;" % name)
        C.append("volatile uint32_t *%s = (uint32_t *)0x%08X;  /* %s word */" % (name, val, cls))
    else:
        H.append("extern uint32_t %s;" % name)
        C.append("uint32_t %s = 0x%08X;  /* %s value */" % (name, val, cls))

H.append("")
H.append("#endif /* GLOBALS_H */")
C.append("")

open(ROOT + r"\firmware\inc\globals.h", "w", encoding="utf-8").write("\n".join(H))
open(ROOT + r"\firmware\globals.c", "w", encoding="utf-8").write("\n".join(C))

# ── 4. 报告 ──────────────────────────────────────────────────
from collections import Counter
cnt = Counter(cls for _, _, cls in normal.values())
ty_cnt = Counter(sym_ty(n, c) for n, (_, _, c) in normal.items())
OUT.append("符号总数: %d（去重）" % len(normal))
OUT.append("分类: " + ", ".join("%s=%d" % (k, v) for k, v in cnt.most_common()))
OUT.append("类型分布: " + ", ".join("%s=%d" % (k, v) for k, v in ty_cnt.most_common()))
OUT.append("")
OUT.append("ptr_word（volatile uint32_t*，纯解引用/下标）:")
OUT.append("  " + ", ".join(n for n in sorted(normal) if sym_ty(n, normal[n][2]) == "ptr_word"))
OUT.append("")
OUT.append("ptr_byte（volatile uint8_t*，char 字面量接触）:")
OUT.append("  " + ", ".join(n for n in sorted(normal) if sym_ty(n, normal[n][2]) == "ptr_byte"))
OUT.append("")
OUT.append("value（uint32_t，强转解引用/裸值）:")
OUT.append("  " + ", ".join(n for n in sorted(normal) if sym_ty(n, normal[n][2]) == "value"))
OUT.append("直接 MMIO 符号 %d 个（不生成变量，应替换 reg.h 宏）:" % len(mmio))
for name, addr in mmio:
    OUT.append("  %-24s 0x%08X" % (name, addr))
OUT.append("")
OUT.append("已生成: globals.h extern 声明 + globals.c 定义（%d 个变量）" % len(normal))
OUT.append("")
OUT.append("符号明细（地址 → 初值 → 分类）:")
for name in sorted(normal):
    addr, val, cls = normal[name]
    OUT.append("  %-24s flash 0x%04X = 0x%08X → %s" % (name, addr, val if val is not None else 0, cls))

open(ROOT + r"\evidence\reverse\reports\_globals_report.txt", "w", encoding="utf-8").write("\n".join(OUT))
print("done: %d symbols, %d mmio" % (len(normal), len(mmio)))
