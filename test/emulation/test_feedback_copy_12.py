# -*- coding: utf-8 -*-
"""反馈跨槽复制契约（PC12M-2）：6 处 `fb<->aux1` 复制必须真的跨槽。

背景：PC6M-10 仓曾出现同类真实缺陷——第二路闭环三处「反馈复制」被写成自赋值
（`x = x`），使 PID 反馈由原版量值退化为旧值，而当时的输出级回归恰好没有进入
那三个分支，因此 30/30 与完整 A/B 都仍然通过。12p 的对应风险点是 6 处跨槽复制：

    path1  fb   <- aux1   原机 0xED18 / 0xED7E / 0xEDD2
    path2  aux1 <- fb     原机 0xF1B0 / 0xF216 / 0xF26A

本测试做两件事：
  1) 覆盖门禁：断言这 6 条原机指令在给定用例集合中**全部被执行**（否则测试是假通过）。
  2) 行为契约：比较原 BIN 与自编译 ELF 对 `fb(0x100015B8)`、`aux1(0x100015BC)`
     的**写入序列**（值 + 顺序）。若新固件把跨槽复制写成自赋值，而两槽当时取值不同，
     写入值就会不同 → 直接失败。

另含 6 处原机**同槽自写**（0xEC20/0xEC5C/0xECAE/0xF0B8/0xF0F4/0xF146）的说明：
它们是原厂行为，不应改成跨槽。
"""
import os
import struct
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE  # noqa: E402
import verify_firmware_equivalence_12 as V  # noqa: E402

FB = 0x100015B8
AUX1 = 0x100015BC

# 原机跨槽复制指令地址（用于覆盖门禁）
OLD_COPIES = {
    0xED18: "path1 fb<-aux1",
    0xED7E: "path1 fb<-aux1",
    0xEDD2: "path1 fb<-aux1",
    0xF1B0: "path2 aux1<-fb",
    0xF216: "path2 aux1<-fb",
    0xF26A: "path2 aux1<-fb",
}

PID_STATE = 0x10002000
GAIN_A = 0x10001630
GAIN_B = 0x10001634
GAIN_C = 0x1000163C
LIMIT = 0x10001640
STARTUP_TIMER = 0x10002030
PID_TARGET = 0x100015D0

# (gain_sel, ch4, ch5, gain_a, gain_b, gain_c, limit, timer, target, fb, aux1, locked)
# 前 3 组覆盖 path1 三处复制，后 3 组覆盖 path2 三处；fb != aux1 保证自赋值会被抓出。
CASES = (
    (0, 120, 130, 100, 50, 10, 20, 0, 50, 90, 3000, 5),
    (0, 120, 130, 100, 200, 10, 20, 0, 500, 30, 7000, 4),
    (0, 120, 0, 100, 200, 10, 20, 100, 50, 120, 9999, 5),
    (1, 120, 130, 100, 200, 10, 20, 0, 50, 110, 3050, 5),
    (1, 120, 130, 300, 50, 10, 20, 0, 500, 40, 30, 4),
    (1, 0, 130, 300, 500, 10, 20, 100, 60, 1500, 3, 5),
)


def _run(is_new, case, want_old_sites):
    gain_sel, ch4, ch5, gain_a, gain_b, gain_c, limit, timer, target, fb, aux1, locked = case
    uc = V.machine(is_new)
    V._seed_output_stage(uc, 1, gain_sel, locked, 1, 100, ch4, ch5)
    for addr, val in ((PID_STATE, 1), (GAIN_A, gain_a), (GAIN_B, gain_b),
                      (GAIN_C, gain_c), (LIMIT, limit), (STARTUP_TIMER, timer),
                      (PID_TARGET, target), (FB, fb), (AUX1, aux1)):
        uc.mem_write(addr, struct.pack("<I", val))
    writes = []
    sites = []

    def mem_hook(m, access, address, size, value, user):
        writes.append((address, value))

    def code_hook(m, address, size, user):
        if address in want_old_sites:
            sites.append(address)

    uc.hook_add(UC_HOOK_MEM_WRITE, mem_hook, begin=FB, end=AUX1 + 3)
    if not is_new:
        for address in want_old_sites:
            uc.hook_add(UC_HOOK_CODE, code_hook, begin=address, end=address + 1)
    entry = V.SYMS["output_stage"] if is_new else V.PAIRS["output_stage"][0]
    V.run(uc, entry, max_insn=2_000_000)
    return writes, sites


def main():
    covered = set()
    failures = 0
    for index, case in enumerate(CASES):
        old_writes, old_sites = _run(False, case, OLD_COPIES)
        new_writes, _ = _run(True, case, OLD_COPIES)
        covered |= set(old_sites)
        if old_writes != new_writes:
            failures += 1
            print("  [FAIL] case#%d 反馈槽写入序列不同" % index)
            print("     case =", case)
            print("     OLD  =", [("0x%X" % a, v) for a, v in old_writes])
            print("     NEW  =", [("0x%X" % a, v) for a, v in new_writes])
            continue
        # 只统计真正发生复制的用例（排除本用例未触发的站点）
        hit = set(old_sites) & set(OLD_COPIES)
        if hit:
            print("  case#%d 命中复制点 %s，写入序列一致（%d 次写）"
                  % (index, sorted("0x%X" % a for a in hit), len(old_writes)))

    missing = set(OLD_COPIES) - covered
    if missing:
        failures += 1
        print("  [FAIL] 6 处跨槽复制未被任何用例覆盖: %s"
              % sorted("0x%X" % a for a in missing))
    if failures:
        print("  [FAIL] 反馈跨槽复制契约 %d 项失败" % failures)
        return 1
    print("  [PASS] 反馈跨槽复制契约：6/6 处已覆盖，原 BIN 与自编译 ELF 写入序列一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
