# PC12M-2 数据与内存布局

> 整理日期：2026-08-31。

| 区域 | 范围 | 用途 |
|---|---|---|
| Flash | `0x00000000..0x0003FFFF` | 代码、只读数据、SRAM 初始镜像 |
| CRP | `0x000002FC..0x000002FF` | 固定 `0xFFFFFFFF` |
| SRAM0 data | `0x10000000..0x1000210F` | 原固件初始化数据镜像，8464 B |
| SRAM0 bss/stack | `0x10002110..0x1000299F` | 清零区与原始栈顶边界 |
| 初始 SP | `0x100029A0` | 十二相向量表首项 |
| SRAM1 | `0x2007C000..0x2007FFFF` | 重编译工程自身全局 |

生成资产：

- `firmware/assets/ram_data_image.bin`
- `firmware/data_image.s`
- `firmware/globals.c` / `firmware/inc/globals.h`
- `firmware/src/strpool.c`

生成报告只保留十二相版本：`evidence/reverse/reports/_globals_report_12.txt` 与
`_strpool_report_12.txt`。任何地址调整必须回验十二相反汇编与原 BIN。
