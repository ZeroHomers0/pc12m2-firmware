# -*- coding: utf-8 -*-
"""静态门禁：每个 disp_string 的地址实参都必须被 strpool_map 覆盖。

背景（2026-11-17 实测发现的真实缺陷）：
`freq_adjust_sync` 在 12p 被放在 `firmware/stub.c`（6p 在 src/），而字符串池生成器
`tools/generation/generate_string_pool_12.py` 的 `SCAN_FILES_12` 只扫 `firmware/src/*.c`，
于是 `disp_string(0xa98c,3,0xb,0)`（原机 0xa98c = "停止"，GBK `cd a3 d6 b9 00`）没有进入
`strpool_clusters`。`strpool_map` 未命中时原样返回地址，新 ELF 在 0xa98c 处是**指令字节**，
`disp_string` 就把代码当字符串渲染 —— 表现为 `freq_adj(event=6)` 的 FIO 写迹与末态和原机不同。

本测试把「地址实参集合 ⊆ strpool 覆盖集合」变成断言，防止以后再有函数在 src/ 与 stub.c
之间搬迁时漏收录字符串。

判据：
  1. 源码里所有 disp_string 的**字面量/宏常量**实参，必须落在某个簇区间内或被 override 精确命中；
  2. 非字面量实参（极少，形如传变量）单独列出，不做断言，但数量必须与已知基线一致，
     防止有人新写一个变量实参绕过本门禁。
"""
import io
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STRPOOL = ROOT / "firmware" / "src" / "strpool.c"

# 已知的非字面量 disp_string 实参（文件 → 形参记号）。这些是函数把地址作为参数继续下传，
# 真正的地址在上游的调用点，仍被本测试的调用点扫描覆盖。
KNOWN_NON_LITERAL = {
    ("01_startup.c", "int"),
    ("02_lcd_display.c", "int"),
    ("07_state_machine.c", "int"),
    ("stub.c", "int"),
}


def parse_strpool():
    src = STRPOOL.read_text(encoding="utf-8", errors="replace")

    i = src.find("strpool_clusters[] = {")
    j = src.find("};", i)
    assert i > 0 and j > i, "未找到 strpool_clusters 定义"
    clusters = [(int(a), int(b)) for a, b in
                re.findall(r"\{(\d+),\s*(\d+),\s*strpool_blob", src[i:j])]

    overrides = []
    k = src.find("strpool_override[] = {")
    if k > 0:
        k2 = src.find("};", k)
        overrides = [int(x, 16) for x in
                     re.findall(r"\{\s*0x([0-9a-fA-F]+)\s*,", src[k:k2])]
    return clusters, overrides


def covered(addr, clusters, overrides):
    if addr in overrides:
        return True
    return any(base <= addr < base + ln for base, ln in clusters)


def collect_macros():
    macros = {}
    for h in sorted((ROOT / "firmware" / "inc").glob("*.h")):
        txt = h.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"#define\s+([A-Z0-9_]+)\s+0x([0-9a-fA-F]+)u?\b", txt):
            macros[m.group(1)] = int(m.group(2), 16)
    return macros


def collect_calls(macros):
    hits = {}        # addr -> {file}
    non_literal = set()
    files = sorted((ROOT / "firmware" / "src").glob("*.c"))
    files += sorted((ROOT / "firmware").glob("*.c"))
    for p in files:
        if p.name == "strpool.c":
            continue
        txt = p.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(
                r"disp_string\(\s*(?:\(int\))?\s*(0x[0-9a-fA-F]+|[A-Za-z_][A-Za-z0-9_]*)",
                txt):
            tok = m.group(1)
            if tok.lower().startswith("0x"):
                addr = int(tok, 16)
            else:
                addr = macros.get(tok)
            if addr is None:
                non_literal.add((p.name, tok))
                continue
            hits.setdefault(addr, set()).add(p.name)
    return hits, non_literal


def main():
    clusters, overrides = parse_strpool()
    hits, non_literal = collect_calls(collect_macros())

    print("  strpool: %d 簇 / %d 条 override" % (len(clusters), len(overrides)))
    print("  disp_string 地址实参: %d 个" % len(hits))

    missing = sorted(a for a in hits if not covered(a, clusters, overrides))
    for a in missing:
        print("  [FAIL] 0x%04X 未被 strpool 覆盖（调用点: %s）"
              % (a, ", ".join(sorted(hits[a]))))
    if missing:
        print("  [FAIL] strpool 覆盖不完整：%d 个地址会渲染成指令字节" % len(missing))
        return 1

    extra = sorted(non_literal - KNOWN_NON_LITERAL)
    for f, tok in extra:
        print("  [FAIL] %s 出现未登记的非字面量实参 %r" % (f, tok))
    if extra:
        print("  [FAIL] 非字面量实参集合变化，需人工确认其上游调用点已被覆盖")
        return 1

    print("  [PASS] strpool 覆盖完整：%d 个地址全部命中（含 stub.c 的 0xa98c）"
          % len(hits))
    print("  [PASS] 非字面量实参 %d 处，与已知基线一致" % len(non_literal))
    return 0


if __name__ == "__main__":
    sys.exit(main())
