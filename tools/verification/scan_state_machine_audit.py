"""状态机 A/B 深度审计（不进 verify 主套件，临时审计用）。

背景：verify_firmware_equivalence.STATE_MACHINE 只比 SRAM 末态，有两类盲区：
  ① GPIO/继电器叶函数**调用参数**（问题2型：主屏刷新开环分支 pin20_ctrl(1)→(0) 即此类）；
  ② 需长帧数才显现的状态（问题1型：TIMEOUT3>0x1F4 回绕+擦除、软起 b4c 自动置 1）。

本审计：
  1) 复用 STATE_MACHINE 全部场景，新增对比两固件 7 个 GPIO/继电器叶函数的调用序列
     (函数名, r0 参数) —— 命中问题2型；
  2) 加长跑场景（case3 编辑态 / 主界面空转），对比 SRAM 末态 —— 命中问题1型。
"""
import sys
import struct

sys.path.insert(0, 'tools/verification')
import verify_firmware_equivalence as V
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0

# 叶函数旧地址（原 BIN）—— 出处见各模块函数头注释
OLD_FUNCS = {
    'p020': 0x10588,   # out_relay_p020   P0.20 RLY3
    'p021': 0x105A8,   # out_relay_p021   P0.21 RLY2
    'p120': 0x105C8,   # fio1_pin20_ctrl  P1.20 恒压 LED
    'p121': 0x105E8,   # fio1_pin21_ctrl  P1.21 恒流 LED
    'p123': 0x10608,   # fio1_pin23_ctrl  P1.23 故障 LED
    'p122': 0xE946,    # fio1_pin22_ctrl  P1.22 运行 LED
    'p022': 0xE966,    # fio0_pin22_ctrl  P0.22 运行继电器 RLY1
}

# 新固件：取主符号 + 全部 .part.* 内联副本地址（编译器可能内联部分调用点）
NEW_FUNCS = {
    'p020': 'out_relay_p020',
    'p021': 'out_relay_p021',
    'p120': 'fio1_pin20_ctrl',
    'p121': 'fio1_pin21_ctrl',
    'p123': 'fio1_pin23_ctrl',
    'p122': 'fio1_pin22_ctrl',
    'p022': 'fio0_pin22_ctrl',
}


def targets(old):
    if old:
        return {n: {a} for n, a in OLD_FUNCS.items()}
    out = {}
    for name, sym in NEW_FUNCS.items():
        out[name] = {a for k, a in V.SYMS.items()
                     if k == sym or k.startswith(sym + '.')}
    return out


# new 固件 LCD 驱动带 Delay(1)（每写一字节前等待），渲染一次 case3 全页需约 386 万条指令
# （其中 ~94% 是 Delay 循环体）；old 固件 LCD 驱动无 Delay，2M 预算即够。
# 若 case3 场景沿用 2M，new 固件渲染到第 3 行就被截断，第 4 行标题 + item0 的
# fio1_pin20/21_ctrl 开环 LED 从未执行 → 5 处假 GPIO-SEQ DIFF。
# 同理 menu==1 主界面 key∈{0x17,1,0xe,4} 会调用 disp_clear()（12288 次 disp_data ×
# 2 Delay ≈ 737 万条指令），2M 下 new 固件连清屏都跑不完，key=1 分支的
# *TIMEOUT2=0x3c 设不上 → (1,0,0,1) SRAM DIFF。故这些场景都需足量预算。
CASE3_MAX_INSN = 10_000_000


def run_case(old, entry, menu, menu2, menu3, key, max_insn=None):
    if max_insn is None:
        # 会整页渲染/清屏的场景开销大，需足量预算；其余场景 2M 够
        max_insn = (CASE3_MAX_INSN if menu in (1, 3) else 2_000_000)
    uc = V.machine(old)
    uc.mem_write(0x10001744, bytes((menu, menu2, menu3)))
    uc.mem_write(0x10001698, struct.pack("<IIIII", 4000, 4001, 4002, 4003, 4004))
    uc.mem_write(0x10001660, struct.pack("<I", 90))
    uc.mem_write(0x10001710, bytes((2, 10, 10, 0, 0, 0, 2, 2, 2)))
    # 注意：V.machine(old) 参数 new=True=新固件；targets(old) 参数 old=True=OLD 地址。
    # 二者语义相反，这里必须用 not old，否则 old 固件配 NEW 地址、new 固件配 OLD 地址，
    # 会在错误固件的普通指令流上假命中叶函数地址（曾造成 menu=0x1E 假差异）。
    tgt = targets(not old)
    calls = []

    def hk(machine, address, size, user):
        r0 = machine.reg_read(UC_ARM_REG_R0)
        for name, addrs in tgt.items():
            if address in addrs:
                calls.append((name, r0))
                break
    uc.hook_add(UC_HOOK_CODE, hk)
    uc.reg_write(UC_ARM_REG_R0, key)
    V.run(uc, entry, max_insn=max_insn)
    sram = bytes(uc.mem_read(0x10000000, 0x2200))
    return calls, sram


def audit():
    cases = []
    for key in (0, 1, 2, 3, 4, 5, 6, 0x16, 0x17, 0x21):
        cases.append((1, 0, 0, key))
    for menu in (2, 3, 4, 5, 7, 0x14, 0x1E):
        for menu2 in (0, 1, 3):
            for menu3, key in ((0, 0), (0, 2), (0, 3), (1, 2), (1, 3)):
                cases.append((menu, menu2, menu3, key))

    bad = 0
    for menu, menu2, menu3, key in cases:
        r0 = run_case(False, 0x458C, menu, menu2, menu3, key)
        r1 = run_case(True, V.SYMS['state_machine'], menu, menu2, menu3, key)
        label = (menu, menu2, menu3, key)
        if r0[0] != r1[0]:
            bad += 1
            print(f"GPIO-SEQ DIFF {label}: old={len(r0[0])} new={len(r1[0])}")
            for i, (a, b) in enumerate(zip(r0[0], r1[0])):
                if a != b:
                    print(f"  call[{i}] old={a} new={b}")
            if len(r0[0]) != len(r1[0]):
                tail = min(len(r0[0]), len(r1[0]))
                print(f"  old extra: {r0[0][tail:tail+8]}")
                print(f"  new extra: {r1[0][tail:tail+8]}")
        elif r0[1] != r1[1]:
            diffs = [(0x10000000 + i, a, b) for i, (a, b) in
                     enumerate(zip(r0[1], r1[1])) if a != b]
            bad += 1
            print(f"SRAM DIFF {label}: {len(diffs)} bytes, first={diffs[:4]}")
    print(f"[audit 1] 全场景 GPIO 序列 + SRAM：{len(cases)} 场景，差异 {bad}")

    # 长跑：case3 编辑态空转（TIMEOUT3 累计过 0x1F4 触发回绕+擦除，软起 b4c 置 1）
    for label, menu, menu2, menu3, key, maxi in (
            ("case3编辑态", 3, 0, 1, 0, 8_000_000),
            ("case3查看态", 3, 0, 0, 0, 8_000_000),
            ("主界面空转", 1, 0, 0, 0, 8_000_000)):
        r0 = run_case(False, 0x458C, menu, menu2, menu3, key, maxi)
        r1 = run_case(True, V.SYMS['state_machine'], menu, menu2, menu3, key, maxi)
        print(f"[audit 2] {label}:")
        print(f"  旧: calls={len(r0[0])} 新: calls={len(r1[0])}")
        if r0[0] != r1[0]:
            print(f"  !! GPIO-SEQ DIFF: old={r0[0][-12:]} new={r1[0][-12:]}")
        if r0[1] != r1[1]:
            diffs = [(hex(0x10000000 + i), a, b) for i, (a, b) in
                     enumerate(zip(r0[1], r1[1])) if a != b]
            print(f"  !! SRAM DIFF {len(diffs)} bytes, first={diffs[:6]}")
        else:
            print(f"  SRAM 末态一致")


if __name__ == "__main__":
    audit()
