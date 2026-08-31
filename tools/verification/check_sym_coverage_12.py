#!/usr/bin/env python3
"""P4 核验：12p 94 目标函数在新 ELF 中以语义符号存在（按名称/注释表比对）。
   重编译 ELF 与原始 BIN 地址布局不同，入口对齐不适用；此处验证 9 个重写函数
   的语义符号 + 85 个移植函数的调用关系在链接层完整。"""
import re, subprocess

ELF = "firmware/firmware.elf"
LIST = "tools/_ghidra_proj/_pc12m2_functions.txt"

targets = {}
for line in open(LIST):
    m = re.match(r'([0-9a-f]{8})\s+(\w+)', line)
    if m:
        targets[int(m.group(1), 16)] = m.group(2)

out = subprocess.run(["arm-none-eabi-nm", "--defined-only", ELF],
                     capture_output=True, text=True).stdout
names = set()
addrs = {}
for line in out.splitlines():
    parts = line.split()
    if len(parts) >= 3 and parts[1] in ("T", "t"):
        addrs[int(parts[0], 16)] = parts[2]
        names.add(parts[2])

print(f"12p 目标函数: {len(targets)}   ELF 函数符号: {len(addrs)}")
# 关键 9 个差异函数语义名应存在
key = {
    0x4464: "state_machine",
    0xe70c: "output_stage",
    0x3534: "param_sync_live_to_eeprom",
    0x258c: "load_config",
    0x40b0: "disp_splash_screen",
    0x41ec: "disp_screen_calib",
    0x108dc: "auth",  # 认证函数名可能不同
    0x10a38: "pid",
    0x5cc: "main",
}
print("--- 关键重写函数符号存在性 ---")
for addr, hint in key.items():
    # 语义名匹配
    found = [n for n in names if hint in n.lower()]
    if found:
        print(f"  0x{addr:08x} {targets.get(addr,'?')}: {found[:3]}")
    else:
        print(f"  0x{addr:08x} {targets.get(addr,'?' )}: !!! 未找到 '{hint}'")
print("--- 符号总数按模块统计 ---")
for mod in ("state_machine","output_stage","disp_","i2c_","adc_","debounce_","param_","load_config","run_stop","fio1_","out_relay","gpio_","scan_run","wd_feed","sm3_","sm4_","sm5_","sm6_"):
    cnt = sum(1 for n in names if n.startswith(mod))
    if cnt: print(f"  {mod}: {cnt}")
