#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
w8_modbus_test.py — W8 阶段 B 的 Modbus-RTU 通信及语义验证脚本。

按序执行（每条都打印 PASS/FAIL）：
   1. 读 reg 40~45（Ug给定 / IA / IB / IC / IF / Uf）——未接主回路应≈0 或面板值
   2. 写 reg40=500 再回读 —— 验证「读=实测 / 写=注入」的**读写不对称**（回读≠500 才是正常发现）
   3. 写 reg61=1 / 0 —— 远程输出使能，配套测 P0.20 / RLY3（BOM：P0.20=备用继电器，语义待实测定论）

用法：
    python tools/w8/w8_modbus_test.py --port COM5 --baud 9600 --addr 1
缺省：COM5 / 9600 / 8N1 / addr=1

依赖：minimalmodbus（`python -m pip install minimalmodbus pyserial`）
结论依据：firmware/src/08_modbus_dispatch.c 反汇编（reg40/reg61 语义）。
"""
import sys, time, argparse
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

try:
    import minimalmodbus
except ImportError:
    print("[错误] 缺 minimalmodbus 库。请运行:  python -m pip install minimalmodbus pyserial")
    sys.exit(1)

REG_MEAS = list(range(40, 46))   # reg40-45: Ug/IA/IB/IC/IF/Uf

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port',  default='COM5', help='USB-RS485 串口号')
    ap.add_argument('--baud',  type=int, default=9600, help='波特率（与面板通讯菜单一致）')
    ap.add_argument('--addr',  type=int, default=1,    help='从站地址（面板通讯菜单地址）')
    ap.add_argument('--parity',default='N',   help='N/O/E')
    args = ap.parse_args()

    try:
        inst = minimalmodbus.Instrument(args.port, args.addr)
    except Exception as e:
        print(f"[错误] 打开 {args.port} 失败: {e}")
        print("  检查: 串口号是否存在(w8_serial_detect.py)、是否被其他程序占用。")
        sys.exit(1)

    inst.serial.baudrate = args.baud
    inst.serial.bytesize = 8
    inst.serial.parity   = args.parity
    inst.serial.stopbits = 1
    inst.serial.timeout  = 1

    # 默认读寄存器的功能码 0x03；写单 0x06
    def rd(reg, dec=0):
        return inst.read_register(reg, dec, 3)
    def wr(reg, val, dec=0):
        return inst.write_register(reg, val, dec, 6)

    print(f"== 连接 {args.port} @ {args.baud} 8N1, 从站 {args.addr} ==")

    # --- 1. 读 reg 40-45 ---
    print("\n[1/4] 读 reg40-45（未接主回路应≈0 或面板值）:")
    vals = {}
    for r in REG_MEAS:
        try:
            v = rd(r)
            vals[r] = v
            print(f"    reg{r} = {v}")
        except Exception as e:
            print(f"    reg{r} 读失败: {e}")
    if vals:
        print("    [PASS] 至少读回部分寄存器。若全是 0 → 未接主回路，符合预期。")
    else:
        print("    [FAIL] 一个都读不到 → 查地址/波特率/A-B 反接/板子上电。")

    # --- 2. reg40 读写不对称 ---
    print("\n[2/4] reg40 读写不对称验证（读=实测 / 写=注入）:")
    try:
        wr(40, 500)
        time.sleep(0.3)
        back = rd(40)
        print(f"    写 reg40=500 后回读 = {back}")
        if back != 500:
            print("    [PASS] 回读≠500 → 读写不对称成立（回读的是实测值，非注入值）。这正是本项目预期。")
        else:
            print("    [NOTE] 回读=500 → 此板 reg40 或为对称语义，请记录（与反汇编不符，值得注意）。")
    except Exception as e:
        print(f"    [FAIL] reg40 测试异常: {e}")

    # --- 3. reg61 远程输出使能 ---
    print("\n[3/4] reg61 远程输出使能（配套人工测 P0.20 / RLY3）:")
    try:
        wr(61, 1)
        print("    已写 reg61=1。请用万用表测 P0.20 是否变高 / 听 RLY3 是否吸合。")
        time.sleep(1.5)
        wr(61, 0)
        print("    已写 reg61=0。请测 P0.20 是否复位。")
        print("    [NOTE] 记录 P0.20 / RLY1(RUN) / RLY2(ALM) 三者谁动 —— 这是 W8 要定论的 reg61 语义。")
    except Exception as e:
        print(f"    [FAIL] reg61 测试异常: {e}")

    # --- 4. 汇总 ---
    print("\n[4/4] 提示:")
    print("  · 若全部读不到: 先运行 tools/w8/w8_serial_detect.py 确认 COM 口，再用 --port 指定。")
    print("  · 读到的数值+你的人工测量(万用表) → 贴回给 AI 做标定换算(reg44/45 等)。")

if __name__ == '__main__':
    main()
