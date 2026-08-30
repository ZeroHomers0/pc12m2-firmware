# -*- coding: utf-8 -*-
"""w8_stack_sentinel.py — W8 阶段1 SRAM 哨兵：测中断负载下最低 MSP 栈水位

背景（W8_HARDWARE_TEST 阶段1 必检项，W7 遗留）：
  原固件栈区 = 0x100027C8（栈底）.. 0x100029C8（栈顶，原始 BIN 初始 SP）共 512B。
  offline `-fstack-usage` 只证最大单函数静态帧 48B，不含调用链/中断嵌套，
  必须实机测中断负载下的最低 MSP 水位并保留安全余量。

原理（固件复用 P1.30/P1.29 调试脚 → 运行中无法 halt 读 SP，J-Link V9.70 亦无循环命令）：
  1. connect-under-reset 停核（固件启动代码未跑，.bss 未清零）。
  2. ClrRESET 放行 → 启动代码清 .bss（含栈区 0x100027C8..0x100029C8 → 全 0 基线）。
  3. 固件运行 N 秒（TIMER1 LCD 扫描 / TIMER2 / 按键 / 菜单 / Modbus 等中断负载）。
  4. SetRESET 停核（不复位执行，内存保留运行后残留）。
  5. mem32 读栈区 → 从栈顶往下找「最后一个非 0 字」= 历史最低栈覆写位 ≈ 最低 MSP。
     （启动清零后，栈未到区域保持 0；栈活跃/曾到区域残留非 0 运行值。）

判定：最低水位 > 栈底 0x100027C8 + 安全余量（默认 128B）→ PASS。
      （水位 ≤ 栈底 → 栈溢出覆盖业务 .bss/.data，危险。）

用法：
  生成脚本：  python tools/w8/w8_stack_sentinel.py gen --run-ms 60000 -o sentinel.jlink
  执行：      JLink.exe -device LPC1765 -if SWD -speed 4000 -CommanderScript sentinel.jlink > sentinel.log
  分析：      python tools/w8/w8_stack_sentinel.py analyze sentinel.log
"""
import re, sys, argparse, os

sys.stdout.reconfigure(encoding='utf-8')

STACK_TOP = 0x100029C8      # 栈顶（初始 SP）
STACK_BOT = 0x100027C8      # 栈底（安全水位下界）
STACK_NWORDS = (STACK_TOP - STACK_BOT) // 4   # 128 字
JLINK = '/d/software/SEGGER/JLink_V970/JLink.exe'


def gen(run_ms=60000, out='w8_stack_sentinel.jlink'):
    script = f"""// W8 阶段1：SRAM 哨兵测最低 MSP 栈水位（栈区 0x100027C8..0x100029C8, 512B）
// connect-under-reset 停核 → ClrRESET 运行 {run_ms}ms → SetRESET 停核读栈区残留。
// 只读+复位，不写 Flash/EEPROM。前置：断开市电/门极/功率负载，仅控制电。
device LPC1765
if SWD
speed 4000
SetRESET
sleep 200
connect
ClrRESET
sleep {run_ms}
SetRESET
sleep 200
connect
// 栈区 0x100027C8..0x100029C8 全量读回（{STACK_NWORDS} 字）
mem32 0x{STACK_BOT:08X}, {STACK_NWORDS}
exit
"""
    with open(out, 'w', encoding='utf-8') as f:
        f.write(script)
    print(f"[gen] 已生成 {out}（运行窗口 {run_ms/1000:.0f}s，运行期间请操作面板按键/菜单产生中断负载）")
    print(f"  执行： \"{JLINK}\" -device LPC1765 -if SWD -speed 4000 -CommanderScript {out} > sentinel.log")


def _parse_mem32(log_text):
    """解析 J-Link `mem32 addr, n` 输出 → {地址: 值}（字节地址 → 字值）。"""
    words = {}
    for line in log_text.splitlines():
        m = re.match(r'\s*([0-9A-Fa-f]{8})\s*=\s*(.*)$', line)
        if not m:
            continue
        base = int(m.group(1), 16)
        vals = m.group(2).split()
        for i, v in enumerate(vals):
            if len(v) == 8 and re.fullmatch(r'[0-9A-Fa-f]{8}', v):
                words[base + 4 * i] = int(v, 16)
    return words


def analyze(log_path):
    text = open(log_path, encoding='utf-8', errors='ignore').read()
    words = _parse_mem32(text)
    if not words:
        print(f"[analyze] 未从 {log_path} 解析到 mem32 数据（检查连接与输出）")
        return 1
    # 补齐 0x100027C8..0x100029C4（128 字）
    w = {}
    for off in range(0, STACK_NWORDS * 4, 4):
        w[STACK_BOT + off] = words.get(STACK_BOT + off, None)
    missing = [a for a, v in w.items() if v is None]
    if missing:
        print(f"[analyze] 警告：{len(missing)} 个字未读到，最低地址 0x{min(missing):08X}")
    # 从栈顶（0x100029C4，SP 压栈首个字）往下找最后一个非 0 字 = 历史最低栈覆写位
    last_nonzero = None
    nz_run = 0
    for off in range(STACK_NWORDS * 4 - 4, -1, -4):   # 从高地址（栈顶附近）往下
        addr = STACK_BOT + off
        val = w.get(addr)
        if val:
            last_nonzero = addr
            nz_run += 1
        else:
            break
    if last_nonzero is None:
        print("[analyze] 栈区全 0——固件可能未运行/复位异常，需重试")
        return 1
    lowest_msp = last_nonzero
    depth = STACK_TOP - (lowest_msp + 4)             # 栈顶到最低覆写位下方的深度
    margin = lowest_msp - STACK_BOT                  # 最低水位距栈底（安全下界）余量
    print("=== W8 阶段1 SRAM 哨兵分析 ===")
    print(f"  栈顶 _estack        : 0x{STACK_TOP:08X}")
    print(f"  栈底（安全下界）    : 0x{STACK_BOT:08X}")
    print(f"  连续非0字跨度(向下) : {nz_run} 字")
    print(f"  最低栈覆写位(≈最低MSP): 0x{lowest_msp:08X}")
    print(f"  栈深度(顶→水位)     : {depth} B")
    print(f"  距栈底余量          : {margin} B")
    safe = margin >= 128
    print(f"  判定                 : {'PASS（余量≥128B）' if safe else '⚠ FAIL（余量<128B，栈溢出风险）'}"
          if margin > 0 else "  判定                 : ⚠ 水位触底（栈溢出覆盖业务数据）")
    print(f"  （安全余量阈值 128B；最低水位必须 > 栈底 0x{STACK_BOT:08X}）")
    return 0 if safe else 1


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description='W8 阶段1 SRAM 哨兵（最低 MSP 栈水位）')
    sub = ap.add_subparsers(dest='cmd', required=True)
    g = sub.add_parser('gen', help='生成 .jlink 脚本')
    g.add_argument('--run-ms', type=int, default=60000, help='运行窗口毫秒（默认 60000）')
    g.add_argument('-o', '--out', default='w8_stack_sentinel.jlink')
    a = sub.add_parser('analyze', help='解析 J-Link 日志')
    a.add_argument('log')
    args = ap.parse_args()
    if args.cmd == 'gen':
        gen(args.run_ms, args.out)
    else:
        sys.exit(analyze(args.log))
