# 原固件备份记录 —— PC12M-2（十二相 SCR 控制板，LPC1765FBD100）

- **备份日期**：2026-08-30
- **通道**：ISP（Flash Magic 13.65 / fm.exe），COM7（FTDI USB-TTL），9600 波特
- **签名**：LPC1765 `0x26013733`（637613875）
- **CRP @0x2FC**：`0xFFFFFFFF`（无读保护）
- **成品**：`pc12m2_orig.bin`，262144 B（256 KiB）
  - **SHA-256**：`2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`
  - 非 0xFF 内容 76158 字节（0x0–0x12C67），其余为空白
  - 向量表：SP=0x100029A0、Reset=0x000001A9
- **读取方式**：分 16 块 Intel HEX（`part00..part14.hex` + `part15a.hex`），`w8_combine_hex.py` 合并
- **稳定性交叉校验**：重读 0x0–0x4000（`verify_part00.hex`）与首块逐字节一致（0 差异）
- **边界说明**：ISP 读 Flash 末尾（0x3FE00–0x40000）会因 fm.exe 结束哨兵读越界而报错，
  但实际数据由 `part15a.hex` 完整覆盖（0x3C000–0x3FFFF），全为 0xFF 空白，无内容损失

## 铁律

- 本文件与 `pc12m2_orig.bin` 属敏感备份，**已 gitignore，绝不入库**。
- **复制一份到第二处独立物理位置**（U 盘 / 另一台机）后再进行任何擦写操作。
- 与 6 相位参考固件（`D:\code\LPC1765FBD100\decompiled\LPC1765.bin`）逐字节差异 70629 处，
  属重新编译所致；差异分析须在反汇编/函数级进行。
