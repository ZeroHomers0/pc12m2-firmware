# CI检出目录可移植性修正（2026-11-18）

前批本地完整门禁全部通过并推送后，十二相GitHub Actions运行44228951012在回归导入阶段失败：test_execution_completion.py用ROOT.name == PC12M-2选择验证器；实际检出目录为pc12m2-firmware，误选六相模块。没有发生固件A/B业务比较失败；失败日志及上传证据包保留。

扫描还发现test_terminal_event_contract.py有相同判断。两仓两处均改为按金标准文件pc12m2_orig.bin是否存在识别项目，原机地址、执行完成判定、比较字段和业务源码不变。历史quick12审计脚本的固定本地路径不在CI入口调用链中，保留为历史过程工具。

使用临时Path.name属性探针，把实际项目根目录名报告为unrelated-checkout-name，保留真实文件路径与镜像；在此条件下经加载核对器运行完成门禁及端子事件A/B。六相3个匹配模块、十二相2个匹配模块均通过；端子扫描1176例、去抖88例均无差异。探针只改变用于项目选择的目录名观测值，不改变CPU状态、PC或业务内存。

此修正未修改业务源码或BIN，未烧写。前批冻结证据不改写；本次新增证据在evidence/reverse/reports/ci_checkout_portability_2026-11-18/。远端复跑结果以修正后提交对应的GitHub Actions为准。
