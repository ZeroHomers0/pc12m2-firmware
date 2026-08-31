# 工具目录（PC12M-2 / 12 相板）

> 本目录为 12 相板（PC12M-2）逆向工程的当前工具集。
> 6 相（PC6M-10）时代的验证/生成/审计脚本已于 2026-08-31 清理（用户持两份工程独立源文件，
> 此处删除不影响 6p 工程 `D:\code\LPC1765FBD100\decompiled`）。

## 分类与工具清单

### verification/ — 离线验证（主入口：`verify_firmware_equivalence_12.py`）

| 脚本 | 功能 |
|---|---|
| `verify_firmware_equivalence_12.py` | **主入口**：Unicorn A/B——原固件 `backup/pc12m2_orig.bin` vs 新 ELF 关键函数（VECTOR/PIN_CONFIG/GPIO/AUTH/TIMER/WDT/UART3/MODBUS/CLOSED_LOOP/STATE_MACHINE…） |
| `dump_disasm_12.py` | 从原 BIN 生成 12p 逐函数反汇编金标准（`evidence/reverse/disassembly/functions/`） |
| `check_sym_coverage_12.py` | 12p 命名变量（165）SRAM 访问覆盖审计 |
| `dump_12_isrs.py` | 12p 向量表 ISR 反汇编 dump |
| `sm_struct.py` / `state_machine_briefing.md` | 12p 状态机结构/简报 |
| `check_readwidth.py` | 反汇编 strb/str vs `globals.c` 类型，找 byte 槽被定义 word（12p 地址） |
| `verify_mem_xref.py` | 模块 `.c` SRAM 访问 vs 金标准 ref，双向判漏（通用） |
| `verify_startup.py` | host 模拟 Reset_Handler 启动链路（读 12p `firmware.bin`/`ram_data_image.bin`） |
| `verify_strpool.py` | `strpool_map` 字符串映射正确性（读 12p 原 BIN） |

### generation/ — 从原 BIN/反编译生成源码或报告（12p 版本）

| 脚本 | 功能 |
|---|---|
| `generate_globals_12.py` | 12p 提取 DAT_/PTR_ 符号 → `globals.h/.c`（含 165 语义命名） |
| `extract_ram_data_image_12.py` | 12p IAR 压缩 `.data` 解压 → `firmware/assets/ram_data_image.bin` |
| `generate_string_pool_12.py` | 12p 生成 `strpool.c`（GBK blob + 簇表 + strpool_map） |
| `_modbus_tables_12.py` | 12p modbus 写单块/表权威规格生成 |

### maintenance/ — 源码符号、位宽和常量的机械维护

| 脚本 | 功能 |
|---|---|
| `apply_consts_12.py` | 12p `.c` 的语义常量 → `consts.h` 宏 |
| `fix_readwidth.py` | byte 槽被定义为 word 的符号 → `uint8_t*` |
| `rename_locals.py` | 按函数边界做局部变量/参数重命名 |
| `rename_symbols.py` | 全局变量语义化命名（DAT_ → 人类可读名） |

### _ghidra_proj/ — 本地 Ghidra 工程辅助（不入库，见 .gitignore）

`pool_map.py`（池槽→SRAM 映射）、`apply_symbols.py`（165 命名应用）、
`map_pools_6to12*.py`/`map_syms_6to12.py`（6→12 对照）、`disasm_*.py`、`sram_*.py` 等。

### w8/ — 实机阶段辅助

| 脚本 | 功能 |
|---|---|
| `w8_analyze_wave.py` | 示波器 CSV → 触发脉宽/周期/间隔 → 电角度判定 |
| `w8_backup_orig.py` | 原固件备份（只读，CRP + 双份 256 KiB + SHA 核对） |
| `w8_combine_hex.py` | 备份 `part*.hex` → `backup/pc12m2_orig.bin` |
| `w8_isp_probe.py` | LPC17xx UART0 ISP 只读探测（autobaud + 只读命令） |
| `w8_modbus_test.py` | Modbus-RTU 通信与语义验证（reg40-45 / 写注入 / reg61） |
| `w8_serial_detect.py` | 枚举串口识别 USB-RS485 |
| `w8_stack_sentinel.py` | SRAM 哨兵测中断负载下最低 MSP 栈水位 |
| `w8_stack_watermark.py` | Unicorn 离线模拟最低 MSP 栈水位（依赖 `test/support/unicorn_harness.py`） |

### jlink/ — 免安装 J-Link 最小集

`JLink.exe` + DLL + USB 驱动。**`JLink.exe` 是全项目 J-Link 唯一调用路径**，
无需安装 J-Link 软件；所有 SWD 烧写/探测一律从这里调用（命令见 `操作文档.md` §4）。

### archive/ — 一次性历史工具（仅供追溯，不作为当前流程入口）

目录仅保留说明；6p 时代的 `decompress_iar.py`、`extract_decompressor.py`、
`extract_modbus_branches.py` 等已于 2026-08-31 清理（6p 项目有独立源文件）。

## 规则

- 所有当前脚本必须从脚本位置推导项目根目录，禁止写死 `D:\code\...`。
- `archive/` 只读追溯，不作为当前流程入口。
- 12p 验证一律以 `backup/pc12m2_orig.bin` 为原始 BIN（根目录 `LPC1765.bin` 不存在）。
