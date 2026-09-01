# PC12M-2 十二相可编译固件

本目录是十二相 LPC1765 GCC 重建工程，不是六相源码镜像。

```bash
bash build.sh
```

工程包含启动与链接、十二相 SRAM 初始镜像、globals、字符串池和 13 个功能模块。
关键十二相布局：Flash 256 KiB，CRP 固定在 `0x2FC`（`lpc1765.ld` `.crp` 段 + startup.s
`_crp_word=0xFFFFFFFF` + build.sh `--gap-fill 0xFF`），SRAM data 结束于 `0x10002110`，
初始 SP 为 `0x100029A0`。

当前状态：P5 完整 A/B 已全部 PASS，认证已永久放行（`01_startup.c` `main()` 强制
`*lock=0`，`auth_pass_flag@0x100020C0` **0=通过**）；修复记录见
`../docs/analysis/P5_VERIFICATION_PROGRESS.md`。在冻结烧写基线哈希前，本目录产物不得烧写。
