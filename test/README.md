# 固件离线测试

统一入口：

```bash
python test/run_tests.py
python test/run_tests.py crc16
```

`run_tests.py` 递归发现测试，并保持按名称过滤能力。

## 测试模块清单（25 个，必须全部通过）

### static/ — host 静态/结构检查（不执行机器码）

| 文件 | 覆盖 |
|---|---|
| `test_crc16_tables_and_semantics.py` | CRC 表逐字节 vs 原 BIN flash 表；`crc16` 长度语义（处理全部 len 字节） |
| `test_modbus_register_map.py` | Modbus 读写映射对称：read/write 同 SRAM 地址、同位宽、保留区落 g_scratch |
| `test_parameter_sync_structure.py` | 参数 live→EEPROM 同步结构（shadow 复制 + i2c_write 模式） |

### emulation/ — Unicorn 执行级验证（A/B 差分 或 真执行编译产物）

| 文件 | 类型 | 覆盖 |
|---|---|---|
| `test_adc_scan_sequence_equivalence.py` | A/B 差分 | ADC 通道扫描、配置/增益组合和除数边界，共 1296 次转换 |
| `test_adc_wait_done_equivalence.py` | A/B 差分 | `adc0_wait_done` 读 AD0GDR 回归护栏（W8 指针坑 bug） |
| `test_case3_edit_equivalence.py` | A/B 差分 | 参数编辑 case 3 的按键路径与边界行为 |
| `test_closed_loop_equivalence.py` | A/B 差分 | `closed_loop_integral` PID（除数表/死区/钳位） |
| `test_closed_loop_wrapper_equivalence.py` | A/B 差分 | `closed_loop_wrapper` 重算/回绕不重算两分支 |
| `test_control_multitick_equivalence.py` | A/B 差分 | 控制状态机跨多周期执行及故障态输入扫描 |
| `test_crc16_against_model.py` | 真执行 | 编译 `crc16` vs Python 模型 |
| `test_crc16_firmware_equivalence.py` | A/B 差分 | `crc16` 原始 vs 编译 |
| `test_delay_loop_structure.py` | 指令级结构 | `Delay`/I2C 延时循环的次数、线性指令成本与原 BIN 对照；源码保持常规 C 循环 |
| `test_eeprom_load_config_equivalence.py` | A/B 差分 | EEPROM 配置加载、默认值及派生状态 |
| `test_eeprom_sync_matrix_equivalence.py` | A/B 差分 | live→EEPROM 单字节扰动矩阵与批量写入顺序（280 例） |
| `test_input_scan_equivalence.py` | A/B 差分 | `input_scan_state` FIO 扫描回归护栏（W8 指针坑 bug） |
| `test_interrupt_sequence_equivalence.py` | A/B 差分 | 外部中断与定时器中断的连续调用状态/寄存器轨迹 |
| `test_modbus_dispatch_execution.py` | 真执行 | `modbus_dispatch` 完整 0x06/0x10 帧、CRC 错误、非法地址/值/长度及异常响应 |
| `test_modbus_read_register_equivalence.py` | A/B 差分 | `modbus_read_reg` reg 0x00-0x3F |
| `test_modbus_write_multi_equivalence.py` | A/B 差分 | `modbus_write_multi` reg 0x00-0x3F |
| `test_parameter_sync_execution.py` | 真执行 | `param_sync_live_to_eeprom`（hook i2c_write_byte） |
| `test_peripheral_leaf_trace_equivalence.py` | A/B 差分 | I2C/UART 叶函数及 UART 初始化后的 MMIO/RAM 状态（45 例） |
| `test_relay_state_machine_equivalence.py` | A/B 差分 | 继电器输出与控制状态组合 |
| `test_state_machine_call_trace_equivalence.py` | A/B 差分 | 主状态机输入消抖、运行/停止扫描及 GPIO 子调用轨迹（100 例） |
| `test_state_machine_multitick_matrix_equivalence.py` | A/B 差分 | MENU/故障/复位/急停/运行组合的 2/10 周期矩阵（222 例） |
| `test_uart_rx_sequence_equivalence.py` | A/B 差分 | UART3 接收 ISR 多字节序列、帧缓冲与边界状态 |

### support/

- `unicorn_harness.py`：ELF/BIN 加载、符号查询、内存种子和 A/B 差分公共支持。

## 规则

- 修改源码后必须运行对应回归测试；A/B 差分测试是「原始 vs 编译」执行级等价护栏。
- 当前完整入口结果为 `25/25`；新增或删除测试文件时必须同步更新本清单与模块总数。
- 独立的更大验证矩阵不在本目录重复实现，主入口为 `tools/verification/verify_firmware_equivalence.py`。
