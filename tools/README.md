# 工具目录

## 分类与工具清单

### audit/ — 只读独立审计（不调用测试套件）

| 脚本 | 功能 |
|---|---|
| `codex_audit_crc_tables.py` | 独立比对 CRC 表 vs 原始固件 |
| `codex_audit_function_coverage.py` | 原始函数入口 vs 可编译源码/ELF 覆盖 |
| `codex_audit_globals.py` | `globals.c` DAT/PTR 字面量 vs 原 BIN 逐项比对 |
| `codex_audit_strpool.py` | `strpool.c` 映射簇 vs 原 BIN 独立核对 |

### generation/ — 从原 BIN/反编译生成源码或报告

| 脚本 | 功能 |
|---|---|
| `extract_data_segments.py` | 提取数据段常量（字符串/查表/指针）→ `evidence/reverse/reports/_data_extract.txt` |
| `extract_ram_data_image.py` | IAR 压缩 `.data` 解压 → `firmware/assets/ram_data_image.bin` + 校验 |
| `generate_globals.py` | 提取 DAT_/PTR_ 符号 → `globals.h/.c` |
| `generate_string_pool.py` | 生成 `strpool.c`（GBK blob + 簇表 + strpool_map） |
| `locate_sram_mirrors.py` | 定位 SRAM `.data` 初始镜像 + 字符串池补扫（结论已并入 `extract_ram_data_image`） |

### ghidra/ — Ghidra 辅助脚本

| 脚本 | 功能 |
|---|---|
| `create_isr_functions.py` | 从向量表创建并命名 8 个 Cortex-M3 ISR 函数 |

### maintenance/ — 源码符号、位宽和常量的机械维护

| 脚本 | 功能 |
|---|---|
| `apply_consts_08.py` / `09` / `12` | 对应 `.c` 的语义常量 → `consts.h` 宏 |
| `fix_readwidth.py` | byte 槽被定义为 word 的符号 → `uint8_t*` |
| `rename_locals.py` | 按函数边界做局部变量/参数重命名 |
| `rename_symbols.py` | 全局变量语义化命名（DAT_ → 人类可读名） |

### verification/ — 离线验证（当前主入口：`verify_firmware_equivalence.py`）

| 脚本 | 功能 |
|---|---|
| `verify_firmware_equivalence.py` | **主入口**：直接执行原固件 vs 新 ELF 的关键函数 |
| `check_readwidth.py` | 反汇编 strb/str vs `globals.c` 类型，找 byte 槽被定义 word |
| `verify_mem_xref.py` | 模块 `.c` SRAM 访问 vs 金标准 ref，双向判漏 |
| `verify_modbus_c.py` | `08_modbus_dispatch.c` 写分支 vs asm 分支表 |
| `verify_periph_xref.py` | 外设地址（0x2xxxxxxx/0x4xxxxxxx）交叉引用验证 |
| `verify_readwidth_all.py` | 反汇编自动提取 SRAM 访问宽度 vs `globals.c` |
| `verify_sm_addresses.py` | `07_state_machine.c` SRAM 地址 vs 金标准 |
| `verify_startup.py` | host 模拟 Reset_Handler 启动链路 |
| `verify_strpool.py` | `strpool_map` 字符串映射正确性 |

### w8/ — 实机阶段辅助

| 脚本 | 功能 |
|---|---|
| `w8_analyze_wave.py` | 示波器 CSV → 触发脉宽/周期/间隔 → 电角度判定 |
| `w8_backup_orig.py` | 原固件备份（只读，CRP + 双份 256 KiB + SHA 核对） |
| `w8_isp_probe.py` | LPC17xx UART0 ISP 只读探测（autobaud + 只读命令） |
| `w8_modbus_test.py` | Modbus-RTU 通信与语义验证（reg40-45 / 写注入 / reg61） |
| `w8_serial_detect.py` | 枚举串口识别 USB-RS485 |
| `w8_stack_sentinel.py` | SRAM 哨兵测中断负载下最低 MSP 栈水位 |

### jlink/ — 免安装 J-Link 最小集

`JLink.exe` + DLL + USB 驱动。**`JLink.exe` 是全项目 J-Link 唯一调用路径**，
无需安装 J-Link 软件；所有 SWD 烧写/探测一律从这里调用（命令见 `操作文档.md` §4）。

### archive/ — 一次性历史工具（仅供追溯，不作为当前流程入口）

| 脚本 | 功能 |
|---|---|
| `decompress_iar.py` | IAR `__iar_copy_data` 压缩解压器 |
| `extract_decompressor.py` | 提取反汇编 0x100-0x164 区间（解压器权威反汇编） |
| `extract_modbus_branches.py` | 提取 Modbus 写单寄存器分支表 |

## 规则

- 所有当前脚本必须从脚本位置推导项目根目录，禁止写死 `D:\code\...`。
- `archive/` 只读追溯，不作为当前流程入口。
