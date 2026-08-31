# PC12M-2 十二相 SCR 控制板固件

本仓库只维护 PC12M-2 十二相固件。MCU 为 LPC1765FBD100。

六相成熟项目只作为只读参考；十二相事实以本项目原始 BIN、十二相反汇编和十二相实测为准。

当前状态：可编译 C 工程已经建立，P5 Unicorn A/B 等价验证进行中；尚未形成可烧写放行基线。
当前工作区缺少被忽略的 `backup/pc12m2_orig.bin`，找回并核验哈希前禁止擦写。

入口：

- 项目约束与当前状态：`AGENTS.md`
- 文档导航：`DOCUMENTATION_INDEX.md`
- 构建与硬件操作：`操作文档.md`
- P5 进度：`docs/analysis/P5_VERIFICATION_PROGRESS.md`
- W8 总控：`docs/w8/W8_TEST_MASTER.md`

```powershell
# Git Bash 中构建
cd firmware
bash build.sh

# 原 BIN 找回后执行
cd ..
python test/run_tests.py
python tools/verification/verify_firmware_equivalence_12.py
```
