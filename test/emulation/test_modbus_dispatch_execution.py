# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_modbus_dispatch.py — 真实执行 modbus_dispatch，验证 Modbus 帧处理
#
# 用 Unicorn 执行编译的 modbus_dispatch。把标准 Modbus RTU 请求帧放进
# FRAME(0x100022A4)、设 RX_LEN/RX_STATE/SLAVE_ADDR，验证：
#   * 合法帧（CRC 正确，且 frame[2]==0x10 的 0x1001..0x103F 寄存器布局）
#     → 走 0x03 读分支 → uart3_tx_byte(5+cnt*2=7) 发 7 字节响应，
#       TXBUF=[addr,0x03,字节数,数据,CRC]
#   * CRC 错帧 → 异常分支 uart3_tx_byte(5) 发异常响应（功能码置 0x80）
# 为规避 UART 外设硬时序，hook uart3_tx_byte：它参数 r0 = "发 n 字节"的计数，
# 实际字节在 TXBUF。hook 跳过函数体（PC=LR），只记录 r0 即得"本次要发的字节数"。
#
# ★ A/B 差分：同一请求帧分别跑【原始 LPC1765.bin】与【编译 firmware.elf】的
#   modbus_dispatch，断言 TXBUF 逐字节一致。2026-08-29 修复记：本固件 16 位
#   字段一律大端（frame[4]<<8|frame[5]），旧测试把计数当小端写死（cnt_lo/cnt_hi
#   颠倒）恰与反编译 bug 一致而同绿，漏过帧解析字节序回归（bug #3）。
#
# 若 unicorn 不可用 → SKIP。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'test', 'support'))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
from unicorn import *
from unicorn.arm_const import *
from unicorn_harness import load_firmware, load_original, lookup

FRAME, RX_LEN, RX_STATE, SLAVE_ADDR, TXBUF = 0x100022A4, 0x10001792, 0x10001790, 0x100016FF, 0x1000236C

# ── 固件 crc16 模型（处理全部 len 字节 = 标准 Modbus CRC），仅用于断言自洽 ──
def _load_tables():
    b = open(os.path.join(ROOT, 'LPC1765.bin'), 'rb').read()
    return b[0x11034:0x11034+256], b[0x11134:0x11134+256]

def crc16_fw(data, length):
    hi, lo = _load_tables()
    ch = 0xff; cl = 0xff
    for i in range(length):
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]
    return (cl | (ch << 8)) & 0xffff

def map_periph(e):
    for base, ln in [(0x2009C000, 0x4000), (0x40090000, 0x4000), (0x4009C000, 0x1000),
                     (0x4002C000, 0x2000), (0x400FC000, 0x1000), (0xE000E100, 0x2000)]:
        try:
            e.mem_map(base, ln, UC_PROT_ALL)
        except UcError:
            pass

# ── 在指定固件上跑一帧（frame 含 CRC），返回 (tx_count, TXBUF) ──
def run_frame(load_fn, dispatch_addr, tx_addr, frame, slave=0x01, gain_sel=0x56):
    e = load_fn()
    map_periph(e)
    tx_count = []
    def hook_tx(uc, addr, size, user):
        tx_count.append(uc.reg_read(UC_ARM_REG_R0) & 0xff)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
    e.hook_add(UC_HOOK_CODE, hook_tx, begin=tx_addr, end=tx_addr + 1)
    for i, b in enumerate(frame):
        e.mem_write(FRAME + i, bytes([b]))
    e.mem_write(RX_LEN, bytes([len(frame)]))
    e.mem_write(RX_STATE, bytes([5]))
    e.mem_write(SLAVE_ADDR, bytes([slave]))
    e.mem_write(0x10001634, bytes([gain_sel]))
    e.reg_write(UC_ARM_REG_LR, 0xFF000000)
    e.emu_start(dispatch_addr | 1, 0xFF000000)
    n = tx_count[0] if tx_count else -1
    txb = bytes(e.mem_read(TXBUF, max(n, 0)))
    return n, txb

def run_outcome(load_fn, dispatch_addr, tx_addr, frame, slave=0x01, gain_sel=0x56):
    """异常/短帧可能按原厂行为越界终止；统一返回可比较结果。"""
    try:
        return ("OK",) + run_frame(load_fn, dispatch_addr, tx_addr, frame, slave, gain_sel)
    except UcError as ex:
        return ("UC_ERR", ex.errno)

# 原始固件固定地址（编译固件从 map 解析）
ORIG_DISPATCH = 0xB642
ORIG_TX      = 0xAE0C

def main():
    try:
        import unicorn  # noqa
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    FUNC_dispatch = lookup('modbus_dispatch')
    FUNC_uart3_tx = lookup('uart3_tx_byte')

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    # ── 场景1：合法读请求 站1 读寄存器 0x1001(#1) 数量1 ──
    #   16 位字段大端：cnt = frame[4]<<8 | frame[5]；请求体 [01 03 10 01 00 01] → cnt=1。
    #   该帧即实机通讯测试帧（用户 2026-08-28 报告，CRC=0x0AD1）。gain_sel 预置 0x56。
    req = bytes([0x01, 0x03, 0x10, 0x01, 0x00, 0x01])
    req_crc = crc16_fw(req, 6)
    check("读请求帧 CRC 自洽（0x0AD1）", req_crc == 0x0AD1, f"0x{req_crc:04X}")
    frame_ok = req + bytes([req_crc & 0xff, req_crc >> 8])

    G = 0x56
    n_o, tx_o = run_frame(load_original, ORIG_DISPATCH, ORIG_TX, frame_ok, gain_sel=G)
    n_n, tx_n = run_frame(load_firmware, FUNC_dispatch, FUNC_uart3_tx, frame_ok, gain_sel=G)

    # 寄存器值 0x0056 大端 → 数据 [0x00,0x56]；响应 [01 03 02 00 56 crc]
    exp = bytes([0x01, 0x03, 0x02, 0x00, G])
    rsp_crc = crc16_fw(exp, 5)
    exp_full = exp + bytes([rsp_crc & 0xff, rsp_crc >> 8])
    check("原固件读分支发 7 字节", n_o == 7, f"tx_count={[hex(x) for x in [n_o]]}")
    check("新固件读分支发 7 字节", n_n == 7, f"tx_count={[hex(x) for x in [n_n]]}")
    check("原固件响应 = [01 03 02 0056 crc]", tx_o == exp_full, f"0x{tx_o.hex().upper()}")
    check("新固件响应 = [01 03 02 0056 crc]", tx_n == exp_full, f"0x{tx_n.hex().upper()}")
    check("A/B 差分：原/新 TXBUF 逐字节一致", tx_o == tx_n,
          f"原 0x{tx_o.hex().upper()} vs 新 0x{tx_n.hex().upper()}")

    # ── 场景2：合法帧体但 CRC 最后一字节错 → 应走 CRC 异常分支 ──
    frame_bad = req + bytes([(req_crc & 0xff) ^ 0xFF, req_crc >> 8])   # 破坏 CRC 低字节
    n_o, tx_o = run_frame(load_original, ORIG_DISPATCH, ORIG_TX, frame_bad, gain_sel=G)
    n_n, tx_n = run_frame(load_firmware, FUNC_dispatch, FUNC_uart3_tx, frame_bad, gain_sel=G)
    check("原固件 CRC 错帧 → 异常 5B [01 83 04]", n_o == 5 and tx_o[:3] == bytes([1, 0x83, 4]),
          f"0x{tx_o.hex().upper()}")
    check("新固件 CRC 错帧 → 异常 5B [01 83 04]", n_n == 5 and tx_n[:3] == bytes([1, 0x83, 4]),
          f"0x{tx_n.hex().upper()}")

    # ── 场景3：站址不匹配 → 兜底，不发（空 tx_count）──
    c_o = run_frame(load_original, ORIG_DISPATCH, ORIG_TX, frame_ok, slave=0x02, gain_sel=G)[0]
    c_n = run_frame(load_firmware, FUNC_dispatch, FUNC_uart3_tx, frame_ok, slave=0x02, gain_sel=G)[0]
    check("原固件站址不匹配 → 不发送", c_o == -1, f"tx_count={hex(c_o)}")
    check("新固件站址不匹配 → 不发送", c_n == -1, f"tx_count={hex(c_n)}")

    # ── 场景4：0x06 单写完整帧（reg 0x1001 / gain_sel=2）──
    req06 = bytes([0x01, 0x06, 0x10, 0x01, 0x00, 0x02])
    crc06 = crc16_fw(req06, len(req06))
    frame06 = req06 + bytes([crc06 & 0xff, crc06 >> 8])
    n_o, tx_o = run_frame(load_original, ORIG_DISPATCH, ORIG_TX, frame06, gain_sel=G)
    n_n, tx_n = run_frame(load_firmware, FUNC_dispatch, FUNC_uart3_tx, frame06, gain_sel=G)
    check("0x06 原固件发送 8 字节回显", n_o == 8, f"0x{tx_o.hex().upper()}")
    check("0x06 新固件发送 8 字节回显", n_n == 8, f"0x{tx_n.hex().upper()}")
    check("0x06 完整帧 A/B 响应一致", tx_o == tx_n,
          f"原 0x{tx_o.hex().upper()} vs 新 0x{tx_n.hex().upper()}")

    # ── 场景5：0x10 多写完整帧（从 reg 0x1001 起写 1 个寄存器）──
    req10 = bytes([0x01, 0x10, 0x10, 0x01, 0x00, 0x01, 0x02, 0x00, 0x35])
    crc10 = crc16_fw(req10, len(req10))
    frame10 = req10 + bytes([crc10 & 0xff, crc10 >> 8])
    n_o, tx_o = run_frame(load_original, ORIG_DISPATCH, ORIG_TX, frame10, gain_sel=G)
    n_n, tx_n = run_frame(load_firmware, FUNC_dispatch, FUNC_uart3_tx, frame10, gain_sel=G)
    check("0x10 原固件发送 8 字节确认", n_o == 8, f"0x{tx_o.hex().upper()}")
    check("0x10 新固件发送 8 字节确认", n_n == 8, f"0x{tx_n.hex().upper()}")
    check("0x10 完整帧 A/B 响应一致", tx_o == tx_n,
          f"原 0x{tx_o.hex().upper()} vs 新 0x{tx_n.hex().upper()}")

    # ── 场景6：异常结构/边界/短帧，仅以原BIN行为为期望 ──
    def framed(body):
        c = crc16_fw(body, len(body))
        return body + bytes([c & 0xff, c >> 8])

    abnormal = [
        ("0x06 值越界", framed(bytes([1, 6, 0x10, 1, 0, 3]))),
        ("0x06 不存在寄存器", framed(bytes([1, 6, 0x10, 0x3F, 0, 1]))),
        ("0x10 数量为0", framed(bytes([1, 0x10, 0x10, 1, 0, 0, 0]))),
        ("0x10 数量超限", framed(bytes([1, 0x10, 0x10, 1, 0, 0x3F, 0x7E]))),
        ("0x10 字节数不匹配", framed(bytes([1, 0x10, 0x10, 1, 0, 2, 2, 0, 1]))),
        ("不支持功能码", framed(bytes([1, 4, 0x10, 1, 0, 1]))),
        ("1字节短帧", bytes([1])),
        ("2字节短帧", bytes([1, 3])),
    ]
    for name, bad_frame in abnormal:
        out_o = run_outcome(load_original, ORIG_DISPATCH, ORIG_TX, bad_frame, gain_sel=G)
        out_n = run_outcome(load_firmware, FUNC_dispatch, FUNC_uart3_tx, bad_frame, gain_sel=G)
        check(f"{name} A/B结果一致", out_o == out_n, f"原={out_o} 新={out_n}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
