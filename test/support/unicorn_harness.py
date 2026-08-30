# -*- coding: utf-8 -*-
# =============================================================================
# unicorn_harness.py — 用 Unicorn 加载 firmware.elf，可调真实编译固件函数
#
# 背景：无宿主编译器/qemu，只有 cross 的 arm-none-eabi-gcc（产出 ARM Thumb ELF）。
#   本机装 Unicorn 2.1.4 后，可在 CPU 仿真器里真正执行编译产物的 Thumb 代码，
#   且 0x1000xxxx 映射为真实可读写内存 → 天然解决反编译 g_/DAT_ 指针的 SRAM 重定向。
#
# 用途：构造虚拟数据 → 在仿真 SRAM 里设初始值 → 按 AAPCS 调函数地址 → 回读副作用。
# 这是"用构造虚拟数据测真实固件"的最强路径。
#
# 用法：
#   from unicorn_harness import load_firmware, run_function
#   h = load_firmware()
#   run_function(h, FUNC_crc16, [ptr, len])   # 按 ARGS_SPEC 传参/回读
# =============================================================================
import os, struct
from unicorn import *
from unicorn.arm_const import *

HERE   = os.path.dirname(os.path.abspath(__file__))
ROOT   = os.path.dirname(os.path.dirname(HERE))
FW     = os.path.join(ROOT, 'firmware', 'firmware.elf')

# 段布局（来自 ELF 解析，2026-08-23）
FLASH_BASE  = 0x0
FLASH_MAP   = 0x40000          # 0..0x3FFFF（.isr_vector + .text + .rodata 表）
SRAM0_BASE  = 0x10000000
SRAM0_LEN   = 0x8000           # 32K（.fw_image + .fw_bss + 栈）
SRAM1_BASE  = 0x2007C000
SRAM1_LEN   = 0x4000           # 16K（.data/.bss 指针变量本体）
ESTACK      = 0x10006768

# ── 符号地址解析（firmware.map）───────────────────────────────────────────────
# 硬编码函数地址在源码改动后会漂移，统一从 linker map 解析，测试自动跟随。
MAP_PATH = os.path.join(os.path.dirname(FW), 'firmware.map')

def lookup(name, fallback=None):
    """从 firmware.map 解析符号地址。行格式：'  0x0000xxxx   符号名$'。
    找不到 → fallback；无 fallback 则抛错（避免静默用错址）。"""
    import re
    if not os.path.exists(MAP_PATH):
        raise FileNotFoundError(f"make 前需先跑 build.sh 生成 {MAP_PATH}")
    pat = re.compile(r'\s+0x([0-9a-fA-F]+)\s+([A-Za-z_][A-Za-z0-9_$]*)\s*$')
    for line in open(MAP_PATH, encoding='utf-8', errors='ignore'):
        m = pat.match(line)
        if m and m.group(2) == name:
            return int(m.group(1), 16)
    if fallback is not None:
        return fallback
    raise KeyError(f"符号 {name} 未在 {MAP_PATH} 中找到（需重新 build.sh）")


def parse_sections(path):
    d = open(path, 'rb').read()
    endian = '<'
    shoff = struct.unpack(endian+'I', d[0x20:0x24])[0]
    shentsize = struct.unpack(endian+'H', d[0x2e:0x30])[0]
    shnum = struct.unpack(endian+'H', d[0x30:0x32])[0]
    shstr = struct.unpack(endian+'H', d[0x32:0x34])[0]
    shstr_off = shoff + shstr*shentsize
    shstr_sec_off = struct.unpack(endian+'I', d[shstr_off+0x10:shstr_off+0x14])[0]
    shstr_sec_size = struct.unpack(endian+'I', d[shstr_off+0x14:shstr_off+0x18])[0]
    strtab = d[shstr_sec_off:shstr_sec_off+shstr_sec_size]
    secs = {}
    for i in range(shnum):
        so = shoff + i*shentsize
        name_off = struct.unpack(endian+'I', d[so+0x00:so+0x04])[0]
        name = strtab[name_off:strtab.index(b'\0', name_off)].decode()
        addr = struct.unpack(endian+'I', d[so+0x0c:so+0x10])[0]
        off  = struct.unpack(endian+'I', d[so+0x10:so+0x14])[0]
        size = struct.unpack(endian+'I', d[so+0x14:so+0x18])[0]
        secs[name] = (addr, off, size)
    return secs, d

def load_firmware(elf_path=FW):
    secs, d = parse_sections(elf_path)
    e = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    # FLASH：isr_vector(0x0) + text(0xd4) 及其后所有只读（含 .rodata 表）
    e.mem_map(FLASH_BASE, FLASH_MAP, UC_PROT_ALL)
    if '.text' in secs:
        a, o, s = secs['.text']
        e.mem_write(a, d[o:o+s])
    if '.isr_vector' in secs:
        a, o, s = secs['.isr_vector']
        e.mem_write(a, d[o:o+s])
    # SRAM0：.fw_image 初始镜像 + 其余清零（含栈区）
    e.mem_map(SRAM0_BASE, SRAM0_LEN, UC_PROT_ALL)
    if '.fw_image' in secs:
        a, o, s = secs['.fw_image']
        e.mem_write(a, d[o:o+s])
    # SRAM1：.data（指针变量本体）+ 其余清零
    e.mem_map(SRAM1_BASE, SRAM1_LEN, UC_PROT_ALL)
    if '.data' in secs:
        a, o, s = secs['.data']
        e.mem_write(a, d[o:o+s])
    # 默认 SP；默认 T=1（ARM 函数从 +1 地址调用触发 Thumb）
    e.reg_write(UC_ARM_REG_SP, ESTACK)
    e.reg_write(UC_ARM_REG_LR, 0x03000000)  # 哨兵返回地址（LR 指向内存外部 → EmulationStop）
    return e

def call(e, func_addr, args=None, ret_spec='r0'):
    """按 AAPCS 调 func_addr（Thumb '|1'），传 args（r0,r1,r2..），返回 ret_spec 指定寄存器值。
    函数用 bx lr 返回，LR 设哨兵地址 → emu_start 停在该址即返回。"""
    STOP = 0xFF000000
    regs = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
    if args:
        for i, v in enumerate(args[:4]):
            e.reg_write(regs[i], v)
    e.reg_write(UC_ARM_REG_LR, STOP)
    e.emu_start(func_addr | 1, STOP)
    if ret_spec == 'r1':
        return e.reg_read(UC_ARM_REG_R1)
    return e.reg_read(UC_ARM_REG_R0)


# ── 原始固件加载（A/B 差分基准用）───────────────────────────────────────────────
# 原始 LPC1765.bin 是 0x0 起 flash 的裸镜像；指向同一 SRAM（0x1000xxxx/0x2007C000）。
ORIG_BIN = os.path.join(ROOT, 'LPC1765.bin')

def load_original(bin_path=ORIG_BIN):
    """加载原始 LPC1765.bin 到 flash 0x0，映射同样的 SRAM0/SRAM1、设同样 SP。
    原始固件的指针全局（DAT_0000b4c4 等）存于 flash 0xb4c4，运行期从 flash 读目标地址，
    与编译版（指针存 SRAM1）.data 的区别不影响目标 SRAM 写入。"""
    e = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    e.mem_map(FLASH_BASE, FLASH_MAP, UC_PROT_ALL)
    d = open(bin_path, 'rb').read()
    e.mem_write(FLASH_BASE, d)                 # 赤裸 flash 镜像
    e.mem_map(SRAM0_BASE, SRAM0_LEN, UC_PROT_ALL)   # 清零（原固件运行期初始化）
    e.mem_map(SRAM1_BASE, SRAM1_LEN, UC_PROT_ALL)
    e.reg_write(UC_ARM_REG_SP, ESTACK)
    e.reg_write(UC_ARM_REG_LR, 0x03000000)
    return e


def differential(func_orig, func_new, args, seed, region=(0x10001000, 0x10003F00)):
    """A/B 差分等价测试：同一 RAM 种子下分别跑【原始固件】与【编译固件】的同功能函数，
    比较返回值 + region 内存末态是否一致。不一致 → 反编译重构与原机码语义背离（W7 抓 bug）。
    region 默认覆盖参数/全局区 0x10001000-0x10003F00（避开盘区 0x10005xxx+）。
    返回值：(ret_orig, ret_new, same, out_orig, out_new)。
    注意：本函数对纯 RAM 读写/算数的【叶函数】有效；若函数有子调用或触外设需另行 hook。"""
    def runner(loader, fn, args):
        e = loader()
        seed(e)                                # 每种 RAM 种子独立
        lo, hi = region
        pre_new = bytes(e.mem_read(lo, hi - lo))
        # 按 AAPCS 传 args（r0-r3），与 call() 一致
        regs = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
        if args:
            for i, v in enumerate(args[:4]):
                e.reg_write(regs[i], v)
        e.reg_write(UC_ARM_REG_LR, 0xFF000000)
        e.emu_start(fn | 1, 0xFF000000)        # Cortex-M3 = Thumb；起始地址|1 进入 Thumb 真执行
        ret = e.reg_read(UC_ARM_REG_R0)
        post = bytes(e.mem_read(lo, hi - lo))
        return ret, post
    ret_o, post_o = runner(load_original, func_orig, args)
    ret_n, post_n = runner(load_firmware, func_new, args)
    same = (ret_o == ret_n) and (post_o == post_n)
    return ret_o, ret_n, same, post_o, post_n


def seed_addr_value(emu, lo, hi):
    """把 [lo,hi) 每个 4 字节字写成 = 其地址值（小端），使每个全局都有唯一非零值。
    可直接发现「原机码与编译码读到不同地址/不同位宽/不同值」的映射错位——A/B 下最严苛。"""
    import struct
    buf = bytearray()
    for a in range(lo, hi, 4):
        buf += struct.pack('<I', a)
    emu.mem_write(lo, bytes(buf))
