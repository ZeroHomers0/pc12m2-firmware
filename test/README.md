# PC12M-2 离线测试

```powershell
python test/run_tests.py
python test/run_tests.py crc16
```

当前保留三项十二相静态检查：CRC 表与语义、Modbus 映射、参数同步结构。

更完整的十二相执行级矩阵统一由
`tools/verification/verify_firmware_equivalence_12.py` 提供，不再保留六相地址的旧仿真测试副本。
CRC 静态检查需要 `backup/pc12m2_orig.bin`。
