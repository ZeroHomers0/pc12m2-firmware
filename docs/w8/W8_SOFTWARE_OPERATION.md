# PC12M-2 W8 软件与仪器操作

> 更新日期：2026-08-31。构建、备份和烧写的唯一命令源为根目录 `操作文档.md`。

## 软件

- Arm GNU Toolchain 14.2.Rel1
- Python 3.12，依赖 `pyserial`、`minimalmodbus`、`unicorn`
- 仓库内 `tools/jlink/JLink.exe`
- Flash Magic / `fm.exe`（ISP）

## 工具入口

| 工具 | 用途 |
|---|---|
| `tools/w8/w8_serial_detect.py` | 枚举串口 |
| `tools/w8/w8_isp_probe.py` | ISP 只读探测 |
| `tools/w8/w8_combine_hex.py` | 合并分块备份 |
| `tools/w8/w8_modbus_test.py` | Modbus 验证 |
| `tools/w8/w8_analyze_wave.py` | 分析示波器 CSV |

示波器与信号源必须先在隔离低压环境确认接地和量程。测试结果回填 `W8_TEST_MASTER.md`。
