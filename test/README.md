# PC12M-2 离线测试

```powershell
python test/run_tests.py
python test/run_tests.py crc16
```

当前保留三项十二相静态检查：CRC 表与语义、Modbus 映射、参数同步结构。

更完整的十二相执行级矩阵统一由
`tools/verification/verify_firmware_equivalence_12.py` 提供，不再保留六相地址的旧仿真测试副本。
CRC 静态检查需要 `backup/pc12m2_orig.bin`。

## 额外覆盖测试（任务 #4，2026-08-31 新增）

`test/emulation/test_extra_coverage_12.py`：参考 6p `test/emulation` 补 9 个未覆盖测试组，
全部 A/B 差分（OLD `backup/pc12m2_orig.bin` vs NEW `firmware/firmware.elf`）：
adc_wait_done / adc_scan_channels / input_scan_state / uart_rx_sequence / modbus_dispatch /
eeprom_sync_matrix / interrupt_sequence / control_multitick / case3_edit，**113/113 PASS**。

```powershell
# 改源码后先重建固件，再跑
cd firmware; bash build.sh; cd ..
python test/emulation/test_extra_coverage_12.py
```

依赖 `tools/verification/verify_firmware_equivalence_12.py` 的 machine/run/snapshot/SYMS。
差分曾暴露并修复 2 个真实移植 bug（adc 增益对调、modbus 0x10 异常路径漏写），
详见 6p 侧 `PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md`。
