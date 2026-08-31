# PC12M-2 W8 实机验证入口

> 更新日期：2026-08-31。

当前尚未进入上板烧写：P5 全量 A/B PASS、额外覆盖 113/113 PASS、认证已永久放行
（`01_startup.c` main() 强制 `*lock=0`，`auth_pass_flag==0`=通过）；剩余前置为
**冻结待上板固件哈希**，并核对 6p 侧 `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md`
「三、实机核对项」。

阅读顺序：

1. `W8_TEST_MASTER.md`
2. `W8_PRE_HARDWARE_VALIDATION_2026-08-31.md`
3. `W8_HARDWARE_TEST_2026-08-31.md`
4. `W8_SOFTWARE_OPERATION.md`
5. 根目录 `操作文档.md`

停止线：无双备份不擦写；P5 不全过不上板；空载十二路时序不通过不进入低压或带载。
