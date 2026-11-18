# 软件压力验证与自动门禁（2026-11-18）

本批按用户“按你的建议继续”的授权，补充关键共享量的中断交错、持续状态长序列、EEPROM提交断电边界，以及当前分支/PR自动门禁。仅做代码和模拟验证，没有烧写、重启或实机操作。业务源码与固件镜像未修改，认证永久放行和已授权显示/背光定制保留。

## 执行范围

- `test/emulation/test_shared_state_preemption_matrix.py`：14种真实读写语义点，各在执行前/后插入ISR、覆盖两控制通道，共56种交错；另外15种显式屏蔽输入。目标包括启动配置、故障置位/清除、输出门控/节拍/相位及Modbus参数/发送槽。实际插入EINT3或UART3函数体，保留中断产生的SRAM/MMIO副作用，恢复主线程CPU上下文后必须正常返回。
- 每个抢占停点均先做无ISR暂停/恢复与完整不中断执行的对照。CODE回调里的`emu_stop`在Thumb IT块里曾延迟，故改用独立执行发现语义地址，再以`emu_start`的精确until地址分段，并断言PC。空ISR必须被缺少实际MMIO副作用的门禁拒绝。
- `test/emulation/test_cross_module_event_sequences.py`：同一CPU、SRAM和EEPROM持续执行；只有明确的重启事件新建CPU，介质原样继承。真实ADC转换/扫描、TIMER0/EINT3、输入扫描、状态机、输出/PID、UART逐字节RX/间隙/TX ISR、菜单密码与保存、故障、复位命令和重启串联。菜单参数保存后、复位后和最终重启后均断言实际参数与EEPROM字节。
- 默认每通道1830个事件；扩展门禁使用3个固定seed×2通道，每条尾部另含300个随机合法交错事件。必须实际进入PID、限流/限压并退出、出现运行与故障，并完成两次介质继承重启。逐事件对比业务SRAM `0x10000000..0x100028FF`、MMIO写迹、显示调用、延时摘要、EEPROM事件/镜像及实际应答。
- 差异记录先冻结完整事件、镜像哈希与首差异签名，再开始缩减，即使中途被打断也保留完整反例；缩减只接受同类可重现差异，异常/超时不算重现。输出函数空跑的仿真内缺陷注入用于验证检出与自动缩减，正常镜像通过新建Session隔离。
- `test/emulation/test_eeprom_commit_cut_matrix.py`：7个多字节参数，在每个实际字节提交之前、之后及正常完成后比较，共35例；新CPU只继承EEPROM介质，重新执行`load_config`，比较恢复SRAM、读写事件及介质。

## 自动门禁与加载核对

`tools/verification/run_software_gates.py --extended`分别执行完整回归、官方原BIN A/B、菜单定制、背光定制、启动适配、既有负向控制和扩展长序列；每个子进程单独记录退出码、日志哈希与镜像加载计数。开始时核对原BIN固定SHA及长度，并冻结当前BIN/ELF/map与固件输入哈希，每步前后检查未变。

`run_attested_entry.py`在仿真器实际加载结束时读取Flash，与金标准/当前BIN逐字节核对；旧ELF加载器只核对其实际装载节，不把未装载填充区说成整BIN加载。核对器不安装业务执行hook、不改变PC或业务返回。独立错误镜像注入必须被拒绝，恢复正确字节后通过。官方验证入口在既有模块中执行主块，防止重新定义加载器绕过核对。

两仓`.github/workflows/build.yml`将本次分支、main推送以及PR/手动触发接入门禁。build任务只有contents:read，成功后归档固件，门禁证据无论成功失败均尝试上传。原滚动发布拆到独立publish任务，必须同时满足本仓main推送及build成功；本次分支、PR、手动和外仓均不发布。事件矩阵及删除条件的负向测试由`test/static/test_workflow_release_policy.py`验证。依赖固定Unicorn 2.1.4、Capstone 5.0.7、PyYAML 6.0.3。

GitHub语义参考：[工作流语法](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)、[任务条件](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-jobs-with-conditions)。本地验证与远端执行状态在本批manifest/交付记录中分别记录，不互相替代。

## 模型与结论边界

- 抢占是单层函数体交错，尚未实现真正Cortex-M异常入栈、嵌套中断、尾链和周期精度。UC_MODE_THUMB不提供可依赖的PRIMASK/BASEPRI硬件行为，屏蔽来自显式调度输入，并结合NVIC使能、pending和priority内存判断；不能说硬件屏蔽已实测。
- 长序列tick为约定函数调度单位，不宣称真实2ms。ADC样本、GPIO电平、UART就绪和I2C外器件由模型提供；显示叶与延时使用既有独立契约摘要。复位命令必须到达原机确切自跳，与正常返回分开判定。
- EEPROM验证的是明确字节提交边界，不证明真实芯片写周期中间的半写电气结果。
- 有限场景通过不能换算成无bug概率或任意RAM、任意中断时刻的全状态等价证明。本批没有发现新业务差异时，不制造源码修复。

## 验证工具修正记录

首轮两仓完整回归分别83/83与58/58通过，实际镜像加载核对通过；之后总门禁汇总把Windows反斜线键与固定正斜线键混用，触发KeyError。已统一使用`Path.as_posix()`，并纳入compiler_flags.txt等构建文本输入，再重跑完整门禁。初次驱动失败日志和最终日志分别保存，不把汇总失败隐藏成整套通过。

## 本项目结果

PC12M-2：完整回归 **58/58**、共享状态抢占 **71/71**、EEPROM提交切点 **35/35** 通过。扩展长序列6条全部通过，合计 **10980事件 / 10132tick / 764次PID**，逐条统计见`cross_module_event_sequences.json`。三类屏蔽、空ISR拒绝、错误镜像拒绝、输出空跑同签名反例缩减以及发布条件负向测试通过。

修正路径汇总后的完整门禁 **7/7** 通过，官方原BIN A/B、菜单/背光、启动适配、负向控制与扩展长序列均有独立退出码与加载核对记录。证据目录：`evidence/reverse/reports/software_stress_ci_2026-11-18/`。首轮汇总失败及其通过的回归日志单独保留。

当前BIN SHA256：`6B9F08E1E0DF0824AD67A5BA040C0E6C262B339801CC46CDEB7EAAB1D7C5E903`，与本批开始时相同。没有新增业务源码修复，没有烧写/实机。本报告冻结的是本地结果；远端CI执行以随后推送提交对应的GitHub Actions结果为准。
