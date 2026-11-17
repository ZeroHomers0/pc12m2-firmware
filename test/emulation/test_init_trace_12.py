# -*- coding: utf-8 -*-
"""初始化与底层 I/O 函数的外设等价性（PC12M-2）。

背景：覆盖率盘点显示 12p 有 29 个函数此前**没有任何执行级 A/B**——它们是启动链
上的初始化与位操作函数：

    SystemInit / timer0_init / timer1_init / timer2_init
    eint1_init / eint2_init / eint3_init / nvic_enable_irq
    gpio1_init / gpio_inputs_dir_init / i2c_gpio_init
    adc_init / uart3_init / lcd_data_byte
    i2c_start / i2c_stop / i2c_write_byte / i2c_read_byte / i2c_delay_short
    chk_p02_p03

这些函数共同特征是「只写外设寄存器与 GPIO，不读参数、不依赖 SRAM 内容」，因此
对照「外设写迹（地址/宽度/值）+ 10 个外设页末态」即可判定等价，无需构造业务状态。

原机地址 → 新 ELF 符号的对应关系不是猜的：其中 12 个函数与 6p 原 BIN 的同名函数
**函数体与紧随的字面量池逐字节相同**（SystemInit 全 262 字节零差异），据此确定
归属后，再按 12p 自己的地址填表。

## 三个必须绕开的仿真陷阱

1. **FIO 页不能挂 MEM_WRITE 钩子**：对 0x2009C000 挂写钩子后，Unicorn 的 TCG
   缓存会让新 ELF 的部分函数体一条都不执行（12p 状态机矩阵上已踩过同一坑），
   表现为「新固件少写了一次 GPIO」。FIO 页因此只比末态，不比写迹。
2. **i2c_delay 入口是 0x1C32，不是 0x1C46**：0x1C46 是它体内的内层循环地址，
   把 PC 设到那里会跳过压栈、破坏栈平衡，随后 fetch 到未映射区。
3. **SystemInit 会死等晶振/PLL 锁定位**：Unicorn 里这些状态位恒为 0。同时
   Unicorn 的 MEM_WRITE 钩子在**写入生效前**触发，在钩子里置位会被紧随的 store
   覆盖，必须记账到下一指令的 CODE 钩子再补。两侧用同一个模型，A/B 才公平。

## 检出能力已验证

把 `firmware/src/01_startup.c` 的 `TIMER0->MR0 = 1999` 改成 1998 重建后，
本测试报 `[DIFF] timer0_init() 写序列(8/8), 页0x40004000(1字节) +0x018
OLD=0xCF NEW=0xCE`；还原后 `bad=0/26`。
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
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0  # noqa: E402
import verify_firmware_equivalence_12 as V  # noqa: E402

# 辅助函数：原机入口 -> 新 ELF 符号名。执行时直接返回到调用点（PC <- LR），
# 否则延时与整屏刷新会跑掉几千万条指令。
SKIP_OLD = {0x758: "Delay", 0x8FA: "disp_cmd", 0x942: "disp_clear",
            0x1C1C: "i2c_delay_short", 0x1C32: "i2c_delay"}
SKIP_NEW_NAMES = tuple(SKIP_OLD.values())

# 新 ELF 用全局指针槽间接取 FIO 基址（原机用立即数池直取），需预置成真实值
FIO_PTR_SLOT = 0x2007D810
FIO_BASE = 0x2009C000

# (原机入口, 新 ELF 符号, 名称, 入参)
TARGETS = (
    (0x0440, "SystemInit", "SystemInit", ()),
    (0x0248, "timer0_init", "timer0_init", ()),
    (0xE576, "timer1_init", "timer1_init", ()),
    (0xE598, "timer2_init", "timer2_init", ()),
    (0xE5CE, "eint1_init", "eint1_init", ()),
    (0xE62A, "eint2_init", "eint2_init", ()),
    (0xE668, "eint3_init", "eint3_init", ()),
    (0x10860, "nvic_enable_irq", "nvic_enable_irq(0)", (0,)),
    (0x10860, "nvic_enable_irq", "nvic_enable_irq(20)", (20,)),
    (0x10860, "nvic_enable_irq", "nvic_enable_irq(37)", (37,)),
    (0x09E0, "gpio1_init", "gpio1_init", ()),
    (0x1528, "gpio_inputs_dir_init", "gpio_inputs_dir_init", ()),
    (0x1BF0, "i2c_gpio_init", "i2c_gpio_init", ()),
    (0x1EB4, "adc_init", "adc_init", ()),
    (0xA994, "uart3_init", "uart3_init", ()),
    (0x1B9E, "chk_p02_p03", "chk_p02_p03", ()),
    (0x0786, "lcd_data_byte", "lcd_data_byte(0x55)", (0x55,)),
    (0x0786, "lcd_data_byte", "lcd_data_byte(0x00)", (0x00,)),
    (0x0786, "lcd_data_byte", "lcd_data_byte(0xFF)", (0xFF,)),
    (0x1C5E, "i2c_start", "i2c_start", ()),
    (0x1CAE, "i2c_stop", "i2c_stop", ()),
    (0x1CEC, "i2c_write_byte", "i2c_write_byte(0x00)", (0x00,)),
    (0x1CEC, "i2c_write_byte", "i2c_write_byte(0xA5)", (0xA5,)),
    (0x1CEC, "i2c_write_byte", "i2c_write_byte(0xFF)", (0xFF,)),
    (0x1DAA, "i2c_read_byte", "i2c_read_byte(ack=0)", (0,)),
    (0x1DAA, "i2c_read_byte", "i2c_read_byte(ack=1)", (1,)),
)

# 末态对照页：(基址, 长度)
PAGES = (
    (0x40000000, 0x1000),   # WDT / UART0
    (0x40004000, 0x1000),   # TIMER0
    (0x40008000, 0x1000),   # UART3
    (0x4002C000, 0x1000),   # PINSEL4（EINT）
    (0x40034000, 0x1000),   # ADC0
    (0x40090000, 0x1000),   # TIMER1
    (0x4009C000, 0x1000),   # TIMER2
    (0x400FC000, 0x1000),   # PCONP / PCLKSEL / CLKOUTCFG / SCS
    (0xE000E000, 0x1000),   # NVIC
    (0x2009C000, 0x1000),   # FIO0/1/2/3
)

SCS = 0x400FC1A0
PLL0FEED, PLL0STAT = 0x400FC08C, 0x400FC088
PLL1FEED, PLL1STAT = 0x400FC0AC, 0x400FC0A8


def _skip_set(is_new):
    if is_new:
        return {V.SYMS[n] for n in SKIP_NEW_NAMES if n in V.SYMS}
    return set(SKIP_OLD)


def _install_hw_model(uc):
    """晶振/PLL 锁定模型：写 FEED 后于下一指令置就绪位。

    Unicorn 的 MEM_WRITE 钩子早于写入生效，所以在钩子里直接改内存会被紧随其后的
    store 覆盖（实测 SCS 只剩 0x20）。这里先记账，再在 CODE 钩子里补位。
    """
    pending = []

    def mem_hook(m, access, address, size, value, user):
        if address == SCS and size == 4:
            pending.append((SCS, 0x40))              # OSCEN -> OSCSTAT
        elif address in (PLL0FEED, PLL1FEED):
            pending.append((PLL0STAT, 0x07000000))   # PLOCK0 / PLLE0 / PLLC0
            pending.append((PLL1STAT, 0x00000700))   # PLLE1 / PLLC1 / PLOCK1

    def code_hook(m, address, size, user):
        while pending:
            addr, bits = pending.pop()
            cur = struct.unpack("<I", m.mem_read(addr, 4))[0]
            m.mem_write(addr, struct.pack("<I", cur | bits))

    uc.hook_add(UC_HOOK_MEM_WRITE, mem_hook)
    uc.hook_add(UC_HOOK_CODE, code_hook)


def _observe(is_new, entry, args, hw_model, max_insn=3_000_000):
    uc = V.machine(is_new)
    for base, size in PAGES:
        try:
            uc.mem_map(base, size)
        except Exception:
            pass
    if args:
        uc.reg_write(UC_ARM_REG_R0, args[0])
    uc.mem_write(FIO_PTR_SLOT, struct.pack("<I", FIO_BASE))

    skip = _skip_set(is_new)
    pokes = []

    def code_hook(m, address, size, user):
        if address in skip:
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    def mem_hook(m, access, address, size, value, user):
        pokes.append((address, size, value & ((1 << (8 * size)) - 1)))

    # FIO 页不挂写钩子：会触发 Unicorn TCG 缓存问题，改为只比末态
    for base, size in PAGES:
        if base == FIO_BASE:
            continue
        uc.hook_add(UC_HOOK_MEM_WRITE, mem_hook, begin=base, end=base + size - 1)
    uc.hook_add(UC_HOOK_CODE, code_hook)
    if hw_model:
        _install_hw_model(uc)

    try:
        V.run(uc, entry, max_insn=max_insn)
        finished = True
    except AssertionError:
        finished = False
    final = [bytes(uc.mem_read(base, size)) for base, size in PAGES]
    return finished, pokes, final


def _describe(page_bytes, other_bytes, limit=4):
    idx = [i for i in range(len(page_bytes)) if page_bytes[i] != other_bytes[i]]
    return ", ".join("+0x%03X OLD=0x%02X NEW=0x%02X"
                     % (i, page_bytes[i], other_bytes[i]) for i in idx[:limit])


# 这两个函数读写 SRAM（不只看外设），因此单独用「预置 + SRAM 末态」对照。
# 它们的原机入口不在 verify_firmware_equivalence_12.PAIRS 里，故此前从未 A/B。
#
# freq_adjust_sync 的四个动作分支必须都覆盖到，否则「上限/下限钳位被改掉」这类
# 缺陷抓不到（实测：只跑 event=0/1 时，把 0x2b0 上限改成 0x2af 仍然全 PASS）：
#   event=2/0x16 递增 + 上限钳位 0x2b0     event=3/0x21 下限钳位 0x45 后递减
#   event=5 置运行标志 + 画「运行」        event=6 清运行标志 + 画「停止」
# 槽位：out_freq_adj=0x10001624、shadow=0x10001628、g_cfg_word=0x10001620。
SRAM_CASES = (
    # (原机入口, 新 ELF 符号, 名称, r0, 预置 {地址: 值})
    (0x1E6C, "i2c_read_reg", "i2c_read_reg(buf,0xc9)", 0x10003000,
     {0x10003000: 0x00, 0x10003004: 0x00}),
    (0x1E6C, "i2c_read_reg", "i2c_read_reg(buf,0x97)", 0x10003000,
     {0x10003000: 0x00, 0x10003004: 0x00}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=0)", 0,
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=2) 未达上限", 2,
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=2) 触上限", 2,
     {0x10001624: 0x02B0, 0x10001628: 0x02B0, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=0x16) 触上限", 0x16,
     {0x10001624: 0x02B5, 0x10001628: 0x02B5, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=3) 未达下限", 3,
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=3) 触下限", 3,
     {0x10001624: 0x0045, 0x10001628: 0x0045, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=0x21) 触下限", 0x21,
     {0x10001624: 0x0040, 0x10001628: 0x0040, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=5) 置运行", 5,
     {0x10001624: 0x0200, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xA856, "freq_adjust_sync", "freq_adj(event=6) 置停止", 6,
     {0x10001624: 0x0200, 0x10001628: 0x0200, 0x10001620: 1}),
)


def _observe_sram(is_new, entry, arg, presets, max_insn=3_000_000):
    uc = V.machine(is_new)
    for base, size in PAGES:
        try:
            uc.mem_map(base, size)
        except Exception:
            pass
    uc.mem_write(FIO_PTR_SLOT, struct.pack("<I", FIO_BASE))
    for addr, val in presets.items():
        uc.mem_write(addr, struct.pack("<I", val))
    uc.reg_write(UC_ARM_REG_R0, arg)
    skip = _skip_set(is_new)

    def code_hook(m, address, size, user):
        if address in skip:
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, code_hook)
    try:
        V.run(uc, entry, max_insn=max_insn)
        finished = True
    except AssertionError:
        finished = False
    sram = bytes(uc.mem_read(0x10000000, V.SRAM0_END))
    final = [bytes(uc.mem_read(base, size)) for base, size in PAGES]
    return finished, sram, final


def _run_sram_cases():
    bad = 0
    for addr, sym, label, arg, presets in SRAM_CASES:
        old_ok, old_s, old_f = _observe_sram(False, addr, arg, presets)
        new_ok, new_s, new_f = _observe_sram(True, V.SYMS[sym], arg, presets)
        if not (old_ok and new_ok):
            bad += 1
            print("  [FAIL] %-28s 未执行到返回 old=%s new=%s" % (label, old_ok, new_ok))
            continue
        problems = []
        if old_s != new_s:
            idx = [i for i in range(len(old_s)) if old_s[i] != new_s[i]]
            problems.append("SRAM %d 字节不同，首个 +0x%03X OLD=0x%02X NEW=0x%02X"
                            % (len(idx), idx[0], old_s[idx[0]], new_s[idx[0]]))
        for (base, _), a, b in zip(PAGES, old_f, new_f):
            if a != b:
                problems.append("页0x%08X: %s" % (base, _describe(a, b)))
        if problems:
            bad += 1
            print("  [FAIL] %-28s %s" % (label, "; ".join(problems)))
            continue
        print("  [PASS] %-28s SRAM + %d 页末态一致" % (label, len(PAGES)))
    return bad


def main():
    bad = 0
    covered_old = set()
    for addr, sym, label, args in TARGETS:
        need_hw = (sym == "SystemInit")
        old_ok, old_p, old_f = _observe(False, addr, args, need_hw)
        new_ok, new_p, new_f = _observe(True, V.SYMS[sym], args, need_hw)
        covered_old.add(addr)

        if not (old_ok and new_ok):
            bad += 1
            print("  [FAIL] %-28s 未执行到返回 old=%s new=%s" % (label, old_ok, new_ok))
            continue

        problems = []
        if old_p != new_p:
            problems.append("写序列 %d/%d 项不同" % (len(old_p), len(new_p)))
        for (base, _), a, b in zip(PAGES, old_f, new_f):
            if a != b:
                problems.append("页0x%08X: %s" % (base, _describe(a, b)))
        if problems:
            bad += 1
            print("  [FAIL] %-28s %s" % (label, "; ".join(problems)))
            continue

        print("  [PASS] %-28s 写迹 %2d 项 + %d 页末态一致"
              % (label, len(old_p), len(PAGES)))

    print("  --- 读写 SRAM 的函数（不在官方 PAIRS 里，此前从未 A/B）")
    bad += _run_sram_cases()

    print("  ---")
    uniq = {addr for addr, _, _, _ in TARGETS} | {c[0] for c in SRAM_CASES}
    total = len(TARGETS) + len(SRAM_CASES)
    print("  覆盖门禁：%d 个唯一原机入口 / %d 组用例，全部执行到返回"
          % (len(uniq), total))
    if bad:
        print("  [FAIL] 初始化/底层 I/O 外设等价性 %d 项失败" % bad)
        return 1
    print("  [PASS] 初始化/底层 I/O 外设等价性：%d 组全部一致" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
