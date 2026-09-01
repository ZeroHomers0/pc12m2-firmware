# AGENTS.md — PC12M-2 十二相 SCR 控制板（LPC1765FBD100）

> 本文件是 AI 接手 PC12M-2 项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目定位

本仓库只维护 **PC12M-2 十二相 SCR 移相触发控制板**。MCU 为 NXP LPC1765FBD100。
PC6M-10 六相项目已经完成全部实验并达到成熟状态，只作为只读参考；禁止修改六相仓库，
也不得把六相结论直接当作十二相事实。

十二相判断必须以本项目的原始固件、反汇编和 A/B 执行结果为准。六相源码只能帮助理解
同构结构、命名和验证方法。

## 当前状态（2026-08-31）

- 原固件曾通过 ISP 完整读取，记录 SHA-256 为
  `2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`，
  长度 262144 B，CRP=`0xFFFFFFFF`，详见 `backup/BACKUP_RECORD.md`。
- 原固件 `backup/pc12m2_orig.bin` 已就位并核验 SHA-256
  （`2BC608683992AD6DB2D3CB75129BF57EA37D6D681D7C2C0A132E064EBD271BD1`）；
  2026-08-31 按用户决定另存根目录副本 `pc12m2_orig.bin` 并提交入库（覆盖原「绝不入库」铁律，
  详见 `backup/BACKUP_RECORD.md`）。擦写板上 Flash 前仍需制作第二物理副本。
- P0-P4 已完成：十二相函数清单、反汇编、数据镜像、全局映射和可编译 C 工程已建立。
- P5 Unicorn A/B 等价验证**已全部 PASS**（2026-08-31）：向量、GPIO、TIMER1 972 例、
  Modbus 65 读/320 写、闭环、输出级、状态机 130 例、显示 106 例+全执行 4 例、
  去抖、认证、MISC 全过。本次修复：debounce 计数器槽错位、三个认证函数 R0 残留、
  strpool/显示串地址、void 屏渲染函数改行为校验。详见
  `docs/analysis/P5_VERIFICATION_PROGRESS.md`。
- **任务 #4（测试覆盖查漏）已完成**：参考 6p `test/` 全部测试，为 12p 补 9 个未覆盖
  测试组共 **113 例全部 A/B 等价 PASS**，脚本保留于 `test/emulation/test_extra_coverage_12.py`。
  差分暴露并修复 2 个真实移植 bug：adc0_scan_channels 增益全局对调（OLD 0x1F6C 铁证）、
  modbus_dispatch 0x10 异常路径漏写 menu_param_4（OLD 0xDFB4 先写后比）。
  详见 6p 侧 `PC6M-10/docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md`。
- **任务 #6（认证放行）已完成**：2026-08-31 决定（抄板，同 6p）——防抄板认证永久放行。
  `01_startup.c` `main()` 认证段后强制 `*lock = 0`（`auth_pass_flag`@0x100020C0，**0=通过**，
  与 6p 的 1=放行相反）；`auth_verify_loop()` 调用保留（保持原厂语义与 A/B 等价），
  锁机分支（"报警忙碌/CPU 忙碌"）正常不可达。重建固件后 `verify_firmware_equivalence_12.py`
  全 PASS（含 `AUTH: PASS funcs=3`）、113/113 PASS 无回归。
- **任务 #5（W8 问题复查）已完成**：逐项复查 6p 实机（`PC6M-10/docs/w8/`）暴露的 19 项问题
  + 6 项流程/清单，**12p 无任何一项需要代码级修复**——6 项移植期已修（ADC 指针、FIO 读址、
  case3 钳位/闪烁、Modbus 大端、恢复出厂串地址、复位坐标）、3 项原厂设计、12p 独有的
  认证放行/case1/case1E 三分支门控/主屏 LED 错译均已完成。详见 6p 侧
  `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md`。
- 认证已永久放行，12p 源码可稳定运行；尚未冻结烧写基线哈希，W8 分级实测尚未开始。
- **2026-09-01 产品信息定制**：菜单 8「产品版本信息」屏 4 行文本覆写为
  型号:PC12M-2 / 版本:V2.0 / 厂商:XIANPOWER / 电话:029-84205750（改
  `tools/generation/generate_string_pool_12.py` 的 `PRODUCT_INFO_OVERRIDES` 重新生成
  `strpool.c`，`strpool_map` 前置查表；flash 地址与执行路径不变，A/B 验证全 PASS）。
  详见 `docs/analysis/P5_VERIFICATION_PROGRESS.md`。

## 必须遵守

- 全程中文交流。
- 只修改本仓库；六相仓库只读参考。
- 十二相结论必须由 `pc12m2_orig.bin`、十二相反汇编或十二相实测支持。
- 修改源码后必须执行十二相 A/B 验证，不能以六相测试结果替代。
- 未找回并核对原固件备份及第二物理副本前，绝不擦写 Flash。
- CRP 非 `0xFFFFFFFF` 时绝不执行 unlock、recover 或 mass erase。
- 上板时只接控制电，断开市电、门极和功率负载；未完成分级放行不得带载。
- J-Link 只使用 `tools/jlink/JLink.exe`；无探针时走 ISP。
- 十二相原始反汇编与过程证据不得删除；六相重复副本已获用户授权清理。

## 目录职责

```text
backup/                     十二相原固件备份、ISP 分块与日志（敏感、gitignore）
firmware/                   十二相可编译源码工程
docs/project/               十二相项目状态、应用与数据布局
docs/analysis/              十二相模块结论与 P5 验证进度
docs/w8/                    十二相实机验证流程、规范与状态
evidence/hardware/          硬件资料；board/display/reports 为六相参考，十二相事实须另行验证
evidence/reverse/           十二相逆向证据
  disassembly/functions/    带引用注释的逐函数反汇编，当前权威
  disassembly/raw/          十二相原始逐函数导出
  notes/                    十二相状态机人工拆解过程记录
  reports/                  十二相生成报告
test/                       十二相静态检查与公共 Unicorn 支持
tools/                      十二相生成、验证、实机与 J-Link 工具
```

## 常用入口

操作入口（详见 `操作文档.md`；根目录三个 `.bat` 已删除，跨机不通用）：
- 构建 —— `cd firmware && bash build.sh`（产物 `firmware.bin/hex/elf/map`，SHA/尺寸见 操作文档.md §2）
- SWD 烧写（日常主通道，首选）—— 打包版 `tools/jlink/JLink.exe` + `-CommanderScript`，命令见 操作文档.md §3
- ISP 烧写（SWD 连不上时的解困通道）—— Flash Magic + USB-TTL，接线与用法见 操作文档.md §4
- 重启固件一律**物理断电再上电**（J-Link 驱动 nRESET 复位会悬挂 SWD，干扰复用调试脚的固件运行）。

```powershell
# 构建（Git Bash）
cd firmware
bash build.sh

# 静态测试（CRC 测试需原 BIN，已就位）
cd ..
python test/run_tests.py

# 十二相完整 A/B 验证；需要原 BIN 和已构建固件
python tools/verification/verify_firmware_equivalence_12.py
```

## 下一步

1. 冻结待上板固件哈希（`firmware/firmware.bin` SHA-256 与长度记录到文档）。
2. 冻结后进入 `docs/w8/W8_TEST_MASTER.md` 的分级实测（只接控制电，断开市电/门极/功率负载）。
3. 上板前先按 `PC6M-10/docs/analysis/PC12M2_W8_ISSUES_REVIEW.md`「三、实机核对项」核对：
   SWD 介入路径（P1.30/P1.29 复用，须 connect-under-reset 或 ISP）、板内 `0x2FC` CRP 实读
   （文件值≠板内值）、12p 触发波形 2-3 判据等。
