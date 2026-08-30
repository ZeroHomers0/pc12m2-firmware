#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
w8_analyze_wave.py — 读示波器导出的 CSV，自动统计触发脉冲的脉宽/周期/相邻组间隔，并换算电角度。

背景（W8 阶段 C / D）：固件按 50/60Hz 电网发 SCR 触发脉冲，预期
   触发脉宽 ≈ 12°、相邻触发组间隔 ≈ 60°（50Hz 下 12°≈0.667ms、60°≈3.333ms）。
本脚本从波形里量出这些数字，判断是否符合预期。

用法：
    python tools/w8/w8_analyze_wave.py 波形.csv [--v-thresh 1.5] [--col 0,1] [--period 0.02]

    --v-thresh  判断"高电平脉冲"的电压阈值（默认 1.5V）
    --col       逗号分隔的两列为【时间列, 电压列】的 0 起索引（默认 0,1）
    --period    已知工频周期（秒），用于换算出电角度；默认按实测周期自动估计

依赖：仅标准库（csv）。输出可贴回给 AI 进一步分析。
"""
import sys, csv, argparse
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--v-thresh', type=float, default=1.5, help='高电平阈值(V)')
    ap.add_argument('--col', default='0,1', help='时间列,电压列(0起索引)')
    ap.add_argument('--period', type=float, default=None, help='已知工频周期(秒)，如0.02=50Hz')
    args = ap.parse_args()

    tc, vc = (int(x) for x in args.col.split(','))

    # 读 CSV：第一列时间(s)，第二列电压(V)，跳过可能的表头
    t, v = [], []
    with open(args.file, newline='', encoding='utf-8-sig') as f:
        for row in csv.reader(f):
            if len(row) <= max(tc, vc):
                continue
            try:
                t.append(float(row[tc]))
                v.append(float(row[vc]))
            except ValueError:
                continue   # 表头或异常行
    if len(t) < 4:
        print("[错误] 有效数据点过少。请确认 CSV 是两列(时间,电压)数值，或用 --col 指定对应列。")
        return

    # 找到高电平区间（v>=threshold）作为脉冲，相邻区间若靠得太近(<最小缝)合并
    pulses = []   # (start, end)
    i = 0
    n = len(v)
    in_pulse = False
    for i in range(n):
        hi = v[i] >= args.v_thresh
        if hi and not in_pulse:
            start = t[i]; in_pulse = True
        elif not hi and in_pulse:
            pulses.append((start, t[i])); in_pulse = False
    if in_pulse:
        pulses.append((start, t[-1]))
    if not pulses:
        print(f"[结果] 没抓到任何 ≥{args.v_thresh}V 的脉冲。")
        print("  检查: 探头比例选对没、垂直挡位对不对、触发电平、阈值 {args.v_thresh} 是否太高/太低。")
        return

    widths = [b - a for a, b in pulses]
    starts = [a for a, _ in pulses]
    intervals = [starts[k+1] - starts[k] for k in range(len(starts)-1)]

    def mean(xs): return sum(xs)/len(xs) if xs else 0.0
    w_mean = mean(widths)
    per = args.period if args.period else mean(intervals)
    # 电角度 = Δt / 周期 × 360°
    def deg(dt): return dt/per*360.0 if per else 0.0

    print(f"== {args.file} 波形分析 ==")
    print(f"  数据点: {len(t)}  采样范围: {t[0]:.4f}s ~ {t[-1]:.4f}s")
    print(f"  脉冲数: {len(pulses)}")
    print(f"  平均脉宽     : {w_mean*1000:.3f} ms   (~{deg(w_mean):.1f}°)" if per else
          f"  平均脉宽     : {w_mean*1000:.3f} ms")
    print(f"  平均周期/间隔: {per*1000:.3f} ms   (若为相邻叠瓦源触发即参考工频周期)")
    if len(intervals) >= 2:
        i_mean = mean(intervals)
        print(f"  相邻脉冲间隔 : {i_mean*1000:.3f} ms   (~{deg(i_mean):.1f}°)")
        print(f"  间隔极值     : {min(intervals)*1000:.3f} ~ {max(intervals)*1000:.3f} ms")

    # 判定
    print("\n== 判定 ==")
    # 参考：50Hz 下 12°=0.667ms、60°=3.333ms、20ms=周期
    def near(ms, target, tol): return abs(ms - target) <= tol
    w_ms = w_mean*1000
    per_ms = per*1000
    # 60° 间隔检查
    six = any(near(iv*1000, per_ms*60/360, 0.3) for iv in intervals if iv > 0)
    print(f"  工频周期    : {per_ms:.2f} ms  -> {'≈50Hz' if abs(per_ms-20)<1.5 else '≈60Hz' if abs(per_ms-16.667)<1.5 else '未知'}")
    print(f"  脉宽~12°    : q{'OK' if near(w_ms, per_ms*12/360, 0.4) else 'NO'}  ({w_ms:.3f}ms vs 期望{per_ms*12/360:.3f}ms)")
    print(f"  相邻组间隔60°: {six}")
    print("\n请把这份输出贴回给 AI，结合阶段预期(W8_HARDWARE_TEST §C)做最终判定。")

if __name__ == '__main__':
    main()
