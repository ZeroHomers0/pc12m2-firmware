# PC12M-2 十二相 SCR 控制板工程（LPC1765FBD100）

PC12M-2 是 PC6M-10（六相）的十二相孪生板：MCU 与外围几乎一致，控制程序仅有细微差别
（触发相数 6→12）。本仓库用于备份、逆向、差异对比与重建该板固件。

## 快速开始

- AI/维护人员先读 `AGENTS.md`。
- 全部文档入口见 `DOCUMENTATION_INDEX.md`。
- 参考项目（六相，已逆向完成）：`D:\code\LPC1765FBD100\decompiled`。

```bash
# 合并 ISP 分块备份为完整 bin
python tools/w8/w8_combine_hex.py

# （SWD 版只读备份，需 J-Link 探针）
python tools/w8/w8_backup_orig.py
```

当前状态：项目结构已搭建；原固件备份进行中；尚未逆向与重建固件。
