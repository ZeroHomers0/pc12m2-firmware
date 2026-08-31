# PC12M-2 I2C 参数同步

> 整理日期：2026-08-31。地址与行为以十二相反汇编和源码为准。

- 参数存储使用 GPIO 模拟 I2C，设备地址为 `0x53`，行为符合 24C02 类 EEPROM。
- `load_config` 十二相入口为 `0x258C`，负责启动读取、默认值与活动参数初始化。
- `param_sync_live_to_eeprom` 十二相入口为 `0x3534`，负责活动参数向影子区及 EEPROM 同步。
- 多数配置槽按字节访问；16 位参数显式拆分高低字节。位宽必须以 `ldrb/strb` 与 `ldr/str`
  的十二相反汇编证据判断。
- 当前结构检查位于 `test/static/test_parameter_sync_structure.py`。
- 完整执行级等价性由 `tools/verification/verify_firmware_equivalence_12.py` 覆盖。

禁止直接套用其他型号的 SRAM 地址或 EEPROM 寄存器结论。
