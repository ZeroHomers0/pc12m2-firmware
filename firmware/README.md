# PC12M-2 十二相可编译固件

本目录是十二相 LPC1765 GCC 重建工程，不是六相源码镜像。

```bash
bash build.sh
```

工程包含启动与链接、十二相 SRAM 初始镜像、globals、字符串池和 13 个功能模块。
关键十二相布局：Flash 256 KiB，CRP 固定在 `0x2FC`，SRAM data 结束于 `0x10002110`，
初始 SP 为 `0x100029A0`。

当前 P5 尚未全部通过，已知问题见 `../docs/analysis/P5_VERIFICATION_PROGRESS.md`。
在完整 A/B 全过并冻结哈希前，本目录产物不得烧写。
