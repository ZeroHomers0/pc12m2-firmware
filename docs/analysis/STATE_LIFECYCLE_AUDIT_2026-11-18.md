# 状态机启停、故障复位与模拟外设复核（第五批）

日期：2026-11-18。仅代码/模拟，不烧写、不操作硬件。分别执行两个项目自己的原BIN与当前新ELF；保留认证放行、菜单/产品/字体及背光定制。

本批未发现新的业务逻辑差异，未改业务源码或固件镜像。修复的是两个项目公共执行器的临时钩子泄漏；补齐启停、急停、故障复位、运行统计及受控外设模型验证。**本批契约通过不等于实施方案S0–S7已全部关闭。**

## 新增执行证据

| 范围 | 本仓执行结果与观察契约 |
| --- | --- |
| 连续启停 | 108条链、每链266拍，共28728拍；主屏/三相监视屏×3种控制手法×2种启动方式×3种辅助模式×3种急停配置。GPIO输入实际持续跨过50拍去抖；逐拍保留SRAM，比较显示参数、GPIO写序、EEPROM事务与介质 |
| 故障复位 | 72例：两个屏×16个单故障位及全位组合×复位启用/禁用。36例NORMAL_RETURN保留故障；36例EXPECTED_NONRETURN实际清故障、关输出、两段各3000轮外层及6000份AA/55喂狗写序，最后确切自跳 |
| 运行计时与落盘 | 64组，每组3拍；30000拍阈值前后、分钟59/60、小时与整字回绕；十二相不套用六相的第三层小时计数与pending/retry逻辑；P0.9真实去抖触发影子/EEPROM写入 |
| 缺相与恢复 | 320组，每组4拍；启用/禁用、三路信号8种组合、计数0/4/5/255/FFFFFFFF与检测周期边界，比较故障产生、下一拍封锁、信号恢复清低三位及实际再启动 |
| 模式切换 | 两条持久链，共1248拍；在同一双机中切换控制手法、启动方式、辅助/急停模式，跨去抖边界，不逐拍重置内部历史 |
| 有限外设模型 | 52组真实ISR调度；先执行真实STR小程序验证模型，再跑实际初始化与ISR，比较SRAM、MMIO写序、GPIO锁存、使能/挂起及清中断结果 |

最终完整回归40/40、官方原BIN A/B、菜单16项正反显及背光28例专项通过。回归/官方A/B采集1983个原分支候选：双出口1277、单出口472、未命中234。额外将14688例全菜单矩阵接入同镜像采集，合并结果见menu_branch_inventory.json；候选统计不是全程序等价率。完整结果见本批`manifest.json`、`final_regression_and_ab.log`及`branch_inventory.json`。门禁失败/未完成时归档工具拒绝生成完成摘要。定制文字与背光沿用各自专项门禁。

全量40模块门禁后另追加PID量程表1272例，原40个BCS分支均命中双出口；累计PID积分函数/包装函数94个条件分支94/94双出口。菜单扩展后原条件分支双出口1506，再补PID后1522、单出口402、未命中59，见pid_branch_inventory.json。这些是同镜像追加专项，不声称已在此前40模块命令内执行。

PID探针初稿把反馈量程选择写到0x1000200C，虽比较一致但未覆盖替代表；原BIN字面量池0x10EE4/0x11134均指向0x10002004。已据此纠正并加40个BCS双出口门禁，初稿仅作探索证据，不作替代表验收。六相对应槽为0x10002024。

## 执行器修复与检出能力

原`run()`每次安装全地址CODE返回钩子，却不删除；传入`writes`时写钩子也滞留到后续调用。一次266拍的分析中，返回检查回调累计执行约1400万次（仅六相采样），而正常业务观察器约10万次。此为验证工具性能/观察生命周期问题，不能称为设备运行故障。

修复：返回钩子限定在返回哨兵地址；在`finally`中释放本次安装的返回/写钩子，保留调用者自己的观察器。正常返回断言继续执行，异常和预算耗尽不变成通过。

- 新门禁验证正常100次调用、两侧预算失败及非法地址异常后均无本次钩子遗留。
- 从HEAD提取旧`run()`反测，新门禁实际报告钩子泄漏；恢复当前运行器后通过。
- 仅在新机仿真内存将`wd_feed`入口替换为直接返回：仍实际走完复位外层并到达自跳，但AA/55写序断言失败。恢复后原BIN/新ELF一致；磁盘原BIN/固件均未改动。
- 修复前被主动中断的长任务日志仅作过程记录，不计为通过；最终108链和完整门禁从头重跑。

## 模型与测试干预契约

显示叶函数替身记录调用与参数；LCD像素/总线由既有完整显示与定制专项负责。EEPROM仅在外部字节边界响应，寄存器封装及业务状态机真实执行。故障复位只加速已定位的2000次纯空转内层，3000次外层及每次喂狗均实际执行，没有整段跳过复位流程。

新增有限MMIO模型按NXP编写的[UM10360](https://datasheet4u.com/pdf/729117/UM10360.pdf)第6、9、21章登记GPIO SET/CLR、MASK、定时器IR W1C与TCR复位、NVIC ISER/ICER/ISPR/ICPR。副作用在实际STR之后的下一条指令前生效；不安装已知会干扰Unicorn的FIO MEM_WRITE钩子。调度限PRIGROUP=0、高5位优先级、显式PRIMASK/BASEPRI/活动优先级输入；不声称实现完整异常控制器。

**FIOCLR是只写寄存器，本模型读回0仅是显式探针假设。** 不以模型通过推断真实AHB对只写寄存器的读行为。当前未模拟电气复用、总线周期、自动定时器、完整异常栈帧/尾链或所有中断临界点。进入自跳证明的是等待契约，没有证明看门狗真实复位时刻；启动向量/数据初始化由现有独立契约覆盖。

干预扫描范围从`test`、`tools/verification`扩展至`tools/audit`，补上此前遗漏的审计入口；逐项加入ID及REVIEW_REQUIRED状态。历史`full_review_2026-11-16`工具也保留在清单，不自动当作当前验收门禁。扫描发现机制不等于证明机制有害，也不自动批准其等价性。

## 尚未关闭与验收边界

本批关闭的是以上有限输入域和调度下的新增契约。原条件分支台账仍需逐项核销缺少出口的可达项；不能把“输出级150/150”、菜单矩阵或本批108条链推广到全程序。测试干预仍需逐条对应作用层次及替代证据；清单中的REVIEW_REQUIRED没有自动变为已批准。

FIOCLR读行为、其它外设未建模语义、更多优先级分组/抢占临界点与全部参数跨积保留未关闭。`pending_branch_outcomes.json`记录本轮未双出口的候选，不把其自动判为不可达；历史数据误反汇编与未解析CFG也必须分别处理。

固件保持72040字节、SHA-256 `0832CC36133114D16BE760A66E8E52438BAAA7262090CE8C9DF823C7CE37018B`。本批未重建业务镜像，使用并校验第四批固定产物；实际加载原BIN、新BIN/ELF及测试/工具源码哈希见manifest。

## 复现

```powershell
python test/emulation/test_state_lifecycle_contract.py --audit
python test/emulation/test_state_runtime_recovery.py
python test/emulation/test_peripheral_semantic_contract.py
python tools/audit/lifecycle_negative_controls.py
python tools/audit/software_branch_inventory.py --full-ab
python tools/audit/branch_cfg_classification.py
python tools/audit/test_intervention_inventory.py
python tools/audit/extend_menu_branch_inventory.py
python tools/audit/branch_cfg_classification.py tmp/menu_branch_inventory.json
python tools/audit/extend_menu_branch_inventory.py --pid-only
python tools/audit/branch_cfg_classification.py tmp/pid_branch_inventory.json
```

`--audit`为108链；默认回归保留覆盖全部手法/启动方式的12条代表链及完整72例故障复位。完整矩阵JSON与日志应先归档，默认回归会另输出其代表链统计。证据目录：`evidence/reverse/reports/state_lifecycle_audit_2026-11-18/`。
