# W8 实机前离线验证记录

> 本文件只保存离线证据和适用边界；硬件执行步骤见 `W8_HARDWARE_TEST_2026-08-22.md`。

## 结论

当前固件可以进入断开门极与功率负载的分级实测，不能由离线结果直接推断带载等价。

| 项目 | 当前结果 |
|---|---|
| 构建尺寸 | `text 61936 / data 3000 / bss 2188` |
| 原始 `LPC1765.bin` SHA-256 | `DD629EAC0243A13CECC826B1FC2B4B54066F7647D8C1D09A62D5CAF7B4F13F65` |
| 当前 `firmware.bin` SHA-256 | `F032EFB70BB3942C4999D7C1F2D0DEBB64125F004C4E19405CAB0DD08F5EAA44` |
| 工具链 | Arm GNU Toolchain 14.2.Rel1，GCC 14.2.1 |
| 测试套件 | 11/11 模块 PASS |
| 输出级 A/B 矩阵 | 144/144 PASS |
| 状态机 A/B 矩阵 | 115/115 PASS |
| TIMER1 矩阵 | 126 例 PASS |
| Modbus 矩阵 | 65 读、320 写 PASS |

## 冻结标签说明

`pre-hw-test-01` 指向 `95345e4`，其旧固件哈希为
`A3DF49A8D5395E94BDEB771FFE87207017A9AAF0C74B9B0272A81052692CA778`。
该标签保存状态机控制流修复前的历史基线，不代表当前上机产物。

## 独立验证证据

`tools/verification/verify_firmware_equivalence.py` 直接执行原始 BIN 与新 ELF，并比较返回值、SRAM 末态及关键 MMIO 写序。

- `output_stage` 覆盖 2 种配置、3 种增益选择、3 种控制模式、4 种故障状态和运行标志，共 144 例。
- `state_machine` 覆盖主界面、菜单导航和参数编辑等 115 条可终止路径。
- 首轮状态机差异定位到输入去抖状态 `0x1000157F/0x10001581`，并进一步修复 case3/4/5/7 的
  `TIMEOUT3` 公共尾部；这些路径已转为回归用例。

## 栈检查边界

使用 `-fstack-usage` 分析 102 个 C 函数：最大单函数静态栈帧 48 B；`output_stage` 40 B、
`state_machine` 24 B、`EINT3_IRQHandler` 24 B。该结果不包含完整调用链和中断嵌套，首次实机
必须用哨兵测量 MSP 最低水位。

## 尚未由离线验证证明

- 实际板卡引脚电平、继电器动作和认证外围是否与资料一致。
- 三相过零相序、6/12 路门极脉冲的真实时序和故障撤销延迟。
- ADC 前端、reg44/45 工程量标定及 EEPROM 掉电保存。
- 中断嵌套下的真实栈水位，以及低压和强电负载行为。

这些项目只能按 `W8_HARDWARE_TEST_2026-08-22.md` 逐级验证。
