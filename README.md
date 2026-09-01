# PC12M-2 十二相 SCR 控制板固件

本仓库只维护 PC12M-2 十二相固件。MCU 为 LPC1765FBD100。

六相成熟项目只作为只读参考；十二相事实以本项目原始 BIN、十二相反汇编和十二相实测为准。

当前状态：
- P5 Unicorn A/B 等价验证**已全部 PASS**（2026-08-31）。
- 测试覆盖查漏（任务 #4）：额外补 9 组 **113/113 PASS**（`test/emulation/test_extra_coverage_12.py`）。
- 认证已**永久放行**（任务 #6，`01_startup.c` `main()` 强制 `*lock=0`，`auth_pass_flag==0`=通过）。
- 6p W8 实机问题复查（任务 #5）完成：12p 无代码级修复需求。
- 产品信息定制 + 厂商 X/O 字形修复 + 电话行宽修正（2026-09-01/02）已完成，A/B 全 PASS。
- 当前基线：`text 63964 / data 6312 / bss 2192`，`firmware.bin` SHA-256
  `449A13DB053C5A81632E9C66C6B467723BC995BE56E7BEC79425804BF38663E2`。
- **实机测试已通过**（2026-09-02，用户直接实机验证）；W8 分级实测流程未按计划执行、已废弃，
  详见 `AGENTS.md`。

入口：

- 项目约束与当前状态：`AGENTS.md`
- 文档导航：`DOCUMENTATION_INDEX.md`
- 构建与硬件操作：`操作文档.md`
- P5 进度与测试覆盖/认证放行：`docs/analysis/P5_VERIFICATION_PROGRESS.md`
- W8 历史流程档案（分级实测已废弃，实机已直接通过）：`docs/w8/W8_TEST_MASTER.md`
- 复查结果（6p 侧）：`PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md`、
  `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md`

```powershell
# Git Bash 中构建（改源码后）
cd firmware
bash build.sh

cd ..
python test/run_tests.py                      # 静态检查
python tools/verification/verify_firmware_equivalence_12.py   # 完整 A/B
python test/emulation/test_extra_coverage_12.py               # 额外覆盖 113/113
```
