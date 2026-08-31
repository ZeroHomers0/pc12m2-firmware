# PC12M-2 UART3 / Modbus RTU

> 整理日期：2026-08-31。

- UART3 通过隔离收发链路提供 Modbus RTU，从站支持功能码 `0x03`、`0x06`、`0x10`。
- 十二相入口：RX 组帧 `0xAC40`、UART3 ISR `0xAC78`、CRC16 `0xACD4`、
  读寄存器 `0xAD04`、多寄存器写 `0xB050`、分发 `0xB3B2`。
- CRC16 高低表位于原 BIN `0x111D8` / `0x112D8`。
- P5 已通过寄存器 `0x00..0x40` 的 65 项读取和 320 项写入矩阵。
- 已确认的原厂不对称：寄存器 `0x17/0x18` 读取活动 PID 槽、写入参数银行源槽。

验证入口：`tools/verification/verify_firmware_equivalence_12.py`；静态映射检查：
`test/static/test_modbus_register_map.py`。
