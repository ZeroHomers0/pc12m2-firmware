# PC12M-2 工具目录

| 目录 | 用途 |
|---|---|
| `verification/` | 十二相反汇编导出、静态审计和完整 A/B 验证 |
| `generation/` | 从十二相原 BIN 生成 SRAM 镜像、globals 和字符串池 |
| `maintenance/` | 十二相源码机械维护 |
| `ghidra/` | Ghidra 辅助脚本 |
| `w8/` | 十二相备份、ISP、Modbus、波形和栈工具 |
| `jlink/` | 仓库内唯一 J-Link 运行时 |

主验证入口：`verification/verify_firmware_equivalence_12.py`。
生成和验证均以 `backup/pc12m2_orig.bin` 为十二相金标准；缺少该文件时不得伪造结果。
