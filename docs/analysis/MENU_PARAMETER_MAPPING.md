# PC12M-2 菜单与参数映射

> 整理日期：2026-08-31。

当前映射来源：

1. 十二相 `state_machine@0x4464` 反汇编；
2. 十二相 `load_config@0x258C` 与 `param_sync@0x3534`；
3. 十二相 Modbus 读写入口；
4. `firmware/globals.c` 的十二相 SRAM 地址；
5. 显示面板文本，仅用于解释菜单文字，不用于推定硬件接线。

菜单参数的唯一可执行定义位于 `firmware/src/07_state_machine.c`、
`firmware/src/06_param_system.c` 和 `firmware/src/08_modbus_dispatch.c`。

已知特殊项：Modbus `0x17/0x18` 读活动 PID 值、写参数银行源值，属于原厂设计。
在 P5 状态机与显示矩阵全部通过前，本文件只作为导航，不宣称菜单行为已 100% 等价。
